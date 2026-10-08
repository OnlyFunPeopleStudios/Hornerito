// Pantalla Archivos de HORNERITO: estado USB, microSD, red y transferencia
// de archivos (servidor HTTP -> /sdcard/Music). Ver hornerito/transfer/.
#ifndef HORNERITO_FILES_UI_H_
#define HORNERITO_FILES_UI_H_

#include <lvgl.h>

class HorneritoFilesUi {
public:
    // Crea la pantalla (una sola vez). Llamar bajo el lock del display.
    void Build();
    // Se ejecuta cada vez que se entra a la pantalla.
    void OnShow();

    lv_obj_t* screen() const { return screen_; }

private:
    static void TimerCb(lv_timer_t* timer);
    static void BackCb(lv_event_t* event);
    void Refresh();

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* usb_label_ = nullptr;
    lv_obj_t* sd_label_ = nullptr;
    lv_obj_t* net_label_ = nullptr;
    lv_obj_t* transfer_label_ = nullptr;
    lv_obj_t* file_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;
};

#endif  // HORNERITO_FILES_UI_H_
