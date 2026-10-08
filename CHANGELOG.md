# Changelog

## [2.3.0] — 2026-10-07

### Added

- **Vinculación de calendario Google por QR + iCal** sin backend permanente: el equipo expone `http://<ip>:8080/pair`, recibe la URL del calendario y hace sync iCal directamente (parser iCal propio, configuración manual iCal y sync conservados).
- Rejilla de diagnóstico de audio: sondeo de la cadena de memoria del decoder MP3 en la primera reproducción (`Sondeo cadena mp3`), con telemetría de heap en cada etapa.

### Fixed

- **`Fail to init MP3 decoder ret 10` (música muda)** — la cadena de ~28 kB que el decoder pvmp3 monta en su primer decode no cabía en el heap fragmentado. Fixes: downmix a mono *in situ* en el buffer del frame (sin buffer intermedio), remuestreo de salida por trozos de 256 muestras (el remuestreador se calienta con la ventana `hold` puesta) y precarga del remuestreador 44100→48000 antes de abrir el stream.
- Entrada y salida de audio a **48000 Hz** (antes 44100→24000 recortaba la banda de 12–22 kHz y plegaba los agudos).

## [2.2.x]

- Firmware base sobre xiaozhi-esp32: asistente de voz (MQTT + wake word + TTS), música desde microSD, juegos táctiles y pantalla AMOLED.
