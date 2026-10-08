#include "hornerito/ui/hornerito_wifi_ui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include <esp_log.h>
#include <esp_wifi.h>

#include <ssid_manager.h>
#include <wifi_manager.h>

#include "hornerito/ui/hornerito_keyboard.h"
#include "hornerito/ui/hornerito_ui.h"
#include "material_symbols.h"

namespace {

const char* TAG = "WIFI_UI";

constexpr uint32_t kScanShowMs = 3000;       // duración visible de "Buscando..."
constexpr uint32_t kConnectTimeoutMs = 35000;  // timeout del intento de conexión
constexpr size_t kMaxRows = 20;              // filas máximas en la lista

const lv_font_t* TextFont() { return HorneritoTextFont(); }
const lv_font_t* IconFont() { return HorneritoIconFont(); }

void StyleDarkButton(lv_obj_t* button) {
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2E4058), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
}

// SSID crudo (hasta 32 bytes, puede no llevar NUL).
std::string RawSsid(const wifi_ap_record_t& rec) {
    const char* p = reinterpret_cast<const char*>(rec.ssid);
    return std::string(p, strnlen(p, sizeof(rec.ssid)));
}

// Versión para mostrar: solo ASCII imprimible (SSID = bytes arbitrarios).
std::string SanitizeSsid(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        out.push_back(c >= 0x20 && c < 0x7F ? static_cast<char>(c) : '?');
    }
    return out;
}

bool FindSaved(const std::string& ssid, std::string* password) {
    for (const auto& item : SsidManager::GetInstance().GetSsidList()) {
        if (item.ssid == ssid) {
            if (password != nullptr) {
                *password = item.password;
            }
            return true;
        }
    }
    return false;
}

void DimLabel(lv_obj_t* label) {
    lv_obj_set_style_text_color(label, lv_color_hex(0x8899AA), 0);
}

}  // namespace

