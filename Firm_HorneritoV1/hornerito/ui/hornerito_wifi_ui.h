// Pantalla de configuración Wi-Fi de HORNERITO: estado de la conexión, lista
// de redes (escaneo real, señal, candado, red conectada), conexión con
// contraseña mediante el teclado LVGL y "olvidar red", todo desde el propio
// dispositivo táctil, sin PC ni teléfono.
//
// Se apoya en la infraestructura ya existente del proyecto (componente
// 78__esp-wifi-connect): WifiManager (eventos/estado), SsidManager (NVS,
// hasta 10 redes) y el escaneo de WifiStation. No toca el portal de
// configuración cautivo: sigue siendo el fallback si la UI falla.
//
// Concurrencia: toda la UI vive en el hilo LVGL (timers/eventos). Los datos
// de red se leen con las APIs thread-safe de WifiManager y el motivo de
// desconexión se captura con un handler esp_event propio (solo lectura de un
// entero atómico; no consume eventos de nadie).
#ifndef HORNERITO_WIFI_UI_H_
#define HORNERITO_WIFI_UI_H_

#include <lvgl.h>

#include <atomic>
#include <string>
#include <vector>

#include <esp_event.h>
#include <esp_wifi_types_generic.h>  // wifi_ap_record_t

class HorneritoWifiUi {
public:
    // Crea las dos pantallas (una sola vez). Llamar bajo el lock del display.
    void Build();
    // Se ejecuta cada vez que se entra a la pantalla de Wi-Fi.
    void OnShow();

    lv_obj_t* screen() const { return screen_; }
    // true si el usuario está dentro del flujo Wi-Fi (lista o contraseña).
    bool active() const;

private:
    static void TimerCb(lv_timer_t* timer);
    static void BackCb(lv_event_t* event);       // lista -> Configuración
    static void ScanCb(lv_event_t* event);
    static void RowCb(lv_event_t* event);        // CLICKED + LONG_PRESSED
    static void PassBackCb(lv_event_t* event);   // contraseña -> lista
    static void PassConnectCb(lv_event_t* event);
    static void PassEyeCb(lv_event_t* event);
    static void OnStaDisconnected(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data);

    void Refresh();          // estado + información (timer 500 ms)
    void RebuildList();      // pinta rows_ en la lista
    void StartScan();
    void StartConnect(const std::string& ssid, const std::string& password);
    void OpenPassScreen(const std::string& ssid);
    void SelectRow(size_t index);
    void ForgetRow(size_t index);
    const char* FailureText(int reason) const;

    // Pantalla 1: lista de redes
    lv_obj_t* screen_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* info_label_ = nullptr;
    lv_obj_t* list_container_ = nullptr;
    lv_obj_t* scan_btn_ = nullptr;
    // Pantalla 2: contraseña
    lv_obj_t* pass_screen_ = nullptr;
    lv_obj_t* pass_heading_ = nullptr;
    lv_obj_t* pass_ssid_ = nullptr;
    lv_obj_t* pass_ta_ = nullptr;
    lv_obj_t* pass_eye_ = nullptr;

    lv_timer_t* timer_ = nullptr;
    esp_event_handler_instance_t disconnect_inst_ = nullptr;

    std::vector<wifi_ap_record_t> rows_;   // redes mostradas (máx. 20)
    std::string target_ssid_;              // red del intento en curso (crudo)
    std::string error_;                    // mensaje de error (se limpia al reintentar)
    std::string notice_;                   // aviso puntual (p. ej. "red olvidada")
    uint32_t scan_begin_tick_ = 0;
    uint32_t connect_start_tick_ = 0;
    std::atomic<int> last_reason_{0};      // primer motivo STA_DISCONNECTED
    bool scanning_ = false;
    std::atomic<bool> connecting_ = false;  // leído desde el handler esp_event
};

#endif  // HORNERITO_WIFI_UI_H_
