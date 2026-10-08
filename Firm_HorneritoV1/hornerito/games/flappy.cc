#include "hornerito/games/flappy.h"

#include <esp_random.h>

#include <cstdio>

#include "hornerito/motion/hornerito_motion.h"

namespace {

constexpr int32_t kTickMs = 33;
constexpr int32_t kFieldW = 480;
constexpr int32_t kFieldY = 44;             // bajo el marcador
constexpr int32_t kFieldH = 372;            // 416 - 44
constexpr int32_t kBirdX = 120;
constexpr int32_t kBirdSize = 40;
constexpr float kGravity = 0.8f;            // px/tick^2
constexpr float kFlapVy = -6.5f;            // px/tick
constexpr float kMaxFall = 10.0f;

constexpr int32_t kPipeW = 56;
constexpr int32_t kPipeGap = 110;
constexpr int32_t kPipeSpeed = 3;           // px/tick (90 px/s)
constexpr int32_t kPipeSpacing = 300;
constexpr int32_t kPipeTopMin = 40;
constexpr int32_t kPipeTopMax = kFieldH - kPipeGap - 40;

constexpr int32_t kBirdImgSize = 192;       // emoji original
constexpr uint32_t kBirdScale = 256 * kBirdSize / kBirdImgSize;  // 53 ≈ 40 px

}  // namespace

FlappyGame& FlappyGame::GetInstance() {
    static FlappyGame instance;
    return instance;
}

void FlappyGame::TimerThunk(lv_timer_t* timer) {
    static_cast<FlappyGame*>(lv_timer_get_user_data(timer))->Tick();
}

void FlappyGame::OverlayThunk(lv_event_t* event) {
    static_cast<FlappyGame*>(lv_event_get_user_data(event))->Reset();
}

void FlappyGame::UpdateHud() {
    lv_label_set_text_fmt(score_label_, "Puntos: %d", points_);
}

void FlappyGame::Recycle(Pipe& pipe) {
    // Reaparece a la derecha del otro tubo (nunca dentro de la pantalla).
    const Pipe& other = (&pipe == &pipes_[0]) ? pipes_[1] : pipes_[0];
    pipe.x = other.x + kPipeSpacing;
    pipe.top_h = kPipeTopMin +
                 static_cast<int32_t>(esp_random() %
                                      (kPipeTopMax - kPipeTopMin + 1));
    pipe.scored = false;
    lv_obj_set_size(pipe.top, kPipeW, pipe.top_h);
    lv_obj_set_size(pipe.bottom, kPipeW, kFieldH - pipe.top_h - kPipeGap);
    lv_obj_set_pos(pipe.bottom, pipe.x, pipe.top_h + kPipeGap);
}

