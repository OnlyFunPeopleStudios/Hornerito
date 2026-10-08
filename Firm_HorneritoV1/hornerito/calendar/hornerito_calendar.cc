#include "hornerito/calendar/hornerito_calendar.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <wifi_manager.h>

#include "settings.h"

namespace hornerito_calendar {
namespace {

const char* TAG = "CAL";

constexpr size_t kChunk = 3600;  // trozo de caché por clave NVS (<4 KB)
constexpr int kMaxChunks = 8;    // 8 * 3600 = ~28 KB de agenda máx.
constexpr int kMinClock = 1704067200;  // 2024-01-01: por debajo, sin sincronizar

std::atomic<bool> s_running{false};
std::atomic<bool> s_done{false};
std::atomic<int> s_status{-1};

// --- Stream ICS -> Parser (el cuerpo nunca se guarda entero en RAM) -------
int HttpEvent(esp_http_client_event_t* evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA && evt->data_len > 0 && evt->user_data != nullptr) {
        static_cast<hornerito_ics::Parser*>(evt->user_data)->Feed(
            static_cast<const char*>(evt->data), static_cast<size_t>(evt->data_len));
    }
    return 0;
}

void SyncTask(void*) {
    int status = kOk;
    do {
        std::string url = GetUrl();
        if (url.empty()) {
            status = kNoUrl;
            break;
        }
        if (!WifiManager::GetInstance().IsConnected()) {
            status = kNoWifi;
            break;
        }
        if (static_cast<int64_t>(time(nullptr)) < kMinClock) {
            // Sin hora válida el TLS de Google falla (fechas de certificado);
            // la hora llega con el server_time de la OTA.
            status = kBadClock;
            break;
        }

        hornerito_ics::Parser parser(WallNow());
        esp_http_client_config_t cfg = {};
        cfg.url = url.c_str();
        cfg.event_handler = HttpEvent;
        cfg.user_data = &parser;
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.timeout_ms = 15000;
        cfg.buffer_size = 1024;
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (client == nullptr) {
            status = kHttp;
            break;
        }
        // Google comprime con gzip si se lo piden; sin descompresor, pedimos
        // la respuesta en crudo.
        esp_http_client_set_header(client, "Accept-Encoding", "identity");
        esp_err_t err = esp_http_client_perform(client);
        int code = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        if (err != ESP_OK || code != 200) {
            // Solo el código: la URL lleva el token secreto y no se registra.
            ESP_LOGW(TAG, "fetch fallo: err=%d http=%d", static_cast<int>(err), code);
            status = kHttp;
            break;
        }
        parser.Finish();
        if (!parser.saw_vevent()) {
            status = kBadIcs;
            break;
        }
        auto events = parser.Take();
        Settings s("calendar", true);
        std::string blob;
        char buf[48];
        for (const auto& e : events) {
            snprintf(buf, sizeof(buf), "%lld|%lld|%d|", static_cast<long long>(e.start),
                     static_cast<long long>(e.end), e.all_day ? 1 : 0);
            blob += buf;
            for (char c : e.title) {
                blob += (c == '\n' || c == '\r') ? ' ' : c;
            }
            blob += '\n';
        }
        for (int i = 0; i < kMaxChunks; i++) {
            std::string key = "e" + std::to_string(i);
            size_t off = static_cast<size_t>(i) * kChunk;
            if (off < blob.size()) {
                s.SetString(key, blob.substr(off, kChunk));
            } else {
                s.EraseKey(key);
            }
        }
        s.SetInt("last", static_cast<int32_t>(WallNow()));
        ESP_LOGI(TAG, "sync ok: %d eventos", static_cast<int>(events.size()));
        status = kOk;
    } while (0);

    s_status = status;
    s_running = false;
    s_done = true;
    vTaskDelete(nullptr);
}

}  // namespace

int64_t WallNow() {
    return hornerito_ics::WallNow();
}

int64_t StartOfDay(int64_t wall) {
    return hornerito_ics::StartOfDay(wall);
}

std::vector<Event> LoadCache() {
    Settings s("calendar");
    std::string blob;
    for (int i = 0; i < kMaxChunks; i++) {
        std::string part = s.GetString("e" + std::to_string(i));
        if (part.empty()) {
            break;
        }
        blob += part;
    }
    std::vector<Event> out;
    size_t pos = 0;
    while (pos < blob.size()) {
        size_t nl = blob.find('\n', pos);
        if (nl == std::string::npos) {
            break;
        }
        std::string line = blob.substr(pos, nl - pos);
        pos = nl + 1;
        size_t p1 = line.find('|');
        if (p1 == std::string::npos) continue;
        size_t p2 = line.find('|', p1 + 1);
        if (p2 == std::string::npos) continue;
        size_t p3 = line.find('|', p2 + 1);
        if (p3 == std::string::npos) continue;
        Event e;
        e.start = atoll(line.c_str());
        e.end = atoll(line.c_str() + p1 + 1);
        e.all_day = line[p2 + 1] == '1';
        e.title = line.substr(p3 + 1);
        out.push_back(std::move(e));
    }
    return out;
}

int64_t LastUpdate() {
    return Settings("calendar").GetInt("last", 0);
}

std::string GetUrl() {
    return Settings("calendar").GetString("url");
}

void SetUrl(const std::string& url) {
    std::string clean;
    for (char c : url) {
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            clean += c;
        }
    }
    Settings s("calendar", true);
    if (clean != s.GetString("url")) {
        s.SetString("url", clean);
        s.SetInt("last", 0);  // URL nueva: la próxima entrada sincroniza sola
    }
}

void RequestSync() {
    bool expected = false;
    if (!s_running.compare_exchange_strong(expected, true)) {
        return;  // ya hay una en curso
    }
    if (xTaskCreate(SyncTask, "cal_sync", 12288, nullptr, 1, nullptr) != pdPASS) {
        s_running = false;
        s_status = kHttp;
        s_done = true;
    }
}

bool IsSyncing() {
    return s_running.load();
}

bool TakeSyncResult(int* status) {
    if (!s_done.exchange(false)) {
        return false;
    }
    if (status != nullptr) {
        *status = s_status.load();
    }
    return true;
}

}  // namespace hornerito_calendar
