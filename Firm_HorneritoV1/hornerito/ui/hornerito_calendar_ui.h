// Pantalla de agenda del calendario (HOY / MAÑANA / PRÓXIMOS con caché NVS
// y "Última actualización"), menú Calendario (entrada desde Configuración),
// pantalla QR de vinculación (QR -> página /pair del propio equipo en la LAN,
// sin backend) y pantalla para pegar la URL secreta iCal/ICS. Patrón idéntico a
// HorneritoWifiUi: Build() al arrancar, OnShow() al entrar, círculos de
// atrás.
//
// El teclado es el compartido de hornerito_keyboard (se reapunta aquí en
// modo texto). La sincronización es del módulo hornerito_calendar (task
// aparte); esta UI solo pide syncs y pinta resultados.
#ifndef HORNERITO_CALENDAR_UI_H_
#define HORNERITO_CALENDAR_UI_H_

#include <lvgl.h>

#include <cstdint>
#include <string>

class HorneritoCalendarUi {
public:
    void Build();         // idempotente; lo llama HorneritoUi::Start
    void OnShow();        // agenda
    void OnShowEntry();   // menú Calendario (entrada desde Configuración)
    lv_obj_t* screen() const { return screen_; }
    lv_obj_t* entry() const { return menu_screen_; }
    bool active() const;

private:
    static void TimerCb(lv_timer_t* timer);
    void Render();        // agenda
    void RenderMenu();    // menú (Estado / botones)
    std::string FooterText();  // "Última act.: ... - estado" (agenda y menú)
    void OpenUrlScreen();
    void OpenAgenda();
    void OpenQrScreen();  // QR -> página /pair del propio equipo (sin backend)

    // screen_ (agenda): atrás, refrescar, URL, lista, pie de estado.
    static void BackCb(lv_event_t* event);
    static void RefreshCb(lv_event_t* event);
    static void UrlOpenCb(lv_event_t* event);  // engranaje: abre el menú
    // url_screen_ (pegar URL): atrás y guardar (vuelven al menú).
    static void UrlBackCb(lv_event_t* event);
    static void UrlSaveCb(lv_event_t* event);
    // menu_screen_ (entrada).
    static void MenuBackCb(lv_event_t* event);
    static void MenuAgendaCb(lv_event_t* event);
    static void MenuIcalCb(lv_event_t* event);
    static void MenuSyncCb(lv_event_t* event);
    static void MenuLinkCb(lv_event_t* event);  // "Vincular Google Calendar"
    // qr_screen_ (QR de vinculación): atrás.
    static void QrBackCb(lv_event_t* event);
    // Liberación del buffer del canvas al salir de la pantalla.
    static void QrUnloadCb(lv_event_t* event);

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* url_screen_ = nullptr;
    lv_obj_t* menu_screen_ = nullptr;
    lv_obj_t* qr_screen_ = nullptr;
    lv_obj_t* qr_ = nullptr;           // canvas I1 del QR (buffer lazy)
    lv_obj_t* qr_status_ = nullptr;    // instrucciones bajo el QR
    uint8_t* qr_buffer_ = nullptr;     // ~10 KB: malloc al abrir, free al salir
    lv_obj_t* list_ = nullptr;          // contenedor con scroll de la agenda
    lv_obj_t* status_label_ = nullptr;  // "Última act.: ..." + estado
    lv_obj_t* url_ta_ = nullptr;
    lv_obj_t* estado_label_ = nullptr;  // menú: Estado
    lv_timer_t* timer_ = nullptr;

    int last_status_ = -1;      // último estado mostrado (-1 = nada aún)
    bool was_syncing_ = false;
    int64_t last_today_ = -1;   // para repintar al cruzar la medianoche
};

#endif  // HORNERITO_CALENDAR_UI_H_
