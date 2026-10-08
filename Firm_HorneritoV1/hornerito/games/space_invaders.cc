#include "hornerito/games/space_invaders.h"

#include <esp_random.h>

#include <algorithm>
#include <cstdio>

#include "hornerito/motion/hornerito_motion.h"

namespace {

constexpr int kFieldX = 8;
constexpr int kFieldY = 40;
constexpr int kFieldW = 464;
constexpr int kFieldH = 368;

constexpr int kPlayerW = 36;
constexpr int kPlayerH = 16;
constexpr int kPlayerY = kFieldH - 24;

constexpr int kAlienW = 22;
constexpr int kAlienH = 16;
constexpr int kGapX = 8;
constexpr int kGapY = 10;
constexpr int kAlienCols = 8;
constexpr int kAlienRows = 3;
constexpr int kFormW = kAlienCols * kAlienW + (kAlienCols - 1) * kGapX;
constexpr int kStepX = 6;   // px por paso de la formación
constexpr int kDropY = 14;  // px al tocar el borde y cambiar de dirección

constexpr int kTickMs = 50;

constexpr uint32_t kColorPlayer = 0x8CE06A;
constexpr uint32_t kColorShot = 0xF5F5F5;
constexpr uint32_t kColorBomb = 0xF2994A;
constexpr uint32_t kColorRow[3] = {0xEB5757, 0xF2C94C, 0x6FCF97};
constexpr int kRowScore[3] = {30, 20, 10};

bool Overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

}  // namespace

SpaceInvadersGame& SpaceInvadersGame::GetInstance() {
    static SpaceInvadersGame instance;
    return instance;
}

void SpaceInvadersGame::TimerThunk(lv_timer_t* timer) {
    static_cast<SpaceInvadersGame*>(lv_timer_get_user_data(timer))->Tick();
}

void SpaceInvadersGame::OverlayThunk(lv_event_t* event) {
    static_cast<SpaceInvadersGame*>(lv_event_get_user_data(event))->Reset();
}

