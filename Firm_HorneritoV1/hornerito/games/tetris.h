// Tetris (V2.3.0 §8): tablero 10x20 con las 7 piezas clásicas, LVGL 9 puro.
//
// - Tablero 180x360 (celdas de 18 px) a la izquierda; panel derecho con la
//   pieza siguiente y los 4 botones de control.
// - Entrada: botones IZQ/DER/ROT/BAJ + gestos sobre el tablero (tap =
//   girar, swipe izq/der = mover, swipe abajo = soft drop) + tilt del
//   QMI8658: tilt_x mueve la pieza y tilt_y la gira (Enable() en Enter).
// - Ciclo de vida vía GameManager: Exit() detiene timers y desconecta
//   callbacks; los widgets mueren con la pantalla de sesión.
#ifndef HORNERITO_GAMES_TETRIS_H_
#define HORNERITO_GAMES_TETRIS_H_

#include <lvgl.h>

#include <cstdint>
#include <vector>

#include "hornerito/games/game_common.h"
#include "hornerito/games/game_input.h"

class TetrisGame : public HorneritoGame {
public:
    static TetrisGame& GetInstance();

    const char* Name() const override { return "Tetris"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    TetrisGame() = default;

    static void TimerThunk(lv_timer_t* timer);
    static void TiltThunk(lv_timer_t* timer);
    static void BtnThunk(lv_event_t* event);
    static void OverlayThunk(lv_event_t* event);

    void Reset();
    void TiltTick();  // 50 ms: tilt_x mueve (cooldown); tilt_y gira (edge)
    void Tick();  // gravedad: baja una fila o fija la pieza
    bool CanPlace(int32_t px, int32_t py, int32_t rot) const;
    void Spawn();
    void LockPiece();
    void Move(int32_t dx);
    void Rotate();
    void SoftDrop();
    void Sync();  // redibuja tablero + pieza activa desde el estado
    void UpdateHud();
    void UpdatePreview();
    void Die();

    lv_obj_t* content_ = nullptr;
    lv_obj_t* board_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* level_label_ = nullptr;
    lv_obj_t* lines_label_ = nullptr;
    lv_obj_t* btn_left_ = nullptr;
    lv_obj_t* btn_right_ = nullptr;
    lv_obj_t* btn_rotate_ = nullptr;
    lv_obj_t* btn_down_ = nullptr;
    lv_obj_t* piece_[4] = {};   // 4 celdas de la pieza activa
    lv_obj_t* next_cells_[4] = {};  // vista previa (panel derecho)
    lv_obj_t* overlay_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    lv_timer_t* tilt_timer_ = nullptr;
    int tilt_cd_ = 0;             // ms hasta el siguiente paso por tilt
    bool tilt_rot_armed_ = true;  // histéresis del giro por tilt_y
    GameInput input_;

    std::vector<lv_obj_t*> pool_;  // celdas fijadas (crece bajo demanda)

    uint8_t grid_[20][10] = {};  // 0 = vacío; 1..7 = tipo + 1
    int32_t cur_ = 0;            // tipo actual (0..6)
    int32_t next_ = 0;
    int32_t rot_ = 0;            // rotación 0..3
    int32_t px_ = 3;             // caja 4x4 dentro del tablero
    int32_t py_ = 0;
    int score_ = 0;
    int lines_ = 0;
    int level_ = 0;
    bool dead_ = false;
    bool pressing_ = false;
    int32_t down_x_ = 0;
    int32_t down_y_ = 0;
};

#endif  // HORNERITO_GAMES_TETRIS_H_
