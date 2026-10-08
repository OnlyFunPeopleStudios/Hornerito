#include "hornerito/games/maze_runner.h"

#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "hornerito/hornerito_emojis.h"
#include "hornerito/motion/hornerito_motion.h"

namespace {

constexpr int kN = 15;          // laberinto 15x15 (coordenadas impares)
constexpr int kCell = 25;       // pantalla completa: 375x375
constexpr int kBoardX = 52;     // (480 - 375) / 2
constexpr int kBoardY = 40;     // HUD encima (0..38), laberinto 40..415
constexpr int kGoal = 13;       // meta en la celda (13, 13)
constexpr int kSwipeMin = 14;   // px por gesto para dar un paso

constexpr int kTickMs = 100;    // tick maestro (tilt + cuenta atrás)
constexpr int kSecondMs = 1000;
constexpr int kTiltStepMs = 220;    // ms entre pasos al inclinar
constexpr float kTiltThreshold = 0.45f;  // |tilt| mínimo para dar un paso

constexpr uint32_t kColorFloor = 0x16202C;
constexpr uint32_t kColorWall = 0x32465E;
constexpr uint32_t kColorGoal = 0xF2C94C;

constexpr uint32_t kPlayerSize = 21;        // px sobre celda de 25
constexpr uint32_t kPlayerScale = 256 * kPlayerSize / 192;  // emoji 192 px

int TimeForLevel(int level) {
    const int t = 60 - (level - 1) * 5;
    return t < 30 ? 30 : t;
}

}  // namespace

MazeRunnerGame& MazeRunnerGame::GetInstance() {
    static MazeRunnerGame instance;
    return instance;
}

void MazeRunnerGame::TimerThunk(lv_timer_t* timer) {
    static_cast<MazeRunnerGame*>(lv_timer_get_user_data(timer))->Tick();
}

void MazeRunnerGame::OverlayThunk(lv_event_t* event) {
    static_cast<MazeRunnerGame*>(lv_event_get_user_data(event))->Reset();
}

