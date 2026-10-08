// Piedra, Papel o Tijera (V2.3.0 §11): partida contra Hornerito con sus
// emojis existentes (sin assets nuevos). LVGL 9 puro.
//
// - Flujo por rondas: eliges -> "Hornerito está pensando..." (emoji
//   THINKING, ~900 ms) -> revela elección y resultado -> siguiente ronda
//   tocando cualquier opción. Marcador Tú/Hornerito + botón de reinicio.
// - Solo touch (§7): botones grandes en la parte baja.
// - Ciclo de vida vía GameManager (Exit() detiene el timer).
#ifndef HORNERITO_GAMES_ROCK_PAPER_SCISSORS_H_
#define HORNERITO_GAMES_ROCK_PAPER_SCISSORS_H_

#include <lvgl.h>

#include "hornerito/games/game_common.h"

class RockPaperScissorsGame : public HorneritoGame {
public:
    static RockPaperScissorsGame& GetInstance();

    const char* Name() const override { return "Piedra Papel Tijera"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    RockPaperScissorsGame() = default;

    enum class Phase { kPick, kThinking, kResult };

    static void ThinkThunk(lv_timer_t* timer);
    static void BtnThunk(lv_event_t* event);
    static void ResetThunk(lv_event_t* event);

    void OnPick(int choice);
    void Resolve();
    void SetEmoji(int emoji_index);
    void UpdateScore();

    lv_obj_t* content_ = nullptr;
    lv_obj_t* score_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* reveal_label_ = nullptr;
    lv_obj_t* result_label_ = nullptr;
    lv_obj_t* emoji_ = nullptr;
    lv_obj_t* btn_[3] = {};
    lv_obj_t* reset_btn_ = nullptr;
    lv_timer_t* think_timer_ = nullptr;

    Phase phase_ = Phase::kPick;
    int choice_ = 0;
    int score_user_ = 0;
    int score_cpu_ = 0;
};

#endif  // HORNERITO_GAMES_ROCK_PAPER_SCISSORS_H_