void HorneritoWifiUi::Build() {
    if (screen_ != nullptr) {
        return;
    }

    // ---------- Pantalla 1: lista de redes ----------
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen_, 16, 0);

    lv_obj_t* back = lv_button_create(screen_);
    lv_obj_set_size(back, 56, 56);
    StyleDarkButton(back);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(back, BackCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_icon = lv_label_create(back);
    lv_label_set_text(back_icon, MATERIAL_SYMBOLS_ARROW_BACK);
    lv_obj_set_style_text_font(back_icon, IconFont(), 0);
    lv_obj_center(back_icon);

    lv_obj_t* heading = lv_label_create(screen_);
    lv_label_set_text(heading, "Wi-Fi");
    lv_obj_set_style_text_font(heading, TextFont(), 0);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    status_label_ = lv_label_create(screen_);
    lv_label_set_text(status_label_, "Sin conexión");
    lv_obj_set_style_text_font(status_label_, TextFont(), 0);
    lv_obj_set_width(status_label_, 448);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 64);

    info_label_ = lv_label_create(screen_);
    lv_label_set_text(info_label_, "Pulsa Buscar para ver redes");
    lv_obj_set_style_text_font(info_label_, TextFont(), 0);
    lv_obj_set_width(info_label_, 448);
    lv_label_set_long_mode(info_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(info_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info_label_, LV_ALIGN_TOP_MID, 0, 96);
    DimLabel(info_label_);

    lv_obj_t* hint = lv_label_create(screen_);
    lv_label_set_text(hint, "Redes disponibles (mantén pulsado para olvidar)");
    lv_obj_set_style_text_font(hint, TextFont(), 0);
    lv_obj_set_width(hint, 448);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 128);
    DimLabel(hint);

    // Lista de redes: flex vertical con scroll (LV_USE_LIST está desactivado)
    list_container_ = lv_obj_create(screen_);
    lv_obj_set_size(list_container_, 448, 236);
    lv_obj_align(list_container_, LV_ALIGN_TOP_MID, 0, 158);
    lv_obj_set_style_bg_opa(list_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_container_, 0, 0);
    lv_obj_set_style_radius(list_container_, 0, 0);
    lv_obj_set_style_pad_all(list_container_, 0, 0);
    lv_obj_set_scrollbar_mode(list_container_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(list_container_, LV_DIR_VER);
    lv_obj_set_flex_flow(list_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list_container_, 8, 0);

    scan_btn_ = lv_button_create(screen_);
    lv_obj_set_size(scan_btn_, 448, 56);
    StyleDarkButton(scan_btn_);
    lv_obj_align(scan_btn_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(scan_btn_, ScanCb, LV_EVENT_CLICKED, this);
    lv_obj_t* refresh_icon = lv_label_create(scan_btn_);
    lv_label_set_text(refresh_icon, MATERIAL_SYMBOLS_REFRESH);
    lv_obj_set_style_text_font(refresh_icon, IconFont(), 0);
    lv_obj_align(refresh_icon, LV_ALIGN_LEFT_MID, 28, 0);
    lv_obj_t* scan_text = lv_label_create(scan_btn_);
    lv_label_set_text(scan_text, "Buscar");
    lv_obj_set_style_text_font(scan_text, TextFont(), 0);
    lv_obj_center(scan_text);

    // ---------- Pantalla 2: contraseña ----------
    pass_screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pass_screen_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(pass_screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(pass_screen_, 16, 0);

    lv_obj_t* pass_back = lv_button_create(pass_screen_);
    lv_obj_set_size(pass_back, 56, 56);
    StyleDarkButton(pass_back);
    lv_obj_align(pass_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(pass_back, PassBackCb, LV_EVENT_CLICKED, this);
    lv_obj_t* pass_back_icon = lv_label_create(pass_back);
    lv_label_set_text(pass_back_icon, MATERIAL_SYMBOLS_ARROW_BACK);
    lv_obj_set_style_text_font(pass_back_icon, IconFont(), 0);
    lv_obj_center(pass_back_icon);

    // Doble uso: título y mensaje de error de validación.
    pass_heading_ = lv_label_create(pass_screen_);
    lv_label_set_text(pass_heading_, "Conectar a");
    lv_obj_set_style_text_font(pass_heading_, TextFont(), 0);
    lv_obj_set_width(pass_heading_, 448);
    lv_label_set_long_mode(pass_heading_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(pass_heading_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pass_heading_, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_text_color(pass_heading_, lv_color_hex(0x5BC8F5), 0);

    pass_ssid_ = lv_label_create(pass_screen_);
    lv_label_set_text(pass_ssid_, "\u2014");
    lv_obj_set_style_text_font(pass_ssid_, TextFont(), 0);
    lv_obj_set_width(pass_ssid_, 448);
    lv_label_set_long_mode(pass_ssid_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(pass_ssid_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(pass_ssid_, LV_ALIGN_TOP_MID, 0, 60);

    pass_ta_ = lv_textarea_create(pass_screen_);
    lv_obj_set_size(pass_ta_, 344, 48);
    lv_obj_align(pass_ta_, LV_ALIGN_TOP_LEFT, 0, 96);
    lv_textarea_set_one_line(pass_ta_, true);
    lv_textarea_set_password_mode(pass_ta_, true);
    lv_textarea_set_max_length(pass_ta_, 64);
    lv_textarea_set_placeholder_text(pass_ta_, "Contraseña");
    lv_obj_set_style_text_font(pass_ta_, TextFont(), 0);
    lv_obj_set_style_bg_color(pass_ta_, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_text_color(pass_ta_, lv_color_white(), 0);
    lv_obj_set_style_radius(pass_ta_, 10, 0);

    pass_eye_ = lv_button_create(pass_screen_);
    lv_obj_set_size(pass_eye_, 88, 48);
    StyleDarkButton(pass_eye_);
    lv_obj_align(pass_eye_, LV_ALIGN_TOP_LEFT, 360, 96);
    lv_obj_add_event_cb(pass_eye_, PassEyeCb, LV_EVENT_CLICKED, this);
    lv_obj_t* eye_text = lv_label_create(pass_eye_);
    lv_label_set_text(eye_text, "Mostrar");
    lv_obj_set_style_text_font(eye_text, TextFont(), 0);
    lv_obj_center(eye_text);

    // El teclado (hornerito_keyboard: buttonmatrix propio, páginas
    // abc/123/#+=) no se crea aquí: se crea/reapunta en OpenPassScreen,
    // justo al entrar en esta pantalla.

    lv_obj_t* cancel_btn = lv_button_create(pass_screen_);
    lv_obj_set_size(cancel_btn, 214, 56);
    StyleDarkButton(cancel_btn);
    lv_obj_align(cancel_btn, LV_ALIGN_TOP_LEFT, 0, 392);
    lv_obj_add_event_cb(cancel_btn, PassBackCb, LV_EVENT_CLICKED, this);
    lv_obj_t* cancel_text = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_text, "Cancelar");
    lv_obj_set_style_text_font(cancel_text, TextFont(), 0);
    lv_obj_center(cancel_text);

    lv_obj_t* connect_btn = lv_button_create(pass_screen_);
    lv_obj_set_size(connect_btn, 214, 56);
    StyleDarkButton(connect_btn);
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x24507A), LV_STATE_PRESSED);
    lv_obj_align(connect_btn, LV_ALIGN_TOP_LEFT, 234, 392);
    lv_obj_add_event_cb(connect_btn, PassConnectCb, LV_EVENT_CLICKED, this);
    lv_obj_t* connect_text = lv_label_create(connect_btn);
    lv_label_set_text(connect_text, "Conectar");
    lv_obj_set_style_text_font(connect_text, TextFont(), 0);
    lv_obj_center(connect_text);

    timer_ = lv_timer_create(TimerCb, 500, this);
    RebuildList();
}

void HorneritoWifiUi::OnShow() {
    if (screen_ == nullptr) {
        return;
    }
    // Observador del motivo de desconexión (solo escribe un entero atómico;
    // no consume eventos de nadie: esp_event llama a todos los handlers).
    // Se registra aquí y no en Build(): Build corre en el arranque, antes de
    // que exista el event loop de esp_event (fallaba con INVALID_STATE).
    if (disconnect_inst_ == nullptr) {
        esp_err_t err = esp_event_handler_instance_register(
            WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &OnStaDisconnected, this,
            &disconnect_inst_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Register STA_DISCONNECTED failed: %s", esp_err_to_name(err));
        }
    }
    error_.clear();
    notice_.clear();
    if (rows_.empty()) {
        StartScan();  // primera entrada: escanea sola
    }
    Refresh();
}

bool HorneritoWifiUi::active() const {
    lv_obj_t* current = lv_screen_active();
    return current == screen_ || current == pass_screen_;
}

void HorneritoWifiUi::TimerCb(lv_timer_t* timer) {
    auto* self = static_cast<HorneritoWifiUi*>(lv_timer_get_user_data(timer));
    if (self != nullptr) {
        self->Refresh();
    }
}

void HorneritoWifiUi::BackCb(lv_event_t* event) {
    (void)event;
    HorneritoUi::GetInstance().ShowSettings();
}

void HorneritoWifiUi::ScanCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoWifiUi*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->StartScan();
    }
}

void HorneritoWifiUi::PassBackCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoWifiUi*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (self->pass_ta_ != nullptr) {
        lv_textarea_set_text(self->pass_ta_, "");  // nunca queda la contraseña
    }
    lv_screen_load(self->screen_);
}

void HorneritoWifiUi::PassConnectCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoWifiUi*>(lv_event_get_user_data(event));
    if (self == nullptr || self->pass_ta_ == nullptr) {
        return;
    }
    const char* text = lv_textarea_get_text(self->pass_ta_);
    size_t len = (text != nullptr) ? strlen(text) : 0;
    if (len < 8 || len > 64) {
        // Solo se registra la longitud, nunca el contenido.
        ESP_LOGW(TAG, "Password rejected, length: %u", static_cast<unsigned>(len));
        lv_label_set_text(self->pass_heading_, "Contraseña: 8-64 caracteres");
        lv_obj_set_style_text_color(self->pass_heading_, lv_color_hex(0xFF8A80), 0);
        return;
    }
    std::string password(text, len);
    lv_textarea_set_text(self->pass_ta_, "");  // limpiar tras copiar
    self->StartConnect(self->target_ssid_, password);
}

void HorneritoWifiUi::PassEyeCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoWifiUi*>(lv_event_get_user_data(event));
    if (self == nullptr || self->pass_ta_ == nullptr) {
        return;
    }
    bool hidden = lv_textarea_get_password_mode(self->pass_ta_);
    lv_textarea_set_password_mode(self->pass_ta_, !hidden);
    // pass_eye_ es el botón; la etiqueta es su único hijo.
    lv_obj_t* label = lv_obj_get_child(self->pass_eye_, 0);
    if (label != nullptr) {
        lv_label_set_text(label, hidden ? "Ocultar" : "Mostrar");
    }
}

