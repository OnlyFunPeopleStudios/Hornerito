// Capa de entrada táctil común a todos los juegos (V2.2 §5):
//
//   Touch -> GameInput -> Juego
//
// Entrega coordenadas LOCALES al área de juego en tres callbacks
// (pulsación, arrastre y soltado). Cada juego deriva de ahí su gesto
// (swipe en Snake, arrastre de pala en Pong/Breakout, toque en Flappy).
// Los callbacks viven en el objeto: al borrar la pantalla mueren con él;
// además, Exit() de cada juego llama a Detach() explícitamente.
#ifndef HORNERITO_GAMES_GAME_INPUT_H_
#define HORNERITO_GAMES_GAME_INPUT_H_

#include <lvgl.h>

#include <cstdint>
#include <functional>

class GameInput {
public:
    using Handler = std::function<void(int32_t x, int32_t y)>;

    Handler on_down;   // dedo presionado
    Handler on_move;   // dedo en movimiento (arrastre)
    Handler on_up;     // dedo soltado

    // Adjunta la entrada sobre `field` (coords locales a ese objeto).
    void Attach(lv_obj_t* field);
    void Detach();     // quita los callbacks (idempotente)

private:
    static void EventThunk(lv_event_t* event);
    void Handle(lv_event_t* event);
    bool LocalPoint(int32_t& x, int32_t& y) const;

    lv_obj_t* field_ = nullptr;
};

#endif  // HORNERITO_GAMES_GAME_INPUT_H_
