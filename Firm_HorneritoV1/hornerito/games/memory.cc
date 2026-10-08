#include "hornerito/games/memory.h"

#include <esp_random.h>

#include <cstdio>

#include "application.h"
#include "assets/lang_config.h"

namespace {

constexpr int kPadW = 170;
constexpr int kPadH = 130;
constexpr int kPadGap = 14;
constexpr int kGridX = 63;  // (480 - 2*170 - 14) / 2
constexpr int kGridY = 70;
constexpr int kRestMs = 600;   // pausa tras acertar una ronda
constexpr int kOffMs = 180;    // duración del destello al tocar

constexpr uint32_t kBaseColor[4] = {0xEB5757, 0x6FCF97, 0xF2C94C, 0x56CCF2};
constexpr uint32_t kLitColor[4] = {0xFF8A8A, 0xA9EFC4, 0xF9DC8C, 0x9BDCFF};

int StepPeriod(int len) {
    // milisegundos encendido (= milisegundos apagado): más rápido al
    // avanzar de ronda.
    int p = 220 - len * 6;
    return p < 90 ? 90 : p;
}

}  // namespace

MemoryGame& MemoryGame::GetInstance() {
    static MemoryGame instance;
    return instance;
}

void MemoryGame::ShowThunk(lv_timer_t* timer) {
    auto* self = static_cast<MemoryGame*>(lv_timer_get_user_data(timer));
    if (self->state_ == State::kRest) {
        self->StartShow();  // fin de la pausa: siguiente ronda
        return;
    }
    if (self->state_ != State::kShow) {
        lv_timer_pause(timer);
        return;
    }
    if (self->lit_pad_ >= 0) {
        self->Unlight(self->lit_pad_);
        self->lit_pad_ = -1;
        self->show_i_++;
        if (self->show_i_ >= self->len_) {
            // Secuencia mostrada completa: le toca al jugador.
            self->state_ = State::kWait;
            self->idx_ = 0;
            lv_timer_pause(timer);
            lv_label_set_text(self->status_, "Tu turno: repite la secuencia");
        }
    } else {
        self->Light(self->seq_[self->show_i_]);
        // Pop por paso: da ritmo a la secuencia (sonido ya embebido).
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_POPUP);
    }
}

void MemoryGame::OffThunk(lv_timer_t* timer) {
    auto* self = static_cast<MemoryGame*>(lv_timer_get_user_data(timer));
    lv_timer_pause(timer);
    if (self->lit_pad_ >= 0) {
        self->Unlight(self->lit_pad_);
        self->lit_pad_ = -1;
    }
}

void MemoryGame::PadThunk(lv_event_t* event) {
    auto* self = static_cast<MemoryGame*>(lv_event_get_user_data(event));
    if (self->state_ != State::kWait) {
        return;
    }
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    int pad = -1;
    for (int i = 0; i < 4; i++) {
        if (target == self->pads_[i]) {
            pad = i;
            break;
        }
    }
    if (pad < 0) {
        return;
    }

    // Destello del toque (el off_timer_ lo apaga).
    self->Light(pad);
    self->lit_pad_ = static_cast<int8_t>(pad);
    if (self->off_timer_ != nullptr) {
        lv_timer_pause(self->off_timer_);
        lv_timer_resume(self->off_timer_);
    }

    if (self->seq_[self->idx_] == pad) {
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_POPUP);
        self->idx_++;
        if (self->idx_ >= self->len_) {
            // SUCCESS: pausa corta y a por la siguiente ronda.
            self->state_ = State::kRest;
            self->idx_ = 0;
            lv_label_set_text(self->status_, "¡Correcto!");
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
            if (self->show_timer_ != nullptr) {
                lv_timer_set_period(self->show_timer_, kRestMs);
                lv_timer_resume(self->show_timer_);
            }
        }
        return;
    }

    // FAIL: fin del juego.
    Application::GetInstance().PlaySound(Lang::Sounds::OGG_EXCLAMATION);
    self->state_ = State::kOver;
    if (self->off_timer_ != nullptr) {
        lv_timer_pause(self->off_timer_);
    }
    self->Unlight(pad);
    self->lit_pad_ = -1;
    self->Die("Fin del juego");
}

