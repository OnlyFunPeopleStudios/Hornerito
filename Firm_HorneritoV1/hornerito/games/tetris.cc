#include "hornerito/games/tetris.h"

#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "hornerito/motion/hornerito_motion.h"
#include "hornerito/ui/hornerito_ui.h"
#include "material_symbols.h"

namespace {

constexpr int32_t kCols = 10;
constexpr int32_t kRows = 20;
constexpr int32_t kCell = 18;          // celda del tablero
constexpr int32_t kBoardX = 16;
constexpr int32_t kBoardY = 44;
constexpr int32_t kNextCell = 14;      // celda de la vista previa
constexpr int32_t kNextX = 314;        // origen de la vista previa (panel)
constexpr int32_t kNextY = 78;
constexpr int32_t kTapMax = 12;        // px: por debajo = tap (girar)
constexpr int32_t kSwipeMin = 14;      // px: swipe mínimo para actuar
constexpr int32_t kTiltTickMs = 50;    // periodo del chequeo de tilt
constexpr int32_t kTiltStepMs = 180;   // ms entre pasos al inclinar
constexpr float kTiltThreshold = 0.45f;  // |tilt| mínimo para mover/girar
constexpr float kTiltRearm = 0.25f;      // histéresis: rear-armar giro al bajar

constexpr uint32_t kColorBoard = 0x10161E;
// Piezas en orden I O T S Z J L.
constexpr uint32_t kColorCell[7] = {0x4DD0E1, 0xF2C94C, 0xBB6BD9,
                                    0x6FCF97, 0xEB5757, 0x56CCF2, 0xF2994A};
constexpr int32_t kLineScore[5] = {0, 100, 300, 500, 800};

// Formas clásicas: [pieza][rotación][celda 0..3] = offset (x, y) dentro de
// una caja 4x4 (x a la derecha, y hacia abajo). Rotaciones sin SRS: para
// Tetris simplificado basta con que sean consistentes y sin solapes.
const int8_t kShapeX[7][4][4] = {
    {{0, 1, 2, 3}, {2, 2, 2, 2}, {0, 1, 2, 3}, {2, 2, 2, 2}},  // I
    {{1, 2, 1, 2}, {1, 2, 1, 2}, {1, 2, 1, 2}, {1, 2, 1, 2}},  // O
    {{1, 0, 1, 2}, {1, 1, 2, 1}, {0, 1, 2, 1}, {1, 0, 1, 1}},  // T
    {{1, 2, 0, 1}, {1, 1, 2, 2}, {1, 2, 0, 1}, {1, 1, 2, 2}},  // S
    {{0, 1, 1, 2}, {2, 1, 2, 1}, {0, 1, 1, 2}, {2, 1, 2, 1}},  // Z
    {{0, 0, 1, 2}, {1, 2, 1, 1}, {0, 1, 2, 2}, {1, 1, 0, 1}},  // J
    {{2, 0, 1, 2}, {1, 1, 1, 2}, {0, 1, 2, 0}, {0, 1, 1, 1}},  // L
};
const int8_t kShapeY[7][4][4] = {
    {{1, 1, 1, 1}, {0, 1, 2, 3}, {1, 1, 1, 1}, {0, 1, 2, 3}},  // I
    {{0, 0, 1, 1}, {0, 0, 1, 1}, {0, 0, 1, 1}, {0, 0, 1, 1}},  // O
    {{0, 1, 1, 1}, {0, 1, 1, 2}, {1, 1, 1, 2}, {0, 1, 1, 2}},  // T
    {{0, 0, 1, 1}, {0, 1, 1, 2}, {0, 0, 1, 1}, {0, 1, 1, 2}},  // S
    {{0, 0, 1, 1}, {0, 1, 1, 2}, {0, 0, 1, 1}, {0, 1, 1, 2}},  // Z
    {{0, 1, 1, 1}, {0, 0, 1, 2}, {1, 1, 1, 2}, {0, 1, 2, 2}},  // J
    {{0, 1, 1, 1}, {0, 1, 2, 2}, {1, 1, 1, 2}, {0, 0, 1, 2}},  // L
};

int32_t PeriodForLevel(int32_t level) {
    const int32_t p = 700 - 60 * level;
    return p < 140 ? 140 : p;
}

lv_obj_t* MakeButton(lv_obj_t* parent, const char* icon, int32_t x, int32_t y,
                     void (*cb)(lv_event_t*), void* user) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, 124, 64);
    lv_obj_set_pos(button, x, y);
    GameStyleDarkButton(button);
    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, icon);
    lv_obj_set_style_text_font(label, HorneritoIconFont(), 0);
    lv_obj_center(label);
    // PRESSED (patrón Memory, que repite bien en pruebas): actúa al tocar,
    // sin depender de la ruta de liberación (scroll/short_click) de CLICKED.
    lv_obj_add_event_cb(button, cb, LV_EVENT_PRESSED, user);
    return button;
}

}  // namespace