void HorneritoWifiUi::RowCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoWifiUi*>(lv_event_get_user_data(event));
    auto* button = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (self == nullptr || button == nullptr) {
        return;
    }
    size_t index = lv_obj_get_index(button);
    if (index >= self->rows_.size()) {
        return;
    }
    if (lv_event_get_code(event) == LV_EVENT_LONG_PRESSED) {
        self->ForgetRow(index);
    } else {
        self->SelectRow(index);
    }
}

void HorneritoWifiUi::OnStaDisconnected(void* arg, esp_event_base_t event_base,
                                        int32_t event_id, void* event_data) {
    (void)event_base;
    (void)event_id;
    auto* self = static_cast<HorneritoWifiUi*>(arg);
    if (self == nullptr || event_data == nullptr || !self->connecting_) {
        return;
    }
    auto* ev = static_cast<wifi_event_sta_disconnected_t*>(event_data);
    int expected = 0;
    self->last_reason_.compare_exchange_strong(expected, ev->reason);  // primer motivo
}

void HorneritoWifiUi::StartScan() {
    if (scanning_ || screen_ == nullptr) {
        return;
    }
    scanning_ = true;
    error_.clear();
    scan_begin_tick_ = lv_tick_get();
    ESP_LOGI(TAG, "Starting scan");
    WifiManager::GetInstance().RequestScan();
    Refresh();  // pinta "Buscando redes..." y deshabilita el botón
}

