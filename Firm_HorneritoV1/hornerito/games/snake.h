// Snake (V2.2, FASE0 §9): implementación LVGL 9 propia y mínima (decisión
// documentada: FASE0 no trae un repo concreto para los 4 juegos arcade).
//
// - Campo 480x416: marcador arriba + rejilla 20x15 de celdas de 24 px.
// - Cuerpo/food como objetos-entidad (set_pos por tick, sin dirty manual).
// - Entrada: GameInput (capa común Touch -> Game Input -> Juego): arrastre
//   o swipe desde el punto inicial, eje dominante con umbral anti-trepidación.
// - Ciclo de vida vía GameManager: Exit() detiene el timer y desconecta
//   los callbacks; los widgets mueren con la pantalla de sesión.
#ifndef HORNERITO_GAMES_SNAKE_H_
#define HORNERITO_GAMES_SNAKE_H_

#include <lvgl.h>

#include <cstdint>
#include <deque>
#include <vector>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"

class SnakeGame : public HorneritoGame {
public:
    static SnakeGame& GetInstance();

    const char* Name() const override { return "Snake"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    SnakeGame() = default;

    struct Cell {
        int8_t x;
        int8_t y;
    };
    enum class Dir { kUp, kDown, kLeft, kRight };

    void Reset();
    void Tick();
    void SteerFrom(int32_t dx, int32_t dy);
    void Die(bool win);
    void PlaceFood();
    void SyncSegments();

    static void TimerThunk(lv_timer_t* timer);
    static void OverlayThunk(lv_event_t* event);

    lv_obj_t* content_ = nullptr;
    lv_obj_t* grid_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* food_ = nullptr;      // objeto LVGL de la comida
    lv_obj_t* overlay_ = nullptr;
    GameInput input_;
    lv_timer_t* timer_ = nullptr;

    std::deque<Cell> body_;          // body_[0] = cabeza
    std::vector<lv_obj_t*> segs_;    // un objeto por segmento
    Dir dir_ = Dir::kRight;
    Dir pending_dir_ = Dir::kRight;
    Cell food_cell_ = {14, 7};       // posición de la comida en la rejilla
    int points_ = 0;
    bool pressing_ = false;
    int32_t down_x_ = 0;
    int32_t down_y_ = 0;
    bool dead_ = false;
};

#endif  // HORNERITO_GAMES_SNAKE_H_
