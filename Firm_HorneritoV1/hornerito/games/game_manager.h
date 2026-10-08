// Game manager de HORNERITO (V2.2 §9): ciclo de vida de los juegos.
//
//   Juegos (menú) -> GameManager::Start() -> sesión (pantalla propia)
//                                    game->Enter(content)
//   botón Salir  -> GameManager::Exit()  -> game->Exit() + borrar pantalla
//                                    -> vuelve al menú Juegos + estado idle
//
// Reglas de lifecycle (criterios V2.2): cada Exit() detiene timers y
// animaciones y desconecta callbacks; el manager borra después la pantalla
// de sesión (con ello mueren sus widgets y event cbs). Nunca hace falta
// reiniciar: entrar/salir/reentrar es soportado (y se autolimpia cualquier
// sesión huérfana que dejara un evento externo).
#ifndef HORNERITO_GAMES_GAME_MANAGER_H_
#define HORNERITO_GAMES_GAME_MANAGER_H_

#include <lvgl.h>

#include "hornerito/games/game_common.h"

enum class GameId {
    kPet = 0,
    kSnake,
    kPong,
    kBreakout,
    kFlappy,
    kTetris,
    kSpaceInvaders,
    kMazeRunner,
    kRockPaperScissors,
    kMemory,
    kCatchTheEgg,
};

class GameManager {
public:
    static GameManager& GetInstance();

    // Inicia un juego. Exige estado idle (pasa a PLAYING_GAME) y monta la
    // pantalla de sesión. Devuelve false si no pudo (estado no idle / id
    // desconocido) sin romper nada.
    bool Start(GameId id);

    bool Active() const { return current_ != nullptr; }

    // Sale del juego activo: Exit() del módulo, timers off, pantalla borrada,
    // control al menú Juegos y estado de vuelta a idle.
    void Exit();

private:
    GameManager() = default;
    HorneritoGame* Lookup(GameId id);
    void BuildSession();

    lv_obj_t* session_ = nullptr;
    lv_obj_t* content_ = nullptr;
    HorneritoGame* current_ = nullptr;
};

#endif  // HORNERITO_GAMES_GAME_MANAGER_H_