void HorneritoWifiUi::StartConnect(const std::string& ssid,
                                   const std::string& password) {
    target_ssid_ = ssid;
    error_.clear();
    notice_.clear();
    last_reason_.store(0);
    connect_start_tick_ = lv_tick_get();
    connecting_ = true;
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());  // nunca la contraseña

    // Persiste la red en NVS ("wifi") con la infraestructura existente.
    // Si ya existía, sobrescribe la contraseña (SsidManager::AddSsid).
    SsidManager::GetInstance().AddSsid(ssid, password);

    auto& wm = WifiManager::GetInstance();
    if (wm.IsConfigMode()) {
        // Portal activo: salir. El callback ConfigModeExit del tablero
        // arranca la estación con las credenciales recién guardadas.
        ESP_LOGI(TAG, "Exiting config portal");
        wm.StopConfigAp();
    } else if (wm.IsConnected()) {
        if (wm.GetSsid() != ssid) {
            // Cambio de red: reinicio limpio de estación (patrón propio del
            // firmware: Stop + Start).
            wm.StopStation();
            wm.StartStation();
        }
        // Si ya está conectada a esta misma red, el siguiente Refresh confirma.
    } else {
        // Estación activa escaneando: el nuevo scan localiza la red guardada.
        wm.RequestScan();
    }

    lv_screen_load(screen_);
    Refresh();
}

void HorneritoWifiUi::OpenPassScreen(const std::string& ssid) {
    target_ssid_ = ssid;
    lv_label_set_text(pass_heading_, "Conectar a");
    lv_obj_set_style_text_color(pass_heading_, lv_color_hex(0x5BC8F5), 0);
    lv_label_set_text(pass_ssid_, SanitizeSsid(ssid).c_str());
    lv_textarea_set_text(pass_ta_, "");
    lv_textarea_set_password_mode(pass_ta_, true);
    // pass_eye_ es el botón; la etiqueta es su único hijo (igual que en
    // PassEyeCb): llamar a lv_label_set_text sobre el botón corrompe el heap.
    lv_obj_t* eye_label = lv_obj_get_child(pass_eye_, 0);
    if (eye_label != nullptr) {
        lv_label_set_text(eye_label, "Mostrar");
    }
    hornerito_keyboard::Show(pass_screen_, pass_ta_,
                             hornerito_keyboard::Mode::kPassword);
    lv_screen_load(pass_screen_);
}

void HorneritoWifiUi::SelectRow(size_t index) {
    if (index >= rows_.size()) {
        return;
    }
    const auto& rec = rows_[index];
    std::string ssid = RawSsid(rec);
    bool open = rec.authmode == WIFI_AUTH_OPEN;
    std::string password;
    bool saved = FindSaved(ssid, &password);
    ESP_LOGI(TAG, "Selected SSID: %s", ssid.c_str());
    if (open || saved) {
        // Abierta o ya guardada: sin pedir contraseña.
        StartConnect(ssid, open ? std::string() : password);
    } else {
        OpenPassScreen(ssid);
    }
}

