#include "hornerito/games/catch_the_egg.h"

#include <esp_random.h>

#include <algorithm>
#include <cstdio>

#include "hornerito/hornerito_emojis.h"
#include "hornerito/motion/hornerito_motion.h"

namespace {

constexpr int kFieldX = 0;
constexpr int kFieldY = 40;
constexpr int kFieldW = 480;
constexpr int kFieldH = 376;

constexpr int kBasketW = 64;
constexpr int kBasketH = 22;
constexpr int kBasketY = kFieldH - 26;
constexpr int kEggW = 20;
constexpr int kEggH = 26;
constexpr int kBombSize = 16;
constexpr int kHorneritoSize = 40;
constexpr int kHorneritoY = kBasketY - 34;
constexpr uint32_t kHorneritoScale = 256 * kHorneritoSize / 192;  // emoji 192

constexpr int kTickMs = 33;

constexpr uint32_t kColorBasket = 0xA1744B;
constexpr uint32_t kColorEgg = 0xF7E7A6;
constexpr uint32_t kColorBomb = 0xE05050;

bool Overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

}  // namespace

CatchTheEggGame& CatchTheEggGame::GetInstance() {
    static CatchTheEggGame instance;
    return instance;
}

void CatchTheEggGame::TimerThunk(lv_timer_t* timer) {
    static_cast<CatchTheEggGame*>(lv_timer_get_user_data(timer))->Tick();
}

void CatchTheEggGame::OverlayThunk(lv_event_t* event) {
    static_cast<CatchTheEggGame*>(lv_event_get_user_data(event))->Reset();
}

