#include "hornerito/ui/hornerito_keyboard.h"

#include <cstring>

#include "hornerito/ui/hornerito_ui.h"

namespace hornerito_keyboard {
namespace {

lv_obj_t* kb_ = nullptr;  // única instancia (compartida entre pantallas)
lv_obj_t* ta_ = nullptr;  // textarea vinculado
int page_ = 0;            // 0=abc, 1=123, 2=#+=
bool upper_ = false;      // bloqueo de mayúsculas (etiquetas A-Z)

// Mapas lv_buttonmatrix: array de strings con las filas separadas por "\n"
// y terminado en nullptr. Multi-tecla por etiqueta: "Aa", "abc", "123",
// "#+=", "esp" y "SUP"; todo lo demás es un glyph que se escribe tal cual.
const char* const kAbcLower[] = {
    "a", "b", "c", "d", "e", "f", "g", "h", "\n",
    "i", "j", "k", "l", "m", "n", "o", "p", "\n",
    "q", "r", "s", "t", "u", "v", "w", "x", "\n",
    "ñ", "y", "z", "Aa", "123", "esp", "SUP",
    nullptr,
};

const char* const kAbcUpper[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "\n",
    "I", "J", "K", "L", "M", "N", "O", "P", "\n",
    "Q", "R", "S", "T", "U", "V", "W", "X", "\n",
    "Ñ", "Y", "Z", "Aa", "123", "esp", "SUP",
    nullptr,
};

const char* const kNum[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "\n",
    "9", "0", ".", ",", "-", "_", "?", "!", "\n",
    "@", "#", "$", "%", "^", "&", "*", "(", "\n",
    "abc", "#+=", "esp", "SUP",
    nullptr,
};

const char* const kSym[] = {
    ")", "+", "=", "/", "\\", ":", ";", "'", "\n",
    "\"", "[", "]", "{", "}", "`", "|", "~", "\n",
    "<", ">", "\n",
    "abc", "123", "esp", "SUP",
    nullptr,
};

// Cambia de página/mayúsculas: set_map regenera los ctrl_bits a 0, así que
// los flags se reaplican SIEMPRE después de cada set_map.
void ApplyMap() {
    const char* const* map = kAbcLower;
    if (page_ == 1) {
        map = kNum;
    } else if (page_ == 2) {
        map = kSym;
    } else if (upper_) {
        map = kAbcUpper;
    }
    lv_buttonmatrix_set_map(kb_, map);
    // CLICK_TRIG: escribe al soltar el dedo (evita letras repetidas al
    // mantener pulsado). Sin CHECKED ni repetición en ninguna tecla.
    lv_buttonmatrix_set_button_ctrl_all(kb_, LV_BUTTONMATRIX_CTRL_CLICK_TRIG);
}

void KbCb(lv_event_t* event) {
    (void)event;
    if (kb_ == nullptr || ta_ == nullptr) {
        return;
    }
    uint32_t id = lv_buttonmatrix_get_selected_button(kb_);
    const char* text = lv_buttonmatrix_get_button_text(kb_, id);
    if (text == nullptr) {
        return;
    }

    if (strcmp(text, "Aa") == 0) {
        upper_ = !upper_;  // bloqueo de mayúsculas (etiquetas cambian a A-Z)
        ApplyMap();
    } else if (strcmp(text, "abc") == 0) {
        page_ = 0;
        ApplyMap();
    } else if (strcmp(text, "123") == 0) {
        page_ = 1;
        ApplyMap();
    } else if (strcmp(text, "#+=") == 0) {
        page_ = 2;
        ApplyMap();
    } else if (strcmp(text, "esp") == 0) {
        lv_textarea_add_char(ta_, ' ');
    } else if (strcmp(text, "SUP") == 0) {
        lv_textarea_delete_char(ta_);
    } else {
        // Glyph suelto (letra/dígito/símbolo, incluida ñ/Ñ en UTF-8).
        lv_textarea_add_text(ta_, text);
    }
}

}  // namespace

void Show(lv_obj_t* parent, lv_obj_t* ta, Mode mode) {
    if (kb_ == nullptr) {
        kb_ = lv_buttonmatrix_create(parent);
        lv_obj_set_size(kb_, 448, 224);
        lv_obj_align(kb_, LV_ALIGN_TOP_MID, 0, 156);
        lv_obj_set_style_text_font(kb_, HorneritoTextFont(), LV_PART_ITEMS);
        lv_obj_add_event_cb(kb_, KbCb, LV_EVENT_VALUE_CHANGED, nullptr);
    } else if (lv_obj_get_parent(kb_) != parent) {
        lv_obj_set_parent(kb_, parent);
    }

    ta_ = ta;
    upper_ = false;
    page_ = (mode == Mode::kNumeric) ? 1 : 0;
    if (ta != nullptr) {
        lv_textarea_set_password_mode(ta, mode == Mode::kPassword);
    }
    ApplyMap();
}

}  // namespace hornerito_keyboard
