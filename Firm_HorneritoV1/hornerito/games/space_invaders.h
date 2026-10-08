// Space Invaders (V2.3.0 §9): versión compacta estilo arcade, LVGL 9 puro.
//
// - Campo 464x368: nave del jugador abajo, formación de 8x3 invasores,
//   disparos del jugador y bombas de los invasores (pools creados en
//   Enter, sin asignaciones en el game loop).
// - Entrada: arrastre horizontal o inclinación (tilt_x del QMI8658,
//   Enable() en Enter) = mover la nave; tap (sin arrastre) = disparar.
// - Ciclo de vida vía GameManager: Exit() detiene el timer y desconecta
//   callbacks; los widgets mueren con la pantalla de sesión.
#ifndef HORNERITO_GAMES_SPACE_INVADERS_H_
#define HORNERITO_GAMES_SPACE_INVADERS_H_

#include <lvgl.h>

#include <cstdint>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"

class SpaceInvadersGame : public HorneritoGame {
public:
    static SpaceInvadersGame& GetInstance();

    const char* Name() const override { return "Space Invaders"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    SpaceInvadersGame() = default;

    struct Shot {
        lv_obj_t* obj = nullptr;
        bool on = false;
        int x = 0;
        int y = 0;
    };
    struct Bomb {
        lv_obj_t* obj = nullptr;
        bool on = false;
        int x = 0;
        int y = 0;
    };

    static void TimerThunk(lv_timer_t* timer);
    static void OverlayThunk(lv_event_t* event);

    void Reset();
    void Tick();
    void MoveFormation();
    void RepositionAliens();
    void Fire();
    void SpawnBomb();
    int StepInterval() const;
    int BombInterval() const;
    void WaveClear();
    void UpdateHud();
    void Die();

    lv_obj_t* content_ = nullptr;
    lv_obj_t* field_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* wave_label_ = nullptr;
    lv_obj_t* lives_label_ = nullptr;
    lv_obj_t* player_ = nullptr;
    lv_obj_t* aliens_[3][8] = {};
    lv_obj_t* overlay_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    GameInput input_;

    Shot shots_[2];
    Bomb bombs_[3];
    bool alive_[3][8] = {};

    int inv_x_ = 0;   // esquina izq. de la formación (coords. de campo)
    int inv_y_ = 0;
    int dir_ = 1;     // dirección horizontal actual
    int player_x_ = 0;
    int score_ = 0;
    int lives_ = 3;
    int wave_ = 1;
    int alive_count_ = 0;
    int move_cd_ = 0;
    int bomb_cd_ = 0;
    int fire_cd_ = 0;
    int invuln_ = 0;
    bool dead_ = false;
    bool pressing_ = false;
    bool dragged_ = false;
    int down_x_ = 0;
};

#endif  // HORNERITO_GAMES_SPACE_INVADERS_H_
