#include "hornerito/hornerito_emojis.h"

const lv_image_dsc_t* HorneritoEmojiImage(HorneritoEmoji emoji) {
    switch (emoji) {
        case HorneritoEmoji::kListening:
            return &hornerito_listening;
        case HorneritoEmoji::kThinking:
            return &hornerito_thinking;
        case HorneritoEmoji::kSpeaking:
            return &hornerito_speaking;
        case HorneritoEmoji::kError:
            return &hornerito_error;
        case HorneritoEmoji::kIdle:
        default:
            return &hornerito_idle;
    }
}
