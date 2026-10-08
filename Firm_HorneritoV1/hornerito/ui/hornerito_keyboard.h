// Teclado táctil híbrido tipo teléfono para HORNERITO (480x480, un dedo).
//
// lv_buttonmatrix con mapas propios (no lv_keyboard): 4 filas de teclas
// grandes en el hueco fijo 448x224 a y=156 que ocupaba el teclado LVGL,
// con tres páginas alcanzables en ciclo (abc -> 123 -> #+= -> abc) y
// controles dedicados: Aa (mayúsculas), esp (espacio) y SUP (suprimir).
// El glyph ⌫ no existe en HorneritoTextFont(), por eso la etiqueta es SUP.
//
// Un único widget compartido entre pantallas: Show() lo crea la primera
// vez y después lo reapunta al padre/textarea indicados. Los cambios de
// página no tocan el textarea, solo lo escriben letras/números/símbolos.
#ifndef HORNERITO_KEYBOARD_H_
#define HORNERITO_KEYBOARD_H_

#include <lvgl.h>

namespace hornerito_keyboard {

// kText:     texto libre, arranca en página de letras (minúsculas).
// kPassword: como kText + deja el textarea en modo contraseña.
// kNumeric:  arranca en la página numérica (123).
enum class Mode { kText, kPassword, kNumeric };

// Muestra (o reapunta) el teclado dentro de `parent` y lo vincula a `ta`.
// Espacio y posición fijos: 448x224 alineado TOP_MID y=156, pensado para
// pantallas con pad=16 sobre 480x480 (mismo hueco que el teclado antiguo).
void Show(lv_obj_t* parent, lv_obj_t* ta, Mode mode);

}  // namespace hornerito_keyboard

#endif  // HORNERITO_KEYBOARD_H_
