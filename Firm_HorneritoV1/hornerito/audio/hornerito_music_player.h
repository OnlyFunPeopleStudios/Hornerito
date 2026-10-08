// Reproductor de música local de HORNERITO (v1: microSD + MP3).
// - Monta la microSD en el bus SPI ya inicializado por el display
//   (SPI2_HOST, CS=GPIO6, pines CLK/MOSI/MISO 0/1/2 compartidos con el
//   LCD QSPI, según el ejemplo oficial 04_SD_Card de Waveshare).
// - Decodifica MP3 con esp_audio_codec (esp_mp3_dec) y remuestrea al ritmo
//   del codec con esp_ae_rate_cvt (mismo patrón que AudioService).
// - Toda la E/S de audio pasa por el AudioCodec existente (sin tocar drivers).
//
// Los cambios de estado (play/pausa/stop) los hace la UI; el task solo
// decodifica, avanza de pista al llegar a EOF y reporta errores.
#ifndef HORNERITO_MUSIC_PLAYER_H_
#define HORNERITO_MUSIC_PLAYER_H_

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

enum class HorneritoMusicState {
    kNotMounted,  // microSD no montada / sin tarjeta
    kNoSongs,     // tarjeta montada, sin archivos .mp3
    kStopped,
    kPlaying,
    kPaused,
    kError,
};

class HorneritoMusicPlayer {
public:
    static HorneritoMusicPlayer& GetInstance();

    // Relee la tarjeta (montaje + lista de canciones). Asíncrono: lo hace el task.
    void RequestRefresh();

    size_t SongCount() const;
    std::vector<std::string> SongTitles() const;  // nombres de archivo sin ruta

    void PlayIndex(size_t index);
    void TogglePause();  // reproduce/pausa; si está detenido, reanuda la actual
    void Stop();
    void Next();
    void Prev();

    HorneritoMusicState state() const { return state_.load(); }
    size_t current_index() const { return current_index_; }
    std::string CurrentTitle() const;  // nombre del archivo actual

    // --- Acceso exclusivo para la transferencia de archivos (HTTP) ---
    // Garantiza que el reproductor no toca la FAT mientras se escribe:
    // pausa si estaba reproduciendo, descarta comandos, espera a que no haya
    // E/S en curso (hasta 2 s) y, con EndTransfer, rescanea la lista
    // (/sdcard y subcarpetas como /sdcard/Music) y reanuda la pista si procede.
    bool EnsureMounted();       // monta la microSD si aún no lo está
    void BeginTransfer();       // llama antes de escribir (espera E/S idle)
    void EndTransfer();         // llama al cerrar/sincronizar (rescanea y reanuda)
    bool transfer_active() const { return transfer_active_.load(); }

private:
    HorneritoMusicPlayer();
    ~HorneritoMusicPlayer() = default;

    enum class Command { kNone, kPlay, kStop, kRefresh };

    static void TaskEntry(void* arg);
    void TaskLoop();
    void DoRefresh();  // montaje + escaneo (+ reanuda resume_index_ si hay)

    bool MountSd();
    bool ScanSongs();
    // Abre el remuestreador con el heap intacto (asume el ritmo típico de los
    // MP3, 44100 Hz). Solo desde el task (kPlay) para no pisar rate_cvt_.
    void PreopenRateCvt(uint32_t assumed_rate);
    bool OpenStream(size_t index);
    void CloseStream();
    bool DecodeChunk();                          // false = EOF o error de la pista
    bool DeliverPcm(int16_t* pcm, uint32_t bytes);      // false = no se pudo convertir

    mutable std::mutex mutex_;
    Command command_ = Command::kNone;
    size_t command_index_ = 0;

    std::vector<std::string> songs_;  // rutas completas bajo /sdcard
    std::atomic<HorneritoMusicState> state_{HorneritoMusicState::kNotMounted};
    std::atomic<size_t> current_index_{0};

    // Acceso exclusivo de transferencia (mutex_ lo protege junto a command_)
    std::atomic<bool> transfer_active_{false};
    std::atomic<bool> io_active_{false};          // true mientras hay E/S de archivo
    std::atomic<bool> refresh_requested_{false};  // EndTransfer pide rescaneo
    int resume_index_ = -1;                       // pista a reanudar tras el rescaneo

    // microSD
    bool mounted_ = false;
    void* card_ = nullptr;  // sdmmc_card_t*

    // pista actual
    FILE* file_ = nullptr;
    void* decoder_ = nullptr;   // handle de esp_mp3_dec
    void* rate_cvt_ = nullptr;  // handle de esp_ae_rate_cvt (nunca-close)
    uint32_t rate_cvt_src_ = 0;  // ritmo para el que se abrió rate_cvt_
    uint32_t src_rate_ = 0;
    uint8_t src_channels_ = 0;
    std::vector<uint8_t> in_buf_;
    uint32_t pending_ = 0;  // cola del chunk que cruza el límite de lectura
    std::vector<uint8_t> pcm_buf_;
    std::vector<int16_t> out_buf_;
};

#endif  // HORNERITO_MUSIC_PLAYER_H_
