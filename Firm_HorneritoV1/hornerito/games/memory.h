// Memory / Simon Says (V2.3.0 §12): juego de memoria visual con 4 pads.
//
// - Estados: SHOW_SEQUENCE (reproduce) -> WAIT_INPUT (repite el jugador)
//   -> SUCCESS (pausa corta y siguiente ronda) / FAIL (game over).
// - Cada ronda añade un paso y acelera la reproducción (periodo mínimo
//   de 90 ms por paso a partir de ~20 rondas).
// - Solo touch (§12: obligatorio). Pads creados una vez en Enter.
// - Ciclo de vida vía GameManager (Exit() borra ambos timers).
#ifndef HORNERITO_GAMES_MEMORY_H_
#define HORNERITO_GAMES_MEMORY_H_

#include <lvgl.h>

#include <cstdint>

#include "hornerito/games/game_common.h"

class MemoryGame : public HorneritoGame {
public:
    static MemoryGame& GetInstance();

    const char* Name() const override { return "Memory"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    MemoryGame() = default;

    enum class State { kShow, kWait, kRest, kOver };

    static void ShowThunk(lv_timer_t* timer);
    static void OffThunk(lv_timer_t* timer);
    static void PadThunk(lv_event_t* event);
    static void OverlayThunk(lv_event_t* event);

    void Reset();
    void StartShow();
    void Light(int pad);
    void Unlight(int pad);
    void UpdateHud();
    void Die(const char* title);

    lv_obj_t* content_ = nullptr;
    lv_obj_t* hud_ = nullptr;
    lv_obj_t* pads_[4] = {};
    lv_obj_t* status_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    lv_timer_t* show_timer_ = nullptr;  // reproduce secuencia / pausa
    lv_timer_t* off_timer_ = nullptr;   // apaga el pad tras un toque

    State state_ = State::kShow;
    uint8_t seq_[48] = {};
    uint8_t len_ = 0;      // longitud actual (= ronda)
    uint8_t show_i_ = 0;   // paso que se está reproduciendo
    uint8_t idx_ = 0;      // paso que espera el jugador
    int8_t lit_pad_ = -1;  // pad encendido actualmente (-1 = ninguno)
};

#endif  // HORNERITO_GAMES_MEMORY_H_