TetrisGame& TetrisGame::GetInstance() {
    static TetrisGame instance;
    return instance;
}

void TetrisGame::TimerThunk(lv_timer_t* timer) {
    static_cast<TetrisGame*>(lv_timer_get_user_data(timer))->Tick();
}

void TetrisGame::TiltThunk(lv_timer_t* timer) {
    static_cast<TetrisGame*>(lv_timer_get_user_data(timer))->TiltTick();
}

void TetrisGame::BtnThunk(lv_event_t* event) {
    auto* self = static_cast<TetrisGame*>(lv_event_get_user_data(event));
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (target == self->btn_left_) {
        self->Move(-1);
    } else if (target == self->btn_right_) {
        self->Move(1);
    } else if (target == self->btn_rotate_) {
        self->Rotate();
    } else if (target == self->btn_down_) {
        self->SoftDrop();
    }
}

void TetrisGame::OverlayThunk(lv_event_t* event) {
    static_cast<TetrisGame*>(lv_event_get_user_data(event))->Reset();
}

void TetrisGame::Enter(lv_obj_t* content) {
    content_ = content;

    // HUD en la tira superior.
    score_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(score_label_);
    lv_obj_align(score_label_, LV_ALIGN_TOP_LEFT, 16, 8);
    level_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(level_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(level_label_);
    lv_obj_align(level_label_, LV_ALIGN_TOP_MID, 0, 8);
    lines_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(lines_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(lines_label_);
    lv_obj_align(lines_label_, LV_ALIGN_TOP_RIGHT, -16, 8);

    // Tablero (también es el área de gestos).
    board_ = lv_obj_create(content_);
    lv_obj_set_size(board_, kCols * kCell, kRows * kCell);
    lv_obj_set_pos(board_, kBoardX, kBoardY);
    lv_obj_set_style_bg_color(board_, lv_color_hex(kColorBoard), 0);
    lv_obj_set_style_bg_opa(board_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(board_, lv_color_hex(0x2E4058), 0);
    lv_obj_set_style_radius(board_, 0, 0);
    lv_obj_set_style_pad_all(board_, 0, 0);
    lv_obj_set_scrollbar_mode(board_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(board_, LV_OBJ_FLAG_SCROLLABLE);

    // 4 celdas de la pieza activa (posicionadas en Sync()).
    for (int32_t i = 0; i < 4; i++) {
        piece_[i] = GameCreateRect(board_, kColorCell[0], kCell - 2, kCell - 2, 4);
        lv_obj_add_flag(piece_[i], LV_OBJ_FLAG_HIDDEN);
        next_cells_[i] = GameCreateRect(content_, kColorCell[0], kNextCell - 2,
                                  kNextCell - 2, 4);
    }

    // Vista previa de la siguiente pieza.
    lv_obj_t* next_label = lv_label_create(content_);
    lv_label_set_text(next_label, "SIGUIENTE");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x5A6B7C), 0);
    GameApplyFont(next_label);
    lv_obj_align(next_label, LV_ALIGN_TOP_MID, 102, 50);

    // Botones de control (panel derecho, abajo para el pulgar).
    btn_left_ = MakeButton(content_, MATERIAL_SYMBOLS_ARROW_BACK, 212, 250,
                           BtnThunk, this);
    btn_right_ = MakeButton(content_, MATERIAL_SYMBOLS_ARROW_FORWARD, 344, 250,
                            BtnThunk, this);
    btn_rotate_ = MakeButton(content_, MATERIAL_SYMBOLS_REFRESH, 212, 322,
                             BtnThunk, this);
    btn_down_ = MakeButton(content_, MATERIAL_SYMBOLS_ARROW_DOWNWARD, 344, 322,
                           BtnThunk, this);

    // Gestos sobre el tablero: tap = girar, swipe lateral = mover,
    // swipe abajo = soft drop.
    input_.Attach(board_);
    input_.on_down = [this](int32_t x, int32_t y) {
        pressing_ = true;
        down_x_ = x;
        down_y_ = y;
    };
    input_.on_up = [this](int32_t x, int32_t y) {
        if (!pressing_) {
            return;
        }
        pressing_ = false;
        if (dead_) {
            return;
        }
        const int32_t dx = x - down_x_;
        const int32_t dy = y - down_y_;
        const int32_t ax = dx < 0 ? -dx : dx;
        const int32_t ay = dy < 0 ? -dy : dy;
        if (ax <= kTapMax && ay <= kTapMax) {
            Rotate();
        } else if (ax > ay) {
            if (ax >= kSwipeMin) {
                Move(dx > 0 ? 1 : -1);
            }
        } else if (dy > 0 && ay >= kSwipeMin) {
            SoftDrop();
        }
    };

    timer_ = lv_timer_create(TimerThunk, PeriodForLevel(0), this);
    // Movimiento con el sensor: tilt_x mueve la pieza, tilt_y la gira.
    HorneritoMotion::GetInstance().Enable();
    tilt_timer_ = lv_timer_create(TiltThunk, kTiltTickMs, this);
    Reset();
}

void TetrisGame::Exit() {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (tilt_timer_ != nullptr) {
        lv_timer_delete(tilt_timer_);
        tilt_timer_ = nullptr;
    }
    input_.on_down = nullptr;
    input_.on_move = nullptr;
    input_.on_up = nullptr;
    input_.Detach();

    // Los widgets mueren con la pantalla de sesión: solo se sueltan
    // referencias y estado para que re-entrar arranque limpio.
    pool_.clear();
    content_ = nullptr;
    board_ = nullptr;
    score_label_ = nullptr;
    level_label_ = nullptr;
    lines_label_ = nullptr;
    btn_left_ = nullptr;
    btn_right_ = nullptr;
    btn_rotate_ = nullptr;
    btn_down_ = nullptr;
    for (int32_t i = 0; i < 4; i++) {
        piece_[i] = nullptr;
        next_cells_[i] = nullptr;
    }
    overlay_ = nullptr;
    dead_ = false;
    pressing_ = false;
}

void TetrisGame::Reset() {
    if (overlay_ != nullptr) {
        // Borrado diferido: evita eliminar el objeto dentro de su evento.
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    memset(grid_, 0, sizeof(grid_));
    score_ = 0;
    lines_ = 0;
    level_ = 0;
    dead_ = false;
    pressing_ = false;
    tilt_cd_ = 0;
    tilt_rot_armed_ = true;
    cur_ = static_cast<int32_t>(esp_random() % 7);
    next_ = static_cast<int32_t>(esp_random() % 7);
    rot_ = 0;
    px_ = 3;
    py_ = 0;
    UpdateHud();
    UpdatePreview();
    if (timer_ != nullptr) {
        lv_timer_set_period(timer_, PeriodForLevel(level_));
        lv_timer_resume(timer_);
    }
    Sync();
}

bool TetrisGame::CanPlace(int32_t px, int32_t py, int32_t rot) const {
    for (int32_t i = 0; i < 4; i++) {
        const int32_t bx = px + kShapeX[cur_][rot][i];
        const int32_t by = py + kShapeY[cur_][rot][i];
        if (bx < 0 || bx >= kCols || by >= kRows) {
            return false;
        }
        if (by >= 0 && grid_[by][bx] != 0) {
            return false;
        }
    }
    return true;
}

void TetrisGame::TiltTick() {
    if (dead_ || pressing_) {
        return;  // con el dedo en el tablero manda el touch
    }
    auto& motion = HorneritoMotion::GetInstance();
    if (!motion.available()) {
        return;
    }

    // Tilt_Y (frontal) gira la pieza: edge-trigger con histéresis para que
    // una sola inclinación produzca UN giro; rear-marca al bajar de kTiltRearm.
    const float ty = motion.tilt_y();
    if (tilt_rot_armed_) {
        if (ty >= kTiltThreshold || ty <= -kTiltThreshold) {
            Rotate();
            tilt_rot_armed_ = false;
        }
    } else if (ty > -kTiltRearm && ty < kTiltRearm) {
        tilt_rot_armed_ = true;
    }

    if (tilt_cd_ > 0) {
        tilt_cd_ -= kTiltTickMs;
        return;
    }
    const float tx = motion.tilt_x();
    int32_t dx = 0;
    if (tx <= -kTiltThreshold) {
        dx = -1;
    } else if (tx >= kTiltThreshold) {
        dx = 1;
    }
    if (dx == 0) {
        return;
    }
    if (CanPlace(px_ + dx, py_, rot_)) {
        px_ += dx;
        Sync();
    }
    tilt_cd_ = kTiltStepMs;
}

void TetrisGame::Tick() {
    if (dead_) {
        return;
    }
    if (CanPlace(px_, py_ + 1, rot_)) {
        py_++;
        Sync();
    } else {
        LockPiece();
    }
}

void TetrisGame::Move(int32_t dx) {
    if (dead_ || !CanPlace(px_ + dx, py_, rot_)) {
        return;
    }
    px_ += dx;
    Sync();
}

void TetrisGame::Rotate() {
    if (dead_) {
        return;
    }
    const int32_t next_rot = (rot_ + 1) % 4;
    // Wall-kick mínimo: si choca (pared, suelo o pila) empuja la pieza
    // lejos del obstáculo en vez de negar el giro.
    static constexpr int32_t kKickX[5] = {0, -1, 1, -2, 2};
    static constexpr int32_t kKickY[3] = {0, -1, -2};
    for (int32_t ky : kKickY) {
        if (py_ + ky < 0) {
            continue;  // no empujar la pieza por encima del tablero
        }
        for (int32_t kx : kKickX) {
            if (!CanPlace(px_ + kx, py_ + ky, next_rot)) {
                continue;
            }
            px_ += kx;
            py_ += ky;
            rot_ = next_rot;
            Sync();
            return;
        }
    }
}

void TetrisGame::SoftDrop() {
    if (dead_) {
        return;
    }
    if (CanPlace(px_, py_ + 1, rot_)) {
        py_++;
        score_++;
        UpdateHud();
        Sync();
    }
}

void TetrisGame::LockPiece() {
    for (int32_t i = 0; i < 4; i++) {
        const int32_t bx = px_ + kShapeX[cur_][rot_][i];
        const int32_t by = py_ + kShapeY[cur_][rot_][i];
        if (by >= 0 && by < kRows && bx >= 0 && bx < kCols) {
            grid_[by][bx] = static_cast<uint8_t>(cur_ + 1);
        }
    }

    // Limpiar líneas completas (desplazando las filas superiores).
    int32_t cleared = 0;
    for (int32_t y = kRows - 1; y >= 0; y--) {
        bool full = true;
        for (int32_t x = 0; x < kCols; x++) {
            if (grid_[y][x] == 0) {
                full = false;
                break;
            }
        }
        if (!full) {
            continue;
        }
        for (int32_t yy = y; yy > 0; yy--) {
            memcpy(grid_[yy], grid_[yy - 1], kCols);
        }
        memset(grid_[0], 0, kCols);
        cleared++;
        y++;  // reexaminar la misma fila (ahora con contenido nuevo)
    }

    if (cleared > 0) {
        score_ += kLineScore[cleared] * (level_ + 1);
        lines_ += cleared;
        const int32_t new_level = lines_ / 10;
        if (new_level != level_) {
            level_ = new_level;
            if (timer_ != nullptr) {
                lv_timer_set_period(timer_, PeriodForLevel(level_));
            }
        }
        UpdateHud();
    }

    Spawn();
}

void TetrisGame::Spawn() {
    cur_ = next_;
    next_ = static_cast<int32_t>(esp_random() % 7);
    rot_ = 0;
    px_ = 3;
    py_ = 0;
    UpdatePreview();
    if (!CanPlace(px_, py_, rot_)) {
        Die();
        return;
    }
    Sync();
}

void TetrisGame::Sync() {
    // Celdas fijadas desde grid_ (pool creciente, creado una sola vez).
    size_t used = 0;
    for (int32_t y = 0; y < kRows; y++) {
        for (int32_t x = 0; x < kCols; x++) {
            if (grid_[y][x] == 0) {
                continue;
            }
            if (used >= pool_.size()) {
                pool_.push_back(
                    GameCreateRect(board_, 0xFFFFFF, kCell - 2, kCell - 2, 4));
            }
            lv_obj_t* cell = pool_[used++];
            lv_obj_set_pos(cell, x * kCell + 1, y * kCell + 1);
            lv_obj_set_style_bg_color(
                cell, lv_color_hex(kColorCell[grid_[y][x] - 1]), 0);
            lv_obj_clear_flag(cell, LV_OBJ_FLAG_HIDDEN);
        }
    }
    for (size_t i = used; i < pool_.size(); i++) {
        lv_obj_add_flag(pool_[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Pieza activa.
    if (dead_) {
        for (int32_t i = 0; i < 4; i++) {
            lv_obj_add_flag(piece_[i], LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    for (int32_t i = 0; i < 4; i++) {
        const int32_t bx = px_ + kShapeX[cur_][rot_][i];
        const int32_t by = py_ + kShapeY[cur_][rot_][i];
        lv_obj_set_pos(piece_[i], bx * kCell + 1, by * kCell + 1);
        lv_obj_set_style_bg_color(piece_[i], lv_color_hex(kColorCell[cur_]), 0);
        lv_obj_clear_flag(piece_[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void TetrisGame::UpdateHud() {
    if (score_label_ != nullptr) {
        lv_label_set_text_fmt(score_label_, "Puntos: %d", score_);
    }
    if (level_label_ != nullptr) {
        lv_label_set_text_fmt(level_label_, "Nivel: %d", level_);
    }
    if (lines_label_ != nullptr) {
        lv_label_set_text_fmt(lines_label_, "Líneas: %d", lines_);
    }
}

void TetrisGame::UpdatePreview() {
    for (int32_t i = 0; i < 4; i++) {
        const int32_t ox = kShapeX[next_][0][i];
        const int32_t oy = kShapeY[next_][0][i];
        lv_obj_set_pos(next_cells_[i], kNextX + ox * kNextCell,
                       kNextY + oy * kNextCell);
        lv_obj_set_style_bg_color(next_cells_[i], lv_color_hex(kColorCell[next_]), 0);
        lv_obj_clear_flag(next_cells_[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void TetrisGame::Die() {
    if (dead_) {
        return;
    }
    dead_ = true;
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
    Sync();  // oculta la pieza activa
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "Puntos: %d - Toca para reiniciar",
             score_);
    overlay_ = GameCreateOverlay(content_, "Fin del juego", subtitle);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}
