#include "hornerito/transfer/hornerito_transfer.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#include <esp_log.h>

#include "hornerito/audio/hornerito_music_player.h"
#include "hornerito/calendar/hornerito_calendar.h"

#define TAG "HorneritoTransfer"

#define SD_MOUNT_POINT "/sdcard"
#define MUSIC_DIR "/sdcard/Music"

namespace {

// Tope de seguridad por archivo (el espacio real lo impone la propia tarjeta).
constexpr uint64_t kMaxUploadBytes = 256ULL * 1024 * 1024;
constexpr int kRecvRetries = 6;  // reintentos ante timeout de red (5 s c/u)

// Página de subida embebida: PUT por archivo con progreso en el navegador.
const char kIndexHtml[] = R"rawliteral(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HORNERITO - Archivos</title>
<style>
body{font-family:system-ui,sans-serif;background:#101418;color:#e8eef4;margin:0;padding:1.5rem}
main{max-width:40rem;margin:auto}
h1{color:#5bc8f5;letter-spacing:.12em}
label{display:block;border:2px dashed #2e4058;border-radius:14px;padding:2rem;text-align:center;cursor:pointer}
label:hover{border-color:#5bc8f5}
input[type=file]{display:none}
progress{width:100%;height:14px;margin-top:1rem}
#s{margin-top:1rem;min-height:1.4em}
small{color:#8899aa}
</style></head>
<body><main>
<h1>HORNERITO</h1>
<small>Copia archivos a la microSD: destino /sdcard/Music (el reproductor lista los .mp3)</small>
<label>Elige archivo(s) para subir<input type="file" id="f" multiple></label>
<progress id="p" max="100" value="0" hidden></progress>
<div id="s"></div>
<small>No cierres esta página hasta terminar.</small>
</main>
<script>
const f=document.getElementById('f'),s=document.getElementById('s'),p=document.getElementById('p');
f.onchange=async ev=>{
 for(const file of ev.target.files){
  s.textContent='Subiendo '+file.name+'...';p.hidden=false;
  await new Promise(res=>{
   const x=new XMLHttpRequest();
   x.open('PUT','/upload?name='+encodeURIComponent(file.name));
   x.upload.onprogress=e=>{if(e.lengthComputable){
     const pc=Math.round(100*e.loaded/e.total);
     p.value=pc;s.textContent=file.name+' - '+pc+'%';}};
   x.onload=()=>{s.textContent=(x.status===200?'OK: ':'Error '+x.status+': ')+file.name;res();};
   x.onerror=()=>{s.textContent='Error de red con '+file.name;res();};
   x.send(file);
  });
 }
 p.hidden=true;f.value='';
};
</script></body></html>
)rawliteral";

esp_err_t SendText(httpd_req_t* req, const char* status, const char* text) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Nombre de archivo seguro: solo basename, sin ruta, sin "..", 1..100 chars.
bool SanitizeFileName(const char* raw, std::string& out) {
    if (raw == nullptr) {
        return false;
    }
    std::string name = raw;
    if (name.empty() || name.size() > 100) {
        return false;
    }
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos ||
        name.find("..") != std::string::npos || name[0] == '.') {
        return false;
    }
    for (char c : name) {
        if (static_cast<unsigned char>(c) < 0x20) {
            return false;
        }
    }
    out = std::move(name);
    return true;
}

// --- Vinculación Google Calendar sin backend (/pair) -------------------------
// El QR de la UI apunta a esta página (LAN, :8080). El celular guía al usuario
// para copiar la URL secreta iCal de Google y la envía directo al equipo:
// cero servidores intermedios, cero OAuth, cero tokens en el dispositivo.
const char kPairHtmlHead[] = R"rawliteral(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HORNERITO - Vincular Google Calendar</title>
<style>
body{font-family:system-ui,sans-serif;background:#101418;color:#e8eef4;margin:0;padding:1.5rem}
main{max-width:40rem;margin:auto}
h1{color:#5bc8f5;letter-spacing:.12em}
ol{line-height:1.8;padding-left:1.2rem}
a{color:#5bc8f5}
.ok{background:#14401f;border-radius:14px;padding:1rem;margin-bottom:1rem}
.err{background:#4a1a1a;border-radius:14px;padding:1rem;margin-bottom:1rem}
input[type=url]{width:100%;box-sizing:border-box;padding:.8rem;font-size:1rem;border:2px solid #2e4058;border-radius:14px;background:#1e2836;color:#e8eef4}
button{margin-top:1rem;width:100%;padding:.9rem;font-size:1.1rem;border:0;border-radius:14px;background:#24507a;color:#fff}
small{color:#8899aa}
</style></head>
<body><main>
<h1>CALENDARIO</h1>
)rawliteral";

const char kPairHtmlTail[] = R"rawliteral(<ol>
<li>Abrí <a href="https://calendar.google.com/calendar/r/settings" target="_blank" rel="noopener">Google Calendar &rarr; Ajustes</a> con tu cuenta.</li>
<li>Tocá tu calendario &rarr; <b>Dirección secreta en formato iCal</b> &rarr; <b>Copiar dirección</b>.</li>
<li>Pegala abajo y tocá <b>Guardar</b>.</li>
</ol>
<form method="post" action="/pair_set">
<input type="url" name="u" required maxlength="500" placeholder="https://calendar.google.com/calendar/ical/...">
<button>Guardar</button>
</form>
<small>Usá el celular en la misma Wi-Fi que HORNERITO. La dirección es secreta: no la compartas; viaja solo del celular al equipo.</small>
</main></body></html>
)rawliteral";

const char kPairOkBanner[] =
    "<div class=\"ok\">Guardado. HORNERITO está sincronizando tu agenda.</div>";
const char kPairErrBanner[] =
    "<div class=\"err\">La dirección debe empezar con https://.</div>";

esp_err_t RedirectPair(httpd_req_t* req, const char* location) {
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

// application/x-www-form-urlencoded: '+' = espacio y %XX = byte.
std::string UrlDecode(const std::string& in) {
    auto hex = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'a' && h <= 'f') return h - 'a' + 10;
        if (h >= 'A' && h <= 'F') return h - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        char c = in[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%' && i + 2 < in.size()) {
            int hi = hex(in[i + 1]);
            int lo = hex(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out += static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out += c;
    }
    return out;
}

}  // namespace

HorneritoTransfer& HorneritoTransfer::GetInstance() {
    static HorneritoTransfer instance;
    return instance;
}

std::string HorneritoTransfer::current_file() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_file_;
}

std::string HorneritoTransfer::last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

void HorneritoTransfer::SetFile(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_file_ = name;
    last_error_.clear();
}

void HorneritoTransfer::SetError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_error_ = error;
}

void HorneritoTransfer::Start() {
    if (server_running_.load()) {
        return;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.stack_size = 6144;  // handlers con buffers en heap, pero con margen
    // El :80 (y el ctrl 32768) son del portal de configuración Wi-Fi (captive
    // portal en 192.168.4.1); el servidor de archivos vive en 8080.
    config.server_port = 8080;
    config.ctrl_port = 32769;

    httpd_handle_t server = nullptr;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar el servidor HTTP de archivos");
        return;
    }

    httpd_uri_t index_uri = {};
    index_uri.uri = "/";
    index_uri.method = HTTP_GET;
    index_uri.handler = GetIndexHandler;

    httpd_uri_t upload_uri = {};
    upload_uri.uri = "/upload";
    upload_uri.method = HTTP_PUT;
    upload_uri.handler = PutUploadHandler;

    httpd_uri_t pair_uri = {};
    pair_uri.uri = "/pair";
    pair_uri.method = HTTP_GET;
    pair_uri.handler = GetPairHandler;

    httpd_uri_t pair_set_uri = {};
    pair_set_uri.uri = "/pair_set";
    pair_set_uri.method = HTTP_POST;
    pair_set_uri.handler = PostPairSetHandler;

    if (httpd_register_uri_handler(server, &index_uri) != ESP_OK ||
        httpd_register_uri_handler(server, &upload_uri) != ESP_OK ||
        httpd_register_uri_handler(server, &pair_uri) != ESP_OK ||
        httpd_register_uri_handler(server, &pair_set_uri) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudieron registrar los handlers de archivos");
        httpd_stop(server);
        return;
    }

    server_ = server;
    server_running_ = true;
    ESP_LOGI(TAG, "Servidor de archivos listo: http://<ip-del-dispositivo>:8080/");
}

esp_err_t HorneritoTransfer::GetIndexHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t HorneritoTransfer::GetPairHandler(httpd_req_t* req) {
    char query[32] = "";
    const char* banner = "";
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        if (strstr(query, "ok=1") != nullptr) {
            banner = kPairOkBanner;
        } else if (strstr(query, "err=1") != nullptr) {
            banner = kPairErrBanner;
        }
    }
    std::string page = std::string(kPairHtmlHead) + banner + kPairHtmlTail;
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, page.c_str(), page.size());
    return ESP_OK;
}

esp_err_t HorneritoTransfer::PostPairSetHandler(httpd_req_t* req) {
    if (req->content_len == 0 || req->content_len > 1600) {
        return RedirectPair(req, "/pair?err=1");
    }
    std::vector<char> body(req->content_len);
    size_t got = 0;
    while (got < body.size()) {
        int r = httpd_req_recv(req, body.data() + got, body.size() - got);
        if (r <= 0) {
            return RedirectPair(req, "/pair?err=1");
        }
        got += static_cast<size_t>(r);
    }
    std::string raw(body.data(), body.size());
    size_t pos = raw.find("u=");
    if (pos == std::string::npos) {
        return RedirectPair(req, "/pair?err=1");
    }
    std::string url = UrlDecode(raw.substr(pos + 2));
    if (url.size() > 500 || (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)) {
        return RedirectPair(req, "/pair?err=1");
    }
    // URL secreta: solo NVS, jamás en logs ni UART.
    hornerito_calendar::SetUrl(url);
    hornerito_calendar::RequestSync();
    ESP_LOGI(TAG, "URL iCal de Google recibida en /pair_set (%u bytes)",
             static_cast<unsigned>(url.size()));
    return RedirectPair(req, "/pair?ok=1");
}

esp_err_t HorneritoTransfer::PutUploadHandler(httpd_req_t* req) {
    auto& self = GetInstance();
    auto& player = HorneritoMusicPlayer::GetInstance();

    // --- 1. Validar nombre (?name=...) ---
    char query[160];
    char value[112];
    std::string name;
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "name", value, sizeof(value)) != ESP_OK ||
        !SanitizeFileName(value, name)) {
        return SendText(req, "400 Bad Request", "Nombre de archivo no valido");
    }

    // --- 2. Validar tamaño ---
    const uint64_t total = req->content_len;
    if (total == 0) {
        return SendText(req, "400 Bad Request", "Archivo vacio");
    }
    if (total > kMaxUploadBytes) {
        return SendText(req, "413 Request Entity Too Large",
                        "Archivo demasiado grande (maximo 256 MB)");
    }
    if (self.state_.load() == HorneritoTransferState::kTransferring) {
        return SendText(req, "409 Conflict", "Ya hay una transferencia en curso");
    }

    // --- 3. microSD montada + carpeta destino ---
    if (!player.EnsureMounted()) {
        return SendText(req, "503 Service Unavailable", "Sin microSD");
    }
    mkdir(MUSIC_DIR, 0755);  // si ya existe o el VFS no lo soporta, se ignora

    // --- 4. Exclusión con el reproductor (pausa + espera de E/S idle) ---
    player.BeginTransfer();
    self.state_ = HorneritoTransferState::kTransferring;
    self.SetFile(name);
    self.total_ = static_cast<uint32_t>(total);
    self.received_ = 0;

    // --- 5. Abrir destino ---
    std::string path = std::string(MUSIC_DIR) + "/" + name;
    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        player.EndTransfer();
        self.state_ = HorneritoTransferState::kError;
        self.SetError("No se pudo crear el archivo en la microSD");
        return SendText(req, "507 Insufficient Storage",
                        "No se pudo crear el archivo en la microSD");
    }

    // --- 6. Recibir y escribir en streaming ---
    esp_err_t outcome = ESP_OK;
    std::vector<uint8_t> buffer(4096);
    uint64_t received = 0;
    int retries = kRecvRetries;
    while (received < total) {
        size_t want = std::min<uint64_t>(buffer.size(), total - received);
        int n = httpd_req_recv(req, reinterpret_cast<char*>(buffer.data()), want);
        if (n == HTTPD_SOCK_ERR_TIMEOUT && --retries > 0) {
            continue;  // retransmisión de pausas de red (5 s de timeout c/u)
        }
        if (n <= 0) {
            outcome = ESP_FAIL;
            break;
        }
        if (fwrite(buffer.data(), 1, n, file) != static_cast<size_t>(n)) {
            outcome = ESP_FAIL;  // tarjeta llena o error de escritura
            break;
        }
        received += n;
        self.received_ = static_cast<uint32_t>(received);
    }

    // --- 7. Cierre correcto: flush + sync + close ---
    if (outcome == ESP_OK) {
        fflush(file);
        fsync(fileno(file));
    }
    fclose(file);

    // --- 8. Liberar al reproductor: rescanea y reanuda si estaba sonando ---
    player.EndTransfer();

    if (outcome != ESP_OK) {
        remove(path.c_str());  // no dejar archivos a medias
        self.state_ = HorneritoTransferState::kError;
        self.SetError("Transferencia interrumpida o sin espacio");
        ESP_LOGW(TAG, "Transferencia fallida: %s (%llu/%llu)", path.c_str(),
                 (unsigned long long)received, (unsigned long long)total);
        return SendText(req, "500 Internal Server Error",
                        "Transferencia interrumpida (¿espacio en la tarjeta?)");
    }

    self.state_ = HorneritoTransferState::kIdle;
    ESP_LOGI(TAG, "Archivo recibido: %s (%llu bytes)", path.c_str(),
             (unsigned long long)total);
    return SendText(req, "200 OK", "OK");
}
