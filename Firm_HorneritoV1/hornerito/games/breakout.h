// Breakout (V2.2, FASE0 §9): implementación LVGL 9 propia y mínima (decisión
// documentada: FASE0 no trae un repo concreto para los 4 juegos arcade).
//
// - Campo 480x372 bajo el marcador: 40 ladrillos (8x5, colores por fila),
//   pala 80x14 abajo, pelota 12x12, 3 vidas.
// - Entrada: GameInput (táctil, siguiendo el dedo) + QMI8658 tilt_x si el
//   IMU está disponible (si no responde, el táctil sigue funcionando).
// - Ciclo de vida vía GameManager (Exit detiene timer/desconecta callbacks).
#ifndef HORNERITO_GAMES_BREAKOUT_H_
#define HORNERITO_GAMES_BREAKOUT_H_

#include <lvgl.h>

#include <cstdint>
#include <vector>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"

class BreakoutGame : public HorneritoGame {
public:
    static BreakoutGame& GetInstance();

    const char* Name() const override { return "Breakout"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    BreakoutGame() = default;

    void Reset();
    void Serve();
    void Tick();
    void UpdateHud();
    void End(bool won);
    lv_obj_t* EnsureBrick(size_t index);
    static void TimerThunk(lv_timer_t* timer);
    static void OverlayThunk(lv_event_t* event);

    lv_obj_t* content_ = nullptr;
    lv_obj_t* field_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* paddle_ = nullptr;
    lv_obj_t* ball_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    std::vector<lv_obj_t*> bricks_;   // nullptr = ladrillo destruido
    GameInput input_;
    lv_timer_t* timer_ = nullptr;

    int32_t paddle_x_ = 200;
    int32_t bx_ = 0;
    int32_t by_ = 0;
    int32_t vx_ = 0;
    int32_t vy_ = 0;
    int points_ = 0;
    int lives_ = 3;
    int remaining_ = 40;
    bool pressing_ = false;
    bool game_over_ = false;
};

#endif  // HORNERITO_GAMES_BREAKOUT_H_
