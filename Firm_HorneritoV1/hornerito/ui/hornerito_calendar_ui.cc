#include "hornerito/ui/hornerito_calendar_ui.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#include <wifi_manager.h>

#include "hornerito/calendar/hornerito_calendar.h"
#include "hornerito/ui/hornerito_keyboard.h"
#include "hornerito/ui/hornerito_ui.h"
#include "material_symbols.h"

namespace {

const lv_font_t* TextFont() { return HorneritoTextFont(); }
const lv_font_t* IconFont() { return HorneritoIconFont(); }

// qrcodegen (Nayuki) ya está en el binario vía esp_emote_gfx; prototipos
// extern "C" (sin include del componente: C linkage no mangla). Los enums se
// declaran como int (ABI idéntico en RISC-V).
extern "C" {
bool qrcodegen_encodeText(const char* text, uint8_t temp_buffer[], uint8_t qrcode[],
                          int ecl, int min_version, int max_version, int mask,
                          bool boost_ecl);
int qrcodegen_getSize(const uint8_t qrcode[]);
bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y);
}

// Pinta en negro los módulos del QR sobre el canvas I1 (ya en blanco).
// Margen de silencio >= 4 módulos: sin él los escáneres fallan.
void DrawQr(lv_obj_t* canvas, const std::string& text) {
    // nothrow: este build va sin excepciones (CONFIG_COMPILER_CXX_EXCEPTIONS
    // off), un operator new fallido abortaría el equipo.
    std::unique_ptr<uint8_t[]> temp(new (std::nothrow) uint8_t[2048]);
    std::unique_ptr<uint8_t[]> qr(new (std::nothrow) uint8_t[2048]);
    if (temp == nullptr || qr == nullptr) {
        return;
    }
    if (!qrcodegen_encodeText(text.c_str(), temp.get(), qr.get(), 1 /* MEDIUM */,
                              1, 10, -1 /* mask auto */, true)) {
        return;
    }
    int n = qrcodegen_getSize(qr.get());
    int scale = 280 / (n + 8);
    if (scale < 1) {
        scale = 1;
    }
    int content = n * scale;
    int ox = (280 - content) / 2;
    int oy = ox;
    for (int my = 0; my < n; my++) {
        for (int mx = 0; mx < n; mx++) {
            if (!qrcodegen_getModule(qr.get(), mx, my)) {
                continue;
            }
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    lv_canvas_set_px(canvas, ox + mx * scale + dx, oy + my * scale + dy,
                                     lv_color_black(), LV_OPA_COVER);
                }
            }
        }
    }
}

void StyleDarkButton(lv_obj_t* button) {
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2E4058), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
}

void StyleIcon(lv_obj_t* button, const char* icon) {
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, icon);
    lv_obj_set_style_text_font(label, IconFont(), 0);
    lv_obj_center(label);
}

void StyleScreen(lv_obj_t* screen) {
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 16, 0);
}

const char* StatusText(int status) {
    using hornerito_calendar::kBadClock;
    using hornerito_calendar::kBadIcs;
    using hornerito_calendar::kHttp;
    using hornerito_calendar::kNoUrl;
    using hornerito_calendar::kNoWifi;
    switch (status) {
        case kNoWifi: return "Sin Wi-Fi";
        case kBadClock: return "Hora sin sincronizar";
        case kHttp: return "Error HTTP";
        case kBadIcs: return "Respuesta no v\u00e1lida";
        case kNoUrl: return "Sin URL";
        default: return "";
    }
}

}  // namespace

