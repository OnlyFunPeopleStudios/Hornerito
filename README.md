# Hornerito 🦜

**Asistente personal físico** con IA, construido sobre una placa **ESP32-C6 con pantalla AMOLED táctil de 2.16"**. Tiene personalidad, voz, pantalla, música, juegos y calendario — no es "un ESP32 con ChatGPT", es un objeto con el que convive en el escritorio.

**Versión actual:** `2.3.0`

## Qué hace

- 🎙️ **Asistente de voz** — conversación con IA por MQTT sobre el framework [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32), con TTS y wake word detectada on-device.
- 🎵 **Música** — reproductor MP3 desde microSD: cola de reproducción, controles táctiles y remuestreo 44.1 → 48 kHz.
- 📅 **Calendario Google** — sincronización iCal (pull) vinculada por **QR sin backend**: el equipo recibe la URL del calendario por HTTP local (`http://<ip>:8080/pair`) y la gestiona él mismo. Todo el flujo en [docs/vinculacion-google-calendar.md](docs/vinculacion-google-calendar.md).
- 🎮 **Juegos** — juegos táctiles en pantalla.
- 🔔 **Alertas y notificaciones** — avisos visuales + voz.

## Hardware

| | |
|---|---|
| Placa | Waveshare ESP32-C6 Touch AMOLED 2.16" |
| SoC | ESP32-C6 RISC-V single-core, 512 KB SRAM, **sin PSRAM** |
| Flash | 16 MB |
| Audio | Micrófono ES7210 + salida I2S |
| Red | Wi-Fi 2.4 GHz |

La falta de PSRAM es la restricción que define gran parte del firmware: todo (decoder MP3 + remuestreador + UI + red) convive con ~40 KB de heap libre.

## Compilar y flashear

Requisitos: [ESP-IDF v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/get-started/index.html) + Python.

```bash
cd Firm_HorneritoV1
idf.py set-target esp32c6
idf.py build
idf.py -p COM4 flash
```

> Los componentes (`managed_components/`, ~590 MB) **no van en el repo**: el Component Manager los descarga solos al compilar a partir de `dependencies.lock`.

Logs de serie útiles durante desarrollo:

```bash
python tools/hornerito_log.py --port COM4 --output logs/mi-prueba.md
```

## Estructura del repo

```
Firm_HorneritoV1/     firmware ESP-IDF
  hornerito/          código propio: música, calendario, QR, UI
  main/               app base (xiaozhi) + boards
docs/                 PRD, fases de reutilización, decisiones de arquitectura
tools/                utilidades de desarrollo (logger serie)
01_Arte/  Hornerito SVG/   arte y fuentes del personaje
xiaozhi-esp32/        submódulo: framework base (upstream 78/xiaozhi-esp32)
```

## Documentación

- [PRD — visión, funcionalidades y roadmap](docs/PRD%20-%20Hornerito.MD)
- [Fase 0 — reutilización del firmware](docs/HORNERITO_REUTILIZACION_FASE0.md)
- [Fase 0.5 — reutilización](docs/HORNERITO_REUTILIZACION_FASE05.md)
- [Vinculación con Google Calendar (QR + iCal)](docs/vinculacion-google-calendar.md)

## Estado conocido

- La calidad de la música está en mejora continua (objetivo: superar el "sonido de lata" del remuestreo agresivo).
- El endpoint del asistente (`api.tenclass.net`) es externo; los errores 5xx no son del equipo.

## Créditos

Basado en [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) de 78 — gracias por abrir el proyecto.

Hecho con ❤️ por **OnlyFunPeopleStudios**.