void HorneritoWifiUi::ForgetRow(size_t index) {
    if (index >= rows_.size()) {
        return;
    }
    std::string ssid = RawSsid(rows_[index]);
    auto& mgr = SsidManager::GetInstance();
    const auto& list = mgr.GetSsidList();
    int found = -1;
    for (size_t i = 0; i < list.size(); i++) {
        if (list[i].ssid == ssid) {
            found = static_cast<int>(i);
            break;
        }
    }
    auto& wm = WifiManager::GetInstance();
    if (found < 0) {
        notice_ = "No estaba guardada: " + SanitizeSsid(ssid);
        Refresh();
        return;
    }
    ESP_LOGI(TAG, "Forgetting SSID: %s", ssid.c_str());
    bool was_connected = wm.IsConnected() && wm.GetSsid() == ssid;
    mgr.RemoveSsid(found);  // fuera del bucle: invalida la lista interna
    notice_ = "Olvidada: " + SanitizeSsid(ssid);
    if (was_connected) {
        notice_ += " (sigue activa hasta reiniciar)";
    }
    Refresh();
}

void HorneritoWifiUi::RebuildList() {
    auto records = WifiManager::GetInstance().GetScanResults();
    std::sort(records.begin(), records.end(),
              [](const wifi_ap_record_t& a, const wifi_ap_record_t& b) {
                  return a.rssi > b.rssi;
              });

    rows_.clear();
    for (const auto& rec : records) {
        if (rec.ssid[0] == 0) {
            continue;  // SSID oculto
        }
        if (rows_.size() >= kMaxRows) {
            break;
        }
        std::string key = RawSsid(rec);
        bool dup = false;
        for (const auto& seen : rows_) {
            if (RawSsid(seen) == key) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            rows_.push_back(rec);
        }
    }

    while (lv_obj_get_child_cnt(list_container_) > 0) {
        lv_obj_delete(lv_obj_get_child(list_container_, 0));
    }

    if (rows_.empty()) {
        lv_obj_t* empty = lv_label_create(list_container_);
        lv_label_set_text(empty, "Sin redes. Pulsa Buscar.");
        lv_obj_set_style_text_font(empty, TextFont(), 0);
        lv_obj_set_width(empty, 448);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        DimLabel(empty);
        return;
    }

    auto& wm = WifiManager::GetInstance();
    bool is_connected = wm.IsConnected();
    std::string current = SanitizeSsid(wm.GetSsid());
    for (const auto& rec : rows_) {
        std::string shown = SanitizeSsid(RawSsid(rec));
        bool is_current = is_connected && shown == current;

        lv_obj_t* button = lv_button_create(list_container_);
        lv_obj_set_size(button, 448, 52);
        StyleDarkButton(button);
        // SHORT_CLICKED (no CLICKED): en LVGL 9 CLICKED también se dispara
        // tras un long-press y borraría+conectaría a la vez.
        lv_obj_add_event_cb(button, RowCb, LV_EVENT_SHORT_CLICKED, this);
        lv_obj_add_event_cb(button, RowCb, LV_EVENT_LONG_PRESSED, this);

        lv_obj_t* name = lv_label_create(button);
        lv_label_set_text(name, shown.c_str());
        lv_obj_set_style_text_font(name, TextFont(), 0);
        lv_obj_set_width(name, 300);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 10, 0);
        if (is_current) {
            lv_obj_set_style_text_color(name, lv_color_hex(0x7BD88F), 0);
        }

        // candado (seguridad) + barras (señal) + check (conectada)
        std::string icons;
        icons += (rec.authmode == WIFI_AUTH_OPEN) ? MATERIAL_SYMBOLS_LOCK_OPEN
                                                  : MATERIAL_SYMBOLS_LOCK;
        icons += ' ';
        icons += rec.rssi >= -65   ? MATERIAL_SYMBOLS_WIFI
                 : rec.rssi >= -75 ? MATERIAL_SYMBOLS_WIFI_2_BAR
                                   : MATERIAL_SYMBOLS_WIFI_1_BAR;
        if (is_current) {
            icons += ' ';
            icons += MATERIAL_SYMBOLS_CHECK;
        }
        lv_obj_t* right = lv_label_create(button);
        lv_label_set_text(right, icons.c_str());
        lv_obj_set_style_text_font(right, IconFont(), 0);
        lv_obj_set_style_text_color(right, lv_color_hex(0x9FB6CC), 0);
        lv_obj_align(right, LV_ALIGN_RIGHT_MID, -10, 0);
    }
}

