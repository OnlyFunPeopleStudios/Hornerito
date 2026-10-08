#include "hornerito/games/rock_paper_scissors.h"

#include <esp_random.h>

#include "hornerito/hornerito_emojis.h"
#include "hornerito/ui/hornerito_ui.h"
#include "material_symbols.h"

namespace {

constexpr int kThinkMs = 900;

constexpr const char* kMoveName[3] = {"PIEDRA", "PAPEL", "TIJERA"};

// Mapeo de SetEmoji: 0 = idle, 1 = thinking, 2 = triste (error),
// 3 = hablando (feliz). Emojis existentes, sin assets nuevos.
lv_obj_t* MakeChoiceButton(lv_obj_t* parent, const char* text, int32_t x,
                           void (*cb)(lv_event_t*), void* user) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, 140, 76);
    lv_obj_set_pos(button, x, 316);
    GameStyleDarkButton(button);
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, text);
    GameApplyFont(label);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user);
    return button;
}

}  // namespace

RockPaperScissorsGame& RockPaperScissorsGame::GetInstance() {
    static RockPaperScissorsGame instance;
    return instance;
}

void RockPaperScissorsGame::ThinkThunk(lv_timer_t* timer) {
    auto* self =
        static_cast<RockPaperScissorsGame*>(lv_timer_get_user_data(timer));
    if (self->phase_ != Phase::kThinking) {
        lv_timer_pause(timer);  // protección si el timer se disparó pronto
        return;
    }
    self->Resolve();
}

void RockPaperScissorsGame::BtnThunk(lv_event_t* event) {
    auto* self =
        static_cast<RockPaperScissorsGame*>(lv_event_get_user_data(event));
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    for (int i = 0; i < 3; i++) {
        if (target == self->btn_[i]) {
            self->OnPick(i);
            return;
        }
    }
}

void RockPaperScissorsGame::ResetThunk(lv_event_t* event) {
    auto* self =
        static_cast<RockPaperScissorsGame*>(lv_event_get_user_data(event));
    self->score_user_ = 0;
    self->score_cpu_ = 0;
    self->phase_ = Phase::kPick;
    self->UpdateScore();
    lv_label_set_text(self->status_label_, "Elige tu jugada");
    lv_label_set_text(self->reveal_label_, "");
    lv_label_set_text(self->result_label_, "");
    self->SetEmoji(0);
    for (lv_obj_t* button : self->btn_) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
    }
}