void MazeRunnerGame::Enter(lv_obj_t* content) {
    content_ = content;

    hud_ = lv_label_create(content_);
    lv_obj_set_style_text_color(hud_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(hud_);
    lv_obj_align(hud_, LV_ALIGN_TOP_MID, 0, 2);

    // Tablero (también es el área de gestos y el "suelo" del laberinto).
    board_ = lv_obj_create(content_);
    lv_obj_set_size(board_, kN * kCell, kN * kCell);
    lv_obj_set_pos(board_, kBoardX, kBoardY);
    lv_obj_set_style_bg_color(board_, lv_color_hex(kColorFloor), 0);
    lv_obj_set_style_bg_opa(board_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(board_, lv_color_hex(0x2E4058), 0);
    lv_obj_set_style_radius(board_, 0, 0);
    lv_obj_set_style_pad_all(board_, 0, 0);
    lv_obj_set_scrollbar_mode(board_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(board_, LV_OBJ_FLAG_SCROLLABLE);

    goal_ = GameCreateRect(board_, kColorGoal, kCell, kCell, 4);

    // Personaje: el propio Hornerito (asset existente, sin assets nuevos).
    player_ = lv_image_create(board_);
    lv_image_set_src(player_, HorneritoEmojiImage(HorneritoEmoji::kIdle));
    lv_image_set_scale(player_, kPlayerScale);
    lv_obj_set_size(player_, kPlayerSize, kPlayerSize);

    // Control principal: tilt (izq −/der +, adel −/atrás +). El swipe de
    // debajo sigue funcionando como fallback con touch.
    HorneritoMotion::GetInstance().Enable();

    // Swipe sobre el laberinto: un paso por gesto en el eje dominante.
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
        if (ax < kSwipeMin && ay < kSwipeMin) {
            return;
        }
        if (ax > ay) {
            TryMove(dx > 0 ? 1 : -1, 0);
        } else {
            TryMove(0, dy > 0 ? 1 : -1);
        }
    };

    lv_obj_set_pos(goal_, kGoal * kCell, kGoal * kCell);
    timer_ = lv_timer_create(TimerThunk, kTickMs, this);
    Reset();
}

void MazeRunnerGame::Exit() {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    input_.on_down = nullptr;
    input_.on_move = nullptr;
    input_.on_up = nullptr;
    input_.Detach();

    // Los widgets mueren con la pantalla de sesión: solo se sueltan
    // referencias y estado para que re-entrar arranque limpio.
    walls_.clear();
    content_ = nullptr;
    hud_ = nullptr;
    board_ = nullptr;
    goal_ = nullptr;
    player_ = nullptr;
    overlay_ = nullptr;
    dead_ = false;
    pressing_ = false;
    tilt_cd_ = 0;
    second_acc_ = 0;
}

void MazeRunnerGame::Reset() {
    if (overlay_ != nullptr) {
        // Borrado diferido: evita eliminar el objeto dentro de su evento.
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    level_ = 1;
    score_ = 0;
    px_ = 1;
    py_ = 1;
    time_left_ = TimeForLevel(level_);
    dead_ = false;
    pressing_ = false;
    tilt_cd_ = 0;
    second_acc_ = 0;
    BuildMaze();
    lv_obj_set_pos(player_, px_ * kCell, py_ * kCell);
    lv_obj_set_pos(goal_, kGoal * kCell, kGoal * kCell);
    UpdateHud();
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void MazeRunnerGame::BuildMaze() {
    // Regenerar paredes (solo al construir/nivel: nunca en el game loop).
    for (lv_obj_t* wall : walls_) {
        lv_obj_delete(wall);
    }
    walls_.clear();

    memset(m_, 1, sizeof(m_));  // todo es pared al empezar

    // Backtracker iterativo desde (1,1) sobre las celdas impares:
    // cava pasillos siempre en celdas impares dentro de [1, 13], así que
    // el borde queda como muro y el laberinto es perfecto (todo conectado).
    static const int8_t dx[4] = {2, -2, 0, 0};
    static const int8_t dy[4] = {0, 0, 2, -2};
    uint8_t stack[64];
    int sp = 0;
    m_[1][1] = 0;
    stack[sp++] = static_cast<uint8_t>(1 * kN + 1);
    while (sp > 0) {
        const uint8_t cur = stack[sp - 1];
        const int cx = cur % kN;
        const int cy = cur / kN;
        int dirs[4];
        int nd = 0;
        for (int i = 0; i < 4; i++) {
            const int nx = cx + dx[i];
            const int ny = cy + dy[i];
            if (nx >= 1 && nx <= kGoal && ny >= 1 && ny <= kGoal &&
                m_[ny][nx] == 1) {
                dirs[nd++] = i;
            }
        }
        if (nd == 0) {
            sp--;
            continue;
        }
        const int d = dirs[esp_random() % nd];
        const int nx = cx + dx[d];
        const int ny = cy + dy[d];
        m_[cy + dy[d] / 2][cx + dx[d] / 2] = 0;  // abre la pared intermedia
        m_[ny][nx] = 0;
        stack[sp++] = static_cast<uint8_t>(ny * kN + nx);
    }

    // Paredes visibles (una sola creación por nivel).
    for (int y = 0; y < kN; y++) {
        for (int x = 0; x < kN; x++) {
            if (m_[y][x] != 0) {
                walls_.push_back(GameCreateRect(board_, kColorWall, kCell,
                                                kCell, 0));
                lv_obj_set_pos(walls_.back(), x * kCell, y * kCell);
            }
        }
    }
}

void MazeRunnerGame::TryMove(int dx, int dy) {
    if (dead_) {
        return;
    }
    const int nx = px_ + dx;
    const int ny = py_ + dy;
    if (nx < 0 || nx >= kN || ny < 0 || ny >= kN || m_[ny][nx] != 0) {
        return;  // choque con pared
    }
    px_ = nx;
    py_ = ny;
    lv_obj_set_pos(player_, px_ * kCell, py_ * kCell);

    if (px_ == kGoal && py_ == kGoal) {
        AdvanceLevel();
    }
}

void MazeRunnerGame::AdvanceLevel() {
    score_ += 100;
    level_++;
    px_ = 1;
    py_ = 1;
    time_left_ = TimeForLevel(level_);
    BuildMaze();
    lv_obj_set_pos(player_, px_ * kCell, py_ * kCell);
    lv_obj_set_pos(goal_, kGoal * kCell, kGoal * kCell);
    UpdateHud();
}

void MazeRunnerGame::TiltStep() {
    auto& motion = HorneritoMotion::GetInstance();
    if (!motion.available() || pressing_) {
        return;  // con el dedo en el laberinto manda el swipe
    }
    const float tx = motion.tilt_x();
    const float ty = motion.tilt_y();
    // Eje dominante con umbral (izq −/der +; adelante − / atrás +).
    if (tx <= -kTiltThreshold) {
        TryMove(-1, 0);
    } else if (tx >= kTiltThreshold) {
        TryMove(1, 0);
    } else if (ty <= -kTiltThreshold) {  // inclinar lejos = subir
        TryMove(0, -1);
    } else if (ty >= kTiltThreshold) {   // inclinar cerca = bajar
        TryMove(0, 1);
    } else {
        return;
    }
    tilt_cd_ = kTiltStepMs;
}

void MazeRunnerGame::Tick() {
    if (dead_) {
        return;
    }

    // Tilt con cooldown (tick de 100 ms).
    if (tilt_cd_ > 0) {
        tilt_cd_ -= kTickMs;
    } else {
        TiltStep();
    }

    // Cuenta atrás de 1 s.
    second_acc_ += kTickMs;
    if (second_acc_ < kSecondMs) {
        return;
    }
    second_acc_ = 0;
    time_left_--;
    if (time_left_ <= 0) {
        Die("¡Se acabó el tiempo!");
        return;
    }
    UpdateHud();
}

void MazeRunnerGame::UpdateHud() {
    if (hud_ != nullptr) {
        lv_label_set_text_fmt(hud_, "Nivel %d  ·  Pts %d  ·  %d s", level_,
                              score_, time_left_);
    }
}

void MazeRunnerGame::Die(const char* title) {
    if (dead_) {
        return;
    }
    dead_ = true;
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
    char subtitle[72];
    snprintf(subtitle, sizeof(subtitle),
             "Puntos: %d - Nivel: %d - Toca para reiniciar", score_, level_);
    overlay_ = GameCreateOverlay(content_, title, subtitle);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}
