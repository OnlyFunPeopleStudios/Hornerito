// Pantalla de música de HORNERITO (lista de canciones de la microSD +
// controles anterior/play-pausa/siguiente + estado de reproducción).
#ifndef HORNERITO_MUSIC_UI_H_
#define HORNERITO_MUSIC_UI_H_

#include <lvgl.h>

#include <cstddef>

#include "hornerito/audio/hornerito_music_player.h"

class HorneritoMusicUi {
public:
    // Crea la pantalla (una sola vez). Llamar bajo el lock del display.
    void Build();
    // Se ejecuta cada vez que se entra a la pantalla.
    void OnShow();

    lv_obj_t* screen() const { return screen_; }

private:
    static void TimerCb(lv_timer_t* timer);
    static void SongButtonCb(lv_event_t* event);
    static void BackCb(lv_event_t* event);
    static void PrevCb(lv_event_t* event);
    static void PlayCb(lv_event_t* event);
    static void NextCb(lv_event_t* event);

    void Refresh();
    void RebuildList();

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* list_container_ = nullptr;
    lv_obj_t* title_label_ = nullptr;
    lv_obj_t* state_label_ = nullptr;
    lv_obj_t* play_label_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    size_t listed_count_ = 0;
};

#endif  // HORNERITO_MUSIC_UI_H_
