#include "hornerito/audio/hornerito_music_player.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <driver/sdspi_host.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>

#include "esp_ae_rate_cvt.h"
#include "esp_audio_dec.h"
#include "esp_mp3_dec.h"

#include "audio_codec.h"
#include "board.h"

#define TAG "HorneritoMusic"

#define SD_MOUNT_POINT "/sdcard"
#define MUSIC_DIR "/sdcard/Music"  // biblioteca musical UNICA de HORNERITO

// Waveshare ESP32-C6-Touch-AMOLED-2.16: la microSD comparte el bus SPI2_HOST
// del display (CLK=GPIO0, MOSI=GPIO1, MISO=GPIO2, mismas líneas que el
// QSPI del LCD) y usa CS=GPIO6, según el ejemplo oficial 04_SD_Card de
// Waveshare. El bus ya está inicializado por el display, por eso NO se
// llama a spi_bus_initialize: solo se añade el dispositivo con su CS.
#define SD_PIN_CS GPIO_NUM_6
#define SD_SPI_HOST SPI2_HOST

// Remuestreo al ritmo del codec, con la configuración que cabe en el heap
// del C6: complexity 1 + PERF_TYPE_MEMORY (menos INRAM a cambio de CPU:
// para 44100->24000 la tabla del componente marca 0.8% en S3@240, ~2% aquí).
// Complexity 2/SPEED pedía 15364 B contiguos para el filtro timevary y el
// heap fragmentado solo tiene 14336 B (6272 B con pista en marcha) → -2
// (ESP_AE_ERR_MEM_LACK) y la música no arrancaba.
#define RATE_CVT_CFG(_src_rate, _dest_rate, _channel)                                        \
    (esp_ae_rate_cvt_cfg_t) {                                                                \
        .src_rate = (uint32_t)(_src_rate), .dest_rate = (uint32_t)(_dest_rate),              \
        .channel = (uint8_t)(_channel), .bits_per_sample = ESP_AUDIO_BIT16, .complexity = 1, \
        .perf_type = ESP_AE_RATE_CVT_PERF_TYPE_MEMORY,                                       \
    }

namespace {

// Tamaños mínimos viables: el heap pulverizado por la sesión (asistente,
// juegos, LVGL) solo tiene piezas gruesas escasas, y el decoder pvmp3 necesita
// ~27.9 KB en piezas >=1976 B. Cada byte guardado aquí es una pieza gruesa
// devuelta al heap en el arranque. Un frame MP3 válido pide <= 4608 B.
constexpr size_t kInputChunkSize = 2048;   // bytes leídos por paso del decoder
constexpr size_t kPcmBufferInit = 4616;    // buffer PCM de salida (frame max 4608 + margen)
constexpr size_t kResliceSamples = 256;    // trozo de remuestreo por llamada (heap fragmentado
                                           // tras montar la cadena mp3: hueco máx. 1888 B)
constexpr size_t kPcmBufferMax = 16384;   // tope: un frame MP3 válido pide <= 4608 B

// Resize sin excepciones: este build aborta ante bad_alloc (tumba el equipo)
// cuando no hay bloque contiguo. Devuelve false en vez de tirar el aborto y el
// caller descarta el frame/pista.
// ponytail: chequeo-then-resize tiene una carrera residual de ~µs con otras
// tareas; si algún día falla en caliente, reservar el máximo en kPlay.
template <typename T>
bool SafeResize(std::vector<T>& v, size_t n) {
    if (v.capacity() >= n) {
        v.resize(n);
        return true;
    }
    if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < n * sizeof(T) + 512) {
        return false;
    }
    v.resize(n);
    return true;
}

std::string ToLower(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

const char* BaseName(const std::string& path) {
    auto pos = path.find_last_of('/');
    return pos == std::string::npos ? path.c_str() : path.c_str() + pos + 1;
}

// Recoge *.mp3 bajo dir_path en profundidad (/sdcard/Music y subcarpetas,
// profundidad máxima 3). La raíz /sdcard NO se lista (biblioteca única).
bool CollectMp3(const std::string& dir_path, int depth, std::vector<std::string>& out) {
    DIR* dir = opendir(dir_path.c_str());
    if (dir == nullptr) {
        return depth == 0 ? false : true;
    }
    if (depth > 3) {  // profundidad máxima: raíz/Music/subcarpeta/subsubcarpeta
        closedir(dir);
        return true;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        std::string path = dir_path + "/" + name;
        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            CollectMp3(path, depth + 1, out);
            continue;
        }
        auto lower = ToLower(name);
        if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".mp3") == 0) {
            out.push_back(path);
        }
    }
    closedir(dir);
    return true;
}

}  // namespace

