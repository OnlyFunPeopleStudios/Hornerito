#include "hornerito/games/breakout.h"

#include <esp_random.h>

#include <cstdio>

#include "hornerito/motion/hornerito_motion.h"

namespace {

constexpr int32_t kTickMs = 25;
constexpr int32_t kFieldW = 480;
constexpr int32_t kFieldY = 44;             // bajo el marcador
constexpr int32_t kFieldH = 372;            // 416 - 44
constexpr int32_t kPaddleW = 80;
constexpr int32_t kPaddleH = 14;
constexpr int32_t kPaddleY = kFieldH - kPaddleH - 4;
constexpr int32_t kBall = 12;
constexpr int32_t kSpeedX = 2;
constexpr int32_t kSpeedY = 3;

constexpr int32_t kBrickCols = 8;
constexpr int32_t kBrickRows = 5;
constexpr int32_t kBrickW = 56;
constexpr int32_t kBrickH = 16;
constexpr int32_t kBrickGap = 2;
constexpr int32_t kBrickX0 = 9;             // centrado: 8*56 + 7*2 = 462
constexpr int32_t kBrickY0 = 8;
constexpr int kBrickTotal = kBrickCols * kBrickRows;

constexpr uint32_t kRowColors[kBrickRows] = {
    0xE05050, 0xE0A050, 0xE0E050, 0x60C860, 0x5BC8F5,
};

int32_t Clamp32(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

BreakoutGame& BreakoutGame::GetInstance() {
    static BreakoutGame instance;
    return instance;
}

void BreakoutGame::TimerThunk(lv_timer_t* timer) {
    static_cast<BreakoutGame*>(lv_timer_get_user_data(timer))->Tick();
}

void BreakoutGame::OverlayThunk(lv_event_t* event) {
    static_cast<BreakoutGame*>(lv_event_get_user_data(event))->Reset();
}

lv_obj_t* BreakoutGame::EnsureBrick(size_t index) {
    if (bricks_[index] != nullptr) {
        return bricks_[index];
    }
    const int32_t col = static_cast<int32_t>(index) % kBrickCols;
    const int32_t row = static_cast<int32_t>(index) / kBrickCols;
    lv_obj_t* brick = GameCreateRect(
        field_, kRowColors[row], kBrickW, kBrickH, 4);
    lv_obj_set_pos(brick, kBrickX0 + col * (kBrickW + kBrickGap),
                   kBrickY0 + row * (kBrickH + kBrickGap));
    bricks_[index] = brick;
    return brick;
}

void BreakoutGame::UpdateHud() {
    lv_label_set_text_fmt(score_label_, "Puntos: %d   Vidas: %d", points_,
                          lives_);
}

void BreakoutGame::Enter(lv_obj_t* content) {
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

    bricks_.assign(kBrickTotal, nullptr);
    paddle_ = GameCreateRect(field_, 0x5BC8F5, kPaddleW, kPaddleH, 7);
    ball_ = GameCreateRect(field_, 0xF2F5F7, kBall, kBall, kBall / 2);

    // Entrada táctil: el dedo mueve la pala (coords locales al campo).
    input_.Attach(field_);
    input_.on_down = input_.on_move = [this](int32_t x, int32_t y) {
        (void)y;
        pressing_ = true;
        paddle_x_ = Clamp32(x - kPaddleW / 2, 0, kFieldW - kPaddleW);
    };
    input_.on_up = [this](int32_t x, int32_t y) {
        (void)x;
        (void)y;
        pressing_ = false;
    };

    // IMU: tilt_x mueve la pala (Disable() al salir es central en
    // GameManager::Exit). No-op si el QMI8658 no está disponible.
    HorneritoMotion::GetInstance().Enable();

    timer_ = lv_timer_create(TimerThunk, kTickMs, this);
    Reset();
}

void BreakoutGame::Exit() {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    input_.on_down = nullptr;
    input_.on_move = nullptr;
    input_.on_up = nullptr;
    input_.Detach();

    bricks_.clear();  // los ladrillos vivos mueren con la pantalla
    content_ = nullptr;
    field_ = nullptr;
    score_label_ = nullptr;
    paddle_ = nullptr;
    ball_ = nullptr;
    overlay_ = nullptr;
    pressing_ = false;
    game_over_ = false;
}

void BreakoutGame::Serve() {
    bx_ = paddle_x_ + kPaddleW / 2 - kBall / 2;
    by_ = kPaddleY - kBall - 4;
    vx_ = (esp_random() & 1) ? kSpeedX : -kSpeedX;
    vy_ = -kSpeedY;
}

void BreakoutGame::Reset() {
    if (overlay_ != nullptr) {
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    for (size_t i = 0; i < bricks_.size(); i++) {
        if (bricks_[i] == nullptr) {
            EnsureBrick(i);  // reconstruir los destruidos
        }
    }
    points_ = 0;
    lives_ = 3;
    remaining_ = kBrickTotal;
    paddle_x_ = (kFieldW - kPaddleW) / 2;
    game_over_ = false;
    pressing_ = false;
    Serve();
    UpdateHud();
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void BreakoutGame::End(bool won) {
    game_over_ = true;
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "Puntos: %d - Toca para reiniciar",
             points_);
    overlay_ = GameCreateOverlay(
        content_, won ? "¡Ganaste!" : "Fin del juego", subtitle);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}

void BreakoutGame::Tick() {
    if (game_over_) {
        return;
    }

    // Pala: dedo mientras toca; si no, inclinación lateral (QMI8658).
    auto& motion = HorneritoMotion::GetInstance();
    if (!pressing_ && motion.available()) {
        const int32_t range = (kFieldW - kPaddleW) / 2;
        paddle_x_ = range + static_cast<int32_t>(motion.tilt_x() * range);
    }

    bx_ += vx_;
    by_ += vy_;

    // Paredes.
    if (bx_ < 0) {
        bx_ = 0;
        vx_ = -vx_;
    } else if (bx_ + kBall > kFieldW) {
        bx_ = kFieldW - kBall;
        vx_ = -vx_;
    }
    if (by_ < 0) {
        by_ = 0;
        vy_ = -vy_;
    }

    // Pelota perdida: una vida menos.
    if (by_ + kBall > kFieldH) {
        lives_--;
        if (lives_ <= 0) {
            UpdateHud();
            End(false);
            return;
        }
        UpdateHud();
        Serve();
    }

    // Pala: rebote clásico (siempre hacia arriba, ángulo según el impacto).
    if (vy_ > 0 && by_ + kBall >= kPaddleY && by_ <= kPaddleY + kPaddleH &&
        bx_ + kBall >= paddle_x_ && bx_ <= paddle_x_ + kPaddleW) {
        by_ = kPaddleY - kBall;
        vy_ = -kSpeedY;
        vx_ = ((bx_ + kBall / 2) - (paddle_x_ + kPaddleW / 2)) / 10;
        if (vx_ == 0) {
            vx_ = 1;
        } else if (vx_ > 3) {
            vx_ = 3;
        } else if (vx_ < -3) {
            vx_ = -3;
        }
    }

    // Ladrillos: uno por tick, rebote por el eje de menor penetración.
    // Las coordenadas se calculan de la fórmula de la rejilla (coords
    // locales al campo, igual que la pelota).
    for (size_t i = 0; i < bricks_.size(); i++) {
        if (bricks_[i] == nullptr) {
            continue;
        }
        const int32_t col = static_cast<int32_t>(i) % kBrickCols;
        const int32_t row = static_cast<int32_t>(i) / kBrickCols;
        const int32_t x1 = kBrickX0 + col * (kBrickW + kBrickGap);
        const int32_t y1 = kBrickY0 + row * (kBrickH + kBrickGap);
        if (bx_ + kBall > x1 && bx_ < x1 + kBrickW &&
            by_ + kBall > y1 && by_ < y1 + kBrickH) {
            const int32_t center_x = bx_ + kBall / 2;
            if (center_x < x1 || center_x > x1 + kBrickW) {
                vx_ = -vx_;
            } else {
                vy_ = -vy_;
            }
            lv_obj_delete(bricks_[i]);
            bricks_[i] = nullptr;
            remaining_--;
            points_ += 10;
            UpdateHud();
            if (remaining_ <= 0) {
                End(true);
                return;
            }
            break;
        }
    }

    lv_obj_set_pos(paddle_, paddle_x_, kPaddleY);
    lv_obj_set_pos(ball_, bx_, by_);
}
