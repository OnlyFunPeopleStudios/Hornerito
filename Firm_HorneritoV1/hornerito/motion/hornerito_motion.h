// Capa de movimiento/entrada del IMU QMI8658 (V2.2, FASE0 §8).
//
// Arquitectura:
//   QMI8658 -> HorneritoMotion (tilt / shake / accel / gyro) -> Juegos
//
// El driver reutilizable de FASE0 (esp-idf-lib/qmi8658c) NO encaja con la
// arquitectura actual: su transporte (i2cdev) crea su propio bus maestro con
// i2c_new_master_bus() en el puerto indicado, y I2C_NUM_0 ya está ocupado por
// el bus compartido del board (touch CST9217 / PMIC / codec) -> el IDF devuelve
// ESP_ERR_INVALID_STATE (verificado en i2c_common.c). i2cdev no expone ninguna
// API para adoptar un handle de bus existente, y qmi8658c_power_on/power_down
// están declarados sin implementar. Por eso este módulo reutiliza el MAPA DE
// REGISTROS y la SECUENCIA DE CONFIGURACIÓN de qmi8658c (FASE0) sobre un
// transporte propio del proyecto que nunca aborta ante NACK: el chip puede no
// estar cableado y el firmware debe seguir funcionando igual.
//
// Uso por los juegos: Initialize() una vez (desde el board), Enable() al
// entrar, Disable() al salir. El muestreo corre en un lv_timer (task LVGL,
// serializado con las lecturas de touch -> sin contención de bus). Consumo:
// tilt_x()/tilt_y() en [-1..1] y ConsumeShake() como pulso de un solo uso.
#ifndef HORNERITO_MOTION_H_
#define HORNERITO_MOTION_H_

#include <driver/i2c_master.h>
#include <lvgl.h>

#include <cstddef>
#include <cstdint>

class HorneritoMotion {
public:
    static HorneritoMotion& GetInstance();

    // Sonda 0x6B -> 0x6A y verifica WHO_AM_I == 0x05. Nunca aborta: si el
    // QMI8658 no responde, available() queda en false, se informa por log y
    // los juegos táctiles continúan funcionando.
    void Initialize(i2c_master_bus_handle_t bus);

    bool available() const { return available_; }

    // lv_timer de muestreo cada 20 ms. Idempotentes; Disable() limpia estado.
    void Enable();
    void Disable();

    // Lectura puntual (la usa el timer y sirve para pruebas).
    void PollOnce();

    void Accel(float& x, float& y, float& z) const;  // en g
    void Gyro(float& x, float& y, float& z) const;   // en grados/segundo
    float tilt_x() const { return tilt_x_; }         // -1..1 lateral (roll)
    float tilt_y() const { return tilt_y_; }         // -1..1 frontal (pitch)
    bool ConsumeShake();                             // true una sola vez

private:
    HorneritoMotion() = default;

    bool ProbeAndConfigure(uint8_t addr);
    bool WriteReg(uint8_t reg, uint8_t value);
    bool ReadRegs(uint8_t reg, uint8_t* buffer, size_t length);

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;
    bool available_ = false;
    lv_timer_t* timer_ = nullptr;

    float acc_[3] = {0.0f, 0.0f, 1.0f};
    float gyro_[3] = {0.0f, 0.0f, 0.0f};
    float tilt_x_ = 0.0f;
    float tilt_y_ = 0.0f;
    bool shake_pending_ = false;
    uint32_t last_shake_tick_ = 0;
    bool filtered_ = false;
    uint32_t read_errors_ = 0;
};

#endif  // HORNERITO_MOTION_H_