HorneritoMusicPlayer& HorneritoMusicPlayer::GetInstance() {
    static HorneritoMusicPlayer instance;
    return instance;
}

HorneritoMusicPlayer::HorneritoMusicPlayer() {
    in_buf_.resize(kInputChunkSize);
    pcm_buf_.resize(kPcmBufferInit);
    // NOTA: NO abrir/precargar decoder_ aquí. Reservar el estado mp3dec
    // (22908 B contiguos) desde el arranque hambrienta el pico de wakenet
    // (~58 kB a los 12.5 s): esp_mel_filter_init recibe NULL y crashea.
    // DecodeChunk se fuerza a la ruta pvmp3 (1968 B), que siempre cabe.
    xTaskCreate(TaskEntry, "hornerito_music", 6144, this, 5, nullptr);
}

void HorneritoMusicPlayer::TaskEntry(void* arg) {
    static_cast<HorneritoMusicPlayer*>(arg)->TaskLoop();
}

void HorneritoMusicPlayer::RequestRefresh() {
    std::lock_guard<std::mutex> lock(mutex_);
    command_ = Command::kRefresh;
}

size_t HorneritoMusicPlayer::SongCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return songs_.size();
}

std::vector<std::string> HorneritoMusicPlayer::SongTitles() const {
    std::vector<std::string> titles;
    std::lock_guard<std::mutex> lock(mutex_);
    titles.reserve(songs_.size());
    for (const auto& path : songs_) {
        titles.emplace_back(BaseName(path));
    }
    return titles;
}

std::string HorneritoMusicPlayer::CurrentTitle() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto index = current_index_.load();
    if (index >= songs_.size()) {
        return "";
    }
    return BaseName(songs_[index]);
}

void HorneritoMusicPlayer::PlayIndex(size_t index) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        command_ = Command::kPlay;
        command_index_ = index;
    }
    current_index_ = index;
    if (SongCount() > 0) {
        state_ = HorneritoMusicState::kPlaying;
    }
}

void HorneritoMusicPlayer::TogglePause() {
    auto st = state_.load();
    if (st == HorneritoMusicState::kPlaying) {
        state_ = HorneritoMusicState::kPaused;
    } else if (st == HorneritoMusicState::kPaused) {
        state_ = HorneritoMusicState::kPlaying;
    } else {
        PlayIndex(current_index_.load());
    }
}

void HorneritoMusicPlayer::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    command_ = Command::kStop;
    if (state_.load() == HorneritoMusicState::kPlaying ||
        state_.load() == HorneritoMusicState::kPaused) {
        state_ = HorneritoMusicState::kStopped;
    }
}

void HorneritoMusicPlayer::Next() {
    auto count = SongCount();
    if (count == 0) return;
    PlayIndex((current_index_.load() + 1) % count);
}

void HorneritoMusicPlayer::Prev() {
    auto count = SongCount();
    if (count == 0) return;
    auto index = current_index_.load();
    PlayIndex(index == 0 ? count - 1 : index - 1);
}

bool HorneritoMusicPlayer::EnsureMounted() {
    return MountSd();  // MountSd serializa con el task bajo mutex_
}

void HorneritoMusicPlayer::BeginTransfer() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        transfer_active_ = true;
        command_ = Command::kNone;  // descarta play/stop pendientes
        if (state_.load() == HorneritoMusicState::kPlaying) {
            resume_index_ = static_cast<int>(current_index_.load());
            state_ = HorneritoMusicState::kPaused;
        }
    }
    // Espera a que el task termine cualquier E/S de archivo en curso.
    // El task fija io_active_=true dentro de mutex_ antes de leer comandos,
    // por lo que si había E/S en vuelo este bucle la ve y espera.
    uint32_t waited = 0;
    while (io_active_.load() && waited < 2000) {
        vTaskDelay(pdMS_TO_TICKS(10));
        waited += 10;
    }
    if (io_active_.load()) {
        ESP_LOGW(TAG, "El reproductor no quedó inactivo tras 2 s; se continúa igualmente");
    }
}