void SpaceInvadersGame::Enter(lv_obj_t* content) {
    content_ = content;

    score_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(score_label_);
    lv_obj_align(score_label_, LV_ALIGN_TOP_LEFT, 16, 8);
    wave_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(wave_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(wave_label_);
    lv_obj_align(wave_label_, LV_ALIGN_TOP_MID, 0, 8);
    lives_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(lives_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(lives_label_);
    lv_obj_align(lives_label_, LV_ALIGN_TOP_RIGHT, -16, 8);

    // Campo de juego (área de entrada táctil).
    field_ = lv_obj_create(content_);
    lv_obj_set_size(field_, kFieldW, kFieldH);
    lv_obj_set_pos(field_, kFieldX, kFieldY);
    lv_obj_set_style_bg_color(field_, lv_color_hex(0x0D141C), 0);
    lv_obj_set_style_bg_opa(field_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(field_, lv_color_hex(0x2E4058), 0);
    lv_obj_set_style_radius(field_, 0, 0);
    lv_obj_set_style_pad_all(field_, 0, 0);
    lv_obj_set_scrollbar_mode(field_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(field_, LV_OBJ_FLAG_SCROLLABLE);

    player_ = GameCreateRect(field_, kColorPlayer, kPlayerW, kPlayerH, 4);

    // Formación de invasores (3 filas x 8 columnas), creada una sola vez.
    for (int r = 0; r < kAlienRows; r++) {
        for (int c = 0; c < kAlienCols; c++) {
            aliens_[r][c] =
                GameCreateRect(field_, kColorRow[r], kAlienW, kAlienH, 4);
        }
    }

    // Pools de disparos y bombas.
    for (auto& s : shots_) {
        s.obj = GameCreateRect(field_, kColorShot, 4, 12, 2);
        lv_obj_add_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto& b : bombs_) {
        b.obj = GameCreateRect(field_, kColorBomb, 5, 12, 3);
        lv_obj_add_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
    }

    // Entrada: arrastre = mover la nave; tap sin arrastre = disparar.
    input_.Attach(field_);
    input_.on_down = [this](int32_t x, int32_t y) {
        pressing_ = true;
        dragged_ = false;
        down_x_ = x;
        (void)y;
    };
    input_.on_move = [this](int32_t x, int32_t y) {
        if (!pressing_) {
            return;
        }
        (void)y;
        if (x - down_x_ > 8 || down_x_ - x > 8) {
            dragged_ = true;
        }
        player_x_ = std::clamp<int>(x - kPlayerW / 2, 0, kFieldW - kPlayerW);
        lv_obj_set_pos(player_, player_x_, kPlayerY);  // sigue el dedo
    };
    input_.on_up = [this](int32_t x, int32_t y) {
        if (!pressing_) {
            return;
        }
        pressing_ = false;
        if (!dragged_ && !dead_) {
            Fire();
        }
        (void)x;
        (void)y;
    };

    // Movimiento con el sensor: tilt_x (izq −/der +) mueve la nave;
    // Disable() lo hace GameManager::Exit (central).
    HorneritoMotion::GetInstance().Enable();

    timer_ = lv_timer_create(TimerThunk, kTickMs, this);
    Reset();
}

void SpaceInvadersGame::Exit() {
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
    content_ = nullptr;
    field_ = nullptr;
    score_label_ = nullptr;
    wave_label_ = nullptr;
    lives_label_ = nullptr;
    player_ = nullptr;
    for (auto& row : aliens_) {
        for (auto& a : row) {
            a = nullptr;
        }
    }
    for (auto& s : shots_) {
        s.obj = nullptr;
        s.on = false;
    }
    for (auto& b : bombs_) {
        b.obj = nullptr;
        b.on = false;
    }
    overlay_ = nullptr;
    dead_ = false;
    pressing_ = false;
    dragged_ = false;
}

void SpaceInvadersGame::Reset() {
    if (overlay_ != nullptr) {
        // Borrado diferido: evita eliminar el objeto dentro de su evento.
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }

    score_ = 0;
    lives_ = 3;
    wave_ = 1;
    dir_ = 1;
    inv_x_ = (kFieldW - kFormW) / 2;
    inv_y_ = 8;
    player_x_ = (kFieldW - kPlayerW) / 2;
    alive_count_ = kAlienRows * kAlienCols;
    move_cd_ = StepInterval();
    bomb_cd_ = BombInterval();
    fire_cd_ = 0;
    invuln_ = 0;
    dead_ = false;
    pressing_ = false;
    dragged_ = false;

    for (int r = 0; r < kAlienRows; r++) {
        for (int c = 0; c < kAlienCols; c++) {
            alive_[r][c] = true;
            lv_obj_clear_flag(aliens_[r][c], LV_OBJ_FLAG_HIDDEN);
        }
    }
    for (auto& s : shots_) {
        s.on = false;
        lv_obj_add_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
    }
    for (auto& b : bombs_) {
        b.on = false;
        lv_obj_add_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_pos(player_, player_x_, kPlayerY);
    RepositionAliens();
    UpdateHud();
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

int SpaceInvadersGame::StepInterval() const {
    // Más rápido con menos invasores y con oleadas superiores (50 ms/tick).
    const int killed = (kAlienRows * kAlienCols) - alive_count_;
    int ticks = 10 - killed / 5 - (wave_ - 1);
    if (ticks < 3) {
        ticks = 3;
    }
    return ticks;
}

int SpaceInvadersGame::BombInterval() const {
    int ticks = 60 - wave_ * 8;
    if (ticks < 15) {
        ticks = 15;
    }
    // +-25% de aleatoriedad.
    ticks = ticks * (75 + static_cast<int>(esp_random() % 51)) / 100;
    return ticks < 5 ? 5 : ticks;
}

void SpaceInvadersGame::RepositionAliens() {
    for (int r = 0; r < kAlienRows; r++) {
        for (int c = 0; c < kAlienCols; c++) {
            if (alive_[r][c]) {
                lv_obj_set_pos(aliens_[r][c],
                               inv_x_ + c * (kAlienW + kGapX),
                               inv_y_ + r * (kAlienH + kGapY));
            }
        }
    }
}

void SpaceInvadersGame::MoveFormation() {
    const int nx = inv_x_ + dir_ * kStepX;
    if (nx <= 4 || nx + kFormW >= kFieldW - 4) {
        dir_ = -dir_;  // tocar el borde: bajan y cambian de dirección
        inv_y_ += kDropY;
    } else {
        inv_x_ = nx;
    }
    RepositionAliens();

    // ¿Llegaron a la fila del jugador?
    if (inv_y_ + kAlienRows * (kAlienH + kGapY) - kGapY >= kPlayerY) {
        Die();
    }
}

void SpaceInvadersGame::Fire() {
    if (fire_cd_ > 0) {
        return;
    }
    for (auto& s : shots_) {
        if (s.on) {
            continue;
        }
        s.on = true;
        s.x = player_x_ + kPlayerW / 2 - 2;
        s.y = kPlayerY - 14;
        lv_obj_set_pos(s.obj, s.x, s.y);
        lv_obj_clear_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
        fire_cd_ = 5;  // 250 ms entre disparos
        return;
    }
}

void SpaceInvadersGame::SpawnBomb() {
    if (alive_count_ <= 0) {
        return;
    }
    for (auto& b : bombs_) {
        if (b.on) {
            continue;
        }
        // Columna con un invasor vivo; se usa el más bajo de esa columna.
        const int col = static_cast<int>(esp_random() % kAlienCols);
        int row = -1;
        for (int r = kAlienRows - 1; r >= 0; r--) {
            if (alive_[r][col]) {
                row = r;
                break;
            }
        }
        if (row < 0) {
            return;
        }
        b.on = true;
        b.x = inv_x_ + col * (kAlienW + kGapX) + kAlienW / 2 - 2;
        b.y = inv_y_ + row * (kAlienH + kGapY) + kAlienH;
        lv_obj_set_pos(b.obj, b.x, b.y);
        lv_obj_clear_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
}

void SpaceInvadersGame::WaveClear() {
    wave_++;
    inv_x_ = (kFieldW - kFormW) / 2;
    inv_y_ = 8;
    dir_ = 1;
    alive_count_ = kAlienRows * kAlienCols;
    for (int r = 0; r < kAlienRows; r++) {
        for (int c = 0; c < kAlienCols; c++) {
            alive_[r][c] = true;
            lv_obj_clear_flag(aliens_[r][c], LV_OBJ_FLAG_HIDDEN);
        }
    }
    move_cd_ = StepInterval();
    UpdateHud();
}

void SpaceInvadersGame::Tick() {
    if (dead_) {
        return;
    }

    // Nave: tilt_x (izq −/der +) mientras el dedo no toque la pantalla
    // (mismo patrón que Breakout: dedo manda, tilt gobierna al soltar).
    auto& motion = HorneritoMotion::GetInstance();
    if (!pressing_ && motion.available()) {
        const int32_t range = (kFieldW - kPlayerW) / 2;
        const int32_t x =
            range + static_cast<int32_t>(motion.tilt_x() * range);
        const int32_t clamped = std::clamp<int32_t>(x, 0, kFieldW - kPlayerW);
        if (clamped != player_x_) {
            player_x_ = clamped;
            lv_obj_set_pos(player_, player_x_, kPlayerY);
        }
    }

    if (fire_cd_ > 0) {
        fire_cd_--;
    }
    if (invuln_ > 0) {
        invuln_--;
    }

    // Disparos del jugador: suben y chocan con invasores.
    for (auto& s : shots_) {
        if (!s.on) {
            continue;
        }
        s.y -= 10;
        if (s.y + 12 < 0) {
            s.on = false;
            lv_obj_add_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_set_pos(s.obj, s.x, s.y);
        bool hit = false;
        for (int r = 0; r < kAlienRows && !hit; r++) {
            for (int c = 0; c < kAlienCols && !hit; c++) {
                if (!alive_[r][c]) {
                    continue;
                }
                const int ax = inv_x_ + c * (kAlienW + kGapX);
                const int ay = inv_y_ + r * (kAlienH + kGapY);
                if (Overlap(s.x, s.y, 4, 12, ax, ay, kAlienW, kAlienH)) {
                    alive_[r][c] = false;
                    alive_count_--;
                    score_ += kRowScore[r];
                    lv_obj_add_flag(aliens_[r][c], LV_OBJ_FLAG_HIDDEN);
                    hit = true;
                }
            }
        }
        if (hit) {
            s.on = false;
            lv_obj_add_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
            UpdateHud();
            if (alive_count_ <= 0) {
                WaveClear();
            }
        }
    }

    // Bombas de los invasores: bajan y amenazan al jugador.
    for (auto& b : bombs_) {
        if (!b.on) {
            continue;
        }
        b.y += 5;
        if (b.y > kFieldH) {
            b.on = false;
            lv_obj_add_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_set_pos(b.obj, b.x, b.y);
        if (Overlap(b.x, b.y, 5, 12, player_x_, kPlayerY, kPlayerW,
                    kPlayerH)) {
            b.on = false;
            lv_obj_add_flag(b.obj, LV_OBJ_FLAG_HIDDEN);
            if (invuln_ <= 0) {
                lives_--;
                invuln_ = 30;  // 1,5 s de invulnerabilidad
                UpdateHud();
                if (lives_ <= 0) {
                    Die();
                    return;
                }
            }
        }
    }

    // Movimiento de la formación y nuevas bombas.
    if (--move_cd_ <= 0) {
        MoveFormation();
        if (dead_) {
            return;
        }
        move_cd_ = StepInterval();
    }
    if (--bomb_cd_ <= 0) {
        SpawnBomb();
        bomb_cd_ = BombInterval();
    }
}

void SpaceInvadersGame::UpdateHud() {
    if (score_label_ != nullptr) {
        lv_label_set_text_fmt(score_label_, "Puntos: %d", score_);
    }
    if (wave_label_ != nullptr) {
        lv_label_set_text_fmt(wave_label_, "Oleada %d", wave_);
    }
    if (lives_label_ != nullptr) {
        lv_label_set_text_fmt(lives_label_, "Vidas: %d", lives_);
    }
}

void SpaceInvadersGame::Die() {
    if (dead_) {
        return;
    }
    dead_ = true;
    if (timer_ != nullptr) {
        lv_timer_pause(timer_);
    }
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "Puntos: %d - Toca para reiniciar",
             score_);
    overlay_ = GameCreateOverlay(content_, "Fin del juego", subtitle);
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}
