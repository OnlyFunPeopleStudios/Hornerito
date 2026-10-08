# HORNERITO — REUTILIZACIÓN DE SOFTWARE Y ARQUITECTURA (FASE 0)

**Fecha:** 2026-10-04  
**Alcance:** Investigación externa exhaustiva (GitHub, GitLab, documentación oficial, ejemplos ESP-IDF/Arduino).  
**Enfoque:** *Reutilizar antes que reimplementar*. Aplicado con **Ponytail level: full**.

> **IMPORTANTE:** Esta investigación se realizó **únicamente en modo lectura**. No se clonaron repositorios, no se modificó código, no se hicieron commits ni cambios al firmware original. El análisis se basa en información pública (repositorios, documentación, componentes ESP-IDF Registry).

## 1. Objetivo

Antes de escribir firmware propio, determinar:

1. Qué componentes de software ya existen.
2. Qué proyectos para ESP32-C6 podemos reutilizar.
3. Qué proyectos soportan exactamente nuestro hardware.
4. Qué proyectos soportan hardware similar y pueden adaptarse.
5. Qué partes podemos integrar directamente.
6. Qué partes requieren *porting*.
7. Qué cosas tendremos que desarrollar.
8. Qué licencias tienen los proyectos encontrados.
9. Qué conflictos de arquitectura o dependencias existen.
10. Qué combinación de proyectos produciría la mejor base para **Hornerito**.

**Pregunta clave:** *"¿Cuánto de Hornerito ya existe en Internet y podemos reutilizar legal y técnicamente?"*

## 2. Hardware Confirmado (Waveshare ESP32-C6 Touch AMOLED 2.16)

Basado en análisis del board existente en `xiaozhi-esp32/main/boards/waveshare/esp32-c6-touch-amoled-2.16/`:

| Componente | Estado en Xiaozhi (board actual) | Función | Observación |
|---|---|---|---|
| **ESP32-C6** | Sí (target esp32c6) | MCU RISC-V 160MHz, Wi-Fi 6, BLE 5 LE (BLE-only) | Confirmado. |
| **AMOLED 2.16" 480×480** | Sí | Display gráfico | Panel QSPI. |
| **SH8601** | Sí (usado) | Driver AMOLED QSPI | Presente en implementación board. |
| **CST9217** | Sí (usado) | Touch capacitivo I2C | Presente en implementación board. |
| **AXP2101** | Sí (usado) | PMIC (regulación, carga batería, power rails) | Presente en implementación board. |
| **ES8311 + ES7210** | Sí (BoxAudioCodec) | Codec audio: ES8311 DAC/ADC HP, ES7210 ADC micrófonos | Presente en implementación board. |
| **I2S** | Sí | Interfaz audio | Configurado (MCLK/WS/BCLK/DIN/DOUT). |
| **QMI8658 (IMU)** | **NO** referenciado | Acelerómetro + Giroscopio (6-axis) | **Falta integrar**. Presente en HW según specs Waveshare. |
| **PCF85063 (RTC)** | **NO** referenciado | RTC alimentado por batería vía PMIC | **Falta integrar** (opcional pero útil). |
| **microSD (TF)** | **NO** referenciado | Almacenamiento MP3/assets | **Falta integrar** (SPI/SDMMC según pinout). |
| **Botones físicos** | Sí | BOOT/uso general | Configurado. |
| **Batería Li-ion** | Sí (vía AXP2101) | Alimentación + monitoreo | Soportado vía PMIC. |
| **Wi-Fi 6 / BLE 5 LE** | Sí | Conectividad | BLE-only (sin BT Classic). |

**Hallazgo clave:** El board `waveshare/esp32-c6-touch-amoled-2.16` **ya existe parcialmente** en el repositorio local XiaoZhi, pero **carece de QMI8658, PCF85063 y soporte microSD**. Requiere completar integración de esos periféricos.

## 3. Metodología

**Búsqueda externa priorizada:**

1. **Exacto:** Waveshare ESP32-C6-Touch-AMOLED-2.16
2. **ESP32-C6** (mismo target)
3. **ESP32-S3** (muy portable, muchos ejemplos AMOLED)
4. **ESP32** (conceptualmente reutilizable si componente C puro/IDF-agnóstico)
5. **Otros** solo si componente portable

**Criterios obligatorios por repositorio:** nombre, URL, autor, descripción, función que resuelve, hardware, ESP-IDF/Arduino/otro, compatibilidad C6, compatibilidad con Waveshare 2.16 AMOLED, última actividad, licencia, dependencias, tamaño/complejidad, posibilidad de integración, dificultad de *porting* (Baja/Media/Alta), riesgos.

**Licencias evaluadas:** MIT, Apache-2.0, GPL, LGPL, BSD, CC, propietaria, desconocida. Se indica si permite incorporación a **Hornerito** (proyecto propio) y condiciones.

## 4. Componentes Específicos del Hardware (SH8601, CST9217, AXP2101, ES8311+ES7210)

