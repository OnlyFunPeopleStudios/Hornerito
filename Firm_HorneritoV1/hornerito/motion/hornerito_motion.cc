#include "hornerito/motion/hornerito_motion.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cmath>
#include <cstring>

// Registro map y secuencia de configuración tomados del driver recomendado en
// FASE0 §8: esp-idf-lib/qmi8658c (MIT, xyzroe) — qmi8658c.h / qmi8658c.c.
// Solo cambia el transporte: bus i2c_master compartido del board, sin
// ESP_ERROR_CHECK (un NACK no debe reiniciar el equipo).

#define TAG "HorneritoMotion"

namespace {

// Registros (qmi8658c.h)
constexpr uint8_t kRegWhoAmI = 0x00;
constexpr uint8_t kRegRevision = 0x01;
constexpr uint8_t kRegCtrl1 = 0x02;  // Serial interface: ADDR_AI (bit6) / BE (bit5)
constexpr uint8_t kRegCtrl2 = 0x03;  // ODR + escala del acelerómetro
constexpr uint8_t kRegCtrl3 = 0x04;  // ODR + escala del giroscopio
constexpr uint8_t kRegCtrl7 = 0x08;  // bits 0/1 = enable acc/gyro (modo dual)
constexpr uint8_t kRegReset = 0x60;
constexpr uint8_t kRegAccXL = 0x35;  // 0x35..0x3A acc X/Y/Z L/H
constexpr uint8_t kRegGyrXL = 0x3B;  // 0x3B..0x40 gyr X/Y/Z L/H

constexpr uint8_t kWhoAmIValue = 0x05;
constexpr uint8_t kResetValue = 0xB0;
constexpr uint32_t kReplyDelayMs = 25;  // REPLY_DELAY_MS de qmi8658c.c

// Config (enums qmi8658c_*): acc ±4 g @125 Hz, giro ±2048 dps @125 Hz.
constexpr uint8_t kAccScale = 1;   // QMI8658C_ACC_SCALE_4G (bits 4..6 CTRL2)
constexpr uint8_t kAccOdr = 6;     // QMI8658C_ACC_ODR_125 (bits 0..3 CTRL2)
constexpr uint8_t kGyroScale = 7;  // QMI8658C_GYRO_SCALE_2048DPS (CTRL3)
constexpr uint8_t kGyroOdr = 6;    // QMI8658C_GYRO_ODR_125 (bits 0..3 CTRL3)
constexpr uint8_t kModeDual = 3;   // QMI8658C_MODE_DUAL (CTRL7 bits 0..1)
// CTRL1 = 0x40: ADDR_AI=1 + BE=0. FIX (2026-10-04): sin ADDR_AI cada ráfaga
// devuelve el mismo registro repetido (datasheet QMI8658C §5.3) y todos los
// ejes leen el mismo byte (0xBBBB). BE=0 = little-endian, coherente con el
// unpack de PollOnce. Verificado contra el ejemplo oficial Waveshare
// 02_I2C_QMI8658 (escribe 0x60 = AI+BE) y el datasheet.
constexpr uint8_t kCtrl1Value = 0x40;

// Direcciones I2C posibles (qmi8658c.h define 0x6B por defecto; 0x6A si SA0=0).
constexpr uint8_t kAddresses[] = {0x6B, 0x6A};

// Escalas: raw / sens = unidades (qmi8658c.c: acc_scale_sensitivity_table y
// gyro_scale_sensitivity_table equivalen a (1<<14)>>escala y (1<<11)>>escala).
inline float AccSensitivity() {
    return static_cast<float>(1 << 14) / (1 << kAccScale);
}
inline float GyroSensitivity() {
    return static_cast<float>(1 << 11) / (1 << kGyroScale);
}

// Ajuste fino del signo de los ejes: calibrar en hardware (la orientación
// depende de cómo esté montado el chip en la placa).
constexpr float kTiltXSign = 1.0f;
constexpr float kTiltYSign = 1.0f;

constexpr float kTiltFullRangeDeg = 45.0f;  // 45° de inclinación = tilt ±1

// Reposo medido (CALIBRACION_MOVIMIENTO.md §3.1): ay0 = -0.12 g y la
// gravedad apunta hacia -Z (az ≈ -1.10 g) en este montaje de la placa.
constexpr float kAccelAy0 = -0.12f;

// Shake sobre la NORMA RAW del acelerómetro (CALIBRACION_MOVIMIENTO.md §4):
// con la EMA α=0.2 un pico real de 4.26 g llegaba como 1.28 g filtrados y el
// umbral jamás se alcanzaba. Medido: reposo ≤1.24 g, tilt ≤1.41 g, movimiento
// vigoroso≈2.44 g, shake≈4.26 g → |‖a‖raw − 1g| ≥ 1.5 g (‖a‖ ≥ 2.5 g).
constexpr float kShakeThresholdG = 1.5f;
constexpr uint32_t kShakeRefractoryMs = 400;
constexpr float kEmaAlpha = 0.2f;           // suavizado exponencial del acc (tilt)
constexpr int kMaxReadErrors = 20;          // fallos seguidos -> IMU caído

inline float Clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void PollThunk(lv_timer_t* timer) {
    static_cast<HorneritoMotion*>(lv_timer_get_user_data(timer))->PollOnce();
}

}  // namespace