void FlappyGame::Enter(lv_obj_t* content) {
    content_ = content;

    score_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0xF2F5F7), 0);
    GameApplyFont(score_label_);
    lv_obj_align(score_label_, LV_ALIGN_TOP_MID, 0, 2);

    field_ = lv_obj_create(content_);
    lv_obj_set_size(field_, kFieldW, kFieldH);
    lv_obj_align(field_, LV_ALIGN_TOP_MID, 0, kFieldY);
    lv_obj_set_style_bg_opa(field_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(field_, 0, 0);
    lv_obj_set_style_radius(field_, 0, 0);
    lv_obj_set_style_pad_all(field_, 0, 0);
    lv_obj_set_scrollbar_mode(field_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(field_, LV_OBJ_FLAG_SCROLLABLE);

    // Pájaro: emoji propio de Hornerito escalado al tamaño de juego.
    bird_ = lv_image_create(field_);
    lv_image_set_src(bird_, HorneritoEmojiImage(HorneritoEmoji::kSpeaking));
    lv_image_set_scale(bird_, kBirdScale);
    lv_obj_set_size(bird_, kBirdSize, kBirdSize);

    // Tuberías (dos pares, reciclados por Recycle()).
    for (auto& pipe : pipes_) {
        pipe.top = GameCreateRect(field_, 0x60C860, kPipeW, 80, 6);
        pipe.bottom = GameCreateRect(field_, 0x60C860, kPipeW, 80, 6);
    }

    // Aleteo por toque (coords locales al campo) + sacudida opcional.
    input_.Attach(field_);
    input_.on_down = [this](int32_t x, int32_t y) {
        (void)x;
        (void)y;
        Flap();
    };

    // IMU: shake aletea además del toque (Disable() al salir es central en
    // GameManager::Exit). No-op si el QMI8658 no está disponible.
    HorneritoMotion::GetInstance().Enable();

    timer_ = lv_timer_create(TimerThunk, kTickMs, this);
    Reset();
}

void FlappyGame::Exit() {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    input_.on_down = nullptr;
    input_.on_move = nullptr;
    input_.on_up = nullptr;
    input_.Detach();

    content_ = nullptr;
    field_ = nullptr;
    score_label_ = nullptr;
    bird_ = nullptr;
    overlay_ = nullptr;
    for (auto& pipe : pipes_) {
        pipe.top = nullptr;
        pipe.bottom = nullptr;
    }
    game_over_ = false;
}

void FlappyGame::Flap() {
    if (game_over_) {
        return;
    }
    vy_ = kFlapVy;
}

void FlappyGame::Reset() {
    if (overlay_ != nullptr) {
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    bird_y_ = static_cast<float>((kFieldH - kBirdSize) / 2);
    vy_ = -1.0f;
    points_ = 0;
    game_over_ = false;

    pipes_[0].x = 520;
    pipes_[1].x = 520 + kPipeSpacing;
    for (auto& pipe : pipes_) {
        pipe.top_h = kPipeTopMin +
                     static_cast<int32_t>(esp_random() %
                                          (kPipeTopMax - kPipeTopMin + 1));
        pipe.scored = false;
        lv_obj_set_size(pipe.top, kPipeW, pipe.top_h);
        lv_obj_set_size(pipe.bottom, kPipeW, kFieldH - pipe.top_h - kPipeGap);
        lv_obj_set_pos(pipe.top, pipe.x, 0);
        lv_obj_set_pos(pipe.bottom, pipe.x, pipe.top_h + kPipeGap);
    }

    UpdateHud();
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void FlappyGame::End() {
    game_over_ = true;
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "Puntos: %d - Toca para reiniciar",
             points_);
    overlay_ = GameCreateOverlay(content_, "Fin del juego", subtitle);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}

void FlappyGame::Tick() {
    if (game_over_) {
        return;
    }

    // Modalidad alterna: sacudir el dispositivo aletea (IMU opcional).
    if (HorneritoMotion::GetInstance().ConsumeShake()) {
        Flap();
    }

    vy_ += kGravity;
    if (vy_ > kMaxFall) {
        vy_ = kMaxFall;
    }
    bird_y_ += vy_;
    if (bird_y_ < 0.0f) {
        bird_y_ = 0.0f;
        if (vy_ < 0.0f) {
            vy_ = 0.0f;
        }
    }

    for (auto& pipe : pipes_) {
        pipe.x -= kPipeSpeed;
        if (pipe.x + kPipeW < 0) {
            Recycle(pipe);
        }
        if (!pipe.scored && pipe.x + kPipeW < kBirdX) {
            pipe.scored = true;
            points_++;
            UpdateHud();
        }

        // Tuberías: 0..top_h y top_h+gap..H. La caja del pájaro es 40x40.
        if (kBirdX + kBirdSize > pipe.x && kBirdX < pipe.x + kPipeW) {
            const int32_t y = static_cast<int32_t>(bird_y_);
            if (y < pipe.top_h || y + kBirdSize > pipe.top_h + kPipeGap) {
                End();
                return;
            }
        }
        lv_obj_set_pos(pipe.top, pipe.x, 0);
        lv_obj_set_pos(pipe.bottom, pipe.x, pipe.top_h + kPipeGap);
    }

    // Suelo.
    if (static_cast<int32_t>(bird_y_) + kBirdSize > kFieldH) {
        bird_y_ = static_cast<float>(kFieldH - kBirdSize);
        End();
        return;
    }

    lv_obj_set_pos(bird_, kBirdX, static_cast<int32_t>(bird_y_));
}