void MemoryGame::OverlayThunk(lv_event_t* event) {
    static_cast<MemoryGame*>(lv_event_get_user_data(event))->Reset();
}

void MemoryGame::Enter(lv_obj_t* content) {
    content_ = content;

    hud_ = lv_label_create(content_);
    lv_obj_set_style_text_color(hud_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(hud_);
    lv_obj_align(hud_, LV_ALIGN_TOP_MID, 0, 10);

    // 4 pads (2x2), creados una sola vez.
    for (int i = 0; i < 4; i++) {
        const int col = i % 2;
        const int row = i / 2;
        pads_[i] = GameCreateRect(content_, kBaseColor[i], kPadW, kPadH, 16);
        lv_obj_set_pos(pads_[i], kGridX + col * (kPadW + kPadGap),
                       kGridY + row * (kPadH + kPadGap));
        lv_obj_add_flag(pads_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(pads_[i], PadThunk, LV_EVENT_PRESSED, this);
    }

    status_ = lv_label_create(content_);
    lv_obj_set_style_text_color(status_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(status_);
    lv_obj_align(status_, LV_ALIGN_TOP_MID, 0, 360);

    show_timer_ = lv_timer_create(ShowThunk, 200, this);
    lv_timer_pause(show_timer_);
    off_timer_ = lv_timer_create(OffThunk, kOffMs, this);
    lv_timer_pause(off_timer_);

    Reset();
}

void MemoryGame::Exit() {
    if (show_timer_ != nullptr) {
        lv_timer_delete(show_timer_);
        show_timer_ = nullptr;
    }
    if (off_timer_ != nullptr) {
        lv_timer_delete(off_timer_);
        off_timer_ = nullptr;
    }

    // Los widgets mueren con la pantalla de sesión: solo se sueltan
    // referencias y estado para que re-entrar arranque limpio.
    content_ = nullptr;
    hud_ = nullptr;
    for (lv_obj_t*& pad : pads_) {
        pad = nullptr;
    }
    status_ = nullptr;
    overlay_ = nullptr;
    state_ = State::kShow;
    len_ = 0;
    show_i_ = 0;
    idx_ = 0;
    lit_pad_ = -1;
}

void MemoryGame::Light(int pad) {
    lv_obj_set_style_bg_color(pads_[pad], lv_color_hex(kLitColor[pad]), 0);
    lit_pad_ = static_cast<int8_t>(pad);
}

void MemoryGame::Unlight(int pad) {
    lv_obj_set_style_bg_color(pads_[pad], lv_color_hex(kBaseColor[pad]), 0);
}

void MemoryGame::UpdateHud() {
    if (hud_ != nullptr) {
        lv_label_set_text_fmt(hud_, "Ronda %d", len_);
    }
}

void MemoryGame::Reset() {
    if (overlay_ != nullptr) {
        // Borrado diferido: evita eliminar el objeto dentro de su evento.
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    if (show_timer_ != nullptr) {
        lv_timer_pause(show_timer_);
    }
    if (off_timer_ != nullptr) {
        lv_timer_pause(off_timer_);
    }
    for (int i = 0; i < 4; i++) {
        Unlight(i);
    }
    len_ = 0;
    show_i_ = 0;
    idx_ = 0;
    lit_pad_ = -1;
    StartShow();
}

void MemoryGame::StartShow() {
    if (len_ >= sizeof(seq_)) {
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
        Die("¡Secuencia perfecta!");
        return;
    }
    seq_[len_++] = static_cast<uint8_t>(esp_random() % 4);
    show_i_ = 0;
    idx_ = 0;
    lit_pad_ = -1;
    state_ = State::kShow;
    UpdateHud();
    lv_label_set_text(status_, "Memoriza la secuencia");
    if (show_timer_ != nullptr) {
        lv_timer_set_period(show_timer_, StepPeriod(len_));
        lv_timer_resume(show_timer_);
    }
}

void MemoryGame::Die(const char* title) {
    if (show_timer_ != nullptr) {
        lv_timer_pause(show_timer_);
    }
    if (off_timer_ != nullptr) {
        lv_timer_pause(off_timer_);
    }
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "Ronda: %d - Toca para reiniciar",
             len_);
    overlay_ = GameCreateOverlay(content_, title, subtitle);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}
