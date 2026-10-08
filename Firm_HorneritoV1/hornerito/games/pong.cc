#include "hornerito/games/pong.h"

#include <esp_random.h>

#include <cstdio>

#include "hornerito/motion/hornerito_motion.h"

namespace {

constexpr int32_t kTickMs = 33;
constexpr int32_t kFieldW = 480;
constexpr int32_t kFieldY = 44;      // bajo el marcador
constexpr int32_t kFieldH = 372;     // 416 - 44
constexpr int32_t kPaddleW = 16;
constexpr int32_t kPaddleH = 80;
constexpr int32_t kPlayerX = 8;
constexpr int32_t kAiX = kFieldW - 8 - kPaddleW;   // 456
constexpr int32_t kBall = 12;
constexpr int32_t kPaddleSpeed = 3;   // px/tick de la IA (pelota va a 4)
constexpr int32_t kBallSpeed = 4;     // antes 3: se pidió subir la velocidad
constexpr int kTarget = 7;

constexpr uint32_t kColorPlayer = 0x5BC8F5;
constexpr uint32_t kColorAi = 0xE0863F;
constexpr uint32_t kColorBall = 0xF2F5F7;

int32_t Clamp32(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

PongGame& PongGame::GetInstance() {
    static PongGame instance;
    return instance;
}

void PongGame::TimerThunk(lv_timer_t* timer) {
    static_cast<PongGame*>(lv_timer_get_user_data(timer))->Tick();
}

void PongGame::OverlayThunk(lv_event_t* event) {
    static_cast<PongGame*>(lv_event_get_user_data(event))->Reset();
}

void PongGame::Enter(lv_obj_t* content) {
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

    player_ = GameCreateRect(field_, kColorPlayer, kPaddleW, kPaddleH, 8);
    ai_ = GameCreateRect(field_, kColorAi, kPaddleW, kPaddleH, 8);
    ball_ = GameCreateRect(field_, kColorBall, kBall, kBall, kBall / 2);

    // Entrada táctil: el dedo mueve la paleta (coords locales al campo).
    input_.Attach(field_);
    input_.on_down = input_.on_move = [this](int32_t x, int32_t y) {
        (void)x;
        pressing_ = true;
        player_y_ = Clamp32(y - kPaddleH / 2, 0, kFieldH - kPaddleH);
    };
    input_.on_up = [this](int32_t x, int32_t y) {
        (void)x;
        (void)y;
        pressing_ = false;
    };

    // IMU: tilt_y mueve la paleta (Disable() al salir es central en
    // GameManager::Exit). No-op si el QMI8658 no está disponible.
    HorneritoMotion::GetInstance().Enable();

    timer_ = lv_timer_create(TimerThunk, kTickMs, this);
    Reset();
}

void PongGame::Exit() {
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
    player_ = nullptr;
    ai_ = nullptr;
    ball_ = nullptr;
    overlay_ = nullptr;
    pressing_ = false;
    game_over_ = false;
}

void PongGame::Serve() {
    bx_ = (kFieldW - kBall) / 2;
    by_ = (kFieldH - kBall) / 2;
    vy_ = (esp_random() & 1) ? 2 : -2;
}

void PongGame::Reset() {
    if (overlay_ != nullptr) {
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    player_pts_ = 0;
    ai_pts_ = 0;
    player_y_ = (kFieldH - kPaddleH) / 2;
    ai_y_ = (kFieldH - kPaddleH) / 2;
    game_over_ = false;
    pressing_ = false;
    Serve();
    vx_ = kBallSpeed;  // primer saque hacia la IA
    lv_label_set_text_fmt(score_label_, "Tú %d - IA %d", player_pts_, ai_pts_);
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void PongGame::AfterPoint(bool player_scored) {
    lv_label_set_text_fmt(score_label_, "Tú %d - IA %d", player_pts_, ai_pts_);
    if (player_pts_ >= kTarget) {
        End(true);
        return;
    }
    if (ai_pts_ >= kTarget) {
        End(false);
        return;
    }
    // Saque hacia el que ha perdido el punto.
    vx_ = player_scored ? kBallSpeed : -kBallSpeed;
    Serve();
}

void PongGame::End(bool player_won) {
    game_over_ = true;
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
    overlay_ = GameCreateOverlay(content_, player_won ? "¡Ganaste!" : "Perdiste",
                                 "Toca para revancha");
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}

void PongGame::Tick() {
    if (game_over_) {
        return;
    }

    // Paleta del jugador: dedo mientras toca; si no, inclinación (QMI8658).
    auto& motion = HorneritoMotion::GetInstance();
    if (!pressing_ && motion.available()) {
        const int32_t range = (kFieldH - kPaddleH) / 2;
        player_y_ = range + static_cast<int32_t>(motion.tilt_y() * range);
    }

    // IA: sigue la pelota con velocidad limitada (alcanzable).
    const int32_t target = (by_ + kBall / 2) - (ai_y_ + kPaddleH / 2);
    ai_y_ += Clamp32(target, -kPaddleSpeed, kPaddleSpeed);
    ai_y_ = Clamp32(ai_y_, 0, kFieldH - kPaddleH);

    // Pelota.
    bx_ += vx_;
    by_ += vy_;
    if (by_ < 0) {
        by_ = 0;
        vy_ = -vy_;
    } else if (by_ + kBall > kFieldH) {
        by_ = kFieldH - kBall;
        vy_ = -vy_;
    }

    // Colisión con la paleta del jugador.
    if (vx_ < 0 && bx_ <= kPlayerX + kPaddleW && bx_ + kBall >= kPlayerX &&
        by_ + kBall >= player_y_ && by_ <= player_y_ + kPaddleH) {
        bx_ = kPlayerX + kPaddleW;
        vx_ = -vx_;
        vy_ = ((by_ + kBall / 2) - (player_y_ + kPaddleH / 2)) / 12;
        if (vy_ == 0) {
            vy_ = 1;
        }
    }

    // Colisión con la paleta de la IA.
    if (vx_ > 0 && bx_ + kBall >= kAiX && bx_ <= kAiX + kPaddleW &&
        by_ + kBall >= ai_y_ && by_ <= ai_y_ + kPaddleH) {
        bx_ = kAiX - kBall;
        vx_ = -vx_;
        vy_ = ((by_ + kBall / 2) - (ai_y_ + kPaddleH / 2)) / 12;
        if (vy_ == 0) {
            vy_ = -1;
        }
    }

    // Punto.
    if (bx_ + kBall < 0) {
        ai_pts_++;
        AfterPoint(false);
    } else if (bx_ > kFieldW) {
        player_pts_++;
        AfterPoint(true);
    }

    lv_obj_set_pos(player_, kPlayerX, player_y_);
    lv_obj_set_pos(ai_, kAiX, ai_y_);
    lv_obj_set_pos(ball_, bx_, by_);
}