void CatchTheEggGame::Enter(lv_obj_t* content) {
    content_ = content;

    score_label_ = lv_label_create(content_);
    lv_obj_set_style_text_color(score_label_, lv_color_hex(0x9AB2C8), 0);
    GameApplyFont(score_label_);
    lv_obj_align(score_label_, LV_ALIGN_TOP_LEFT, 16, 8);
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
    lv_obj_set_style_border_width(field_, 0, 0);
    lv_obj_set_style_radius(field_, 0, 0);
    lv_obj_set_style_pad_all(field_, 0, 0);
    lv_obj_set_scrollbar_mode(field_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(field_, LV_OBJ_FLAG_SCROLLABLE);

    // Hornerito detrás de la cesta (emoji existente, sin assets nuevos).
    hornerito_ = lv_image_create(field_);
    lv_image_set_src(hornerito_, HorneritoEmojiImage(HorneritoEmoji::kIdle));
    lv_image_set_scale(hornerito_, kHorneritoScale);
    lv_obj_set_size(hornerito_, kHorneritoSize, kHorneritoSize);

    basket_ = GameCreateRect(field_, kColorBasket, kBasketW, kBasketH, 8);

    // Pools de huevos/bombas: 4 slots creados una sola vez.
    for (auto& f : falls_) {
        f.obj = GameCreateRect(field_, kColorEgg, kEggW, kEggH, 10);
        lv_obj_add_flag(f.obj, LV_OBJ_FLAG_HIDDEN);
    }

    // Entrada: drag horizontal = mover la cesta.
    input_.Attach(field_);
    input_.on_down = [this](int32_t x, int32_t y) {
        pressing_ = true;
        (void)x;
        (void)y;
    };
    input_.on_move = [this](int32_t x, int32_t y) {
        if (!pressing_) {
            return;
        }
        (void)y;
        MoveBasket(std::clamp<int>(x - kBasketW / 2, 0, kFieldW - kBasketW));
    };
    input_.on_up = [this](int32_t x, int32_t y) {
        pressing_ = false;
        (void)x;
        (void)y;
    };

    // Complemento Motion: tilt_x mueve la cesta cuando no hay dedo en la
    // pantalla (Disable() al salir está central en GameManager::Exit).
    HorneritoMotion::GetInstance().Enable();

    timer_ = lv_timer_create(TimerThunk, kTickMs, this);
    Reset();
}

void CatchTheEggGame::Exit() {
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
    lives_label_ = nullptr;
    hornerito_ = nullptr;
    basket_ = nullptr;
    for (auto& f : falls_) {
        f.obj = nullptr;
        f.on = false;
    }
    overlay_ = nullptr;
    dead_ = false;
    pressing_ = false;
}

void CatchTheEggGame::Reset() {
    if (overlay_ != nullptr) {
        // Borrado diferido: evita eliminar el objeto dentro de su evento.
        lv_obj_delete_async(overlay_);
        overlay_ = nullptr;
    }
    score_ = 0;
    lives_ = 3;
    spawn_cd_ = 50;  // margen inicial antes de la primera caída
    dead_ = false;
    pressing_ = false;
    for (auto& f : falls_) {
        f.on = false;
        lv_obj_add_flag(f.obj, LV_OBJ_FLAG_HIDDEN);
    }
    MoveBasket((kFieldW - kBasketW) / 2);
    UpdateHud();
    if (timer_ != nullptr) {
        lv_timer_resume(timer_);
    }
}

void CatchTheEggGame::MoveBasket(int x) {
    basket_x_ = x;
    lv_obj_set_pos(basket_, basket_x_, kBasketY);
    lv_obj_set_pos(hornerito_, basket_x_ + (kBasketW - kHorneritoSize) / 2,
                   kHorneritoY);
}

int CatchTheEggGame::FallSpeed() const {
    int s = 3 + score_ / 8;  // dificultad progresiva
    if (s > 9) {
        s = 9;
    }
    return s;
}

int CatchTheEggGame::SpawnInterval() const {
    int ticks = 22 - score_ / 6;
    if (ticks < 9) {
        ticks = 9;
    }
    ticks = ticks * (80 + static_cast<int>(esp_random() % 41)) / 100;
    return ticks < 7 ? 7 : ticks;
}

void CatchTheEggGame::Spawn() {
    for (auto& f : falls_) {
        if (f.on) {
            continue;
        }
        f.on = true;
        f.bomb = (esp_random() % 4) == 0;  // 1 de 4 es bomba
        f.speed = FallSpeed();
        if (f.bomb) {
            lv_obj_set_size(f.obj, kBombSize, kBombSize);
            lv_obj_set_style_bg_color(f.obj, lv_color_hex(kColorBomb), 0);
            lv_obj_set_style_radius(f.obj, kBombSize / 2, 0);
            f.x = static_cast<int>(esp_random() % (kFieldW - kBombSize));
            f.y = -kBombSize;
        } else {
            lv_obj_set_size(f.obj, kEggW, kEggH);
            lv_obj_set_style_bg_color(f.obj, lv_color_hex(kColorEgg), 0);
            lv_obj_set_style_radius(f.obj, kEggW / 2, 0);
            f.x = static_cast<int>(esp_random() % (kFieldW - kEggW));
            f.y = -kEggH;
        }
        lv_obj_set_pos(f.obj, f.x, f.y);
        lv_obj_clear_flag(f.obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
}

void CatchTheEggGame::Tick() {
    if (dead_) {
        return;
    }

    // Tilt lateral cuando el dedo no está en la pantalla.
    auto& motion = HorneritoMotion::GetInstance();
    if (!pressing_ && motion.available()) {
        const int center = (kFieldW - kBasketW) / 2;
        const int x = center +
                      static_cast<int>(motion.tilt_x() * static_cast<float>(center));
        MoveBasket(std::clamp(x, 0, kFieldW - kBasketW));
    }

    for (auto& f : falls_) {
        if (!f.on) {
            continue;
        }
        f.y += f.speed;
        const int w = f.bomb ? kBombSize : kEggW;
        const int h = f.bomb ? kBombSize : kEggH;

        const bool caught = Overlap(f.x, f.y, w, h, basket_x_, kBasketY,
                                    kBasketW, kBasketH);
        if (caught) {
            f.on = false;
            lv_obj_add_flag(f.obj, LV_OBJ_FLAG_HIDDEN);
            if (f.bomb) {
                lives_--;
            } else {
                score_++;
            }
            UpdateHud();
            if (lives_ <= 0) {
                Die();
                return;
            }
            continue;
        }

        if (f.y > kFieldH) {
            f.on = false;
            lv_obj_add_flag(f.obj, LV_OBJ_FLAG_HIDDEN);
            if (!f.bomb) {  // un huevo perdido = una vida
                lives_--;
                UpdateHud();
                if (lives_ <= 0) {
                    Die();
                    return;
                }
            }
            continue;
        }

        lv_obj_set_pos(f.obj, f.x, f.y);
    }

    if (--spawn_cd_ <= 0) {
        Spawn();
        spawn_cd_ = SpawnInterval();
    }
}

void CatchTheEggGame::UpdateHud() {
    if (score_label_ != nullptr) {
        lv_label_set_text_fmt(score_label_, "Puntos: %d", score_);
    }
    if (lives_label_ != nullptr) {
        lv_label_set_text_fmt(lives_label_, "Vidas: %d", lives_);
    }
}

void CatchTheEggGame::Die() {
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