void HorneritoWifiUi::Refresh() {
    if (!active() || status_label_ == nullptr) {
        return;
    }
    auto& wm = WifiManager::GetInstance();

    // --- Transiciones de fase ---
    if (scanning_ && lv_tick_elaps(scan_begin_tick_) >= kScanShowMs) {
        scanning_ = false;
        RebuildList();
        ESP_LOGI(TAG, "Scan complete: %u networks", static_cast<unsigned>(rows_.size()));
    }

    bool connected = wm.IsConnected();
    std::string raw_ssid = wm.GetSsid();
    std::string ssid = SanitizeSsid(raw_ssid);

    if (connecting_) {
        if (connected && raw_ssid == target_ssid_) {
            connecting_ = false;
            error_.clear();
            ESP_LOGI(TAG, "Connected, IP: %s", wm.GetIpAddress().c_str());
        } else if (error_.empty() && last_reason_.load() != 0) {
            error_ = FailureText(last_reason_.load());
            ESP_LOGW(TAG, "Connect failed (reason %d): %s", last_reason_.load(),
                     target_ssid_.c_str());
        } else if (lv_tick_elaps(connect_start_tick_) >= kConnectTimeoutMs) {
            connecting_ = false;
            if (error_.empty()) {
                error_ = "Tiempo de espera agotado";
            }
            ESP_LOGW(TAG, "Connect timeout: %s", target_ssid_.c_str());
        }
    }

    // --- Línea de estado (prioridad: error > conexión en curso > escaneo > OK) ---
    if (!error_.empty()) {
        lv_label_set_text(status_label_, error_.c_str());
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0xFF8A80), 0);
    } else if (connecting_) {
        std::string text = "Conectando a " + SanitizeSsid(target_ssid_) + "...";
        lv_label_set_text(status_label_, text.c_str());
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0x5BC8F5), 0);
    } else if (scanning_) {
        lv_label_set_text(status_label_, "Buscando redes...");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0x5BC8F5), 0);
    } else if (connected) {
        std::string text = "\u2713 Conectado a " + ssid;
        lv_label_set_text(status_label_, text.c_str());
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0x7BD88F), 0);
    } else if (wm.IsConfigMode()) {
        lv_label_set_text(status_label_, "Punto de acceso de configuración");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0x5BC8F5), 0);
    } else {
        lv_label_set_text(status_label_, "Sin conexión");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0x8899AA), 0);
    }

    // --- Línea de información ---
    char info[160];
    if (!notice_.empty()) {
        lv_label_set_text(info_label_, notice_.c_str());
    } else if (connected) {
        snprintf(info, sizeof(info), "IP: %s \u00b7 %d dBm",
                 wm.GetIpAddress().c_str(), wm.GetRssi());
        lv_label_set_text(info_label_, info);
    } else if (wm.IsConfigMode()) {
        snprintf(info, sizeof(info), "Punto de acceso: %s", wm.GetApSsid().c_str());
        lv_label_set_text(info_label_, info);
    } else if (!error_.empty()) {
        lv_label_set_text(info_label_, "Toca la red para reintentar");
    } else if (connecting_) {
        lv_label_set_text(info_label_, "Estableciendo conexión...");
    } else {
        lv_label_set_text(info_label_, "Pulsa Buscar para ver redes");
    }

    // --- Botón Buscar: activo solo cuando no hay escaneo ni conexión en curso ---
    if (scan_btn_ != nullptr) {
        if (scanning_ || connecting_) {
            lv_obj_add_state(scan_btn_, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(scan_btn_, LV_STATE_DISABLED);
        }
    }
}

const char* HorneritoWifiUi::FailureText(int reason) const {
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE:               // 2
        case WIFI_REASON_MIC_FAILURE:               // 14
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:    // 15
        case WIFI_REASON_AUTH_FAIL:                 // 202
        case WIFI_REASON_HANDSHAKE_TIMEOUT:         // 204
            return "Contraseña incorrecta";
        case WIFI_REASON_NO_AP_FOUND:               // 201
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:      // 210
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:      // 211
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:          // 212
            return "Red no disponible";
        default:
            return "No se pudo conectar";
    }
}