void RockPaperScissorsGame::Enter(lv_obj_t* content) {
    content_ = content;

    score_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(score_label_);
    lv_obj_align(score_label_, LV_ALIGN_TOP_MID, 0, 8);

    // Reiniciar marcador (esquina superior derecha).
    reset_btn_ = lv_button_create(content_);
    lv_obj_set_size(reset_btn_, 44, 44);
    lv_obj_set_pos(reset_btn_, 432, 4);
    GameStyleDarkButton(reset_btn_);
    lv_obj_t* reset_icon = lv_label_create(reset_btn_);
    lv_label_set_text(reset_icon, MATERIAL_SYMBOLS_REFRESH);
    lv_obj_set_style_text_font(reset_icon, HorneritoIconFont(), 0);
    lv_obj_center(reset_icon);
    lv_obj_add_event_cb(reset_btn_, ResetThunk, LV_EVENT_CLICKED, this);

    // Hornerito (emoji existente, 110 px).
    emoji_ = lv_image_create(content_);
    lv_image_set_src(emoji_, HorneritoEmojiImage(HorneritoEmoji::kIdle));
    lv_image_set_scale(emoji_, 256 * 110 / 192);
    lv_obj_set_size(emoji_, 110, 110);
    lv_obj_set_pos(emoji_, 185, 36);

    status_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(status_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(status_label_);
    lv_obj_align(status_label_, LV_ALIGN_TOP_MID, 0, 156);

    reveal_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(reveal_label_, lv_color_white(), 0);
    GameApplyFont(reveal_label_);
    lv_obj_align(reveal_label_, LV_ALIGN_TOP_MID, 0, 192);

    result_label_ = lv_label_create(content_);
    GameApplyFont(result_label_);
    lv_obj_align(result_label_, LV_ALIGN_TOP_MID, 0, 224);

    btn_[0] = MakeChoiceButton(content_, "PIEDRA", 20, BtnThunk, this);
    btn_[1] = MakeChoiceButton(content_, "PAPEL", 170, BtnThunk, this);
    btn_[2] = MakeChoiceButton(content_, "TIJERA", 320, BtnThunk, this);

    think_timer_ = lv_timer_create(ThinkThunk, kThinkMs, this);
    lv_timer_pause(think_timer_);

    score_user_ = 0;
    score_cpu_ = 0;
    phase_ = Phase::kPick;
    UpdateScore();
    lv_label_set_text(status_label_, "Elige tu jugada");
    lv_label_set_text(reveal_label_, "");
    lv_label_set_text(result_label_, "");
    SetEmoji(0);
}

void RockPaperScissorsGame::Exit() {
    if (think_timer_ != nullptr) {
        lv_timer_delete(think_timer_);
        think_timer_ = nullptr;
    }

    // Los widgets mueren con la pantalla de sesión: solo se sueltan
    // referencias para que re-entrar arranque limpio.
    content_ = nullptr;
    score_label_ = nullptr;
    status_label_ = nullptr;
    reveal_label_ = nullptr;
    result_label_ = nullptr;
    emoji_ = nullptr;
    for (lv_obj_t*& button : btn_) {
        button = nullptr;
    }
    reset_btn_ = nullptr;
    phase_ = Phase::kPick;
}

void RockPaperScissorsGame::SetEmoji(int emoji_index) {
    HorneritoEmoji emoji = HorneritoEmoji::kIdle;
    switch (emoji_index) {
        case 1:
            emoji = HorneritoEmoji::kThinking;
            break;
        case 2:
            emoji = HorneritoEmoji::kError;  // cara de "me derrotaste"
            break;
        case 3:
            emoji = HorneritoEmoji::kSpeaking;  // cara de "he ganado"
            break;
        default:
            break;
    }
    lv_image_set_src(emoji_, HorneritoEmojiImage(emoji));
}

void RockPaperScissorsGame::UpdateScore() {
    lv_label_set_text_fmt(score_label_, "Tú %d — Hornerito %d", score_user_,
                          score_cpu_);
}

void RockPaperScissorsGame::OnPick(int choice) {
    if (phase_ == Phase::kThinking) {
        return;  // aún está pensando: no se aceptan rondas en paralelo
    }
    choice_ = choice;
    phase_ = Phase::kThinking;
    lv_label_set_text(status_label_, "Hornerito está pensando...");
    lv_label_set_text(reveal_label_, "");
    lv_label_set_text(result_label_, "");
    SetEmoji(1);
    for (lv_obj_t* button : btn_) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
    }
    if (think_timer_ != nullptr) {
        lv_timer_resume(think_timer_);
    }
}

void RockPaperScissorsGame::Resolve() {
    const int cpu = static_cast<int>(esp_random() % 3);
    const int diff = (choice_ - cpu + 3) % 3;

    lv_label_set_text_fmt(reveal_label_, "Tú %s — Hornerito %s",
                          kMoveName[choice_], kMoveName[cpu]);

    if (diff == 1) {  // (elegido - cpu) == 1 -> ganas
        score_user_++;
        lv_label_set_text(result_label_, "¡GANASTE!");
        lv_obj_set_style_text_color(result_label_, lv_color_hex(0x6FCF97), 0);
        SetEmoji(2);
    } else if (diff == 2) {
        score_cpu_++;
        lv_label_set_text(result_label_, "PERDISTE");
        lv_obj_set_style_text_color(result_label_, lv_color_hex(0xEB5757), 0);
        SetEmoji(3);
    } else {
        lv_label_set_text(result_label_, "EMPATE");
        lv_obj_set_style_text_color(result_label_, lv_color_hex(0xF2C94C), 0);
        SetEmoji(0);
    }

    UpdateScore();
    lv_label_set_text(status_label_, "Toca una opción para la siguiente ronda");
    phase_ = Phase::kResult;
    for (lv_obj_t* button : btn_) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
    }
    if (think_timer_ != nullptr) {
        lv_timer_pause(think_timer_);
    }
}
