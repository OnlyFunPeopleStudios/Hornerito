// Transferencia de archivos PC -> microSD de HORNERITO.
//
// DECISIÓN TÉCNICA (inspeccionado antes de implementar):
// el ESP32-C6 de esta placa NO tiene controlador USB-OTG, solo USB
// Serial/JTAG (sdkconfig: CONFIG_SOC_USB_SERIAL_JTAG_SUPPORTED=y y no existe
// CONFIG_SOC_USB_OTG_SUPPORTED; CONFIG_ESP_ROM_USB_OTG_NUM=-1; el ejemplo
// tusb_msc de IDF soporta S2/S3/P4/H4, no C6). Por lo tanto USB Mass Storage
// NO es viable: el conector USB no puede enumerarse como disco.
//
// Alternativa implementada: servidor HTTP (esp_http_server) sobre el Wi-Fi
// ya existente. Desde el navegador de la PC se sube cada archivo con un PUT
// en streaming a /sdcard/Music/, con progreso visible en el propio navegador
// y en la pantalla Archivos de HORNERITO. Sin herramientas adicionales en la PC.
#ifndef HORNERITO_TRANSFER_H_
#define HORNERITO_TRANSFER_H_

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

#include <esp_http_server.h>

enum class HorneritoTransferState {
    kIdle,         // sin transferencia activa
    kTransferring, // subida en curso
    kError,        // última transferencia falló (ver last_error())
};

class HorneritoTransfer {
public:
    static HorneritoTransfer& GetInstance();

    // Arranca el servidor HTTP (puerto 80). Idempotente; no bloquea.
    void Start();

    HorneritoTransferState state() const { return state_.load(); }
    bool server_running() const { return server_running_.load(); }
    uint32_t received_bytes() const { return received_.load(); }
    uint32_t total_bytes() const { return total_.load(); }
    std::string current_file() const;
    std::string last_error() const;

private:
    HorneritoTransfer() = default;
    ~HorneritoTransfer() = default;

    static esp_err_t GetIndexHandler(httpd_req_t* req);
    static esp_err_t PutUploadHandler(httpd_req_t* req);
    // Vinculación Google Calendar SIN backend: página /pair que guía el
    // copiado de la URL secreta iCal y /pair_set que la guarda en NVS.
    static esp_err_t GetPairHandler(httpd_req_t* req);
    static esp_err_t PostPairSetHandler(httpd_req_t* req);

    void SetFile(const std::string& name);
    void SetError(const std::string& error);

    void* server_ = nullptr;  // httpd_handle_t
    std::atomic<HorneritoTransferState> state_{HorneritoTransferState::kIdle};
    std::atomic<bool> server_running_{false};
    std::atomic<uint32_t> received_{0};
    std::atomic<uint32_t> total_{0};
    mutable std::mutex mutex_;
    std::string current_file_;
    std::string last_error_;
};

#endif  // HORNERITO_TRANSFER_H_