void HorneritoMusicPlayer::EndTransfer() {
    std::lock_guard<std::mutex> lock(mutex_);
    transfer_active_ = false;
    refresh_requested_ = true;  // rescanea y reanuda (independiente del slot command_)
}

bool HorneritoMusicPlayer::MountSd() {
    std::lock_guard<std::mutex> lock(mutex_);  // montaje único (task o transferencia)
    if (mounted_) {
        return true;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 5;
    mount_config.allocation_unit_size = 512;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;  // bus ya inicializado por el display
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = SD_PIN_CS;
    slot.host_id = SD_SPI_HOST;

    sdmmc_card_t* card = nullptr;
    auto err = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot, &mount_config, &card);
    if (err != ESP_OK || card == nullptr) {
        ESP_LOGW(TAG, "No se pudo montar la microSD: %s", esp_err_to_name(err));
        card_ = nullptr;
        return false;
    }
    card_ = card;
    mounted_ = true;
    ESP_LOGI(TAG, "microSD montada en %s", SD_MOUNT_POINT);
    return true;
}

bool HorneritoMusicPlayer::ScanSongs() {
    // Biblioteca musical única: solo /sdcard/Music (y subcarpetas). Si la
    // carpeta no existe aún, la lista queda vacía (la crea la transferencia).
    std::vector<std::string> found;
    CollectMp3(MUSIC_DIR, 0, found);

    std::sort(found.begin(), found.end(),
              [](const std::string& a, const std::string& b) { return ToLower(a) < ToLower(b); });

    std::lock_guard<std::mutex> lock(mutex_);
    songs_ = std::move(found);
    return mounted_;  // false solo si no hay tarjeta montada
}

