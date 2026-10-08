#include "hornerito/ui/hornerito_files_ui.h"

#include <esp_netif.h>

#include <cstdio>
#include <string>

#include "hornerito/audio/hornerito_music_player.h"
#include "hornerito/transfer/hornerito_transfer.h"
#include "hornerito/ui/hornerito_ui.h"
#include "material_symbols.h"

namespace {

// IP de la interfaz STA (vacía si no hay conexión).
std::string StaIp() {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        return "";
    }
    esp_netif_ip_info_t ip = {};
    if (esp_netif_get_ip_info(netif, &ip) != ESP_OK || ip.ip.addr == 0) {
        return "";
    }
    char buffer[16];
    snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&ip.ip));
    return buffer;
}

void ApplyFont(lv_obj_t* obj, const lv_font_t* font) {
    if (font != nullptr) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
}

}  // namespace

void HorneritoFilesUi::Build() {
    if (screen_ != nullptr) {
        return;
    }

    // En esta versión de LVGL una pantalla se crea como objeto sin padre.
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen_, 16, 0);

    // Cabecera: volver + título
    lv_obj_t* back = lv_button_create(screen_);
    lv_obj_set_size(back, 56, 56);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x2E4058), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(back, lv_color_white(), 0);
    lv_obj_set_style_radius(back, 14, 0);
    lv_obj_set_style_shadow_width(back, 0, 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(back, BackCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* back_icon = lv_label_create(back);
    lv_label_set_text(back_icon, MATERIAL_SYMBOLS_ARROW_BACK);
    ApplyFont(back_icon, HorneritoIconFont());
    lv_obj_center(back_icon);

    lv_obj_t* heading = lv_label_create(screen_);
    lv_label_set_text(heading, "Archivos");
    ApplyFont(heading, HorneritoTextFont());
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    // Columna de estados
    lv_obj_t* content = lv_obj_create(screen_);
    lv_obj_set_size(content, 448, 360);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 16, 0);

    auto add_line = [&](const char* text) -> lv_obj_t* {
        lv_obj_t* label = lv_label_create(content);
        lv_label_set_text(label, text);
        ApplyFont(label, HorneritoTextFont());
        lv_obj_set_width(label, 448);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        return label;
    };

    // Estado USB: el C6 solo tiene USB Serial/JTAG (sin OTG => sin MSC)
    usb_label_ = add_line("USB: solo consola (sin MSC)");
    sd_label_ = add_line("microSD: --");
    net_label_ = add_line("Red: --");
    transfer_label_ = add_line("Transferencia: inactiva");
    file_label_ = add_line("Archivo: \u2014");

    timer_ = lv_timer_create(TimerCb, 500, this);
    Refresh();
}

void HorneritoFilesUi::OnShow() {
    if (screen_ != nullptr) {
        Refresh();
    }
}

void HorneritoFilesUi::TimerCb(lv_timer_t* timer) {
    auto* self = static_cast<HorneritoFilesUi*>(lv_timer_get_user_data(timer));
    self->Refresh();
}

void HorneritoFilesUi::BackCb(lv_event_t* event) {
    (void)event;
    HorneritoUi::GetInstance().ShowHome();
}

void HorneritoFilesUi::Refresh() {
    char buffer[160];

    // microSD
    auto& player = HorneritoMusicPlayer::GetInstance();
    auto count = player.SongCount();
    switch (player.state()) {
        case HorneritoMusicState::kNotMounted:
            snprintf(buffer, sizeof(buffer), "microSD: sin tarjeta");
            break;
        case HorneritoMusicState::kNoSongs:
            snprintf(buffer, sizeof(buffer), "microSD: sin canciones");
            break;
        case HorneritoMusicState::kError:
            snprintf(buffer, sizeof(buffer), "microSD: error");
            break;
        default:
            snprintf(buffer, sizeof(buffer), "microSD: %u canciones",
                     static_cast<unsigned>(count));
            break;
    }
    lv_label_set_text(sd_label_, buffer);

    // Red
    auto ip = StaIp();
    if (ip.empty()) {
        lv_label_set_text(net_label_, "Red: sin conexion");
    } else {
        snprintf(buffer, sizeof(buffer), "Web: http://%s:8080/", ip.c_str());
        lv_label_set_text(net_label_, buffer);
    }

    // Transferencia
    auto& transfer = HorneritoTransfer::GetInstance();
    if (!transfer.server_running()) {
        lv_label_set_text(transfer_label_, "Transferencia: servidor detenido");
    } else if (transfer.state() == HorneritoTransferState::kTransferring) {
        lv_label_set_text(transfer_label_, "Transferencia: en curso");
    } else if (transfer.state() == HorneritoTransferState::kError) {
        auto error = transfer.last_error();
        snprintf(buffer, sizeof(buffer), "Transferencia: error - %s",
                 error.empty() ? "desconocido" : error.c_str());
        lv_label_set_text(transfer_label_, buffer);
    } else {
        lv_label_set_text(transfer_label_, "Transferencia: inactiva");
    }

    // Archivo actual + progreso
    auto file = transfer.current_file();
    if (file.empty()) {
        lv_label_set_text(file_label_, "Archivo: \u2014");
    } else if (transfer.state() == HorneritoTransferState::kTransferring) {
        auto total = transfer.total_bytes();
        auto received = transfer.received_bytes();
        unsigned percent = total > 0 ? static_cast<unsigned>(
                               (static_cast<uint64_t>(received) * 100) / total)
                                     : 0;
        snprintf(buffer, sizeof(buffer), "%s - %u%% (%u/%u KB)", file.c_str(), percent,
                 static_cast<unsigned>(received / 1024),
                 static_cast<unsigned>(total / 1024));
        lv_label_set_text(file_label_, buffer);
    } else {
        snprintf(buffer, sizeof(buffer), "%s (ultimo)", file.c_str());
        lv_label_set_text(file_label_, buffer);
    }
}
