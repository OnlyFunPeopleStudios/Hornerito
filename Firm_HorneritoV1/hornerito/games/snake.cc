#include "hornerito/games/snake.h"

#include <esp_random.h>

#include <cstdio>

namespace {

constexpr int32_t kTickMs = 130;
constexpr int32_t kCell = 24;           // celda de la rejilla
constexpr int32_t kCols = 20;           // 480 / 24
constexpr int32_t kRows = 15;           // rejilla 480x360 bajo el marcador
constexpr int32_t kGridY = 44;
constexpr int32_t kSteerThreshold = 16;  // px de arrastre mínimo para girar

constexpr uint32_t kColorHead = 0x8CE06A;
constexpr uint32_t kColorBody = 0x3FBF6A;
constexpr uint32_t kColorFood = 0xE05050;

bool Same(int32_t ax, int32_t ay, int32_t bx, int32_t by) {
    return ax == bx && ay == by;
}

}  // namespace

SnakeGame& SnakeGame::GetInstance() {
    static SnakeGame instance;
    return instance;
}

void SnakeGame::TimerThunk(lv_timer_t* timer) {
    static_cast<SnakeGame*>(lv_timer_get_user_data(timer))->Tick();
}

void SnakeGame::OverlayThunk(lv_event_t* event) {
    static_cast<SnakeGame*>(lv_event_get_user_data(event))->Reset();
}

