// Pong (V2.2, FASE0 §9): implementación LVGL 9 propia y mínima (decisión
// documentada: FASE0 no trae un repo concreto para los 4 juegos arcade).
//
// - Campo 480x372 (bajo el marcador): paleta del jugador a la izquierda,
//   paleta IA a la derecha, pelota 12x12. Primeros a 7 puntos.
// - Entrada: GameInput (táctil, siguiendo el dedo) + QMI8658 tilt_y si el
//   IMU está disponible (control por inclinación además del táctil; si el
//   IMU no responde, el control táctil sigue funcionando).
// - Ciclo de vida vía GameManager (Exit detiene timer/desconecta callbacks).
#ifndef HORNERITO_GAMES_PONG_H_
#define HORNERITO_GAMES_PONG_H_

#include <lvgl.h>

#include <cstdint>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"

class PongGame : public HorneritoGame {
public:
    static PongGame& GetInstance();

    const char* Name() const override { return "Pong"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    PongGame() = default;

    void Reset();
    void Serve();
    void Tick();
    void AfterPoint(bool player_scored);
    void End(bool player_won);
    static void TimerThunk(lv_timer_t* timer);
    static void OverlayThunk(lv_event_t* event);

    lv_obj_t* content_ = nullptr;
    lv_obj_t* field_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* player_ = nullptr;
    lv_obj_t* ai_ = nullptr;
    lv_obj_t* ball_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    GameInput input_;
    lv_timer_t* timer_ = nullptr;

    int32_t player_y_ = 146;
    int32_t ai_y_ = 146;
    int32_t bx_ = 0;
    int32_t by_ = 0;
    int32_t vx_ = 0;
    int32_t vy_ = 0;
    int player_pts_ = 0;
    int ai_pts_ = 0;
    bool pressing_ = false;
    bool game_over_ = false;
};

#endif  // HORNERITO_GAMES_PONG_H_
