// Interfaz propia de HORNERITO (V2.2): pantalla principal con la mascota y el
// menú (Asistente / Música / Archivos / Estado / Juegos / Configuración),
// más la navegación a la pantalla de XiaoZhi, al selector de juegos y a la
// configuración Wi-Fi. Inspirada en el launcher de la firmware de fábrica
// Waveshare (rejilla de accesos + música desde microSD), implementada sobre
// LVGL y los componentes ya existentes del proyecto.
//
// No modifica el UI de XiaoZhi: reutiliza su pantalla activa como pantalla
// "Asistente" y le añade un botón flotante de menú.
#ifndef HORNERITO_UI_H_
#define HORNERITO_UI_H_

#include <lvgl.h>

#include "device_state.h"
#include "hornerito/hornerito_emojis.h"
#include "hornerito/ui/hornerito_calendar_ui.h"
#include "hornerito/ui/hornerito_files_ui.h"
#include "hornerito/ui/hornerito_music_ui.h"
#include "hornerito/ui/hornerito_wifi_ui.h"

// Fuentes del tema actual (nullptr si no hay tema LVGL todavía).
const lv_font_t* HorneritoTextFont();
const lv_font_t* HorneritoIconFont();

class HorneritoUi {
public:
    static HorneritoUi& GetInstance();

    // Llamar una vez, justo después de display->SetupUI().
    void Start();

    void ShowHome();
    void ShowAssistant();
    void ShowMusic();
    void ShowFiles();
    void ShowStatus();
    void ShowGames();
    void ShowSettings();
    void ShowWifi();
    void ShowCalendar();

    bool started() const { return started_; }

private:
    HorneritoUi() = default;

    static void TimerCb(lv_timer_t* timer);
    static void ScreenOffCb(lv_event_t* event);   // ciclo "Apagado de pantalla"
    static void UpdateScreenOffLabel();

    void OnTick(DeviceState state);
    void BuildHome();
    void BuildStatus();
    void BuildGames();
    void BuildSettings();
    void BuildAssistantEntry();
    void UpdatePet(DeviceState state);
    void UpdateStatusInfo(DeviceState state);

    lv_obj_t* assistant_screen_ = nullptr;  // pantalla de XiaoZhi
    lv_obj_t* home_ = nullptr;
    lv_obj_t* status_ = nullptr;
    lv_obj_t* games_menu_ = nullptr;
    lv_obj_t* settings_ = nullptr;
    lv_obj_t* settings_brightness_ = nullptr;  // slider de brillo
    lv_obj_t* settings_off_label_ = nullptr;   // valor de "Apagado"
    lv_obj_t* pet_ = nullptr;
    lv_obj_t* state_label_ = nullptr;
    lv_obj_t* status_state_ = nullptr;
    lv_obj_t* status_battery_ = nullptr;
    lv_obj_t* status_storage_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    HorneritoEmoji last_emoji_ = HorneritoEmoji::kIdle;
    DeviceState last_state_ = kDeviceStateUnknown;
    bool started_ = false;
    bool home_shown_ = false;
    bool was_configuring_ = false;
    // Asistente sin tocar la pantalla: último intento de auto-escucha y
    // "el usuario fue al inicio" (cierra la conversación en curso).
    int64_t last_auto_listen_us_ = 0;
    bool leaving_assistant_ = false;
    HorneritoMusicUi music_ui_;
    HorneritoFilesUi files_ui_;
    HorneritoWifiUi wifi_ui_;
    HorneritoCalendarUi calendar_ui_;
};

#endif  // HORNERITO_UI_H_