void SnakeGame::Enter(lv_obj_t* content) {
    content_ = content;

    score_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(score_label_);
    lv_obj_align(score_label_, LV_ALIGN_TOP_MID, 0, 2);

    grid_ = lv_obj_create(content_);
    lv_obj_set_size(grid_, kCols * kCell, kRows * kCell);
    lv_obj_align(grid_, LV_ALIGN_TOP_MID, 0, kGridY);
    lv_obj_set_style_bg_opa(grid_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_, 0, 0);
    lv_obj_set_style_radius(grid_, 0, 0);
    lv_obj_set_style_pad_all(grid_, 0, 0);
    lv_obj_set_scrollbar_mode(grid_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(grid_, LV_OBJ_FLAG_SCROLLABLE);

    food_ = GameCreateRect(grid_, kColorFood, kCell - 4, kCell - 4, 6);

    // Capa común de entrada: Touch -> Game Input -> Juego.
    input_.Attach(grid_);
    input_.on_down = [this](int32_t x, int32_t y) {
        pressing_ = true;
        down_x_ = x;
        down_y_ = y;
    };
    input_.on_move = [this](int32_t x, int32_t y) {
        if (pressing_) {
            SteerFrom(x - down_x_, y - down_y_);
        }
    };
    input_.on_up = [this](int32_t x, int32_t y) {
        if (pressing_) {
            SteerFrom(x - down_x_, y - down_y_);
        }
        pressing_ = false;
    };

    timer_ = lv_timer_create(TimerThunk, kTickMs, this);
    Reset();
}

void SnakeGame::Exit() {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    input_.on_down = nullptr;
    input_.on_move = nullptr;
    input_.on_up = nullptr;
    input_.Detach();

    // Los widgets mueren con la pantalla de sesión: aquí solo se sueltan
    // referencias y estado para que re-entrar arranque limpio.
    segs_.clear();
    body_.clear();
    content_ = nullptr;
    grid_ = nullptr;
    score_label_ = nullptr;
    food_ = nullptr;
    overlay_ = nullptr;
    dead_ = false;
    pressing_ = false;
}

void SnakeGame::Reset() {
    if (overlay_ != nullptr) {
        // Borrado diferido: evita eliminar el objeto dentro de su propio evento.
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    for (lv_obj_t* segment : segs_) {
        lv_obj_delete(segment);
    }
    segs_.clear();

    body_.clear();
    body_.push_back({7, 7});
    body_.push_back({6, 7});
    body_.push_back({5, 7});
    body_.push_back({4, 7});
    dir_ = Dir::kRight;
    pending_dir_ = Dir::kRight;
    points_ = 0;
    dead_ = false;
    pressing_ = false;

    PlaceFood();
    SyncSegments();
    lv_label_set_text_fmt(score_label_, "Puntos: %d", points_);
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void SnakeGame::SteerFrom(int32_t dx, int32_t dy) {
    if (dead_) {
        return;
    }
    const int32_t ax = dx < 0 ? -dx : dx;
    const int32_t ay = dy < 0 ? -dy : dy;
    if (ax < kSteerThreshold && ay < kSteerThreshold) {
        return;  // trepidación por debajo del umbral
    }

    Dir want;
    if (ax > ay) {
        want = dx > 0 ? Dir::kRight : Dir::kLeft;
    } else {
        want = dy > 0 ? Dir::kDown : Dir::kUp;
    }

    // Prohibido giro de 180° respecto de la dirección actual.
    if ((want == Dir::kUp && dir_ == Dir::kDown) ||
        (want == Dir::kDown && dir_ == Dir::kUp) ||
        (want == Dir::kLeft && dir_ == Dir::kRight) ||
        (want == Dir::kRight && dir_ == Dir::kLeft)) {
        return;
    }
    pending_dir_ = want;
}

void SnakeGame::Tick() {
    if (dead_ || body_.empty()) {
        return;
    }

    int8_t nx = body_[0].x;
    int8_t ny = body_[0].y;
    switch (dir_) {
        case Dir::kUp:
            ny--;
            break;
        case Dir::kDown:
            ny++;
            break;
        case Dir::kLeft:
            nx--;
            break;
        case Dir::kRight:
            nx++;
            break;
    }

    if (nx < 0 || nx >= kCols || ny < 0 || ny >= kRows) {
        Die(false);
        return;
    }

    const bool grow = Same(nx, ny, food_cell_.x, food_cell_.y);
    // La cola se libera al mover, salvo que se vaya a comer (entonces crece).
    const size_t limit = body_.size() - (grow ? 0 : 1);
    for (size_t i = 0; i < limit; i++) {
        if (Same(nx, ny, body_[i].x, body_[i].y)) {
            Die(false);
            return;
        }
    }

    body_.push_front({nx, ny});
    if (grow) {
        points_++;
        lv_label_set_text_fmt(score_label_, "Puntos: %d", points_);
        PlaceFood();
        if (dead_) {
            return;  // victoria: tablero completo
        }
    } else {
        body_.pop_back();
    }

    dir_ = pending_dir_;  // aplicar el giro pendiente tras mover
    SyncSegments();
}

void SnakeGame::PlaceFood() {
    for (int tries = 0; tries < 400; tries++) {
        const int32_t x = static_cast<int32_t>(esp_random() % kCols);
        const int32_t y = static_cast<int32_t>(esp_random() % kRows);
        bool occupied = false;
        for (const auto& cell : body_) {
            if (Same(x, y, cell.x, cell.y)) {
                occupied = true;
                break;
            }
        }
        if (!occupied) {
            food_cell_ = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
            lv_obj_set_pos(food_, x * kCell + 2, y * kCell + 2);
            return;
        }
    }
    Die(true);  // sin celdas libres: victoria
}

void SnakeGame::SyncSegments() {
    while (segs_.size() < body_.size()) {
        segs_.push_back(GameCreateRect(grid_, kColorBody, kCell - 2, kCell - 2, 6));
    }
    for (size_t i = 0; i < body_.size(); i++) {
        lv_obj_set_pos(segs_[i], body_[i].x * kCell, body_[i].y * kCell);
    }
    if (!segs_.empty()) {
        lv_obj_set_style_bg_color(segs_[0], lv_color_hex(kColorHead), 0);
    }
}

void SnakeGame::Die(bool win) {
    if (dead_) {
        return;
    }
    dead_ = true;
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "Puntos: %d - Toca para reiniciar",
             points_);
    overlay_ = GameCreateOverlay(content_, win ? "¡Ganaste!" : "Fin del juego",
                                 subtitle);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}