HorneritoMotion& HorneritoMotion::GetInstance() {
    static HorneritoMotion instance;
    return instance;
}

bool HorneritoMotion::WriteReg(uint8_t reg, uint8_t value) {
    if (dev_ == nullptr) {
        return false;
    }
    uint8_t buffer[2] = {reg, value};
    return i2c_master_transmit(dev_, buffer, sizeof(buffer), 100) == ESP_OK;
}

bool HorneritoMotion::ReadRegs(uint8_t reg, uint8_t* buffer, size_t length) {
    if (dev_ == nullptr) {
        return false;
    }
    return i2c_master_transmit_receive(dev_, &reg, 1, buffer, length, 100) == ESP_OK;
}

bool HorneritoMotion::ProbeAndConfigure(uint8_t addr) {
    if (dev_ != nullptr) {
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
    }

    i2c_device_config_t cfg = {};
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = addr;
    cfg.scl_speed_hz = 400 * 1000;
    if (i2c_master_bus_add_device(bus_, &cfg, &dev_) != ESP_OK) {
        dev_ = nullptr;
        return false;
    }

    auto remove = [this]() {
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
    };

    uint8_t who = 0;
    if (!ReadRegs(kRegWhoAmI, &who, 1) || who != kWhoAmIValue) {
        remove();
        return false;
    }

    // Secuencia: reset -> CTRL1 (ADDR_AI) -> CTRL7 modo dual -> CTRL2 -> CTRL3.
    // El reset restaura los defaults de CTRL1, por eso va después de él.
    if (!WriteReg(kRegReset, kResetValue)) {
        remove();
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(kReplyDelayMs));

    if (!WriteReg(kRegCtrl1, kCtrl1Value) || !WriteReg(kRegCtrl7, kModeDual) ||
        !WriteReg(kRegCtrl2, (kAccScale << 4) | kAccOdr) ||
        !WriteReg(kRegCtrl3, (kGyroScale << 4) | kGyroOdr)) {
        remove();
        return false;
    }

    // Verificación del modo, igual que qmi8658c_setup().
    uint8_t ctrl7 = 0;
    if (!ReadRegs(kRegCtrl7, &ctrl7, 1) || (ctrl7 & 0x03) != kModeDual) {
        remove();
        return false;
    }

    uint8_t revision = 0;
    ReadRegs(kRegRevision, &revision, 1);
    ESP_LOGI(TAG, "QMI8658 detectado en 0x%02X (revisi\u00f3n 0x%02X)", addr, revision);
    return true;
}

