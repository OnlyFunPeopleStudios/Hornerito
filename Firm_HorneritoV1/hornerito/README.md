# hornerito/

Código propio de HORNERITO (V2.1), compilado por el componente `main` (ver
`main/CMakeLists.txt`: GLOB de `hornerito/*.cc` y `hornerito/*.c`).

- `ui/` — pantallas propias: principal con mascota y menú (`hornerito_ui.*`),
  música (`hornerito_music_ui.*`), archivos (`hornerito_files_ui.*`).
- `audio/` — reproductor de música: microSD (SDSPI sobre el bus del display)
  + MP3 (`esp_audio_codec`) con remuestreo (`esp_ae_rate_cvt`). Expone el
  acceso exclusivo (`BeginTransfer`/`EndTransfer`) usado por la transferencia.
- `transfer/` — servidor HTTP (`esp_http_server`) que recibe archivos desde el
  navegador de la PC y los escribe en `/sdcard/Music/` con flush/sync.
  El ESP32-C6 no tiene USB-OTG, por eso no hay USB MSC (ver cabecera del módulo).
- `assets/emojis/generated/` — arrays LVGL finales generados por
  `scripts/assets/gen_emojis.py` (no editar a mano; no dependen de `01_Arte`
  en build ni en runtime).
- `hornerito_emojis.*` — mapa de estados (IDLE/LISTENING/THINKING/SPEAKING/ERROR).

Nota: si se regeneran o añaden imágenes, relanzar el build para que CMake
vuelva a hacer el GLOB de fuentes.
