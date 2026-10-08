// Común de los juegos de HORNERITO (V2.2): interfaz de todo módulo
// gestionado por GameManager + helpers de estilo/overlay compartidos.
//
// Los juegos son módulos independientes (bajo acoplamiento): el manager
// construye la pantalla de sesión (barra con Salir + título), llama a
// Enter(content) y, al salir, llama a Exit() y borra la pantalla completa.
#ifndef HORNERITO_GAMES_GAME_COMMON_H_
#define HORNERITO_GAMES_GAME_COMMON_H_

#include <lvgl.h>

class HorneritoGame {
public:
    virtual ~HorneritoGame() = default;

    // Nombre visible en la barra superior de la sesión.
    virtual const char* Name() const = 0;

    // Crea widgets, timers y callbacks bajo `content` (480x416).
    virtual void Enter(lv_obj_t* content) = 0;

    // Detiene timers/animaciones y desconecta callbacks. No borra `content`:
    // GameManager borra la pantalla de sesión después de Exit().
    virtual void Exit() = 0;
};

// Estilo de botón oscuro idéntico al resto de pantallas Hornerito.
void GameStyleDarkButton(lv_obj_t* button);

// Aplica la fuente del tema si existe (nunca con puntero nulo).
void GameApplyFont(lv_obj_t* obj);

// Overlay modal centrado (game over / pausa). El caller añade su propio
// LV_EVENT_CLICKED para continuar. Devuelve el panel; borrar con
// lv_obj_delete(panel) o muriendo con el content.
lv_obj_t* GameCreateOverlay(lv_obj_t* parent, const char* title, const char* subtitle);

// Rectángulo-entidad de juego (paletas, ladrillos, tuberías, comida...):
// objeto plano sin bordes ni scroll, listo para lv_obj_set_pos.
lv_obj_t* GameCreateRect(lv_obj_t* parent, uint32_t color, int32_t w,
                         int32_t h, int32_t radius);

#endif  // HORNERITO_GAMES_GAME_COMMON_H_
