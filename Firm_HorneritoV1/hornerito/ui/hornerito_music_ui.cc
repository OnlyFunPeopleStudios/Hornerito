#include "hornerito/ui/hornerito_music_ui.h"

#include <cstdio>
#include <string>

#include "hornerito/ui/hornerito_ui.h"
#include "material_symbols.h"

namespace {

constexpr int kMaxListedSongs = 40;

void StyleDarkButton(lv_obj_t* button) {
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2E4058), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_radius(button, 14, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
}

lv_obj_t* IconButton(lv_obj_t* parent, const char* icon, lv_event_cb_t cb) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, 116, 72);
    StyleDarkButton(button);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x24507A), LV_STATE_PRESSED);
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, icon);
    lv_obj_set_style_text_font(label, HorneritoIconFont(), 0);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, nullptr);
    return button;
}

}  // namespace

void HorneritoMusicUi::Build() {
    if (screen_ != nullptr) {
        return;
    }

    // En esta versión de LVGL una pantalla se crea como objeto sin padre.
    screen_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen_, 16, 0);

    // Cabecera: volver a la pantalla principal + título
    lv_obj_t* back = lv_button_create(screen_);
    lv_obj_set_size(back, 56, 56);
    StyleDarkButton(back);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(back, BackCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_icon = lv_label_create(back);
    lv_label_set_text(back_icon, MATERIAL_SYMBOLS_ARROW_BACK);
    lv_obj_set_style_text_font(back_icon, HorneritoIconFont(), 0);
    lv_obj_center(back_icon);

    lv_obj_t* heading = lv_label_create(screen_);
    lv_label_set_text(heading, "M\u00fasica");
    lv_obj_set_style_text_font(heading, HorneritoTextFont(), 0);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    // Lista de canciones (flex vertical con scroll; LV_USE_LIST está desactivado)
    list_container_ = lv_obj_create(screen_);
    lv_obj_set_size(list_container_, 448, 210);
    lv_obj_align(list_container_, LV_ALIGN_TOP_MID, 0, 76);
    lv_obj_set_style_bg_opa(list_container_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list_container_, 0, 0);
    lv_obj_set_style_radius(list_container_, 0, 0);
    lv_obj_set_style_pad_all(list_container_, 0, 0);
    lv_obj_set_scrollbar_mode(list_container_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(list_container_, LV_DIR_VER);
    lv_obj_set_flex_flow(list_container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list_container_, 8, 0);

    // Canción actual y estado
    title_label_ = lv_label_create(screen_);
    lv_label_set_text(title_label_, "\u2014");
    lv_obj_set_style_text_font(title_label_, HorneritoTextFont(), 0);
    lv_obj_set_width(title_label_, 448);
    lv_label_set_long_mode(title_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, 294);

    state_label_ = lv_label_create(screen_);
    lv_label_set_text(state_label_, "\u2014");
    lv_obj_set_style_text_font(state_label_, HorneritoTextFont(), 0);
    lv_obj_set_width(state_label_, 448);
    lv_label_set_long_mode(state_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(state_label_, LV_ALIGN_TOP_MID, 0, 340);

    // Controles
    lv_obj_t* controls = lv_obj_create(screen_);
    lv_obj_set_size(controls, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_radius(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_style_pad_column(controls, 16, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align(controls, LV_ALIGN_BOTTOM_MID, 0, 0);

    IconButton(controls, MATERIAL_SYMBOLS_SKIP_PREVIOUS, PrevCb);
    lv_obj_t* play = IconButton(controls, MATERIAL_SYMBOLS_PLAY_ARROW, PlayCb);
    play_label_ = lv_obj_get_child(play, 0);
    IconButton(controls, MATERIAL_SYMBOLS_SKIP_NEXT, NextCb);

    timer_ = lv_timer_create(TimerCb, 500, this);
    RebuildList();
    Refresh();
}

void HorneritoMusicUi::OnShow() {
    if (screen_ == nullptr) {
        return;
    }
    auto& player = HorneritoMusicPlayer::GetInstance();
    auto state = player.state();
    // Solo reintenta el montaje si todavía no hay lista (así no se pierde
    // la posición al volver de otra pantalla).
    if (state == HorneritoMusicState::kNotMounted || state == HorneritoMusicState::kNoSongs) {
        player.RequestRefresh();
    }
    Refresh();
}

void HorneritoMusicUi::TimerCb(lv_timer_t* timer) {
    auto* self = static_cast<HorneritoMusicUi*>(lv_timer_get_user_data(timer));
    self->Refresh();
}

void HorneritoMusicUi::SongButtonCb(lv_event_t* event) {
    auto* self = static_cast<HorneritoMusicUi*>(lv_event_get_user_data(event));
    auto* button = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (self == nullptr || button == nullptr) {
        return;
    }
    HorneritoMusicPlayer::GetInstance().PlayIndex(
        static_cast<size_t>(lv_obj_get_index(button)));
}

void HorneritoMusicUi::BackCb(lv_event_t* event) {
    (void)event;
    HorneritoUi::GetInstance().ShowHome();
}

void HorneritoMusicUi::PrevCb(lv_event_t* event) {
    (void)event;
    HorneritoMusicPlayer::GetInstance().Prev();
}

void HorneritoMusicUi::PlayCb(lv_event_t* event) {
    (void)event;
    HorneritoMusicPlayer::GetInstance().TogglePause();
}

void HorneritoMusicUi::NextCb(lv_event_t* event) {
    (void)event;
    HorneritoMusicPlayer::GetInstance().Next();
}

void HorneritoMusicUi::RebuildList() {
    auto& player = HorneritoMusicPlayer::GetInstance();
    auto titles = player.SongTitles();
    listed_count_ = titles.size();

    while (lv_obj_get_child_cnt(list_container_) > 0) {
        lv_obj_delete(lv_obj_get_child(list_container_, 0));
    }

    if (titles.empty()) {
        lv_obj_t* empty = lv_label_create(list_container_);
        lv_label_set_text(empty, "Sin canciones (.mp3 en la microSD)");
        lv_obj_set_style_text_font(empty, HorneritoTextFont(), 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x8899AA), 0);
        lv_obj_set_width(empty, 448);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    size_t shown = titles.size() < kMaxListedSongs ? titles.size() : kMaxListedSongs;
    for (size_t i = 0; i < shown; i++) {
        lv_obj_t* button = lv_button_create(list_container_);
        lv_obj_set_size(button, 448, 52);
        StyleDarkButton(button);
        lv_obj_add_event_cb(button, SongButtonCb, LV_EVENT_CLICKED, this);

        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, titles[i].c_str());
        lv_obj_set_style_text_font(label, HorneritoTextFont(), 0);
        lv_obj_set_width(label, 420);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
    }

    if (titles.size() > shown) {
        lv_obj_t* more = lv_label_create(list_container_);
        lv_label_set_text(more, "...");
        lv_obj_set_style_text_font(more, HorneritoTextFont(), 0);
        lv_obj_set_style_text_color(more, lv_color_hex(0x8899AA), 0);
    }
}

void HorneritoMusicUi::Refresh() {
    auto& player = HorneritoMusicPlayer::GetInstance();

    if (player.SongCount() != listed_count_) {
        RebuildList();  // la tarjeta se releyó (p. ej. al insertarla)
    }

    char text[96];
    switch (player.state()) {
        case HorneritoMusicState::kNotMounted:
            snprintf(text, sizeof(text), "Sin tarjeta (microSD)");
            break;
        case HorneritoMusicState::kNoSongs:
            snprintf(text, sizeof(text), "Sin canciones (.mp3)");
            break;
        case HorneritoMusicState::kPlaying: {
            auto count = player.SongCount();
            snprintf(text, sizeof(text), "Reproduciendo  %u/%u",
                     static_cast<unsigned>(player.current_index() + 1),
                     static_cast<unsigned>(count));
            break;
        }
        case HorneritoMusicState::kPaused: {
            auto count = player.SongCount();
            snprintf(text, sizeof(text), "En pausa  %u/%u",
                     static_cast<unsigned>(player.current_index() + 1),
                     static_cast<unsigned>(count));
            break;
        }
        case HorneritoMusicState::kError:
            snprintf(text, sizeof(text), "Error de reproduccion");
            break;
        case HorneritoMusicState::kStopped:
        default:
            snprintf(text, sizeof(text), "Detenido");
            break;
    }
    lv_label_set_text(state_label_, text);

    auto title = player.CurrentTitle();
    lv_label_set_text(title_label_, title.empty() ? "\u2014" : title.c_str());

    if (play_label_ != nullptr) {
        lv_label_set_text(
            play_label_, player.state() == HorneritoMusicState::kPlaying
                             ? MATERIAL_SYMBOLS_PAUSE
                             : MATERIAL_SYMBOLS_PLAY_ARROW);
    }
}
