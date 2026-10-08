# HORNERITO V2.1

Firmware propio de **HORNERITO**. Proyecto ESP-IDF independiente.

- Versión: **2.1.0** (`PROJECT_VER` en `CMakeLists.txt`; visible en el binario
  y en la pantalla Estado del propio dispositivo)
- Placa: Waveshare ESP32-C6 Touch AMOLED 2.16" (`esp32c6`)
- SDK: ESP-IDF v6.1
- Base reutilizada: `78/xiaozhi-esp32` (MIT), copiada e integrada como código propio.

`02_xiaozhi-v2.2.5.bin` es el firmware funcional de referencia (backup). No tocar.

## Build

```powershell
. 'C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1'
$env:PYTHONUTF8=1
idf.py build
```

Firmware generado: `build\hornerito.bin`.

## Qué incluye V2.1

- Identidad **HORNERITO V2.1** (texto de arranque, título de pantalla
  principal, pantalla Estado con la versión del binario). Sin referencias
  visibles a XiaoZhi (SSID `Hornerito-*`, menús y textos propios);
  `CUSTOM_WAKE_WORD_DISPLAY` por defecto "Hola" (inactivo en C6).
- **Pantalla Archivos**: estado USB, microSD, red (URL web), transferencia en
  curso, archivo actual y progreso.
- **Transferencia PC → microSD**: el ESP32-C6 NO tiene USB-OTG (solo USB
  Serial/JTAG), por lo que USB MSC no es viable; se usa un servidor HTTP en el
  propio dispositivo: desde el navegador se sube a `/sdcard/Music/`
  (PUT en streaming con progreso). Pausa segura del reproductor durante la
  escritura, `fflush`+`fsync`+`fclose`, rescaneo de la lista al terminar.
- Menú principal en rejilla 2x2: Asistente / Música / Archivos / Estado.

## Calendario (agenda + Google Calendar)

- **Agenda** HOY/MAÑANA/PRÓXIMOS desde un feed iCal, con caché offline en NVS y
  sincronización periódica. La URL es secreta: nunca se imprime en logs.
- **Vinculación de Google Calendar sin backend**: *Configuración → Calendario →
  Vincular Google Calendar* → QR en pantalla → el celular (misma Wi-Fi) abre la
  página del propio equipo (`http://<ip>:8080/pair`), que guía el copiado de la
  **dirección secreta iCal** de Google Calendar y la envía directo al equipo.
  Sin OAuth, sin tokens, sin servidores de terceros. Respaldo manual:
  *Configurar iCal* (teclado del equipo).
- Documentación (arquitectura, investigación OAuth, desmontaje FASE 0.5,
  pendientes del dueño): [`docs/vinculacion-google-calendar.md`](../docs/vinculacion-google-calendar.md).

## Qué incluye V2 (heredado)

- **Emojis de estado** (IDLE/LISTENING/THINKING/SPEAKING/ERROR) integrados
  desde `01_Arte` como imágenes LVGL propias:
  `python scripts/assets/gen_emojis.py` →
  `01_Arte` → `work/assets` (intermedios) → `hornerito/assets/emojis/generated`.
- **Pantalla principal con mascota y menú**, estilo launcher de la firmware de
  fábrica Waveshare, sobre LVGL.
- **Reproductor de música v1**: lee `.mp3` de la microSD (SDSPI en el bus del
  display, CS=GPIO6), lista, selección por touch, play/pausa, anterior/siguiente,
  nombre de canción y estado. Implementado en `hornerito/audio/` con
  `esp_audio_codec` + `esp_ae_rate_cvt` sobre el AudioCodec existente.

Limpieza de intermedios (solo tras verificar): `scripts/assets/clean_intermediate_assets.ps1`.

## Estructura

```text
Firm_HorneritoV1/
├── CMakeLists.txt              ← proyecto hornerito (PROJECT_VER = 2.1.0)
├── sdkconfig / sdkconfig.defaults[.esp32c6]
├── partitions/                 ← tablas de particiones (v1/v2)
├── scripts/                    ← assets (gen_emojis.py, limpieza), lenguaje...
├── managed_components/         ← dependencias (component manager)
├── main/                       ← base XiaoZhi: app, boards, audio, protocols, display, mcp
│   └── boards/waveshare/esp32-c6-touch-amoled-2.16/   ← board activo
├── hornerito/                  ← código propio de HORNERITO
│   ├── ui/                     ← principal, música, archivos, estado
│   ├── audio/                  ← reproductor de música (microSD + MP3)
│   ├── transfer/               ← servidor HTTP de transferencia a /sdcard/Music
│   ├── assets/emojis/generated ← imágenes LVGL generadas (finales)
│   └── hornerito_emojis.*      ← mapa de estados -> emoji
├── work/assets/                ← intermedios de assets (limpiables con el script)
└── 02_xiaozhi-v2.2.5.bin       ← referencia/backup (no modificar)
```

## Estado

V2.1 compilado: identidad, pantalla Archivos y transferencia HTTP por Wi-Fi.
Pendiente de verificación en dispositivo (no se flashea desde aquí): montaje
real de la microSD, subida desde un navegador y reproducción.
USB MSC: NO VIABLE en ESP32-C6 (sin controlador USB-OTG).
