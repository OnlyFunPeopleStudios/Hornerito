// Catch the Egg (V2.3.0 §13): caen huevos desde arriba y hay que
// atraparlos con la cesta que controla Hornerito. LVGL 9 puro.
//
// - Huevos (puntuan) y bombas (restan vida) caen desde arriba; cesta con
//   Hornerito detrás (emoji existente). Vidas, dificultad progresiva
//   (velocidad y cadencia crecen con los puntos) y game over.
// - Entrada: drag horizontal = mover la cesta (touch siempre funciona);
//   tilt_x con Motion.Enable() en Enter como complemento (Disable() al
//   salir está central en GameManager::Exit).
// - Pools creados en Enter: sin asignaciones en el game loop.
#ifndef HORNERITO_GAMES_CATCH_THE_EGG_H_
#define HORNERITO_GAMES_CATCH_THE_EGG_H_

#include <lvgl.h>

#include <cstdint>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"

class CatchTheEggGame : public HorneritoGame {
public:
    static CatchTheEggGame& GetInstance();

    const char* Name() const override { return "Catch the Egg"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    CatchTheEggGame() = default;

    struct Fall {
        lv_obj_t* obj = nullptr;
        bool on = false;
        bool bomb = false;
        int x = 0;
        int y = 0;
        int speed = 0;
    };

    static void TimerThunk(lv_timer_t* timer);
    static void OverlayThunk(lv_event_t* event);

    void Reset();
    void Tick();
    void Spawn();
    int FallSpeed() const;
    int SpawnInterval() const;
    void MoveBasket(int x);
    void UpdateHud();
    void Die();

    lv_obj_t* content_ = nullptr;
    lv_obj_t* field_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* lives_label_ = nullptr;
    lv_obj_t* hornerito_ = nullptr;
    lv_obj_t* basket_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    GameInput input_;

    Fall falls_[4];
    int basket_x_ = 0;
    int score_ = 0;
    int lives_ = 3;
    int spawn_cd_ = 0;
    bool dead_ = false;
    bool pressing_ = false;
};

#endif  // HORNERITO_GAMES_CATCH_THE_EGG_H_
