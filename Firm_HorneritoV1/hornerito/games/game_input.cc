#include "hornerito/games/game_input.h"

void GameInput::Attach(lv_obj_t* field) {
    Detach();
    field_ = field;
    lv_obj_add_flag(field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(field, EventThunk, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(field, EventThunk, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(field, EventThunk, LV_EVENT_RELEASED, this);
}

void GameInput::Detach() {
    if (field_ == nullptr) {
        return;
    }
    lv_obj_remove_event_cb_with_user_data(field_, EventThunk, this);
    field_ = nullptr;
}

void GameInput::EventThunk(lv_event_t* event) {
    static_cast<GameInput*>(lv_event_get_user_data(event))->Handle(event);
}

void GameInput::Handle(lv_event_t* event) {
    int32_t x = 0;
    int32_t y = 0;
    if (!LocalPoint(x, y)) {
        return;
    }
    switch (lv_event_get_code(event)) {
        case LV_EVENT_PRESSED:
            if (on_down) on_down(x, y);
            break;
        case LV_EVENT_PRESSING:
            if (on_move) on_move(x, y);
            break;
        case LV_EVENT_RELEASED:
            if (on_up) on_up(x, y);
            break;
        default:
            break;
    }
}

bool GameInput::LocalPoint(int32_t& x, int32_t& y) const {
    if (field_ == nullptr) {
        return false;
    }
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr) {
        return false;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    // En LVGL 9 los coords de un objeto ya son absolutos de pantalla
    // (el punto del indev también), así que basta restar su origen.
    lv_area_t global;
    lv_obj_get_coords(field_, &global);
    x = p.x - global.x1;
    y = p.y - global.y1;
    return true;
}
