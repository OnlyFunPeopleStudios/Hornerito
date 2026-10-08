// Emojis de HORNERITO (estado del asistente).
// Imágenes generadas por scripts/assets/gen_emojis.py desde 01_Arte ->
// work/assets -> hornerito/assets/emojis/generated/*.c (arrays LVGL RGB565A8).
#ifndef HORNERITO_EMOJIS_H_
#define HORNERITO_EMOJIS_H_

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_image_dsc_t hornerito_idle;        // IDLE
extern const lv_image_dsc_t hornerito_listening;   // LISTENING
extern const lv_image_dsc_t hornerito_thinking;    // THINKING
extern const lv_image_dsc_t hornerito_speaking;    // SPEAKING
extern const lv_image_dsc_t hornerito_error;       // ERROR

#ifdef __cplusplus
}
#endif

enum class HorneritoEmoji {
    kIdle,
    kListening,
    kThinking,
    kSpeaking,
    kError,
};

const lv_image_dsc_t* HorneritoEmojiImage(HorneritoEmoji emoji);

#endif  // HORNERITO_EMOJIS_H_