void HorneritoMotion::Initialize(i2c_master_bus_handle_t bus) {
    if (available_) {
        return;
    }
    if (bus == nullptr) {
        ESP_LOGE(TAG, "Bus I2C nulo: QMI8658 no inicializado");
        return;
    }
    bus_ = bus;

    for (uint8_t addr : kAddresses) {
        if (ProbeAndConfigure(addr)) {
            available_ = true;
            return;
        }
    }

    ESP_LOGE(TAG, "QMI8658 no detectado (0x6B/0x6A): IMU deshabilitado, "
                  "los juegos táctiles siguen funcionando");
}

void HorneritoMotion::Enable() {
    if (!available_ || timer_ != nullptr) {
        return;
    }
    timer_ = lv_timer_create(PollThunk, 20, this);
    filtered_ = false;
}

void HorneritoMotion::Disable() {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    shake_pending_ = false;
    tilt_x_ = 0.0f;
    tilt_y_ = 0.0f;
    filtered_ = false;
}

void HorneritoMotion::PollOnce() {
    if (!available_) {
        return;
    }

    uint8_t raw[12];
    bool ok = ReadRegs(kRegAccXL, raw, 6) && ReadRegs(kRegGyrXL, raw + 6, 6);
    if (!ok) {
        if (++read_errors_ >= kMaxReadErrors) {
            available_ = false;
            Disable();
            ESP_LOGE(TAG, "QMI8658 dejó de responder: IMU deshabilitado");
        }
        return;
    }
    read_errors_ = 0;

    int16_t v[6];
    for (int i = 0; i < 6; i++) {
        v[i] = static_cast<int16_t>((raw[i * 2 + 1] << 8) | raw[i * 2]);
    }

    const float acc_sens = AccSensitivity();
    const float gyr_sens = GyroSensitivity();
    float raw_acc[3];
    for (int i = 0; i < 3; i++) {
        raw_acc[i] = static_cast<float>(v[i]) / acc_sens;
        gyro_[i] = static_cast<float>(v[i + 3]) / gyr_sens;
    }

    // Suavizado exponencial (la primera muestra inicializa el filtro).
    for (int i = 0; i < 3; i++) {
        acc_[i] = filtered_ ? (acc_[i] + kEmaAlpha * (raw_acc[i] - acc_[i])) : raw_acc[i];
    }
    filtered_ = true;

    // Tilt con la gravedad medida en -Z (CALIBRACION_MOVIMIENTO.md §5: en
    // reposo az ≈ -1.10 g; usar -az como referencia evita la saturación ±180°
    // que provocaba el viejo atan2(ax, az) con az < 0). Signos medidos:
    // ax- = izquierda, ax+ = derecha, ay- = adelante, ay+ = atrás.
    constexpr float kRad2Deg = 57.2957795f;
    const float roll = std::atan2(acc_[0], -acc_[2]) * kRad2Deg;
    const float pitch = std::atan2(acc_[1] - kAccelAy0, -acc_[2]) * kRad2Deg;
    tilt_x_ = Clampf(roll / kTiltFullRangeDeg, -1.0f, 1.0f) * kTiltXSign;
    tilt_y_ = Clampf(pitch / kTiltFullRangeDeg, -1.0f, 1.0f) * kTiltYSign;

    // Shake sobre la norma RAW (kShakeThresholdG): el impulso rápido no
    // sobrevive al EMA. Periodo refractario anti-rebote.
    const float norm_raw = std::sqrt(raw_acc[0] * raw_acc[0] + raw_acc[1] * raw_acc[1] +
                                     raw_acc[2] * raw_acc[2]);
    const uint32_t now = lv_tick_get();
    if (std::fabs(norm_raw - 1.0f) >= kShakeThresholdG &&
        now - last_shake_tick_ > kShakeRefractoryMs) {
        shake_pending_ = true;
        last_shake_tick_ = now;
    }
}

void HorneritoMotion::Accel(float& x, float& y, float& z) const {
    x = acc_[0];
    y = acc_[1];
    z = acc_[2];
}

void HorneritoMotion::Gyro(float& x, float& y, float& z) const {
    x = gyro_[0];
    y = gyro_[1];
    z = gyro_[2];
}

bool HorneritoMotion::ConsumeShake() {
    bool pending = shake_pending_;
    shake_pending_ = false;
    return pending;
}