// Abre el remuestreador ANTES de que fopen/decode fragmenten el heap (se
// llama desde el task en kPlay, nunca desde la UI, para no pisar rate_cvt_).
// Asume 44100 Hz (la inmensa mayoría de MP3); si el archivo real tiene otro
// ritmo, DeliverPcm lo reabre y, si no cabe, la pista aborta con log.
void HorneritoMusicPlayer::PreopenRateCvt(uint32_t assumed_rate) {
    if (rate_cvt_ != nullptr) {
        return;  // nunca-close: DeliverPcm gestiona cambios de ritmo
    }
    auto* codec = Board::GetInstance().GetAudioCodec();
    uint32_t out_rate = codec->output_sample_rate();
    if (assumed_rate == out_rate) {
        return;  // sin remuestreo
    }
    esp_ae_rate_cvt_cfg_t cfg = RATE_CVT_CFG(assumed_rate, out_rate, 1);
    auto ret = esp_ae_rate_cvt_open(&cfg, &rate_cvt_);
    if (rate_cvt_ == nullptr) {
        ESP_LOGW(TAG, "No se pudo preabrir el remuestreador (%d) [libre %u, bloque max %u]", ret,
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        return;
    }
    rate_cvt_src_ = assumed_rate;
    ESP_LOGI(TAG, "Remuestreador preabierto %u->%u Hz [libre %u, bloque max %u]", assumed_rate,
             out_rate, heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    // Calienta el buffer interno del remuestreador (2596 B): la primera
    // process() lo asigna y, si esperamos al primer frame, la apertura del
    // decoder (~28 kB con una pieza contigua de ~8.5 kB) ya dejó el bloque
    // máximo en 1760 B → "Fallo el remuestreo (-2)" y música muda tras usar
    // WiFi/asistente. Se resguarda casi todo el bloque más grande mientras
    // calienta para que la asignación caiga en OTRO fragmento: así el
    // decoder conserva después sus ~8.5 kB contiguos y el bloque vuelve a
    // quedar intacto al liberar. Sin fragmento >=2596 B falla como antes
    // (se loguea; no empeora).
    const uint32_t kWarmSamples = 1152;  // máx. muestras mono por trama MP3
    uint32_t max_out = 0;
    esp_ae_rate_cvt_get_max_out_sample_num(rate_cvt_, kWarmSamples, &max_out);
    void* hold = nullptr;
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    if (largest > 2596 + 64) {
        hold = heap_caps_malloc(largest - 2596 + 64, MALLOC_CAP_INTERNAL);
    }
    auto* sil = static_cast<int16_t*>(
        heap_caps_calloc(kWarmSamples, sizeof(int16_t), MALLOC_CAP_INTERNAL));
    auto* outb = static_cast<int16_t*>(heap_caps_malloc(
        (static_cast<size_t>(max_out) + 16) * sizeof(int16_t), MALLOC_CAP_INTERNAL));
    if (sil != nullptr && outb != nullptr) {
        uint32_t actual = static_cast<uint32_t>(max_out) + 16;
        auto wret = esp_ae_rate_cvt_process(rate_cvt_, (esp_ae_sample_t)sil, kWarmSamples,
                                             (esp_ae_sample_t)outb, &actual);
        ESP_LOGI(TAG, "Remuestreador calentado (%d) [libre %u, bloque max %u]", wret,
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    } else {
        ESP_LOGW(TAG, "Sin memoria para calentar el remuestreador [libre %u, bloque max %u]",
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    }
    if (outb != nullptr) {
        heap_caps_free(outb);
    }
    if (sil != nullptr) {
        heap_caps_free(sil);
    }
    if (hold != nullptr) {
        heap_caps_free(hold);
    }
}

bool HorneritoMusicPlayer::OpenStream(size_t index) {
    std::string path;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= songs_.size()) {
            return false;
        }
        path = songs_[index];
    }

    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        ESP_LOGE(TAG, "No se pudo abrir %s", path.c_str());
        return false;
    }

    if (decoder_ == nullptr) {
        if (esp_mp3_dec_open(nullptr, 0, &decoder_) != ESP_AUDIO_ERR_OK || decoder_ == nullptr) {
            ESP_LOGE(TAG, "No se pudo crear el decodificador MP3");
            decoder_ = nullptr;
            fclose(file);
            return false;
        }
    } else {
        esp_mp3_dec_reset(decoder_);
    }

    // rate_cvt_ NO se cierra: se preabrió con el heap intacto y se reutiliza
    // (never-close, igual que AudioService). Si el archivo tiene otro ritmo,
    // DeliverPcm lo reabre.
    src_rate_ = 0;
    src_channels_ = 0;
    pending_ = 0;

    file_ = file;
    current_index_ = index;
    return true;
}

void HorneritoMusicPlayer::CloseStream() {
    if (file_ != nullptr) {
        fclose(file_);
        file_ = nullptr;
    }
    pending_ = 0;
    // rate_cvt_ y decoder_ se conservan (reset en OpenStream): reabrirlos con
    // el heap fragmentado era lo que fallaba con -2 (ESP_AE_ERR_MEM_LACK).
}

bool HorneritoMusicPlayer::DeliverPcm(int16_t* pcm, uint32_t bytes) {
    size_t samples = bytes / sizeof(int16_t);
    if (samples == 0) {
        return true;
    }

    auto* codec = Board::GetInstance().GetAudioCodec();
    uint32_t out_rate = codec->output_sample_rate();

    // Abre el remuestreador si falta o si el preabierto era de otro ritmo
    // (archivo que no era 44100 Hz): reabrir en este punto puede fallar si el
    // heap ya está fragmentado, por eso se preabre en kPlay.
    if (src_rate_ > 0 && src_rate_ != out_rate &&
        (rate_cvt_ == nullptr || rate_cvt_src_ != src_rate_)) {
        if (rate_cvt_ != nullptr) {
            esp_ae_rate_cvt_close(rate_cvt_);
            rate_cvt_ = nullptr;
        }
        esp_ae_rate_cvt_cfg_t cfg = RATE_CVT_CFG(src_rate_, out_rate, 1);
        auto ret = esp_ae_rate_cvt_open(&cfg, &rate_cvt_);
        if (rate_cvt_ == nullptr) {
            ESP_LOGE(TAG, "No se pudo crear el remuestreador (%d) [libre %u, bloque max %u]", ret,
                     heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            return false;
        }
        rate_cvt_src_ = src_rate_;
        ESP_LOGI(TAG, "Remuestreando MP3 de %u Hz a %u Hz [libre %u, bloque max %u]", src_rate_,
                 out_rate, heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    }
    // Solo pasa por el remuestreador si está abierto para ESTE ritmo: un
    // preabierto a 44100 con un archivo de 24000 Hz no debe usarse.
    const bool use_cvt = rate_cvt_ != nullptr && rate_cvt_src_ == src_rate_;

    const int16_t* data = pcm;
    size_t count = samples;

    // Downmix estereo -> mono in situ en frame.buffer: con la cadena mp3
    // montada el bloque máximo cae a ~2176 B y mono_buf_ pedía 2304+512
    // ("Sin memoria para downmix") → TODOS los frames se descartaban y la
    // música salía muda. Es seguro: el índice de escritura (i) queda siempre
    // por detrás del de lectura (2i) y pcm_buf_ se reescribe en cada decode.
    if (src_channels_ >= 2) {
        count = samples / 2;
        for (size_t i = 0; i < count; i++) {
            pcm[i] = static_cast<int16_t>((static_cast<int32_t>(pcm[2 * i]) + pcm[2 * i + 1]) / 2);
        }
    }

    std::vector<int16_t>* out = nullptr;
    if (use_cvt) {
        // Remuestrea y emite en trozos de kResliceSamples: a salida 48000 Hz
        // un trama completo (1152 muestras) pide max_out=1381+16 (2794 B +
        // 512 de margen = 3306 B) y con la cadena mp3 montada el bloque
        // máximo cae a 1888 B → TODOS los frames fallaban ("Sin memoria para
        // salida remuestreada") y la música salía muda. Por trozos de 256 se
        // piden ~423 muestras (~846 + 512 = 1358 ≤ 1888): entra en el hueco
        // y, desde el primer frame, la capacidad cubre todos los trozos
        // siguientes (sin allocs en caliente). Precrescer out_buf_ con el
        // heap intacto NO sirve: rompe el hueco de 14336 B que la cadena
        // necesita (pieza 7 = 8208 B → "Fail to init MP3 decoder ret 10").
        uint32_t max_out = 0;
        esp_ae_rate_cvt_get_max_out_sample_num(rate_cvt_,
                                                static_cast<uint32_t>(kResliceSamples),
                                                &max_out);
        if (!SafeResize(out_buf_, max_out + 16)) {
            ESP_LOGW(TAG, "Sin memoria para salida remuestreada (%u) [libre %u, bloque max %u]",
                     (unsigned)(max_out + 16), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            return false;
        }
        if (!codec->output_enabled()) {
            codec->EnableOutput(true);
        }
        for (size_t off = 0; off < count; off += kResliceSamples) {
            const uint32_t n = static_cast<uint32_t>(count - off < kResliceSamples
                                                         ? count - off
                                                         : kResliceSamples);
            uint32_t actual = max_out + 16;
            auto ret = esp_ae_rate_cvt_process(rate_cvt_, (esp_ae_sample_t)(data + off), n,
                                                (esp_ae_sample_t)out_buf_.data(), &actual);
            if (ret != ESP_AE_ERR_OK) {
                ESP_LOGE(TAG, "Fallo el remuestreo (%d)", ret);
                return false;
            }
            out_buf_.resize(actual);
            codec->OutputData(out_buf_);
        }
        return true;
    }
    if (!SafeResize(out_buf_, count)) {
        return false;
    }
    out_buf_.assign(data, data + count);
    out = &out_buf_;

    // El codec solo escribe si está habilitado; AudioService lo desactiva al
    // terminar su reproducción, así que lo reactivamos si hace falta.
    if (!codec->output_enabled()) {
        codec->EnableOutput(true);
    }
    codec->OutputData(*out);
    return true;
}

bool HorneritoMusicPlayer::DecodeChunk() {
    // Cadencia de frames para diagnóstico: sano = OK ~38/s, err ~0-1;
    // si err se dispara, el problema de audio es de decode.
    static uint32_t ok_frames = 0, err_frames = 0;
    static int64_t stat_us = 0;
    int64_t now_us = esp_timer_get_time();
    if (now_us - stat_us >= 5000000) {
        stat_us = now_us;
        ESP_LOGI(TAG, "frames OK=%u err=%u [libre %u, bloque max %u]", ok_frames, err_frames,
                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
        ok_frames = 0;
        err_frames = 0;
    }

    // Cabecera Layer III válida completa: sync de 11 bits y versión, bitrate
    // y sample-rate no reservados. Además de saltar tags ID3, rechaza los
    // falsos syncs de los payloads (las oleadas de "error:12").
    auto es_cabecera = [](const uint8_t* p, uint32_t len) {
        return len >= 3 && p[0] == 0xFF && (p[1] & 0xE0) == 0xE0 && (p[1] & 6) == 2 &&
               ((p[1] >> 3) & 3) != 1 && (p[2] >> 4) != 0x0F && ((p[2] >> 2) & 3) != 3;
    };

    while (true) {
        // Cola de la iteración anterior: una trama que cruza el límite de
        // lectura se conserva (memmove al frente) y se completa con datos
        // nuevos en vez de descartarse; antes se perdía 1 trama por chunk de
        // 4096 B = skip/click constante durante toda la canción.
        size_t pend = pending_;
        size_t n = fread(in_buf_.data() + pend, 1, in_buf_.size() - pend, file_);
        if (pend + n == 0) {
            return false;  // EOF (o error de lectura: tarjeta retirada)
        }
        pending_ = 0;
        bool need_more = false;

        esp_audio_dec_in_raw_t raw = {};
        raw.buffer = in_buf_.data();
        raw.len = static_cast<uint32_t>(pend + n);

        while (raw.len > 0 && !need_more) {
            // esp_mp3_dec_decode elige decoder según (buffer[1] & 6):
            //   == 2 (Layer III) → pvmp3, pide solo 1968 B (siempre caben);
            //   != 2             → mp3dec, pide 22908 B contiguos → con el heap
            //                       fragmentado de reproducción falla (error -2
            //                       "no memory for MP2") y mataba la pista a los
            //                       2-3 s. Solo decode con cabecera válida.
            if (!es_cabecera(raw.buffer, raw.len)) {
                uint32_t i = 0;
                while (i < raw.len && !es_cabecera(raw.buffer + i, raw.len - i)) {
                    i++;
                }
                if (i >= raw.len) {
                    // Sin sync válido en el resto. Si este llamado sí trajo
                    // datos nuevos y la cola cabe en el buffer, leer más y
                    // reintentar (cabecera partida por el límite del chunk);
                    // si no, descartar (progreso garantizado).
                    if (n > 0 && raw.len < in_buf_.size()) {
                        memmove(in_buf_.data(), raw.buffer, raw.len);
                        pending_ = raw.len;
                        need_more = true;
                    } else {
                        ESP_LOGD(TAG, "Sin sync Layer III, se descartan %u bytes", raw.len);
                    }
                    break;
                }
                raw.buffer += i;
                raw.len -= i;
            }

            esp_audio_dec_out_frame_t frame = {};
            frame.buffer = pcm_buf_.data();
            frame.len = static_cast<uint32_t>(pcm_buf_.size());

            esp_audio_dec_info_t info = {};
            auto ret = esp_mp3_dec_decode(decoder_, &raw, &frame, &info);

            if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                if (frame.needed_size <= pcm_buf_.size()) {
                    return false;  // inconsistente: pide menos de lo que hay
                }
                if (frame.needed_size <= kPcmBufferMax && SafeResize(pcm_buf_, frame.needed_size)) {
                    continue;
                }
                // Sin memoria para crecer (o needed > tope): cae en el manejo
                // de trama corrupta de abajo (avanza 1 byte) en vez de abortar.
                // needed_size basura de una trama corrupta: NO redimensionar.
                // Un resize sin tope crecía pcm_buf_ a ~37 kB con un valor
                // inventado por el decoder: clavaba ~29 kB permanentes que
                // fragmentaban el heap (bloque máx. 6272 B) y mataban después el
                // remuestreador (-2) y el TTS del asistente (abort por OOM).
                // Cae en el manejo de trama corrupta de abajo (avanza 1 byte).
            }
            if (ret != ESP_AUDIO_ERR_OK) {
                err_frames++;
                // Trama corrupta, falso sync o trama incompleta: si el decoder
                // no avanzó, hay que decidir entre guardar la cola (falta data)
                // o re-sincronizar avanzando 1 byte.
                if (raw.consumed == 0 || raw.consumed > raw.len) {
                    // 1 log/s como máximo, con los números de heap por si reaparece
                    // un OOM del decoder.
                    static int64_t last_err_us = 0;
                    int64_t now_err_us = esp_timer_get_time();
                    if (now_err_us - last_err_us >= 1000000) {
                        last_err_us = now_err_us;
                        ESP_LOGW(TAG, "MP3 ilegible (error %d, needed %u) [libre %u, bloque max %u]",
                                 ret, frame.needed_size,
                                 heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
                    }
                    if (n > 0 && raw.len < in_buf_.size()) {
                        // Falta data de la trama (cruza el límite del chunk o
                        // está a medias): conservar y leer el resto.
                        memmove(in_buf_.data(), raw.buffer, raw.len);
                        pending_ = raw.len;
                        need_more = true;
                        break;
                    }
                    raw.buffer += 1;
                    raw.len -= 1;
                    continue;
                }
                raw.buffer += raw.consumed;
                raw.len -= raw.consumed;
                continue;
            }

            if (frame.decoded_size > 0) {
                if (src_rate_ == 0 && info.sample_rate > 0) {
                    src_rate_ = info.sample_rate;
                    src_channels_ = info.channel > 0 ? info.channel : 1;
                    ESP_LOGI(TAG, "MP3 %u Hz, %u canal(es) [libre %u, bloque max %u]", src_rate_,
                             src_channels_, heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
                }
                if (!DeliverPcm(reinterpret_cast<int16_t*>(pcm_buf_.data()), frame.decoded_size)) {
                    return false;
                }
                ok_frames++;
            }

            if (raw.consumed == 0 || raw.consumed > raw.len) {
                break;
            }
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
        }

        // need_more: pending_ conserva la cola y esta llamada TERMINA. TaskLoop
        // vuelve en 1 ms, relee state_ y command_ (pausa/siguiente/stop) y en la
        // próxima llamada lee los datos que faltan y completa la trama. Salir
        // aquí era crítico: con un while(true) que releía el archivo, el task
        // nunca volvía a TaskLoop y pausa/siguiente/stop se ignoraban (y el
        // log de frames se imprimía una sola vez por partida).
        return true;
    }
}

void HorneritoMusicPlayer::DoRefresh() {
    CloseStream();
    int resume = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        resume = resume_index_;
        resume_index_ = -1;
    }
    if (MountSd() && ScanSongs()) {
        auto count = SongCount();
        ESP_LOGI(TAG, "Biblioteca /sdcard/Music: %u cancion(es)", static_cast<unsigned>(count));
        if (resume >= 0 && static_cast<size_t>(resume) < count && OpenStream(resume)) {
            state_ = HorneritoMusicState::kPlaying;  // reanuda la pista previa
            return;
        }
        current_index_ = 0;
        state_ = count > 0 ? HorneritoMusicState::kStopped : HorneritoMusicState::kNoSongs;
    } else {
        state_ = HorneritoMusicState::kNotMounted;
    }
}

void HorneritoMusicPlayer::TaskLoop() {
    for (;;) {
        Command command = Command::kNone;
        size_t index = 0;
        const bool refresh = refresh_requested_.exchange(false);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            command = command_;
            index = command_index_;
            command_ = Command::kNone;
            if (transfer_active_) {
                command = Command::kNone;  // sin E/S de audio durante la transferencia
            }
            // io_active_ se activa dentro de mutex_: BeginTransfer (que toma el
            // mutex antes de esperar) siempre verá esta E/S si está en curso.
            io_active_ = true;
        }

        switch (command) {
            case Command::kRefresh:
                DoRefresh();
                break;
            case Command::kStop:
                CloseStream();
                // Libera lo pesado: el decoder (~29 kB repartidos en trozos
                // que dejan el bloque máximo en 3776 B), el remuestreador
                // (5.4 kB) y los buffers. Con el heap así partido el TLS del
                // asistente no conseguía 4437 B y abortaba (operator new).
                if (decoder_ != nullptr) {
                    esp_mp3_dec_close(decoder_);
                    decoder_ = nullptr;
                }
                if (rate_cvt_ != nullptr) {
                    esp_ae_rate_cvt_close(rate_cvt_);
                    rate_cvt_ = nullptr;
                    rate_cvt_src_ = 0;
                }
                out_buf_.clear();
                out_buf_.shrink_to_fit();
                pcm_buf_.clear();
                pcm_buf_.shrink_to_fit();
                pcm_buf_.resize(kPcmBufferInit);  // no nulo: frame.buffer apunta aquí
                ESP_LOGI(TAG, "Recursos liberados [libre %u, bloque max %u]",
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
                state_ = SongCount() > 0 ? HorneritoMusicState::kStopped
                                         : (mounted_ ? HorneritoMusicState::kNoSongs
                                                     : HorneritoMusicState::kNotMounted);
                break;
            case Command::kPlay:
                PreopenRateCvt(44100);  // con el heap intacto, antes de fopen/decode
                CloseStream();
                // Sondeo diagnóstico: ¿cabe ahora la cadena del decoder pvmp3
                // (1968+4224+2304+2312+4224+2304+2312+8192 con redondeo de
                // cabecera)? Solo mide cuando la cadena va a allocarse de
                // verdad (decoder_ == nullptr): con la sesión anterior viva
                // OpenStream la reutiliza y el sondeo daba "NO cabe" falso.
                // Si no cabe, la pista no arranca (ret 10) y el log dice qué
                // pieza falta y con qué heap, sin adivinar.
                if (decoder_ == nullptr) {
                    static const size_t kChain[] = {1976, 4232, 2312, 2320,
                                                    4232, 2312, 2320, 8208};
                    void* held[8] = {};
                    int fail = 8;
                    for (int i = 0; i < 8; i++) {
                        held[i] = heap_caps_malloc(kChain[i], MALLOC_CAP_DEFAULT);
                        if (held[i] == nullptr) {
                            fail = i;
                            break;
                        }
                    }
                    ESP_LOGW(TAG, "Sondeo cadena mp3: %s (pieza %d, %u B) [libre %u, bloque max %u]",
                             fail == 8 ? "cabe" : "NO cabe", fail < 8 ? fail : -1,
                             fail < 8 ? static_cast<unsigned>(kChain[fail]) : 0u,
                             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                             static_cast<unsigned>(
                                 heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
                    for (int i = 0; i < fail; i++) {
                        free(held[i]);
                    }
                }
                if (OpenStream(index)) {
                    state_ = HorneritoMusicState::kPlaying;
                } else {
                    state_ = HorneritoMusicState::kError;
                }
                break;
            default:
                break;
        }
        if (refresh) {
            DoRefresh();  // rescaneo pedido por EndTransfer
        }

        auto state = state_.load();
        uint32_t delay_ms = 100;
        if (state == HorneritoMusicState::kPlaying) {
            if (transfer_active_) {
                delay_ms = 50;  // transferencia en curso: no se decodifica
            } else {
                bool ok = DecodeChunk();
                // El decode + la escritura I2S bloqueante marcan el ritmo:
                // los 100 ms de reserva hacían el ciclo más lento que el
                // tiempo real con MP3 de bitrate alto (4096 B ≈ 102 ms a
                // 320 kbps) → underrun → "se corta y salta".
                delay_ms = 1;
                if (!ok && !transfer_active_) {
                    // Fin de pista: pasa a la siguiente; al final de la lista, para.
                    auto next = current_index_.load() + 1;
                    CloseStream();
                    if (next < SongCount() && OpenStream(next)) {
                        state_ = HorneritoMusicState::kPlaying;
                    } else {
                        state_ = next < SongCount() ? HorneritoMusicState::kError
                                                    : HorneritoMusicState::kStopped;
                    }
                }
            }
        }
        io_active_ = false;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}