void HorneritoCalendarUi::Build() {
    if (screen_ != nullptr) {
        return;
    }

    // --- Agenda ---
    screen_ = lv_obj_create(NULL);
    StyleScreen(screen_);

    lv_obj_t* back = lv_button_create(screen_);
    lv_obj_set_size(back, 56, 56);
    StyleDarkButton(back);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(back, BackCb, LV_EVENT_CLICKED, nullptr);
    StyleIcon(back, MATERIAL_SYMBOLS_ARROW_BACK);

    lv_obj_t* refresh = lv_button_create(screen_);
    lv_obj_set_size(refresh, 56, 56);
    StyleDarkButton(refresh);
    lv_obj_align(refresh, LV_ALIGN_TOP_RIGHT, -64, 0);
    lv_obj_add_event_cb(refresh, RefreshCb, LV_EVENT_CLICKED, this);
    StyleIcon(refresh, MATERIAL_SYMBOLS_REFRESH);

    lv_obj_t* url_btn = lv_button_create(screen_);
    lv_obj_set_size(url_btn, 56, 56);
    StyleDarkButton(url_btn);
    lv_obj_align(url_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(url_btn, UrlOpenCb, LV_EVENT_CLICKED, this);
    StyleIcon(url_btn, MATERIAL_SYMBOLS_SETTINGS);

    lv_obj_t* heading = lv_label_create(screen_);
    lv_label_set_text(heading, "Calendario");
    lv_obj_set_style_text_font(heading, TextFont(), 0);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    list_ = lv_obj_create(screen_);
    lv_obj_set_size(list_, 448, 316);
    lv_obj_align(list_, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_opa(list_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_, 0, 0);
    lv_obj_set_style_radius(list_, 0, 0);
    lv_obj_set_style_pad_all(list_, 0, 0);
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(list_, 6, 0);

    status_label_ = lv_label_create(screen_);
    lv_obj_set_size(status_label_, 448, 44);
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 398);
    // Pie con la fuente por defecto (Montserrat 14): cabe de sobra en una
    // línea; con TextFont (30) el estado se cortaría con puntos.
    lv_obj_set_style_text_font(status_label_, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    // --- Pantalla de URL (teclado compartido en modo texto) ---
    url_screen_ = lv_obj_create(NULL);
    StyleScreen(url_screen_);

    lv_obj_t* url_back = lv_button_create(url_screen_);
    lv_obj_set_size(url_back, 56, 56);
    StyleDarkButton(url_back);
    lv_obj_align(url_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(url_back, UrlBackCb, LV_EVENT_CLICKED, this);
    StyleIcon(url_back, MATERIAL_SYMBOLS_ARROW_BACK);

    lv_obj_t* url_heading = lv_label_create(url_screen_);
    lv_label_set_text(url_heading, "URL del calendario");
    lv_obj_set_style_text_font(url_heading, TextFont(), 0);
    lv_obj_align(url_heading, LV_ALIGN_TOP_MID, 0, 12);

    url_ta_ = lv_textarea_create(url_screen_);
    lv_obj_set_size(url_ta_, 448, 48);
    lv_obj_align(url_ta_, LV_ALIGN_TOP_LEFT, 0, 96);
    lv_textarea_set_one_line(url_ta_, true);
    lv_textarea_set_password_mode(url_ta_, false);
    lv_textarea_set_max_length(url_ta_, 500);
    lv_textarea_set_placeholder_text(url_ta_, "https://calendar.google.com/...");
    lv_obj_set_style_text_font(url_ta_, TextFont(), 0);
    lv_obj_set_style_bg_color(url_ta_, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_text_color(url_ta_, lv_color_white(), 0);
    lv_obj_set_style_radius(url_ta_, 10, 0);

    lv_obj_t* cancel = lv_button_create(url_screen_);
    lv_obj_set_size(cancel, 214, 56);
    StyleDarkButton(cancel);
    lv_obj_align(cancel, LV_ALIGN_TOP_LEFT, 0, 392);
    lv_obj_add_event_cb(cancel, UrlBackCb, LV_EVENT_CLICKED, this);
    lv_obj_t* cancel_text = lv_label_create(cancel);
    lv_label_set_text(cancel_text, "Cancelar");
    lv_obj_set_style_text_font(cancel_text, TextFont(), 0);
    lv_obj_center(cancel_text);

    lv_obj_t* save = lv_button_create(url_screen_);
    lv_obj_set_size(save, 214, 56);
    StyleDarkButton(save);
    lv_obj_set_style_bg_color(save, lv_color_hex(0x24507A), LV_STATE_PRESSED);
    lv_obj_align(save, LV_ALIGN_TOP_LEFT, 234, 392);
    lv_obj_add_event_cb(save, UrlSaveCb, LV_EVENT_CLICKED, this);
    lv_obj_t* save_text = lv_label_create(save);
    lv_label_set_text(save_text, "Guardar");
    lv_obj_set_style_text_font(save_text, TextFont(), 0);
    lv_obj_center(save_text);

    // --- Pantalla QR de vinculación (QR -> /pair en el propio equipo) ---
    qr_screen_ = lv_obj_create(NULL);
    StyleScreen(qr_screen_);
    // El buffer del canvas (malloc lazy) se devuelve al salir de la pantalla:
    // cero costo de heap permanente y sin punteros colgados (nada recarga
    // qr_screen_ salvo OpenQrScreen, que re-malloca antes de cargarla).
    lv_obj_add_event_cb(qr_screen_, QrUnloadCb, LV_EVENT_SCREEN_UNLOADED, this);

    lv_obj_t* q_back = lv_button_create(qr_screen_);
    lv_obj_set_size(q_back, 56, 56);
    StyleDarkButton(q_back);
    lv_obj_align(q_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(q_back, QrBackCb, LV_EVENT_CLICKED, this);
    StyleIcon(q_back, MATERIAL_SYMBOLS_ARROW_BACK);

    lv_obj_t* q_heading = lv_label_create(qr_screen_);
    lv_label_set_text(q_heading, "Google Calendar");
    lv_obj_set_style_text_font(q_heading, TextFont(), 0);
    lv_obj_align(q_heading, LV_ALIGN_TOP_MID, 0, 12);

    qr_ = lv_canvas_create(qr_screen_);
    lv_obj_set_pos(qr_, 100, 64);
    lv_obj_set_size(qr_, 280, 280);

    qr_status_ = lv_label_create(qr_screen_);
    lv_obj_set_size(qr_status_, 448, 84);
    lv_obj_align(qr_status_, LV_ALIGN_TOP_MID, 0, 364);
    lv_obj_set_style_text_font(qr_status_, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(qr_status_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(qr_status_, lv_color_hex(0x8899AA), 0);

    // --- Menú Calendario (entrada desde Configuración) ---
    menu_screen_ = lv_obj_create(NULL);
    StyleScreen(menu_screen_);

    lv_obj_t* m_back = lv_button_create(menu_screen_);
    lv_obj_set_size(m_back, 56, 56);
    StyleDarkButton(m_back);
    lv_obj_align(m_back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(m_back, MenuBackCb, LV_EVENT_CLICKED, this);
    StyleIcon(m_back, MATERIAL_SYMBOLS_ARROW_BACK);

    lv_obj_t* m_heading = lv_label_create(menu_screen_);
    lv_label_set_text(m_heading, "Calendario");
    lv_obj_set_style_text_font(m_heading, TextFont(), 0);
    lv_obj_align(m_heading, LV_ALIGN_TOP_MID, 0, 12);

    estado_label_ = lv_label_create(menu_screen_);
    lv_obj_set_size(estado_label_, 448, 52);
    lv_obj_align(estado_label_, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_text_font(estado_label_, LV_FONT_DEFAULT, 0);
    lv_label_set_long_mode(estado_label_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(estado_label_, LV_TEXT_ALIGN_CENTER, 0);

    int m_y = 140;
    auto add_menu_btn = [&](const char* text, lv_event_cb_t cb) {
        lv_obj_t* b = lv_button_create(menu_screen_);
        lv_obj_set_size(b, 448, 56);
        StyleDarkButton(b);
        lv_obj_align(b, LV_ALIGN_TOP_MID, 0, m_y);
        m_y += 68;
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
        lv_obj_t* t = lv_label_create(b);
        lv_label_set_text(t, text);
        lv_obj_set_style_text_font(t, TextFont(), 0);
        lv_obj_center(t);
        return b;
    };
    add_menu_btn("Vincular Google Calendar", MenuLinkCb);
    add_menu_btn("Ver agenda", MenuAgendaCb);
    add_menu_btn("Sincronizar ahora", MenuSyncCb);
    add_menu_btn("Configurar iCal", MenuIcalCb);

    timer_ = lv_timer_create(TimerCb, 1000, this);
}

void HorneritoCalendarUi::OnShow() {
    if (screen_ == nullptr) {
        return;
    }
    if (hornerito_calendar::GetUrl().empty()) {
        last_status_ = hornerito_calendar::kNoUrl;
    }
    int64_t last = hornerito_calendar::LastUpdate();
    if (last_status_ != hornerito_calendar::kNoUrl &&
        (last == 0 || hornerito_calendar::WallNow() - last > 300) &&
        !hornerito_calendar::IsSyncing()) {
        hornerito_calendar::RequestSync();
    }
    Render();
}

void HorneritoCalendarUi::OnShowEntry() {
    if (menu_screen_ == nullptr) {
        return;
    }
    RenderMenu();
}

bool HorneritoCalendarUi::active() const {
    lv_obj_t* current = lv_screen_active();
    return current == screen_ || current == url_screen_ || current == menu_screen_ ||
           current == qr_screen_;
}

void HorneritoCalendarUi::TimerCb(lv_timer_t* timer) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        return;
    }
    bool changed = false;
    int status = 0;
    if (hornerito_calendar::TakeSyncResult(&status)) {
        self->last_status_ = status;
        changed = true;
    }
    bool syncing = hornerito_calendar::IsSyncing();
    if (syncing != self->was_syncing_) {
        self->was_syncing_ = syncing;
        changed = true;
    }
    int64_t today = hornerito_calendar::StartOfDay(hornerito_calendar::WallNow());
    if (today != self->last_today_) {
        self->last_today_ = today;
        changed = true;  // incluye el primer tick (pinta nada más arrancar)
    }
    // Reintento automático cada10 min mientras la pantalla esté abierta.
    if (self->active() && !syncing && !hornerito_calendar::GetUrl().empty()) {
        int64_t last = hornerito_calendar::LastUpdate();
        if (last == 0 || hornerito_calendar::WallNow() - last > 600) {
            hornerito_calendar::RequestSync();
            self->was_syncing_ = true;
            changed = true;
        }
    }
    if (lv_screen_active() == self->menu_screen_) {
        self->RenderMenu();  // barato: 2 textos; refresca al terminar un sync
    }
    if (changed) {
        self->Render();
    }
}

// Pie compartido de agenda y menú: "Última act.: dd/MM HH:MM" + estado.
std::string HorneritoCalendarUi::FooterText() {
    std::string status;
    int64_t last = hornerito_calendar::LastUpdate();
    if (last == 0) {
        status = "Sin sincronizar";
    } else {
        int y = 0, m = 0, d = 0;
        hornerito_ics::CivilDate(last, &y, &m, &d);
        int tod = static_cast<int>(last % 86400);
        char head[64];
        snprintf(head, sizeof(head), "\u00daltima act.: %02d/%02d %02d:%02d", m, d,
                 tod / 3600, (tod % 3600) / 60);
        status = head;
    }
    if (hornerito_calendar::IsSyncing()) {
        status += " - Sincronizando...";
    } else if (last_status_ > 0) {
        status += " - ";
        status += StatusText(last_status_);
    }
    return status;
}

void HorneritoCalendarUi::Render() {
    if (list_ == nullptr) {
        return;
    }
    while (lv_obj_get_child_count(list_) > 0) {
        lv_obj_delete(lv_obj_get_child(list_, 0));
    }

    auto events = hornerito_calendar::LoadCache();
    int64_t today = hornerito_calendar::StartOfDay(hornerito_calendar::WallNow());
    int64_t tomorrow = today + 86400;
    int64_t limit = today + 32 * 86400;

    auto add_row = [&](const std::string& text, uint32_t color) {
        lv_obj_t* label = lv_label_create(list_);
        lv_label_set_text(label, text.c_str());
        lv_obj_set_style_text_font(label, TextFont(), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
        lv_obj_set_width(label, 444);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    };

    // Bucket por evento: 0=hoy (incluye el que empezó ayer y sigue),
    // 1=mañana, 2=próximos, -1=fuera de agenda.
    auto bucket_of = [&](const hornerito_calendar::Event& e) {
        if (e.start < today && e.end > today) {
            return 0;
        }
        int64_t d = hornerito_calendar::StartOfDay(e.start);
        if (d == today) return 0;
        if (d == tomorrow) return 1;
        if (d > tomorrow && d < limit) return 2;
        return -1;
    };

    static const char* kHeaders[] = {"HOY", "MA\u00d1ANA", "PR\u00d3XIMOS"};
    bool any = false;
    char buf[192];
    for (int bucket = 0; bucket < 3; bucket++) {
        bool header_done = false;
        for (const auto& e : events) {
            if (bucket_of(e) != bucket) {
                continue;
            }
            if (!header_done) {
                add_row(kHeaders[bucket], 0x5BC8F5);
                header_done = true;
                any = true;
            }
            int tod = static_cast<int>(e.start % 86400);
            if (e.all_day) {
                if (bucket == 2) {
                    int y = 0, m = 0, d = 0;
                    hornerito_ics::CivilDate(e.start, &y, &m, &d);
                    snprintf(buf, sizeof(buf), "%02d/%02d  Todo el d\u00eda  %s", m, d,
                             e.title.c_str());
                } else {
                    snprintf(buf, sizeof(buf), "Todo el d\u00eda  %s", e.title.c_str());
                }
            } else if (bucket == 2) {
                int y = 0, m = 0, d = 0;
                hornerito_ics::CivilDate(e.start, &y, &m, &d);
                snprintf(buf, sizeof(buf), "%02d/%02d %02d:%02d  %s", m, d, tod / 3600,
                         (tod % 3600) / 60, e.title.c_str());
            } else {
                snprintf(buf, sizeof(buf), "%02d:%02d  %s", tod / 3600, (tod % 3600) / 60,
                         e.title.c_str());
            }
            add_row(buf, 0xFFFFFF);
        }
    }
    if (!any) {
        if (hornerito_calendar::GetUrl().empty()) {
            add_row("Configura la URL secreta", 0x9AA7B4);
            add_row("con el icono de ajustes", 0x9AA7B4);
        } else {
            add_row("Sin eventos pr\u00f3ximos", 0x9AA7B4);
        }
    }

    lv_label_set_text(status_label_, FooterText().c_str());
}

void HorneritoCalendarUi::OpenUrlScreen() {
    if (url_screen_ == nullptr) {
        return;
    }
    lv_textarea_set_text(url_ta_, hornerito_calendar::GetUrl().c_str());
    hornerito_keyboard::Show(url_screen_, url_ta_, hornerito_keyboard::Mode::kText);
    lv_screen_load(url_screen_);
}

void HorneritoCalendarUi::RenderMenu() {
    if (menu_screen_ == nullptr) {
        return;
    }
    std::string line;
    const std::string url = hornerito_calendar::GetUrl();
    if (url.empty()) {
        line = "Sin configurar";
    } else if (url.find("calendar.google.com") != std::string::npos) {
        line = "Google Calendar (iCal)";
    } else {
        line = "Modo URL iCal (manual)";
    }
    line += "\n";
    line += FooterText();
    lv_label_set_text(estado_label_, line.c_str());
}

void HorneritoCalendarUi::OpenAgenda() {
    if (screen_ == nullptr) {
        return;
    }
    lv_screen_load(screen_);
    OnShow();  // pinta la caché y lanza sync si está obsoleta
}

// QR de vinculación: apunta a http://<ip>:8080/pair, la página que vive en el
// propio equipo. Cero backend: el celular copia/pega la URL secreta iCal de
// Google y la envía directo al equipo (misma Wi-Fi). Buffer lazy ~10 KB.
void HorneritoCalendarUi::OpenQrScreen() {
    if (qr_screen_ == nullptr) {
        return;
    }
    if (qr_buffer_ == nullptr) {
        // malloc no lanza (este build va sin excepciones): con el heap
        // fragmentado (p. ej. el asistente activo) devuelve NULL y se avisa
        // en la pantalla en vez de abortar. Ver crash 2026-10-06: resize()
        // (operator new) con bloque máx 8.7 KB → __cxa_allocate_exception → abort.
        qr_buffer_ = static_cast<uint8_t*>(
            malloc(LV_DRAW_BUF_SIZE(280, 280, LV_COLOR_FORMAT_I1)));
        if (qr_buffer_ == nullptr) {
            lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(qr_status_,
                              "Memoria insuficiente para el QR.\n"
                              "Cerrá y volvé a abrir en unos segundos.");
            lv_obj_set_style_text_color(qr_status_, lv_color_hex(0xE08080), 0);
            lv_screen_load(qr_screen_);
            return;
        }
        lv_canvas_set_buffer(qr_, qr_buffer_, 280, 280, LV_COLOR_FORMAT_I1);
    }
    std::memset(qr_buffer_, 0xFF,
                LV_DRAW_BUF_SIZE(280, 280, LV_COLOR_FORMAT_I1));  // todo blanco
    // Paleta I1: índice 0 = negro, índice 1 = blanco (vuelve a ponerse por si
    // el memset de arriba pisó la paleta en aperturas posteriores).
    lv_color32_t black = {};
    black.alpha = 255;
    lv_color32_t white = {};
    white.red = white.green = white.blue = 255;
    white.alpha = 255;
    lv_canvas_set_palette(qr_, 0, black);
    lv_canvas_set_palette(qr_, 1, white);

    auto& wm = WifiManager::GetInstance();
    if (!wm.IsConnected() || wm.GetIpAddress().empty()) {
        lv_obj_add_flag(qr_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(qr_status_,
                          "Sin Wi-Fi.\nConectate a una red y abrí este QR de nuevo.");
        lv_obj_set_style_text_color(qr_status_, lv_color_hex(0xE08080), 0);
    } else {
        std::string url = "http://" + wm.GetIpAddress() + ":8080/pair";
        DrawQr(qr_, url);
        lv_obj_clear_flag(qr_, LV_OBJ_FLAG_HIDDEN);
        std::string txt = "Escaneá con el celular (misma Wi-Fi)\n" + url;
        lv_label_set_text(qr_status_, txt.c_str());
        lv_obj_set_style_text_color(qr_status_, lv_color_hex(0x8899AA), 0);
    }
    lv_screen_load(qr_screen_);
}

void HorneritoCalendarUi::BackCb(lv_event_t* event) {
    (void)event;
    HorneritoUi::GetInstance().ShowSettings();
}

void HorneritoCalendarUi::RefreshCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self == nullptr || hornerito_calendar::IsSyncing()) {
        return;
    }
    hornerito_calendar::RequestSync();
    self->was_syncing_ = true;
    self->Render();  // el pie pasa a "Sincronizando..." al instante
}

void HorneritoCalendarUi::UrlOpenCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self != nullptr && self->menu_screen_ != nullptr) {
        // El engranaje de la agenda abre el menú Calendario.
        lv_screen_load(self->menu_screen_);
        self->RenderMenu();
    }
}

void HorneritoCalendarUi::UrlBackCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self == nullptr || self->menu_screen_ == nullptr) {
        return;
    }
    lv_screen_load(self->menu_screen_);
    self->RenderMenu();
}

void HorneritoCalendarUi::UrlSaveCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self == nullptr || self->url_ta_ == nullptr) {
        return;
    }
    // URL secreta: ni log ni UART, solo NVS.
    hornerito_calendar::SetUrl(lv_textarea_get_text(self->url_ta_));
    self->last_status_ = -1;  // estado nuevo: OnShow decide si sincroniza
    if (self->menu_screen_ != nullptr) {
        lv_screen_load(self->menu_screen_);
        self->RenderMenu();
    }
}

// --- Menú Calendario --------------------------------------------------------

void HorneritoCalendarUi::MenuBackCb(lv_event_t* event) {
    (void)event;
    HorneritoUi::GetInstance().ShowSettings();
}

void HorneritoCalendarUi::MenuAgendaCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->OpenAgenda();
    }
}

void HorneritoCalendarUi::MenuIcalCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->OpenUrlScreen();
    }
}

void HorneritoCalendarUi::MenuSyncCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        return;
    }
    if (!hornerito_calendar::IsSyncing() && !hornerito_calendar::GetUrl().empty()) {
        hornerito_calendar::RequestSync();
        self->was_syncing_ = true;
    }
    self->RenderMenu();
}

void HorneritoCalendarUi::MenuLinkCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self != nullptr) {
        self->OpenQrScreen();
    }
}

void HorneritoCalendarUi::QrBackCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self == nullptr || self->menu_screen_ == nullptr) {
        return;
    }
    lv_screen_load(self->menu_screen_);
    self->RenderMenu();
}

void HorneritoCalendarUi::QrUnloadCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoCalendarUi*>(lv_event_get_user_data(event));
    if (self != nullptr && self->qr_buffer_ != nullptr) {
        // Sale del QR (atrás, asistente, lo que sea): devuelve los ~10 KB.
        free(self->qr_buffer_);
        self->qr_buffer_ = nullptr;
    }
}
