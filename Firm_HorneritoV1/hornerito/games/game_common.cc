#include "hornerito/games/game_common.h"

#include "hornerito/ui/hornerito_ui.h"

void GameStyleDarkButton(lv_obj_t* button) {
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2E4058), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_radius(button, 16, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
}

void GameApplyFont(lv_obj_t* obj) {
    const lv_font_t* font = HorneritoTextFont();
    if (font != nullptr) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
}

lv_obj_t* GameCreateOverlay(lv_obj_t* parent, const char* title, const char* subtitle) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(panel, 380, 200);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x141A22), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2E4058), 0);
    lv_obj_set_style_radius(panel, 20, 0);
    lv_obj_set_style_pad_all(panel, 16, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 12, 0);

    lv_obj_t* label = lv_label_create(panel);
    lv_label_set_text(label, title);
    GameApplyFont(label);
    lv_obj_set_style_text_color(label, lv_color_hex(0x5BC8F5), 0);

    if (subtitle != nullptr) {
        lv_obj_t* hint = lv_label_create(panel);
        lv_label_set_text(hint, subtitle);
        GameApplyFont(hint);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x9AB2C8), 0);
    }
    return panel;
}

lv_obj_t* GameCreateRect(lv_obj_t* parent, uint32_t color, int32_t w,
                         int32_t h, int32_t radius) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
