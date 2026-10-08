// Maze Runner (V2.3.0 §10): laberinto generado por backtracker con
// Hornerito como personaje. LVGL 9 puro.
//
// - Laberinto 15x15 a pantalla completa (celdas de 25 px, 375x375 sobre
//   el contenido 480x416), generado al azar y siempre resoluble:
//   de START (1,1) a la META (13,13).
// - Entrada principal: tilt del QMI8658 (Enable() en Enter, Disable()
//   central en GameManager::Exit) con cooldown por paso. Fallback touch:
//   swipe = un paso por gesto, funciona aunque Motion esté desactivado.
// - Temporizador por nivel (tick de 100 ms): victoria al llegar a la
//   META (nivel siguiente); cuenta atrás de 1 s, timeout = fin del juego.
#ifndef HORNERITO_GAMES_MAZE_RUNNER_H_
#define HORNERITO_GAMES_MAZE_RUNNER_H_

#include <lvgl.h>

#include <cstdint>
#include <vector>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"

class MazeRunnerGame : public HorneritoGame {
public:
    static MazeRunnerGame& GetInstance();

    const char* Name() const override { return "Maze Runner"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    MazeRunnerGame() = default;

    static void TimerThunk(lv_timer_t* timer);
    static void OverlayThunk(lv_event_t* event);

    void Reset();
    void Tick();  // 100 ms: tilt + cuenta atrás de 1 s
    void TiltStep();
    void BuildMaze();
    void TryMove(int dx, int dy);
    void AdvanceLevel();
    void UpdateHud();
    void Die(const char* title);

    lv_obj_t* content_ = nullptr;
    lv_obj_t* hud_ = nullptr;
    lv_obj_t* board_ = nullptr;
    lv_obj_t* goal_ = nullptr;
    lv_obj_t* player_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    GameInput input_;

    std::vector<lv_obj_t*> walls_;  // rects de pared (se regeneran por nivel)

    uint8_t m_[15][15] = {};  // 1 = pared, 0 = pasillo
    int px_ = 1;              // celda del jugador
    int py_ = 1;
    int level_ = 1;
    int score_ = 0;
    int time_left_ = 0;
    int tilt_cd_ = 0;      // ms hasta el siguiente paso por tilt
    int second_acc_ = 0;   // acumulado para el segundo (tick de 100 ms)
    bool dead_ = false;
    bool pressing_ = false;
    int down_x_ = 0;
    int down_y_ = 0;
};

#endif  // HORNERITO_GAMES_MAZE_RUNNER_H_