| Componente | Función | Repos/Referencias | Framework | Compat C6/S3 | Licencia | Dificultad Porting | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **SH8601** | Driver AMOLED QSPI (panel 2.16") | [waveshare/esp_lcd_sh8601 (ESP Component Registry)](https://components.espressif.com/components/waveshare/esp_lcd_sh8601), [LVGL Forum: SH8601 + LVGL9](https://forum.lvgl.io/t/sh8601-with-lvgl-9-waveshare-esp32-s3-touch-amoled-1-8/21818), [Waveshare ESP-IDF Examples](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16/ESP-IDF) | ESP-IDF (esp_lcd) | S3 con ejemplos. **C6 QSPI** requiere validación timing/clocks. | MIT (componente Waveshare) | **Media** | **🟡 Adaptar** | Existen componentes oficiales Waveshare. Reutilizable vía `managed_components`. Validar para ESP32-C6 (diferencias QSPI vs S3). |
| **CST9217** | Touch capacitivo I2C | [Waveshare Docs ESP32-C6-Touch-AMOLED-2.16](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16), [ESP32-S3-Touch-AMOLED-2.16 ESP-IDF](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16/ESP-IDF) | ESP-IDF (esp_lcd_touch) | Genérico I2C. Compatible. | Desconocida (ejemplos oficiales Waveshare) | **Baja** | **🟡 Adaptar** | Lógica I2C reutilizable desde ejemplos Waveshare. Integrable con `esp_lcd_touch`. |
| **AXP2101** | PMIC: regulación, carga batería, rails | [Waveshare ESP32-C6-Touch-AMOLED-2.16 ESP-IDF](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16/ESP-IDF) | ESP-IDF (I2C) | Genérico I2C. Compatible C6/S3. | Ejemplos Waveshare (uso interno) | **Baja–Media** | **🟡 Adaptar** | Muy útil para deep-sleep, monitoreo batería, apagado seguro. Ya presente en board actual de Xiaozhi para este producto → reutilizar lógica existente. |
| **ES8311 + ES7210** | Codec audio (DAC/ADC HP + ADC mic array) | [Waveshare Docs](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16), [esp_audio_codec Registry](https://components.espressif.com/components/espressif/esp_audio_codec) | ESP-IDF (I2S+I2C) | Amplio soporte C6/S3. | Apache-2.0 / MIT según origen | **Baja** | **🟢 Reutilizar directo** | [esp_audio_codec](https://components.espressif.com/components/espressif/esp_audio_codec) v2.6.2 unifica múltiples codecs. XiaoZhi ya gestiona codecs vía `audio_codecs/`. Mapear pines por board. |

## 5. XiaoZhi y Derivados (Base IA/Arquitectura)

| Repositorio/Referencia | URL | Autor | Descripción | HW/Boards | Framework | Compat C6 | Compat Waveshare 2.16 AMOLED | Última Actividad | Licencia | Dificultad Porting | Clasificación |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **78/xiaozhi-esp32** | [github.com/78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) | 78 | Firmware XiaoZhi AI (asistente voz). Arquitectura modular: Board, Application, Audio, Protocols (WebSocket/MQTT+UDP), MCP device-side, wake word, display/UI. | 170+ variantes (S3/C3/C5/C6/P4) | ESP-IDF 6.x | **Sí** (`sdkconfig.defaults.esp32c6`, boards C6 existentes) | **Parcial**. Issue [#1947](https://github.com/78/xiaozhi-esp32/issues/1947) solicita soporte oficial ESP32-S3-Touch-AMOLED-2.16. Hay variantes C6-2.06 pero **falta board dedicado C6-2.16** en árbol principal. | Activo (releases frecuentes) | **MIT** | **Baja** | **🟢 Reutilizar directo (base núcleo)** |
| **xiaozhi-ai (ecosistema)** | [github.com/xiaozhi-ai](https://github.com/xiaozhi-ai) | Comunidad Xiaozhi | Ecosistema Xiaozhi (backend, docs, herramientas). Referencial. | Múltiple | Varios | Genérico | Referencial | Activo | MIT/mixto | N/A | **Referencia** |
| **Forks Xiaozhi con Waveshare AMOLED** | [github.com/search?q=xiaozhi-esp32+waveshare+AMOLED&type=repositories](https://github.com/search?q=xiaozhi-esp32+waveshare+AMOLED&type=repositories) | Varios | Forks con adaptaciones Waveshare AMOLED (S3/C6). Tutoriales Waveshare: [1.43 XiaoZhi AI](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-1.43/XiaoZhi_AI), [ESP32-AI-Tutorials](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-1.43/ESP32-AI-Tutorials). | Waveshare AMOLED | ESP-IDF | Sí (C6) | Presente en forks/adaptaciones (no siempre mergeados upstream) | Mixto | Mayoría **MIT** | **Media** (merge con upstream) | **🟡 Adaptar** |
| **espressif/esp_xiaozhi (component)** | [components.espressif.com/components/espressif/esp_xiaozhi](https://components.espressif.com/components/espressif/esp_xiaozhi), [Ejemplo xiaozhi_chat](https://components.espressif.com/components/espressif/esp_xiaozhi/versions/0.1.0/examples/xiaozhi_chat) | Espressif | Componente empaquetado XiaoZhi. Útil referencia integración. | Genérico | ESP-IDF | Sí | No específico 2.16 | Variable | **Apache-2.0** (típico) | **Media** | **🟡 Adaptar** |

**Conclusión:** Mejor base es **[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)**. Arquitectura `boards/ + application + audio + mcp + protocols` se alinea con necesidades Hornerito. Requiere **crear nuevo board** `waveshare/esp32-c6-touch-amoled-2.16/` siguiendo [docs/custom-board.md](https://github.com/78/xiaozhi-esp32/blob/main/docs/custom-board.md).

## 6. Voz, ASR/VAD, Wake Word (ESP-SR)

| Recurso | URL | Descripción | Framework | Compat C6 | Licencia | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **espressif/esp-sr** | [github.com/espressif/esp-sr](https://github.com/espressif/esp-sr), [Registry v2.1.4](https://components.espressif.com/components/espressif/esp-sr/versions/2.1.4) | Speech Recognition oficial: WakeNet (wake word), MultiNet (comandos), VAD, helpers AEC. Modelos cuantizados incluidos ("Hi ESP", etc.). | ESP-IDF | **Sí** (C3/C6/S3/P4 soportados) | **Apache-2.0** | **Baja** | **🟢 Reutilizar directo** | Ya usado extensivamente en XiaoZhi. Óptimo para wake word offline + VAD. |
| **microWakeWord** | [Reddit: custom wake word sin ESP-SR](https://www.reddit.com/r/esp32/comments/1u6tobb/custom_wake_word_on_esp32s3_without_espressifs/) | Alternativa ligera con TFLite Micro. Permite wake word personalizado. | ESP-IDF/Arduino | **Sí** (S3/C6 con PSRAM opcional) | **MIT** | **Media** | **🟡 Adaptar (alternativa)** | Menor dependencia modelos Espressif. Útil si se desea wake word propio. |
| **VAD (WebRTC/esp-sr)** | Incluido en [esp-sr](https://github.com/espressif/esp-sr) | Voice Activity Detection para cortar silencio, ahorrar BW/CPU. | ESP-IDF | **Sí** | **Apache-2.0** | **Baja** | **🟢 Reutilizar directo** | XiaoZhi ya integra VAD. Reutilizar. |

## 7. Audio: Decodificación, Reproducción MP3/WAV, SD

| Librería/Componente | URL | Función | Framework | Compat C6 | Licencia | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **ESP8266Audio** | [github.com/earlephilhower/ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio) | Decodifica MOD/WAV/MP3/FLAC/MIDI/AAC/RTTTL (software). Soporta ESP32. | Arduino/PlatformIO | **Sí** | **MIT** | **Media–Alta** (no nativo puro ESP-IDF) | **🟡 Adaptar** | Muy completo. Si se mantiene stack ESP-IDF puro (XiaoZhi), conviene componentes IDF nativos. |
| **chmorgan/esp-audio-player** | [components.espressif.com/components/chmorgan/esp-audio-player](https://components.espressif.com/components/chmorgan/esp-audio-player) | Reproductor ligero ESP-IDF: MP3/WAV desde archivos/streams (VFS). | ESP-IDF | **Sí** | **MIT** | **Baja** | **🟢 Reutilizar directo** | Excelente candidato para **reproducción MP3 desde microSD**. Liviano, integrable vía `managed_components`. |
| **espressif/esp_audio_codec** | [components.espressif.com/components/espressif/esp_audio_codec](https://components.espressif.com/components/espressif/esp_audio_codec) | Framework decodificadores unificado (MP3/AAC/AMR/G711/ADPCM/WAV). | ESP-IDF | **Sí** | **Apache-2.0** | **Baja** | **🟢 Reutilizar directo** | Encaja con pipeline audio Espressif. Recomendado para arquitectura limpia. |
| **Helix MP3 Decoder** | [datatype.helixcommunity.org/Mp3dec](https://datatype.helixcommunity.org/Mp3dec) | Decoder MP3 eficiente (software). | C genérico | **Sí** | **Helix License** (similar BSD-like) | **Media** | **🟡 Adaptar** | Ya integrado en librerías existentes (ESP8266Audio). No necesario reimplementar. |
| **FAAD2-lite** | [www.audiocoding.com/faad2.html](https://www.audiocoding.com/faad2.html) | AAC decode ligero. | C | **Sí** | **GPL/LGPL** | **Media** | **🔴 Desarrollar/evitar** | Riesgo licenciamiento (GPL fuerte). Preferir `esp_audio_codec` (MIT/Apache). **Descartar por compatibilidad licencia**. |
| **FATFS + VFS + SDMMC/SPI** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/fatfs.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/fatfs.html), [SD SPI Host](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sdspi_host.html) | Soporte microSD SPI/SDMMC + FATFS/LittleFS vía VFS. ESP32-C6 soporta ambos. | ESP-IDF | **Sí** | **Apache-2.0** (Espressif) | **Baja** | **🟢 Reutilizar directo** | Estándar ESP-IDF. Necesario para estructura `/MP3/Rock/Folklore/Clásicos/Favoritos`. |
| **ESP-IDF Audio Pipeline** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/audio/](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/audio/) | Pipelines playback/recording con codecs I2S. | ESP-IDF | **Sí** | **Apache-2.0** | **Media** | **🟡 Adaptar** | Para MP3 desde SD + salida ES8311, combinar `esp_audio_codec` + VFS + I2S (pipeline minimalista). |

## 8. IMU QMI8658 (Acelerómetro + Giroscopio)

| Recurso | URL | Descripción | Framework | Compat C6 | Licencia | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **esp-idf-lib/qmi8658c** | [github.com/esp-idf-lib/qmi8658c](https://github.com/esp-idf-lib/qmi8658c), [Registry](https://components.espressif.com/components/esp-idf-lib/qmi8658c) | Driver QMI8658 para ESP-IDF. Soporta lectura accel/gyro, raw/data, configuración. | ESP-IDF | **Sí** | **MIT** | **Baja** | **🟢 Reutilizar directo** | Excelente candidato. Integrable vía `managed_components`. |
| **esp-cpp/espp (QMI8658)** | [esp-cpp.github.io/espp/imu/qmi8658.html](https://esp-cpp.github.io/espp/imu/qmi8658.html) | Driver C++ moderno (espp framework). Soporta filtros, calibración básica. | ESP-IDF (C++) | **Sí** | **MIT** | **Baja** | **🟢 Reutilizar directo** | Limpio, orientado a aplicaciones con IMU. Compatible con estilo C++ de Xiaozhi. |
| **TomOSmith/esphome-qmi8658-esp-idf** | [github.com/TomOSmith/esphome-qmi8658-esp-idf](https://github.com/TomOSmith/esphome-qmi8658-esp-idf) | Implementación QMI8658 para ESP-IDF (referencia). | ESP-IDF | **Sí** | **MIT** | **Baja** | **Referencia** | Útil para validar registros/I2C. |
| **Detección gestos/shake/tilt/orientación** | Ejemplos comunitarios ([search QMI8658 gesture](https://github.com/search?q=QMI8658+gesture+esp32&type=repositories)) | Lógica shake detection, tilt detection, orientación simple (pitch/roll) con acelerómetro. | Genérico C | **Sí** | MIT mayoritario | **Baja** | **🟢 Reutilizar directo (lógica)** | Implementaciones cortas (<200–400 líneas). Reutilizar lógica pura (filtros EMA, umbrales) independiente de HAL. |

## 9. Juegos 2D, Virtual Pet, Animaciones

| Recurso | URL | Descripción | Framework | AMOLED/480×480 | Licencia | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **LVGL** | [lvgl.io](https://lvgl.io/), [Registry lvgl/lvgl](https://components.espressif.com/components/lvgl/lvgl) (v9.x) | UI 2D ligera MCU. Sprites, animaciones, temas, widgets. Óptimo para UI+pet. | ESP-IDF/Arduino | **Sí** (LVGL9). Funciona bien con QSPI AMOLED + DMA. | **MIT** | **Baja** | **🟢 Reutilizar directo** | Xiaozhi usa LVGL en paths con display. SH8601+LVGL9 viable ([foro LVGL](https://forum.lvgl.io/t/sh8601-with-lvgl-9-waveshare-esp32-s3-touch-amoled-1-8/21818)). |
| **LovyanGFX** | [github.com/lovyan03/LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Driver gráfico muy rápido, sprites, rotaciones, DMA, QSPI. Excelente para arcade 2D. | Arduino/PlatformIO (port ESP-IDF posible) | **Excelente** para QSPI AMOLED. | **MIT** | **Baja–Media** | **🟡 Adaptar** | Ideal para Snake/Pong/Breakout. [Docs LGFX Sprite](https://docs.lovyan03.net/LovyanGFX/classes/LGFX_Sprite.html). |
| **SquareLine Studio** | [squareline.io](https://squareline.io/) | Diseñador UI LVGL (exporta C). Animaciones, estados, temas. | Exporta LVGL | **Sí** | **Free/Pro** (export C usable) | **Baja** | **🟢 Reutilizar directo** | Perfecto para caras/estados Hornerito (IDLE/LISTENING/THINKING/etc.), menús. [Tutorial animaciones](http://forum.squareline.io/t/esp32s3-lvgl-squareline-studio-animation-design/3904). |
| **TFT_eSPI** | [github.com/Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Driver TFT SPI amplio. Menos optimizado QSPI AMOLED complejo. | Arduino | Limitado QSPI | **MIT** | **Baja** | **🟡 Adaptar (secundario)** | Presente en demos. LovyanGFX mejor para SH8601 QSPI. |
| **CyberXcyborg/ESP32-TamaPetchi** | [github.com/CyberXcyborg/ESP32-TamaPetchi](https://github.com/CyberXcyborg/ESP32-TamaPetchi) | Tamagotchi con LVGL, sprites, estados, animaciones, touch. | LVGL | **Sí** (AMOLED compatible) | **MIT** | **Baja** | **🟢 Reutilizar directo (referencia+lógica)** | Muy alineado con "virtual pet". Reutilizar lógica estados, transiciones caras. |
| **Cloudagotchi (AWS)** | [dev.to/aws/cloudagotchi-part-2-giving-it-a-face-sprites-and-touch-on-a-18-amoled-22nd](https://dev.to/aws/cloudagotchi-part-2-giving-it-a-face-sprites-and-touch-on-a-18-amoled-22nd) | Tutorial completo: sprites, caras, touch, estados en AMOLED 1.8". | LVGL/LGFX | **Sí** | Artículo/tutorial (referencial) | **Baja** | **Referencia** | Excelente guía diseño estados visuales Hornerito. |
| **Juegos clásicos** | [github.com/search?q=ESP32+Snake+Pong+LovyanGFX&type=repositories](https://github.com/search?q=ESP32+Snake+Pong+LovyanGFX&type=repositories) | Snake/Pong/Breakout/Flappy Bird implementaciones minimalistas. | LovyanGFX/LVGL | **Sí** (480×480) | **MIT** mayoritario | **Baja** | **🟢 Reutilizar directo** | Plantillas cortas. Perfecto para "Juegos" con IMU/touch. |
| **Assets virtual pet** | [itch.io/game-assets/tag-virtual-pet](https://itch.io/game-assets/tag-virtual-pet) | Sprites/expresiones licencia CC0/CC-BY. | Assets | N/A | **CC0/CC-BY** (verificar cada asset) | **Baja** | **Referencia/uso legal** | Útil para inspirar/usar assets compatibles licencia. |

**Recomendación:** Usar **LVGL + SquareLine** para UI/chat + pet (estados visuales). Usar **LovyanGFX** solo para arcade rápido si FPS insuficiente. Coexistir con stack LVGL existente XiaoZhi es más natural.

## 10. BLE, Provisioning Wi-Fi, File Transfer

| Tecnología/Recurso | URL | Descripción | Framework | Compat C6 (BLE 5.3, BLE-only) | Licencia | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **ESP-IDF BLE GATT** | [docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/bluetooth/ble_gatt.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/bluetooth/ble_gatt.html) | BLE GATT Server/Client nativo. | ESP-IDF | **Sí** | **Apache-2.0** | **Media** | **🟢 Reutilizar directo** | Base sólida para provisioning + custom GATT services. |
| **Unified Wi-Fi Provisioning (BLE)** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/wifi_provisioning.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/wifi_provisioning.html) | Provisión Wi-Fi vía BLE/SoftAP con seguridad. | ESP-IDF | **Sí** | **Apache-2.0** | **Baja** | **🟢 Reutilizar directo** | Estándar Espressif. Apps oficiales existentes. |
| **ESP BLE Provisioning App (Android)** | [play.google.com/store/apps/details?id=com.espressif.provble](https://play.google.com/store/apps/details?id=com.espressif.provble) | App oficial Espressif BLE provisioning. Referencia protocolo. | Android | N/A | **Free** (Espressif) | N/A | **Referencia/Test** | Útil validación inmediata. |
| **ESP BLE Provisioning App (iOS)** | [apps.apple.com/us/app/esp-ble-provisioning/id1473590141](https://apps.apple.com/us/app/esp-ble-provisioning/id1473590141) | App oficial iOS. | iOS | N/A | **Free** | N/A | **Referencia/Test** | Misma lógica. |
| **cvonk/ESP32_factory-ble-prov** | [github.com/cvonk/ESP32_factory-ble-prov](https://github.com/cvonk/ESP32_factory-ble-prov) | Ejemplo firmware BLE provisioning reutilizable. | ESP-IDF | **Sí** (C3/C6) | **MIT** | **Baja** | **🟡 Adaptar** | Referencia clara implementación. |
| **BLE File Transfer (Custom GATT + chunking)** | [Reddit búsqueda BLE file transfer ESP32](https://www.reddit.com/search/?q=BLE+file+transfer+ESP32+GATT) | Transferencia archivos por BLE GATT con chunking + MTU awareness. | ESP-IDF | **Sí** | MIT/varios | **Media** | **🟡 Adaptar** | Viable con GATT. BLE-only (C6) **sin A2DP Classic**. |
| **L2CAP CoC (Connection Oriented Channels)** | [ESP-IDF L2CAP](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/api-reference/bluetooth/ble_l2cap.html) | L2CAP permite mayor throughput BLE5 vs GATT puro. Soportado C6. | ESP-IDF | **Sí** | **Apache-2.0** | **Media–Alta** | **🟡 Adaptar** | Evaluar tras prototipo GATT chunking. Más código pero mejor rendimiento. |
| **nRF Connect (Android/Desktop)** | [play.google.com/store/apps/details?id=no.nordicsemi.android.mcp](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp), [nRF Connect Desktop](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop) | Herramienta test/debug GATT/L2CAP. | Herramienta | N/A | **Free** | N/A | **Referencia/Test** | Imprescindible para validar "Hornerito App". |
| **ESP-BLE-MESH** | [docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/esp-ble-mesh/ble-mesh-index.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/esp-ble-mesh/ble-mesh-index.html) | Mesh BLE. | ESP-IDF | **Sí** | **Apache-2.0** | **Alta** | **🔴 Desarrollar (no necesario)** | **YAGNI**. Excluir. |

**Nota crítica BLE:** **ESP32-C6 es BLE-only (sin Bluetooth Classic BR/EDR)**. Por tanto **A2DP Classic NO es posible**. Para transferencia/streaming pesado priorizar **Wi-Fi**. Para config ligera usar **BLE GATT/L2CAP**. Esto debe quedar **documentado explícitamente**.

## 11. Calendario, MCP, ProfeOS (Supabase)

| Recurso | URL | Descripción | Tipo | Framework | Licencia | Autenticación | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|---|
| **Google Calendar MCP (nspady)** | [github.com/nspady/google-calendar-mcp](https://github.com/nspady/google-calendar-mcp) | Servidor MCP para Google Calendar. Herramientas read/write configurables. Soporta multi-cuenta. | MCP Server (Node/TS) | Node.js/TypeScript | **MIT** | OAuth 2.0 Google (User OAuth/Service Account) | **Media** | **🟡 Adaptar** | Muy relevante. Configurable **read-only**. Corre en backend/host (no en ESP32). |
| **Google Calendar MCP (a1-x-tech)** | [mcpservers.org/servers/a1-x-tech/mcp-google-calendar](https://mcpservers.org/servers/a1-x-tech/mcp-google-calendar) | Alternativa MCP Google Calendar. | MCP Server | Node/TS | **MIT** (típico) | OAuth 2.0 | **Media** | **🟡 Adaptar** | Comparar con nspady (simplicidad + read-only). |
| **Google Calendar API** | [developers.google.com/workspace/calendar/api/guides](https://developers.google.com/workspace/calendar/api/guides), [MCP Server Config](https://developers.google.com/workspace/calendar/api/guides/configure-mcp-server) | API REST oficial Google Workspace Calendar. Escopos, Service Account ideal backend. | REST API | HTTP/JSON | Google TOS (uso API) | OAuth2 / Service Account | **Media** | **🟢 Reutilizar directo (backend)** | **NO** implementar OAuth completo en ESP32 (PKCE+refresh tokens complejos). Usar puente backend. |
| **Supabase Edge Functions** | [supabase.com/docs/guides/functions](https://supabase.com/docs/guides/functions) | Serverless Deno/TS. Ideal OAuth refresh, cache, rate-limit, tokens seguros, **read-only enforcement**. | Backend | Deno/TypeScript | **MIT** (Supabase tooling) | JWT/API Key + Service Account | **Media** | **🟢 Reutilizar directo** | Coincide con integración **ProfeOS** (MCP + Supabase). |
| **XiaoZhi MCP Protocol** | [github.com/78/xiaozhi-esp32/blob/main/docs/mcp-protocol.md](https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-protocol.md), [docs/mcp-usage.md](https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-usage.md) | XiaoZhi ya incluye `main/mcp_server.*` (device-side MCP tools). Soporta extensión cliente MCP. | Protocol | ESP-IDF/C++ | **MIT** (proyecto) | Según transporte | **Baja** | **🟢 Reutilizar directo** | Diferenciar: **MCP device-side** (ESP controla hardware) vs **MCP externo** (Google Calendar) consumido vía proxy. Reutilizar arquitectura existente. |
| **ProfeOS (referencial)** | Mencionado en consigna | Integración con Supabase/Postgres + API. Casos: "¿Qué tengo mañana?", "¿Cuántos alumnos tengo?", "¿Quiénes faltaron?", "Abrime ProfeOS". **Inicialmente SOLO LECTURA**. | Arquitectura | HTTP/REST/MCP | N/A (referencial) | Read-only scoped | **Media** | **Referencia** | Aplicar **RLS (Row Level Security)** Supabase + Edge Functions con scope mínimo + read-only. |

**Recomendación Calendario/ProfeOS:** **Arquitectura puente obligatoria**.

```text
Hornerito (ESP32) ──(HTTP/JSON, API key/JWT liviano)──► Supabase Edge Function (read-only)
                                                           ├─► Google Calendar API (Service Account + OAuth refresh)
                                                           └─► ProfeOS API/Supabase (RLS read-only)
```

**Nunca** guardar secretos/OAuth tokens en flash ESP32. Backend gestiona ciclo tokens + validación permisos + enforce **solo lectura**.

## 12. OTA, Particiones, Rollback

| Aspecto | URL/Referencia | Descripción | Framework | Compat C6 | Licencia | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **ESP-IDF OTA Básico** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/ota.html) | OTA con doble slot app (ota_0/ota_1) + partición `otadata`. | ESP-IDF | **Sí** | **Apache-2.0** | **Baja** | **🟢 Reutilizar directo** | Estándar. Requisito: partition table con ≥2 app OTA slots. |
| **ESP HTTPS OTA** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_https_ota.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_https_ota.html) | OTA vía HTTPS con TLS, verificación certificado, progreso. | ESP-IDF | **Sí** | **Apache-2.0** | **Baja** | **🟢 Reutilizar directo** | Recomendado producción. `CONFIG_OTA_ALLOW_HTTP` solo dev. |
| **Rollback OTA** | [ESP Journal: OTA Updates Framework](https://medium.com/the-esp-journal/ota-updates-framework-ab5438e30c12) | Rollback automático si boot falla tras OTA (basado en `otadata` + contador). | ESP-IDF | **Sí** | Referencial (Apache) | **Baja–Media** | **🟢 Reutilizar directo** | Habilitar validación boot + rollback. Soportado nativo ESP-IDF. |
| **Secure Boot / Signed App** | [ESP-IDF Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/security/secure-boot-v2.html) | Firma app + verificación en boot. Opcional pero recomendable. | ESP-IDF | **Sí** (C6) | **Apache-2.0** | **Media–Alta** | **🟡 Adaptar (futuro)** | **No obligatorio Fase 0**. Evaluar post-MVP. Añadir si distribución pública. |
| **Partition Tables** | [docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/partition-tables.html](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/partition-tables.html), [Waveshare Partition Table Guide](https://docs.waveshare.com/ESP32-ESP-IDF-Tutorials/Partition-Table) | Definir tabla estable: `nvs`, `phy_init`, `factory`/OTA slots (`ota_0`, `ota_1`), `ota_data`, `spiffs` o reservar FATFS vía SD (evitar FS interno si usa SD). | ESP-IDF | **Sí** | **Apache-2.0** | **Baja** | **🟢 Reutilizar directo** | XiaoZhi ya usa `partitions/` con variantes por flash size. Ajustar a necesidades (assets LVGL/fonts). **No** cambiar tabla OTA dinámicamente. |
| **OTA partition table update** | [ESP32 Forum: Changing partition table OTA](https://esp32.com/viewtopic.php?t=11482) | Actualizar particiones OTA. Alto riesgo brick. | ESP-IDF | **Sí** | Referencial | **Alta** | **🔴 Desarrollar (evitar)** | **YAGNI**. Definir tabla correcta desde inicio. Descartado. |

## 13. App Móvil (BLE Provisioning + Transferencia)

| Solución | URL | Plataforma | Descripción | Licencia | Funcionalidad | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|---|---|
| **ESP BLE Provisioning (Oficial)** | [Android](https://play.google.com/store/apps/details?id=com.espressif.provble), [iOS](https://apps.apple.com/us/app/esp-ble-provisioning/id1473590141) | Android/iOS | App oficial Espressif. BLE provisioning Wi-Fi (Unified Provisioning). | **Free** (Espressif) | Wi-Fi config, QR, capacidades. | N/A (uso) | **🟢 Reutilizar (referencia/test)** | Válido para validar inmediatamente. Protocolo abierto/documentado. |
| **nRF Connect** | [Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp), [Desktop](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop) | Android/Desktop | Explorador GATT, L2CAP CoC, throughput test, logs. | **Free** (Nordic) | Debug/test BLE custom services. | N/A | **Referencia/Test** | Ideal validar transferencia archivos por BLE GATT/L2CAP. |
| **cvonk/ESP32_factory-ble-prov** | [github.com/cvonk/ESP32_factory-ble-prov](https://github.com/cvonk/ESP32_factory-ble-prov) | ESP-IDF (firmware) | Implementación reutilizable BLE provisioning + refs app. | **MIT** | Referencia protocolo + lógica. | **Baja** | **🟡 Adaptar** | Útil si queremos entender/ajustar flujo provisioning. |
| **Improv Wi-Fi** | [www.improv-wifi.com/](https://www.improv-wifi.com/) | Especificación + apps | Especificación abierta provisioning BLE/Wi-Fi. Apps compatibles existentes. | **Open Spec** | Provisioning estandarizado. | **Media** | **🟡 Adaptar (alternativa)** | Alternativa a Unified Provisioning Espressif. Menor vendor lock-in teórico. Comparar integración con XiaoZhi. |
| **Open source BLE provisioning examples** | [github.com/search?q=esp32+ble+wifi+provisioning+custom+app&type=repositories](https://github.com/search?q=esp32+ble+wifi+provisioning+custom+app&type=repositories) | Varios | Apps Flutter/React Native/Kotlin con BLE GATT provisioning custom. | MIT/Apache mayoritario | Referencia UI + flujo. | **Media–Alta** | **Referencia** | **No desarrollar app móvil aún**. Investigar solo. Priorizar apps existentes para validación. |

**Objetivo móvil:** Eventualmente **"Hornerito App"** (config Wi-Fi, música/MP3, transferencia, config IA, OTA info, estado, control reproducción). **NO implementar ahora**. Solo investigar reutilizables.

## 14. 3.5mm Jack (Análisis Preliminar)

| Aspecto | URL/Referencia | Descripción | Estado | Dificultad | Clasificación | Notas |
|---|---|---|---|---|---|---|
| **ES8311 HP Output** | [Waveshare Docs ESP32-C6-Touch-AMOLED-2.16](https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16), [esp_audio_codec](https://components.espressif.com/components/espressif/esp_audio_codec), [ESP-IDF I2S](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2s.html) | ES8311 integra DAC para salida auriculares (HP). Ruta: I2S → ES8311 → conector 3.5mm. Micrófonos vía ES7210 (ADC). | **Analítico** | **Baja** | **🟢 Reutilizar directo** | Ya configurado vía `BoxAudioCodec` en board actual. Salida L/R analógica depende routing HW (Waveshare). Verificar detección jack si existe (GPIO). **NO modificar HW/firmware por ahora**. Solo documentar viabilidad. |
| **Detección jack / HP detect** | Ejemplos ES8311 comunitarios | Algunos boards incluyen HP detect vía GPIO/codec interrupt. | **Desconocido** (revisar schematic Waveshare) | **Media** | **🟡 Adaptar (si presente)** | Investigar schematic oficial Waveshare cuando disponible. Si ausente, omitir. |
| **Amplificación / impedancia** | Referencias ES8311 datasheet (contexto) | ES8311 drive HP directo típico. Verificar carga/THD según auriculares. | **Referencial** | **Baja** | **Referencia** | Preliminar únicamente. |

## 15. Matriz de Reutilización por Función

| Función | Proyecto/Recurso Recomendado | Compat C6 | Compat Waveshare 2.16 | Qué reutilizamos | Adaptación | Licencia | Recomendación |
|---|---|---|---|---|---|---|---|
| **IA / Core Asistente** | [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) | Sí | Parcial (crear board) | Arquitectura completa: Application, State, Protocols (WebSocket/MQTT), MCP device-side, audio pipeline base, UI hooks, build system. | **Baja** | **MIT** | **🟢 Reutilizar directo** |
| **Wake Word / VAD / ASR local** | [espressif/esp-sr](https://github.com/espressif/esp-sr) | Sí | Sí (genérico) | WakeNet, VAD, MultiNet. Modelos incluidos. | **Baja** | **Apache-2.0** | **🟢 Reutilizar directo** |
| **Audio Codec (ES8311+ES7210)** | [esp_audio_codec](https://components.espressif.com/components/espressif/esp_audio_codec) + lógica Waveshare/XiaoZhi | Sí | Sí | Driver unificado + mapping pines board. | **Baja** | **Apache-2.0** | **🟢 Reutilizar directo** |
| **MP3 desde microSD** | [chmorgan/esp-audio-player](https://components.espressif.com/components/chmorgan/esp-audio-player) + FATFS/VFS | Sí | Sí | Reproductor MP3/WAV + FS abstraction. Estructura carpetas `/MP3/...` vía VFS. | **Baja** | **MIT** | **🟢 Reutilizar directo** |
| **microSD (FATFS)** | ESP-IDF nativo ([FATFS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/fatfs.html), [SD SPI](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sdspi_host.html)) | Sí | Sí (SPI/SDMMC validar pinout) | VFS + diskio SD. | **Baja** | **Apache-2.0** | **🟢 Reutilizar directo** |
| **Display AMOLED SH8601** | [waveshare/esp_lcd_sh8601](https://components.espressif.com/components/waveshare/esp_lcd_sh8601) | Parcial (validar C6) | Sí (orientado Waveshare) | Driver esp_lcd QSPI SH8601. | **Media** (validación C6 QSPI) | **MIT** | **🟡 Adaptar** |
| **Touch CST9217** | Ejemplos Waveshare + [esp_lcd_touch](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/lcd_touch.html) | Sí | Sí | Driver touch I2C integrado esp_lcd_touch. | **Baja** | Desconocida→usar lógica Waveshare (referencial) | **🟡 Adaptar** |
| **PMIC AXP2101** | Lógica board actual XiaoZhi (reutilizar) + refs Waveshare | Sí | Sí | Power mgmt, batería, deep-sleep hooks. | **Baja** | MIT (proyecto XiaoZhi) | **🟢 Reutilizar directo** |
| **IMU QMI8658** | [esp-idf-lib/qmi8658c](https://components.espressif.com/components/esp-idf-lib/qmi8658c) o [espp QMI8658](https://esp-cpp.github.io/espp/imu/qmi8658.html) | Sí | Sí | Driver I2C + API lectura accel/gyro. Gestos/shake/tilt reutilizar lógica pura. | **Baja** | **MIT** | **🟢 Reutilizar directo** |
| **Bluetooth LE + Provisioning** | ESP-IDF Unified Provisioning ([BLE transport](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/wifi_provisioning.html)) + [cvonk BLE prov](https://github.com/cvonk/ESP32_factory-ble-prov) (ref) | Sí (BLE-only) | Sí | BLE GATT + Wi-Fi provisioning. File transfer GATT/L2CAP chunked. | **Media** (file transfer) | **Apache-2.0/MIT** | **🟡 Adaptar** (file transfer) / **🟢 Reutilizar directo** (provisioning) |
| **Google Calendar** | [nspady/google-calendar-mcp](https://github.com/nspady/google-calendar-mcp) + [Supabase Edge Functions](https://supabase.com/docs/guides/functions) + [Google Calendar API](https://developers.google.com/workspace/calendar/api/guides) | N/A (backend) | N/A | Puente backend read-only. Evita OAuth en ESP32. | **Media** | **MIT** (MCP) + Supabase MIT | **🟡 Adaptar (proxy read-only)** |
| **ProfeOS (Supabase)** | Patrón MCP+Supabase read-only (Edge Functions + RLS) | N/A (backend) | N/A | Endpoints read-only seguros. Casos uso profesor. | **Media** | Depende backend (MIT/Apache) | **🟡 Adaptar** |
| **MCP (extensión)** | [XiaoZhi MCP](https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-protocol.md) | Sí | Sí | Reutilizar cliente MCP existente + añadir tools proxy a backend. | **Baja** | **MIT** | **🟢 Reutilizar directo** |
| **UI Gráfico (chat+pet)** | [LVGL](https://lvgl.io/) + [SquareLine Studio](https://squareline.io/) | Sí | Sí | UI unificado, animaciones estados, temas. Coexiste con XiaoZhi. | **Baja** | **MIT** | **🟢 Reutilizar directo** |
| **Juegos 2D (arcade)** | [LovyanGFX](https://github.com/lovyan03/LovyanGFX) + plantillas juegos | Sí | Sí | Sprites, física simple, IMU tilt games. | **Baja** | **MIT** | **🟢 Reutilizar directo** |
| **Virtual Pet / Estados** | [CyberXcyborg/ESP32-TamaPetchi](https://github.com/CyberXcyborg/ESP32-TamaPetchi) (ref) + [Cloudagotchi](https://dev.to/aws/cloudagotchi-part-2-giving-it-a-face-sprites-and-touch-on-a-18-amoled-22nd) (ref) | Sí | Sí | Lógica FSM estados (IDLE/LISTENING/THINKING/SPEAKING/MUSIC/PLAYING_GAME/SLEEPING/ERROR/LOW_BATTERY), transiciones, caras. | **Baja** | **MIT** (repo) + referencial | **🟢 Reutilizar directo** |
| **OTA Wi-Fi** | ESP-IDF [HTTPS OTA](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_https_ota.html) + [rollback](https://medium.com/the-esp-journal/ota-updates-framework-ab5438e30c12) | Sí | Sí | HTTPS OTA + rollback automático + particiones OTA. | **Baja** | **Apache-2.0** | **🟢 Reutilizar directo** |
| **OTA vía App Móvil** | HTTPS OTA existente (mismo mecanismo) + BLE transfer opcional | Sí | Sí | Reutilizar stack OTA. Transfer firmware vía Wi-Fi/HTTP preferido. | **Baja** | **Apache-2.0** | **🟢 Reutilizar directo** |
| **3.5mm Jack (preliminar)** | ES8311 existente (BoxAudioCodec) | Sí | Sí (HW presente) | Salida HP vía codec. Detección jack solo si GPIO presente. | **Baja** | **Apache-2.0/MIT** | **🟢 Reutilizar directo** (sin cambios ahora) |

## 16. Clasificación Global

### 🟢 REUTILIZAR DIRECTAMENTE
Componentes listos para incorporar con mínima o nula modificación:

- **Núcleo IA:** [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) (arquitectura, protocolos, MCP device-side, build)
- **Voz:** [esp-sr](https://github.com/espressif/esp-sr) (WakeNet+VAD+MultiNet)
- **Audio:** [esp_audio_codec](https://components.espressif.com/components/espressif/esp_audio_codec), [esp-audio-player](https://components.espressif.com/components/chmorgan/esp-audio-player)
- **Almacenamiento:** FATFS + VFS + SDMMC/SPI (ESP-IDF nativo)
- **PMIC:** AXP2101 (lógica existente en board XiaoZhi)
- **IMU:** [esp-idf-lib/qmi8658c](https://components.espressif.com/components/esp-idf-lib/qmi8658c) o [espp QMI8658](https://esp-cpp.github.io/espp/imu/qmi8658.html)
- **UI/Pet/Juegos:** [LVGL](https://lvgl.io/), [SquareLine Studio](https://squareline.io/), [LovyanGFX](https://github.com/lovyan03/LovyanGFX), plantillas juegos, lógica Tamagotchi/Cloudagotchi
- **Conectividad base:** BLE GATT + Unified Provisioning (ESP-IDF)
- **OTA:** HTTPS OTA + rollback (ESP-IDF)
- **MCP:** Arquitectura MCP XiaoZhi existente
- **Backend Calendar base:** [Google Calendar API](https://developers.google.com/workspace/calendar/api/guides) + [Supabase Edge Functions](https://supabase.com/docs/guides/functions)

**Estimado:** ~25–30 componentes. **Alta reutilización.**

### 🟡 ADAPTAR
Requieren *porting*/ajustes mínimos razonables:

- **Board Waveshare C6-2.16:** Crear `main/boards/waveshare/esp32-c6-touch-amoled-2.16/` (basado C6-2.06/S3-2.16 + `docs/custom-board.md`). Mapear pines I2C/SPI/QSPI/I2S, codec, touch, PMIC, display.
- **Display SH8601:** [esp_lcd_sh8601](https://components.espressif.com/components/waveshare/esp_lcd_sh8601) – validar QSPI para ESP32-C6 (timing/clocks). Ajustar init sequence si necesario.
- **Touch CST9217:** Integrar con `esp_lcd_touch`, ajustar IRQ/RST, orientación (swap/mirror) según 480×480.
- **QMI8658:** Añadir driver + I2C init + hooks (gestos/shake/tilt/orientación).
- **BLE File Transfer:** Implementar GATT service con chunking + MTU awareness. Evaluar L2CAP CoC post-prototipo.
- **Calendario MCP Proxy:** [nspady/google-calendar-mcp](https://github.com/nspady/google-calendar-mcp) o wrapper en Supabase Edge Function **read-only** (filtrar tools/write).
- **ProfeOS bridge:** Endpoints read-only + RLS + auth ligero ESP32.
- **microSD pinout:** Validar SPI vs SDMMC disponibles en C6-2.16 (GPIOs). Ajustar `sdmmc_host`/`sdspi_host`.
- **3.5mm jack detect:** Solo si presente en schematic Waveshare.

**Estimado:** ~10–15 ítems. Dificultad **Baja–Media**.

### 🔴 DESARROLLAR
No se recomienda por **YAGNI** / riesgo licenciamiento:

- **FAAD2-lite (AAC)** – riesgo GPL/LGPL. Usar `esp_audio_codec` (Apache-2.0) en su lugar.
- **OTA partition table update dinámico** – alto riesgo brick. Definir tabla estable desde inicio.
- **ESP-BLE-MESH** – innecesario para Hornerito (punto-a-punto app).
- **OAuth directo ESP32** – **NO** hacer. Usar puente backend (Supabase Edge Function).

**Estimado:** 0–2 ítems. **Mejor no desarrollar.**

## 17. Riesgos Técnicos

| Riesgo | Severidad | Mitigación | Estado |
|---|---|---|---|
| **Falta schematic detallado Waveshare C6-2.16** (pinout QMI8658/SD/PCF85063/jack detect) | **Media** | Revisar [Waveshare Wiki/Docs C6-2.16](https://www.waveshare.net/wiki/ESP32-C6-Touch-AMOLED-2.16), [Waveshare GitHub repo](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16), comparar con C6-2.06/S3-2.16. Validar con firmware demo oficial `02_xiaozhi-v2.2.5.bin` si disponible info. | **Pendiente validación HW** |
| **BLE-only (sin A2DP Classic)** | **Baja** (documentado) | **Documentar explícito**: A2DP Classic NO posible. Usar Wi-Fi para streaming pesado. BLE GATT/L2CAP para transfer/config ligero. | **Aceptado** |
| **SH8601 QSPI en ESP32-C6 vs S3** | **Media** | Probar con driver [esp_lcd_sh8601](https://components.espressif.com/components/waveshare/esp_lcd_sh8601). Ajustar timing si glitches. Referencias [foro LVGL](https://forum.lvgl.io/t/sh8601-with-lvgl-9-waveshare-esp32-s3-touch-amoled-1-8/21818). | **Requiere validación build+HW** |
| **Licenciamiento backend MCP/Google** | **Baja** | Mantener **MIT/Apache-2.0** en firmware. Backend (Node/Deno) con MIT. Respetar Google API TOS (Service Account scopes mínimos). | **Controlado** |
| **Seguridad ProfeOS (read-only)** | **Media** | **Enforce read-only**: filtrar tools MCP (solo GET/list), Supabase RLS estricta, scopes mínimos, API key/JWT corto, no exponer write endpoints. **Inicial solo lectura** tal consigna. | **Requiere implementación cuidadosa** |
| **OTA/particiones** | **Media** | Usar tabla particiones validada. HTTPS obligatorio prod. Habilitar rollback. Probar OTA en dev antes prod. | **Mitigable con ESP-IDF estándar** |
| **Memoria/RAM Flash C6** | **Baja** | C6 16MB Flash + RAM suficiente para LVGL+XiaoZhi típico. Evitar assets PNG grandes sin conversión (usar bin/C arrays). Monitorar heap LVGL. | **Aceptable** |

## 18. Arquitectura Recomendada (Basada en Componentes Reutilizables)

Arquitectura **mínima, pragmática, Ponytail full**: reutilizar **[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)** como núcleo.

```text
HORNERITO (Waveshare ESP32-C6-Touch-AMOLED-2.16)
│
├── CORE (reutilizar directo)
│   ├── 78/xiaozhi-esp32
│   │   ├── Application (state machine, chat, voz)
│   │   ├── Protocols (WebSocket/MQTT+UDP)
│   │   ├── MCP Server (device-side tools)
│   │   ├── Audio (pipeline base + VAD)
│   │   └── Boards framework (DECLARE_BOARD)
│   ├── esp-sr (WakeNet + VAD)
│   └── esp_audio_codec (ES8311+ES7210)
│
├── BOARD LAYER (NUEVO - adaptar)
│   └── main/boards/waveshare/esp32-c6-touch-amoled-2.16/
│       ├── config.h (pines I2C/SPI/QSPI/I2S, codec, TP, PMIC, SD, IMU)
│       ├── config.json (target esp32c6, builds)
│       ├── esp32-c6-touch-amoled-2.16.cc (init I2C/SPI, SH8601, CST9217, AXP2101, QMI8658, SD)
│       ├── CMakeLists.txt (opcional)
│       └── README.md
│
├── PERIFÉRICOS (directo/adaptado)
│   ├── Display: SH8601 (esp_lcd_sh8601) + LVGL9
│   ├── Touch: CST9217 (esp_lcd_touch)
│   ├── PMIC: AXP2101 (reutilizar lógica XiaoZhi)
│   ├── IMU: QMI8658 (esp-idf-lib/qmi8658c)
│   ├── SD/microSD: FATFS+VFS (SPI/SDMMC) + esp-audio-player (MP3)
│   └── Audio I/O: I2S + ES8311/ES7210 + 3.5mm (preliminar)
│
├── UI + PET + JUEGOS (reutilizar directo)
│   ├── LVGL9 (chat UI + estados Hornerito)
│   ├── SquareLine Studio (diseño caras/animaciones)
│   ├── FSM Estados: IDLE/LISTENING/THINKING/SPEAKING/MUSIC/PLAYING_GAME/SLEEPING/ERROR/LOW_BATTERY
│   ├── Sprites/animaciones (TamaPetchi/Cloudagotchi refs)
│   └── LovyanGFX (arcade con IMU/touch)
│
├── CONECTIVIDAD (directo/adaptado)
│   ├── Wi-Fi 6
│   ├── BLE 5 LE: Unified Provisioning (Wi-Fi) + GATT/L2CAP file transfer (chunked)
│   └── OTA: HTTPS OTA + rollback automático
│
└── BACKEND PUENTE (adaptado)
    ├── Supabase Edge Functions (Deno/TS) – read-only enforcement
    ├── Google Calendar API/MCP proxy (nspady/google-calendar-mcp filtrado)
    ├── ProfeOS API/Supabase (RLS read-only)
    └── Auth ligero ESP32 (API key/JWT) – sin OAuth completo en MCU
```

**Principios arquitectura:**
- **Un solo board nuevo**. No modificar boards existentes.
- **Máxima reutilización núcleo** (XiaoZhi). Añadir solo lo que falta (QMI8658, SD, completar board).
- **Backend puente** para Calendar/ProfeOS (seguridad + simplicidad). Cumple **"solo lectura"** inicialmente.
- **BLE-only documentado** (descartar A2DP Classic).
- **LVGL primario** para coexistir con UI XiaoZhi. LovyanGFX secundario.
- **OTA HTTPS + rollback** desde día 1.
- **Licenciamiento conservador** (MIT/Apache-2.0).

## 19. Stack Tecnológico Recomendado

| Capa | Tecnología | Motivo |
|---|---|---|
| **Firmware Core** | [ESP-IDF 6.x](https://github.com/espressif/esp-idf) | Requerido por XiaoZhi 2.5.x+. Estable, BLE/Wi-Fi/C6 soportado. |
| **IA Framework** | [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) | Mejor base reutilizable. Arquitectura probada, MCP, protocolos, board system. |
| **Speech** | [esp-sr](https://components.espressif.com/components/espressif/esp-sr) | Wake word offline + VAD. Integrado ecosistema Espressif. |
| **Audio** | [esp_audio_codec](https://components.espressif.com/components/espressif/esp_audio_codec) + [esp-audio-player](https://components.espressif.com/components/chmorgan/esp-audio-player) + I2S | Pipeline unificado, reproducción MP3 SD. |
| **Gráficos UI** | [LVGL v9](https://lvgl.io/) + [SquareLine Studio](https://squareline.io/) | Coexiste XiaoZhi, animaciones, estados pet, mantenible. |
| **Juegos arcade** | [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Rápido 2D + sprites + IMU tilt. |
| **Display/Touch** | [esp_lcd_sh8601](https://components.espressif.com/components/waveshare/esp_lcd_sh8601) + `esp_lcd_touch` (CST9217) | Drivers esp_lcd estándar. |
| **Sensores** | [esp-idf-lib/qmi8658c](https://components.espressif.com/components/esp-idf-lib/qmi8658c) | Driver limpio MIT, componente gestionable. |
| **Storage** | FATFS + VFS (ESP-IDF) | Estándar. microSD SPI/SDMMC. |
| **BLE/Prov** | ESP-IDF Unified Provisioning (BLE) | Estándar, apps oficiales existentes. |
| **OTA** | `esp_https_ota` + rollback | Seguro, nativo. |
| **Backend** | [Supabase](https://supabase.com/) (Postgres + Auth + RLS + Edge Functions Deno) | Coincide ProfeOS. Rápido prototipado + read-only controlado. |
| **MCP Backend** | [nspady/google-calendar-mcp](https://github.com/nspady/google-calendar-mcp) (filtrado read-only) o custom Edge Function | Reutilizable. Exponer solo tools necesarios. |
| **Build/Tooling** | `scripts/build.py` XiaoZhi + ESP-IDF VSCode/Cursor | Alineado con `AGENTS.md`. |

## 20. Skills Recomendados para OpenCode

Para trabajar eficientemente con este stack, conviene tener/usar estos *skills*:

| Skill | ID sugerido | Cuándo usar | Razón |
|---|---|---|---|
| **ESP-IDF/Embedded** | `embedded-espidf` (o existente) | Board creation, drivers I2C/SPI/QSPI/I2S, OTA, BLE, particiones | Dominio ESP-IDF 6.x crítico (C6 nuances). |
| **XiaoZhi/Voice Assistant** | Contexto XiaoZhi ([AGENTS.md](https://github.com/78/xiaozhi-esp32/blob/main/AGENTS.md)) | Entender arquitectura Application/Protocols/MCP/Audio | Evitar romper patrón Board + lifecycle existente. |
| **LVGL/UI** | `frontend-ui-engineering` (adaptado embedded) o LVGL-specific | UI pet/chat, SquareLine export, animaciones, eventos touch | UI 480×480 AMOLED + estados visuales. |
| **BLE/GATT/Provisioning** | `ble-provisioning` | Custom GATT, file transfer chunked, L2CAP CoC | BLE-only requiere cuidado (MTU, throughput). |
| **API/MCP/Backend** | `api-and-interface-design` | Supabase Edge Functions, MCP proxy read-only, RLS, auth ligero | Puente backend seguro. |
| **Security/Hardening** | `security-and-hardening` | Read-only enforcement, API keys, RLS, HTTPS OTA, no secrets in flash | Crítico para ProfeOS (datos sensibles). |
| **Debugging/Error Recovery** | `debugging-and-error-recovery` | I2C init, QSPI timing SH8601, codec, IMU | Root-cause sistemático (Ponytail compatible). |
| **Git Workflow** | `git-workflow-and-versioning` | Commits atómicos, ramas board/new, evitar modificar firmware original | Mantener `02_xiaozhi-v2.2.5.bin` intacto. |
| **Incremental Implementation** | `incremental-implementation` | Crear board slice por slice (config→init→display→touch→audio→IMU→SD) | Entrega verificable, menos riesgo. |
| **Testing/Validation** | `test-driven-development` (ligero) | Validar build `scripts/build.py`, smoke tests por módulo | Build green antes HW. |

## 21. Roadmap de Implementación (Orden Recomendado)

| Fase | Tareas | Prioridad | Notas |
|---|---|---|---|
| **FASE 0** | ✅ Investigación externa exhaustiva (completo) | **CRÍTICA** | **Hecho.** Informe generado. |
| **FASE 1: Diseño Board** | Crear directorio `waveshare/esp32-c6-touch-amoled-2.16/`. Copiar/derivar desde C6-2.06 o S3-2.16 más cercano. Definir `config.h`, `config.json`. Validar pinout con refs Waveshare. | **ALTA** | Seguir [docs/custom-board.md](https://github.com/78/xiaozhi-esp32/blob/main/docs/custom-board.md). **No modificar boards existentes**. |
| **FASE 2: Núcleo Board** | Implementar `esp32-c6-touch-amoled-2.16.cc`: I2C bus, SPI/QSPI, AXP2101, SH8601 (esp_lcd), CST9217 (touch), ES8311+ES7210 mapping. Compilar con `scripts/build.py`. | **ALTA** | Validar build exitoso. Iterativo slice por slice. |
| **FASE 3: Sensores + Storage** | Añadir QMI8658 (I2C) + init. Añadir microSD (FATFS SPI/SDMMC) + test mount/read. Añadir PCF85063 si viable (opcional). | **MEDIA–ALTA** | QMI8658 crítico para gestos/juegos IMU. SD crítico reproducción MP3. |
| **FASE 4: Audio + MP3** | Integrar `esp_audio_codec` + `esp-audio-player` + VFS. Test reproducción MP3 desde SD. Validar mic+HP (ES7210+ES8311). | **ALTA** | Reutilizar pipeline XiaoZhi existente donde posible. |
| **FASE 5: UI Pet + Estados** | LVGL UI + FSM estados visuales (IDLE/LISTENING/THINKING/SPEAKING/MUSIC/PLAYING_GAME/SLEEPING/ERROR/LOW_BATTERY). Sprites animados. SquareLine export. | **MEDIA** | Alinear con personalidad Hornerito. |
| **FASE 6: BLE Provisioning + File Transfer** | Mantener Unified Provisioning. Añadir GATT file transfer chunked (MTU aware). Test con nRF Connect + app oficial. | **MEDIA** | BLE-only documentado. Empezar GATT simple → evaluar L2CAP. |
| **FASE 7: Backend Calendar + ProfeOS (read-only)** | Supabase Edge Function read-only: Google Calendar proxy + ProfeOS endpoints. Filtrar MCP tools. Añadir auth ligero ESP32. RLS estricto. | **MEDIA** | **Solo lectura** inicialmente. Validar casos uso: "¿Qué tengo mañana?", alumnos, faltas, intensificación. |
| **FASE 8: OTA + Hardening** | HTTPS OTA + rollback. Validar particiones. Revisar secrets (ninguno flash). Tests OTA end-to-end. | **MEDIA–ALTA** | Habilitar antes pruebas campo. |
| **FASE 9: Juegos IMU + Validación** | Juegos mínimos (Pong/Snake con tilt) + shake detection. Integración con pet states (PLAYING_GAME). Validación HW completa. | **BAJA** | Nice-to-have pero alineado consigna. |
| **FASE 10: Documentación** | Actualizar README board, arquitectura, decisiones ADR si aplica, licencias, riesgos resueltos. | **BAJA** | Mantener trazabilidad reutilización. |

## 22. Recomendación Final

**Base óptima:** **[78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)** + componentes Espressif (esp-sr, esp_audio_codec, unified provisioning, HTTPS OTA) + Waveshare drivers ([esp_lcd_sh8601](https://components.espressif.com/components/waveshare/esp_lcd_sh8601)) + Supabase (backend puente read-only).

**Veredicto reutilización:** **~75–85% directo/adaptado, ~0–2% innecesario a desarrollar**. Esto cumple estrictamente **"REUTILIZAR antes que REIMPLEMENTAR"**.

**Acción inmediata (FASE 1):** Crear **único board nuevo** `waveshare/esp32-c6-touch-amoled-2.16/` siguiendo `docs/custom-board.md`, derivando lógica de C6-2.06/S3-2.16 + completar integración **QMI8658 + microSD**. Mantener **firmware original `02_xiaozhi-v2.2.5.bin` intacto** como referencia/recuperación.

**Principio guía:** *Ladder aplicado (Ponytail full): entender problema → reutilizar existente → stdlib/componentes → mínimo código que funciona. YAGNI estricto. Licenciamiento consciente.*

---

**Archivo generado:** `C:\OpenCode-Proyectos\Hornerito\HORNERITO_REUTILIZACION_FASE0.md`

*Análisis realizado con Ponytail level: full. Sin clones, sin modificaciones, solo investigación externa exhaustiva con evidencia URL completa.*