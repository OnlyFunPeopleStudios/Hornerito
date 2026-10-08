// Flappy (V2.2, FASE0 §9): implementación LVGL 9 propia y mínima (decisión
// documentada: FASE0 no trae un repo concreto para los 4 juegos arcade).
//
// - Campo 480x372 bajo el marcador: pájaro (emoji propio de Hornerito
//   escalado 192->40 px, prioridad del arte propio sobre el del juego
//   original), dos tuberías recicladas, sin vidas (un choque = fin).
// - Aleteo: toque (GameInput) o sacudida del QMI8658 (modalidad alterna;
//   si el IMU no responde, el toque sigue funcionando).
// - Ciclo de vida vía GameManager (Exit detiene timer/desconecta callbacks).
#ifndef HORNERITO_GAMES_FLAPPY_H_
#define HORNERITO_GAMES_FLAPPY_H_

#include <lvgl.h>

#include <cstdint>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"
#include "hornerito/hornerito_emojis.h"

class FlappyGame : public HorneritoGame {
public:
    static FlappyGame& GetInstance();

    const char* Name() const override { return "Flappy"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    FlappyGame() = default;

    struct Pipe {
        int32_t x = 0;
        int32_t top_h = 80;
        bool scored = false;
        lv_obj_t* top = nullptr;
        lv_obj_t* bottom = nullptr;
    };

    void Reset();
    void Flap();
    void Tick();
    void UpdateHud();
    void Recycle(Pipe& pipe);
    void End();
    static void TimerThunk(lv_timer_t* timer);
    static void OverlayThunk(lv_event_t* event);

    lv_obj_t* content_ = nullptr;
    lv_obj_t* field_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* bird_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    Pipe pipes_[2];
    GameInput input_;
    lv_timer_t* timer_ = nullptr;

    float bird_y_ = 166.0f;
    float vy_ = 0.0f;
    int points_ = 0;
    bool game_over_ = false;
};

#endif  // HORNERITO_GAMES_FLAPPY_H_
