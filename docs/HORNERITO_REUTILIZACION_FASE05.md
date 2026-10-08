# HORNERITO — FASE 0.5: GRABADORA INTELIGENTE + TRANSCRIPCIÓN + MEMORIA

**Fecha:** 2026-10-04
**Alcance:** Investigación específica de la función "Hornerito Recorder" (grabación + transcripción + IA + Obsidian).
**Enfoque:** *Reutilizar antes que reimplementar*. **Ponytail level: full**.
**Documento maestro asociado:** `HORNERITO_REUTILIZACION_FASE0.md` (FASE 0 — reutilización general, intacto).

> **RESTRICCIONES CUMPLIDAS:** Solo investigación. No se modificó firmware, no se modificó código de XiaoZhi, no se creó código productivo, no se clonaron repositorios, no se instalaron dependencias, no se hicieron commits. Datos no confirmados se marcan **⚠️ Pendiente de verificación** o **❓ No verificado** (nunca inventados).

## Convención de verificación y clasificación

| Marca | Significado |
|---|---|
| ✅ | Verificado en esta sesión (fuente citada) o presente en el repo local |
| ⚠️ | Pendiente de verificación (dato razonable pero no confirmado ahora) |
| ❓ | No verificado / desconocido |

| Clase | Significado |
|---|---|
| 🟢 **DIRECTO** | Se usa casi tal cual (mismo HW/mismo runtime) |
| 🟡 **ADAPTAR** | Requiere port/adaptación real, pero el valor justifica el esfuerzo |
| 🔴 **NO CONVIENE** | Coste, licencia, RAM o arquitectura lo descartan (dejarlo como referencia) |

**Búsquedas puntuales usadas (9, todas dentro del tope):** licencia WhisperX · licencia Moonshine · plugin Obsidian Local REST API · wiki Waveshare C6 2.16 · drivers ES7210 + `esp_codec_dev` + ejemplo GMF record→SD · licencia faster-whisper/whisper.cpp · licencia Vosk/sherpa-onnx · licencia pyannote · LICENSE local de xiaozhi-esp32 (leído del disco). No se realizó ninguna investigación exhaustiva nueva.

---

## 1. OBJETIVO

Que Hornerito funcione como una **grabadora inteligente / memoria personal**:

```text
Hornerito (MODO GRABADORA)
   ↓
grabación de audio (I2S, ES7210 doble micrófono)
   ↓
microSD (almacenamiento local, offline-first)
   ↓
transcripción (STT)
   ↓
IA (LLM)
   ↓
información estructurada
   ↓
Markdown
   ↓
Obsidian
```

**Ámbitos de uso:** conversaciones, reuniones, clases, ideas, notas de voz, proyectos, tareas, decisiones, recordatorios, información personal que el usuario quiera conservar. **No limitado al ámbito docente** (ProfeOS es solo un caso de uso más).

**Hallazgo estructurante de la investigación (0.5.1):** el ESP32-C6 es una **máquina de grabar y encolar**, no de transcribir. Toda la transcripción vive *fuera* del chip. Esto define toda la FASE 0.5.

---

## 2. GRABACIÓN DE AUDIO EN ESP32-C6

### 2.1 Hardware target verificado ✅

Fuente: <https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16> · <https://www.waveshare.com/esp32-c6-touch-amoled-2.16.htm> · repo <https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16> (licencia del repo: ❓ **No verificado** — sin LICENSE visible en la vista consultada).

| Ítem | Dato verificado | Impacto en RECORDER |
|---|---|---|
| MCU | ESP32-C6, RISC-V **single-core 160 MHz**, Wi-Fi 6 + BLE 5 (BLE-only, **sin A2DP**) | Una sola CPU para grabar **y** (si existiera) ASR → ASR on-device no cabe |
| RAM | **512 KB HP SRAM + 16 KB LP SRAM + 320 KB ROM**, **sin PSRAM externa** | **Cuello de botella #1.** Ningún modelo Whisper/Vosk cabe en el chip |
| Flash | **16 MB NOR** | Bien para firmware + assets; NO para audio |
| Audio codec | **ES8311** (DAC/ADC, playback) ✅ | Grabación incluso con un solo mic (ES8311 tiene ADC) |
| ADC multicanal | **ES7210** (4 canales, "echo cancellation circuitry") ✅ | **Dual mic** + canal de referencia para AEC |
| IMU | QMI8658 (6 ejes) ✅ | Señal "handover": sacudir/gestear para cortar grabación |
| PMIC | AXP2101 ✅ | Estado de batería para warning LOW_BATTERY antes de cortar REC |
| Pantalla | AMOLED 2.16", 480×480, QSPI, driver **CO5300**; touch **CST9220** ✅ | ⚠️ **Discrepancia:** FASE 1 de este doc dice `CST9217`; la wiki oficial dice **CST9220**. Corregir al llegar a FASE 1 (sin tocarla ahora) |
| RTC | wiki de la variante C6 no confirmó modelo (la variante S3 lista PCF85063ATL) ⚠️ | Si hay RTC → timestamp real en el nombre de archivo |
| microSD | el firmware de autotest de fábrica menciona **"TF card"** ✅ | Slot SD existe; pinout exacto ❓ pendiente (wiki de pines) |

El board de XiaoZhi local (`main/boards/waveshare/esp32-c6-touch-amoled-2.16/`) **ya configura** I2S (MCLK 19, WS 22, BCLK 20, DIN 21, DOUT 23), I2C de codecs (SDA 8/SCL 7), SH8601, CST9217 y AXP2101 → **la base de captura de audio ya existe**. QMI8658 y PCF85063 NO están referenciados (dato FASE 0).

### 2.2 Componentes y proyectos encontrados

| Componente | URL | Licencia | Estado | Qué aporta / Reutilización |
|---|---|---|---|---|
| **`esp_codec_dev`** (driver unificado ES8311+ES7210, "HW verified" para ambos) | <https://github.com/espressif/esp-audio-dev> (`esp_codec_dev/`) | ⚠️ Apache-2.0 (típico Espressif, no re-verificado) | ✅ **Driver a usar** | 🟢 DIRECTO. La tabla oficial marca ES7210 (record) y ES8311 (play+record) como verificados en HW |
| Ejemplo **grabar → encoder → microSD** (`pipeline_record_sdcard`) | <https://components.espressif.com/components/espressif/gmf_examples/examples/pipeline_record_sdcard> | ⚠️ no re-verificado | ✅ **plantilla directa** | 🟢 DIRECTO: CODEC_DEV_RX → encoder → FILE_IO a `/sdcard/*.aac`; clonar el patrón con WAV/Opus |
| Ejemplo ES7210 TDM (4 ch) `i2s_es7210_tdm` | example oficial ESP-IDF (componente `espressif__es7210`) | ⚠️ no re-verificado | ✅ referencia | 🟡 referencia para TDM de 2 mics |
| Pipeline AEC completa (ES7210 TDM ref + esp-sr AEC) | <https://components.espressif.com/components/cube32esp/cube32_bsp/examples/hello_aec_test> | ❓ | referencia | 🟡 referencia de diseño (AEC obliga a 16 kHz mono; loopback HW por canal ES7210) |
| `esp-sr` AEC/AFE | <https://docs.espressif.com/projects/esp-sr/> | ⚠️ no re-verificado | docs apuntan a S3/P4 | 🔴 para C6: **compatibilidad C6: No verificado** |
| Driver viejo `es7210` en `esp-bsp` | <https://github.com/espressif/esp-bsp/tree/master/components/es7210> | ⚠️ no re-verificado | **deprecado** | 🔴 NO usar: "use codec drivers from esp_codec_dev" |
| **ESP-IDF I2S RX (captura)** | <https://github.com/espressif/esp-idf/tree/master/examples/peripherals/i2s> | Apache-2.0 ✅ | oficiales | 🟢 DIRECTO. Base del patrón I2S RX → DMA → ring buffer |
| **atomic14/esp32-i2s-wav-recorder** | <https://github.com/atomic14/esp32-i2s-wav-recorder> | MIT ⚠️ (típico, no re-verificado en sesión) | Arduino | 🟡 ADAPTAR (port a ESP-IDF). Referencia de **WAV writer** (header RIFF PCM16 + seek-back de tamaño) |
| **ESP8266Audio** | <https://github.com/earlephilhower/ESP8266Audio> | MIT ⚠️ (no re-verificado) | Arduino | 🔴 en stack ESP-IDF puro se prefiere `esp_audio_codec` |
| **esp-audio-player** (reproducción) | <https://components.espressif.com/components/chmorgan/esp-audio-player> | MIT ⚠️ | ESP-IDF | 🟢 DIRECTO para **revisar grabaciones** (R2) |

### 2.3 Parámetros de captura (recomendación)

| Parámetro | Recomendación | Nota |
|---|---|---|
| Sample rate | **16.000 Hz** | Whisper es nativo 16 kHz → evita resampleo |
| Bit depth | 16-bit PCM | Estándar, sin conversión |
| Canales | **WAV ESTÉREO (2 canales ES7210)** ✅ | Mantiene AEC/diarización futura a coste 0 (×2 bytes); usar canal 0 si solo se necesita mono |
| Formato archivo | **WAV (PCM16)** | Sin pérdida, header de 44 bytes, robusto ante cortes |
| Buffers | Ring buffer DMA + task escritura 4–16 KB | 512 KB SRAM no admite otra cosa (regla dura) |
| Grabación continua | Append a FATFS por bloques | No reabrir el archivo por bloque |

---

## 3. MICROSD

### 3.1 Recursos

| Recurso | URL | Licencia | Relevancia | Reutilización |
|---|---|---|---|---|
| FATFS + VFS | <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/fatfs.html> | ⚠️ Apache-2.0 no re-verificado | Archivos grandes, append continuo, montaje `/sdcard` | 🟢 DIRECTO |
| SDSPI Host (SPI) | <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sdspi_host.html> | ⚠️ Apache-2.0 no re-verificado | **Asumir SD por SPI** hasta confirmar | 🟢 DIRECTO |
| SDMMC Host | <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/sdmmc_host.html> | ⚠️ Apache-2.0 no re-verificado | Alternativa si el pinout lo soporta | 🟢 si pines disponibles |

❓ **Pinout microSD de la Waveshare C6-2.16: Pendiente de verificación** (wiki de pines). SDSPI a 20–40 MHz ≈ 2,5–5 MB/s prácticos ⚠️ orientativo → sobra holgura frente a los 0,032 MB/s de PCM 16 kHz.

### 3.2 Streaming, robustez y cola

| Tema | Decisión / dato | Nota |
|---|---|---|
| Streaming | **Nunca bufferar el archivo en RAM**: `fwrite` en bloques de **4–16 KB** desde callback/I2S DMA | Regla dura por los 512 KB SRAM |
| Segmentación | Archivos de **60 s** (mucho antes del límite FAT32 de 4 GiB) | Archivos cortos = recuperación tras corte de batería + progreso visible |
| Crash-safety | `manifest.json` **append-only** (`{"t":"...","n":"rec_0001.wav","state":"open\|closed\|queued\|uploaded"}`) tras cada cierre | Base de la cola offline (§12) |
| Integrity | Escribir tamaño/frecuencia reales en el header WAV al cerrar (seek-back) | WAV con tamaño falso rompe los transcritors |
| Cola por directorios | `/REC/pending`, `/REC/processed`, `/REC/failed` | Los directorios son la cola (Ponytail: sin librería externa) |

### 3.3 Tamaños por hora (cálculo propio a partir de formatos estándar)

| Formato | Tasa | MB/hora | Uso |
|---|---|---|---|
| PCM/WAV 16 kHz · 16-bit · mono | 32 KB/s | **115 MB/h** | Máxima fidelidad para ASR, cero CPU extra |
| WAV 48 kHz · 16-bit · mono | 96 KB/s | 345 MB/h | Solo calidad de archivo (no hace falta) |
| Opus 16 kbps mono (estimado) | 2 kB/s | **~7,2 MB/h** | 🟡 Óptimo si hay CPU; 160 MHz single-core → medir |
| Opus 24 kbps mono (estimado) | 3 kB/s | ~10,8 MB/h | Alternativa |

Presupuesto: 115 MB/h → **SD de 16 GB ≈ 139 h**; Opus 16 kbps ≈ **2.200 h** → semanas de clase.

---

## 4. FORMATOS DE AUDIO

| Formato | Calidad | MB/h | Encode en C6 (160 MHz) | Licencia | Veredicto |
|---|---|---|---|---|---|
| **WAV/PCM** | Sin pérdida | **115** | Nativo (memcpy), 0 CPU extra | Libre | 🟢 **ELEGIDO para grabar** (header de 44 bytes, cualquier STT lo traga) |
| **Opus** | Casi sin pérdida | **~7,2–10,8** | Posible pero **benchmark en R3** ⚠️ | ⚠️ BSD-3-Clause (xiph/opus) no re-verificado | 🟡 Optimización futura de subida si benchmark lo aprueba |
| **FLAC** | Sin pérdida | ~60–70 (est.) | Costoso CPU | Libre | 🔴 No conviene: sube CPU sin ahorro relevante vs WAV |
| **AAC** | Con pérdida | ~20–30 | No ideal | Fricción de patentes (mayoría expiradas) | 🔴 No conviene para grabación |
| **MP3** | Con pérdida | ~30–60 | Costoso CPU | Patentado | 🔴 No para grabar (sí para música existente) |

**Conclusión:** **WAV (PCM16) es el formato por defecto**. Opus solo si el benchmark R3 lo aprueba. El ejemplo oficial `pipeline_record_sdcard` produce `.aac`, pero para STT + robustez elegimos WAV.

---

## 5. STT (TRANSCRIPCIÓN)

**Categoría obligatoria:** ⛔ **ninguno de estos motores corre en el ESP32-C6** (512 KB SRAM sin PSRAM). Se dividen en (i) lo que corre en el **host/gateway** (PC del profesor, Raspberry, servidor local) y (ii) lo que sería on-device en un chip con más RAM.

| Motor | URL | Licencia | Corre en C6 | Corre en host | Qué aporta | Clase |
|---|---|---|---|---|---|---|
| **faster-whisper** | <https://github.com/SYSTRAN/faster-whisper> ✅ | **MIT** ✅ | ❌ | ✅ CPU int8 | **Motor por defecto de la pipeline host**: mismo WER que Whisper, ~4× más rápido, VAD Silero incluido, batch | 🟢 DIRECTO |
| **whisper.cpp** | <https://github.com/ggml-org/whisper.cpp> (⚠️ URL actual no re-verificada; antes `ggerganov`) | **MIT** ✅ | ❌ | ✅ | C/C++ sin deps → **ideal para gateway embebido/ligero**; binario único + `whisper-server` | 🟢 DIRECTO |
| **WhisperX** | <https://github.com/m-bain/whisperX> ✅ | **BSD-2-Clause** ✅ | ❌ | ✅ | **Timestamps a nivel de palabra + diarización** (VAD + align + pyannote) → "habló Juan 3:12–5:40" | 🟡 ADAPTAR (Python + Torch) |
| **Vosk** | <https://alphacephei.com/vosk> ✅ | **Apache-2.0** ✅ (API); modelos mayoritariamente Apache-2.0, **algunos LGPL** ⚠️ — <https://alphacephei.com/vosk/models> ✅ | ❌ | ✅ | **Streaming barato y ligero**, API simple, buen español en modelos pequeños | 🟡 ADAPTAR |
| **sherpa-onnx** | <https://github.com/k2-fsa/sherpa-onnx> ✅ | **Apache-2.0** ✅ | ❓ No verificado para ESP32 | ✅ | ONNX: VAD + KWS + TTS + diarización en un runtime, **offline por diseño** | 🟡 ADAPTAR (evaluar port) |
| **Moonshine** | <https://github.com/moonshine-ai/moonshine> (ex `usefulsensors`) ✅ | **MIT (código + modelos EN)** ✅ · **modelos ES: Moonshine AI Community License** (gratis si facturación < 1 US$ M/año) ✅ | ❌ | ✅ | Modelos **tiny ~1 MB**, latencia muy baja → mejor candidato si algún día hay device con PSRAM | 🟡 ADAPTAR (🔴 si no califica como "small business": revisar licencia ES) |
| **pyannote.audio** | <https://github.com/pyannote/pyannote-audio> ✅ | **MIT** (código) ✅ · modelos HF *gated* con acuerdo (v4.0 `community-1`, MIT) ✅ | ❌ | ✅ | **Diarización SOTA**, soporta uso offline/air-gapped | 🟡 ADAPTAR |
| **Whisper (pesos)** | ❓ pendiente | ⚠️ Pendiente por checkpoint | ❌ | ✅ | Base de todo lo anterior | 🟢 como base |
| **silero-vad** (usa WhisperX) | ❓ | ⚠️ Pendiente | ❌ | ✅ | VAD ligero para cortar silencios antes de transcribir | 🟡 |

**Orden de preferencia de la pipeline host:** `faster-whisper (int8, base/small)` → si se necesita word-alignment/diarización: `WhisperX` → diarización vía `WhisperX` o `pyannote` suelto.

**Tamaños de modelo (⚠️ órdenes de magnitud):** tiny ≈ 75 MB · base ≈ 145 MB · small ≈ 480 MB. En el host no hay drama; **en el C6 ninguno cabe**.

### Servicios cloud (STT, opción opcional)

| Servicio | URL | Precio orientativo | Español | Verificación |
|---|---|---|---|---|
| Deepgram | <https://deepgram.com> | ~USD 0,004–0,005/min | Excelente | ⚠️ precios pendientes |
| AssemblyAI | <https://www.assemblyai.com> | ~USD 0,15–0,22/h | Excelente | ⚠️ precios pendientes |
| Google Speech-to-Text | <https://cloud.google.com/speech-to-text> | ~USD 0,036/15 s (standard) | Excelente | ⚠️ precios pendientes |
| Azure Speech | <https://azure.microsoft.com/products/ai-services/speech-services> | ~USD 0,016/min | Excelente | ⚠️ precios pendientes |

**Decisión:** cloud solo como **opcional**; el diseño principal es **offline-first con backend local**.

### ¿Dónde corre STT?

| Opción | Viabilidad | Veredicto |
|---|---|---|
| **En ESP32-C6** | No viable (512 KB RAM sin PSRAM) | 🔴 Descartado por hardware |
| **Teléfono** | Posible (app futura con Vosk/GPU) | 🟡 Alternativa futura |
| **PC/Servidor local (backend)** | Excelente (faster-whisper/sherpa-onnx) | 🟢 **Recomendado** |
| **Cloud** | Funciona, cuesta y requiere Internet | 🟡 Opcional |

---

## 6. DIARIZACIÓN

| Opción | Cómo | Clase |
|---|---|---|
| **WhisperX** (whisper + VAD + align + pyannote) | un solo `transcribe(..., diarize=True)` | 🟡 **recomendada** si se acepta el stack Python |
| **pyannote.audio 4.0 / community-1** suelto | pipeline propia, export offline (git-lfs + token HF) ✅ | 🟡 |
| **"Diarización pobre" en el device** (ES7210 dual-mic → nivel por canal) | sin ML, umbral de energía | 🟢 **complemento barato**: da SPEAKER_A/B groso. ⚠️ No verificado: separación física real de los 2 mics → **probar en R0** |
| **sherpa-onnx** (speaker diarization) | mismo runtime offline | 🟡 |

Límite honesto: con 2 mics a centímetros, la diarización acústica de sala es mediocre → **la diarización seria se hace en el host**.

**Decisión de fase:** **primera versión sin diarización** (solo timestamps por segmento). Diarización = **segunda etapa (R8)** cuando haya uso real con múltiples participantes. Modelos pyannote *gated* (acuerdo en HuggingFace) ⚠️ condiciones pendientes.

---

## 7. VAD / PROCESAMIENTO DE AUDIO

| Componente | URL | Aplicación | Veredicto |
|---|---|---|---|
| **ESP-SR (VAD)** | <https://github.com/espressif/esp-sr> | Detección de actividad de voz en dispositivo | 🟢 disponible en ecosistema XiaoZhi; para AEC/AFE el C6 **no está verificado** 🔴 (docs S3/P4) |
| **silero-vad / webrtcvad** | (ver §5) | VAD en host para cortar silencios antes de STT → ahorra tiempo/costo | 🟢 DIRECTO (host; ya incluido en faster-whisper) |
| **AEC / noise suppression** | pipeline XiaoZhi (`docs/audio-codec-input-audit.md`) + `hello_aec_test` de referencia | Doble micrófono → AEC si hay reproducción simultánea (MODO_DUAL) | 🟡 ADAPTAR (experimento R2; AEC obliga a 16 kHz mono) |
| **Doble micrófono (ES7210)** | ya mapeado en board | Captura estéreo → WAV 2 canales (§2.3) | 🟢 DIRECTO |

---

## 8. IA POST-TRANSCRIPCIÓN → MARKDOWN ESTRUCTURADO

### 8.1 Arquitectura (capa 100 % host, sobre el texto crudo del STT)

```text
AUDIO (WAV)
 ↓
STT (faster-whisper / whisper.cpp / Vosk)
 ↓
TRANSCRIPCIÓN (+ timestamps)
 ↓
LLM (Ollama / llama.cpp — modelo local)
 ↓
INFORMACIÓN ESTRUCTURADA (JSON interno)
 ↓
MARKDOWN (plantilla)
 ↓
OBSIDIAN
```

### 8.2 Piezas

| Pieza | Rol | URL / licencia | Clase |
|---|---|---|---|
| **Plantilla Markdown Hornerito** | Salida canónica: frontmatter (`titulo`, `fecha`, `asignatura`, `participantes`, `share`) + secciones (`resumen`, `conceptos`, `decisiones`, `pendientes`, `tareas`) | propia | 🟢 |
| **LLM local (llama.cpp / Ollama)** | Resumir + estructurar **sin salir de la casa** | <https://github.com/ggml-org/llama.cpp> · ⚠️ MIT no re-verificado · Ollama ❓ | 🟡 |
| **LLM por API** | Mejor calidad; sale texto de la casa | ❓ pricing **Pendiente** | 🟡 opcional con switch |
| **Normalizador propio** | Puntuación, muletillas, timestamps → bullets | script propio | 🟢 |

**Regla:** **el device jamás llama a una LLM**; solo produce audio + manifiesto.

### 8.3 Documento estructurado propuesto

```markdown
---
title: "{{Título generado}}"
date: "{{YYYY-MM-DD HH:mm}}"
duration: "{{mm:ss}}"
tags: [hornerito, grabacion]
participants: ["{{si diarización}}"]
source: hornerito-recorder
---

## Resumen
{{resumen en 3-6 líneas}}

## Temas tratados
- ...

## Decisiones
- ...

## Tareas pendientes
- [ ] ... (fecha si se mencionó)

## Fechas mencionadas
- ...

## Ideas
- ...

## Preguntas pendientes
- ...

## Transcripción completa
{{texto con timestamps por segmento}}
```

---

## 9. OBSIDIAN

| Vía | Detalle | Verificación | Clase |
|---|---|---|---|
| **`obsidian://` URI** (mínimo viable) | `obsidian://new?vault=X&file=Ruta/Nota&content=...` crea/abre la nota. Sin servidor ni API key | ❓ doc oficial **Pendiente** | 🟢 **empezar aquí** (R6, ~20 líneas) |
| **Local REST API (plugin)** | <https://github.com/coddingtonbear/obsidian-local-rest-api> — **MIT** ✅ · HTTPS loopback **27124** (HTTP inseguro 27123) ✅ · auth **Bearer API key** ✅ · endpoints crear/leer/patch/buscar + comandos ✅ | ✅ | 🟢 **producción** (R7; cuerpos grandes, mejor que URI) |
| **mcp-obsidian** | <https://github.com/MarkusPfundstein/mcp-obsidian> | ❓ licencia pendiente | 🟡 solo si hay agente MCP |
| **Escribir `.md` directo en el vault** | gateway con acceso de red a la carpeta | propia | 🟡 alternativa sin plugin |
| **Sincronización** | Git (plugin obsidian-git <https://github.com/Vinzent03/obsidian-git>, MIT ⚠️) o Obsidian Sync (SaaS) | ⚠️ parcial | 🟢 Git recomendado (versionado) |
| **Templater / Dataview** | <https://github.com/SilentVoid13/Templater> · <https://github.com/blacksmithgu/obsidian-dataview> · MIT ⚠️ | ⚠️ | 🟢 complementos (plantillas + índice/memoria) |

**Recomendado:** R6 con **URI** (~20 líneas), R7 con **REST API** cuando haya anexos/frontmatter complejo.

---

## 10. PROYECTOS SIMILARES

| Concepto | Hallazgo | Reutilización |
|---|---|---|
| **AI voice recorder OSS** | <https://github.com/topics/voice-recorder> · <https://github.com/topics/meeting-notes> (mayoría MIT) | Referencia de arquitectura |
| **Personal AI memory** | **mem0** <https://github.com/mem0ai/mem0> Apache-2.0 ⚠️ no re-verificado | 🟡 Referencia para R8+ (búsqueda/memoria) |
| **ESP32 recorder** | atomic14/esp32-i2s-wav-recorder (§2.2) | 🟡 ADAPTAR (WAV writer) |
| **M5Stack / Seeed voice recorder** | ejemplos I2S+SD en repos de fabricantes | Referencia |
| **XiaoZhi recording/transcription** | XiaoZhi ya graba y **transmite** Opus (modo asistente); forks: <https://github.com/search?q=xiaozhi-esp32+record&type=repositories> | 🟡 base del pipeline de audio reutilizable |
| **MeetLLM / AI-Voice-Notes** | <https://github.com/varunshenoy/MeetLLM> · <https://github.com/Neemys/AI-Voice-Notes> | ⚠️ licencias pendientes; solo plantillas/flujo |

**Conclusión:** no existe un proyecto único "ESP32 grabadora + transcripción + Obsidian" → la combinación de piezas (grabador ESP + host STT/LLM + plantilla Obsidian) es el diseño correcto: **reutilización por composición**.

---

## 11. XIAOZHI — COEXISTENCIA MODO ASISTENTE / MODO GRABADORA

Repo local ✅ `C:\OpenCode-Proyectos\Hornerito\xiaozhi-esp32` · **LICENSE: MIT** ✅ (Shenzhen Xinzhi Future Technology Co., Ltd. + Project Contributors).

**Conflicto real:** **XiaoZhi y RECORDER quieren lo mismo** (ES8311 + ES7210 + I2S + DMA + AFE).

| Tema | Análisis |
|---|---|
| Recursos compartidos | I2S bus, canales RX/TX, ES7210 (mics), ES8311 (HP), tarea AFE/wakeword, Wi-Fi |
| Patrón | **Un único dueño del codec por estado** (arbitraje en la FSM): `MODO_ASISTENTE` (AFE + wakeword + cloud) vs `MODO_RECORDER` (grabación pura) vs `MODO_DUAL` (grabar mientras XiaoZhi escucha) |
| `MODO_DUAL` | ❓ **No verificado** que ambos usos del RX puedan coexistir (MCLK compartido + TDM) → **experimento R2** |
| Transiciones | Entrar en RECORDER: suspender sesión, reabrir ES7210 con config de grabación, reencolar. Salir: restaurar |
| Trigger | Botón + gesto IMU (QMI8658) + estado nuevo `RECORDING` en la FSM de FASE 5 (sin modificar esa fase ahora) |
| Wakeword durante grabación | Solo si el consumo lo permite; si no, **modo RECORDER explícito** (más simple y predecible) |
| MCP | Reutilizar `main/mcp_server.*` + `docs/mcp-protocol.md`: tools `recorder.start/stop/status` 🟢 |
| UI | Estados LVGL `RECORDING/PROCESSING/UPLOADING` + indicador LED 🟢 (extender FSM de FASE 5) |
| Riesgo | ❓ **No verificado** qué driver de audio usa el repo local (`esp_audio_codec` vs `esp_codec_dev`) → comprobar en FASE 4 antes de tocar nada |

---

## 12. OFFLINE-FIRST

```text
REC (start)
 ↓
I2S → WAV 60 s append → /sdcard + manifest.json
        (SIN INTERNET: todo esto funciona)
STOP → state: queued
   ... tiempo pasa ...
Wi-Fi vuelve (o se monta la SD en la PC)
 ↓
gateway lee los "queued" (FIFO por t_start)
 ↓
UPLOAD/leído → STT (host) → LLM (host)
 ↓
Markdown → Obsidian
 ↓
state: uploaded  (o failed tras backoff)
```

**Diseño de cola:**

1. **Producción:** WAV 60 s → `manifest.json` con `state: open|closed|queued`.
2. **Encolado:** al cerrar → `queued` + intentos = 0. **Nada depende de red.**
3. **Consumo:** gateway (PC/servicio local) detecta `queued` → lee la SD montada, o la recibe por Wi-Fi/BLE → transcribe → publica `.md` → `uploaded`.
4. **Reintentos:** backoff 1 m → 5 m → 30 m → 2 h, tope 24 h, luego `failed` visible en pantalla.
5. **Idempotencia:** clave `device_id + rec_id + sha1(audio)` → reintentos no duplican notas.
6. **Orden:** FIFO por `t_start`; post-proceso por lote cuando la PC está encendida.

---

## 13. PRIVACIDAD

| Regla | Detalle |
|---|---|
| **Local-first por defecto** | El audio **no sale del dispositivo/red local** salvo opt-in explícito por nota |
| **Indicador físico de grabación** | LED/estado permanente mientras graba (obligatorio en aula) |
| **Opt-in por nota** | Cada `rec_id` lleva `share: local\|cloud` en el manifiesto |
| **Minimización** | Device guarda solo audio + metadatos; el texto vive en el vault |
| **Modelos locales** | Opción LLM local ⇒ ni audio ni texto salen de la casa |
| **Retención** | Borrado automático N días tras `uploaded` (parámetro) o manual desde UI |
| **HTTPS / credenciales** | Upload en TLS (mbedTLS); token/JWT corto renovable; **nunca secretos permanentes en flash** (misma regla FASE 0) |
| **Sin cifrado en reposo (MVP)** | FATFS no cifra → YAGNI; futuro si el caso lo exige |
| **Consentimiento en aula** | ⚠️ **No verificado**: obligación legal de grabar clase (jurisdicción) → **gate legal antes de uso público**, no detalle técnico |
| **Precisión en español** | Medir WER real (acento, ruido) antes de prometer |
| **ProfeOS** | Recorder separado de ProfeOS; ProfeOS sigue **solo lectura** |

---

## 14. COSTOS

| Componente | Coste | Verificación |
|---|---|---|
| Grabación + SD + WAV | **$0** (solo SD y batería) | ✅ |
| faster-whisper / whisper.cpp en PC | **$0** (MIT, CPU) | ✅ licencias verificadas |
| WhisperX + pyannote | **$0** (BSD-2 + MIT); costo en GPU/tiempo | ✅ licencias verificadas |
| LLM local | **$0** + RAM/VRAM | ⚠️ licencia no re-verificada |
| LLM/ASR por API | **$ variable** | ❓ **Pendiente** (cotizar al elegir) |
| Obsidian | gratis personal; licencia comercial ❓ | ⚠️ |
| Hardware (board + microSD) | ❓ | precio no verificado en sesión |

**Estimación por horas (orientativa, cloud STT ⚠️ precios a confirmar):**

| Opción | 1 hora | 10 horas | 50 horas | 100 horas |
|---|---|---|---|---|
| **Local** (PC + faster-whisper + Ollama) | ~$0 | ~$0 | ~$0 | ~$0 |
| **Servidor propio** (misma lógica, VPS) | ~$0,00–0,01 | ~$0,10 | ~$0,50 | ~$1 |
| **Cloud STT** (Deepgram/Google ~0,004–0,036/min) | ~$0,24–2 | ~$2,4–20 | ~$12–100 | ~$24–200 |
| **Cloud STT + LLM resumen** | ~$0,3–2,5 | ~$3–25 | ~$15–125 | ~$30–250 |

**Total camino recomendado (local): ≈ $0 recurrente.** Transferencia: WAV 115 MB/h si va por Wi-Fi (si Opus, ~7 MB/h).

---

## 15. ARQUITECTURAS A / B / C

### A — LOCAL ("DUMP & SYNC") — **recomendada (R0–R7)**

```text
Hornerito → microSD → (SD montada en PC o app que la lee)
 → faster-whisper int8 → texto crudo → LLM local (opcional) → plantilla .md
 → obsidian://new (o Local REST API)
```

- ✅ Cero red en el device, firmware simple, privacidad total, costos $0.
- ❌ Hay que tocar la SD (o app de transferencia).

### B — CLOUD

```text
Hornerito → Wi-Fi → Backend cloud → STT cloud → LLM cloud → Obsidian
```

- ✅ Cero infraestructura local.
- ❌ Costos por hora, privacidad, dependencia de Internet permanente, rompe offline-first.
- 🔴 **No recomendada como base**; solo opcional.

### C — HÍBRIDA ("EDGE GATEWAY")

```text
Hornerito → microSD (cola pending) → Wi-Fi (subida por lotes)
 → gateway local (whisper.cpp server / faster-whisper) → Obsidian REST API
```

- ✅ Sin sacar la SD, progreso en pantalla, offline-first real.
- 🟡 Cliente HTTP con reintento; **RAM para TLS en C6 con Wi-Fi+BLE simultáneos: ⚠️ no verificado**.
- Resultado: **A como base → C (híbrida) en R7** → **B solo opcional**.

### (Dato técnico transversal) ON-DEVICE ASR — 🔴 descartada hoy

STT dentro del C6: **inviable** — 512 KB SRAM sin PSRAM frente a modelos de decenas de MB. Incluso Moonshine tiny (~1 MB de pesos) no cabe con activations + runtime. Dejar como **referencia futura** para hardware con PSRAM (S3/P4): candidatos Moonshine o sherpa-onnx. **No bloquea A/C.**

**Justificación de la recomendación:** A respeta 100% offline-first, costos $0 y máxima privacidad; C se adopta en R7 cuando la cola justifique subir sin sacar la SD (y tras verificar RAM/TLS); B solo si el usuario prefiere cloud y asume costos. El "gateway" de A/C puede ser el **mismo servidor/puente que ya usaremos para Calendar/ProfeOS** → una sola pieza de infraestructura compartida (reutilización de arquitectura, no solo de código).

---

## 16. MATRIZ DE REUTILIZACIÓN

| # | Proyecto | URL | Licencia | Compat. C6 | Qué aporta | Reutilización | Limitaciones | Clase |
|---|---|---|---|---|---|---|---|---|
| 1 | **xiaozhi-esp32** (repo local) | repo local (+ <https://github.com/78/xiaozhi-esp32> ⚠️) | **MIT** ✅ | 🟢 (base) | FSM, AFE, Wi-Fi/BLE, UI LVGL, gestión de codec | **Directa** | Modo RECORDER comparte I2S/codec (§11) | 🟢 |
| 2 | **esp_codec_dev** | <https://github.com/espressif/esp-audio-dev> | ⚠️ Apache-2.0 no re-verificado | 🟢 | Drivers ES8311 + ES7210 verificados en HW | **Directa** | Config TDM a medida del board | 🟢 |
| 3 | **gmf_examples `pipeline_record_sdcard`** | <https://components.espressif.com/components/espressif/gmf_examples> | ⚠️ no re-verificado | 🟢 | Patrón **codec→encoder→SD** funcionando | **Adaptación** | Ejemplo, añade abstracción GMF | 🟢 |
| 4 | **`i2s_es7210_tdm`** | example ESP-IDF | ⚠️ no re-verificado | 🟢 | Init TDM ES7210 + máscara de canales | **Referencia→adaptación** | — | 🟢 |
| 5 | **cube32_bsp `hello_aec_test`** | <https://components.espressif.com/components/cube32esp/cube32_bsp> | ❓ | ⚠️ (S3) | Diseño AEC con loopback ES7210, WAV 16 kHz | **Referencia** | Board/SoC ajenos | 🟡 |
| 6 | **esp-sr AEC/AFE** | <https://docs.espressif.com/projects/esp-sr/> | ⚠️ no re-verificado | 🔴/❓ (docs S3) | AEC/NS/VAD/WakeNet | **Referencia** | Orientado S3/P4; **C6 no verificado** | 🔴 |
| 7 | **Waveshare ESP32-C6-Touch-AMOLED-2.16** | <https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-2.16> · <https://docs.waveshare.com/ESP32-C6-Touch-AMOLED-2.16> | ❓ No verificado | 🟢 (es el HW) | Pinout, demos, self-test con TF card | **Directa** | Solo demos Arduino; pinout SD pendiente | 🟢 |
| 8 | **ESP-IDF FatFS/VFS + SDSPI** | docs ESP-IDF ⚠️ | ⚠️ Apache-2.0 no re-verificado | 🟢 | Montaje `/sdcard`, streaming, append | **Directa** | FAT32 4 GiB; poco y seguido | 🟢 |
| 9 | **libopus** | <https://opus-codec.org> | ⚠️ BSD-3-Clause no re-verificado | 🟡 (CPU 160 MHz) | 16 kbps → ~7 MB/h | **Adaptación** (benchmark R3) | CPU; WAV es el default | 🟡 |
| 10 | **faster-whisper** | <https://github.com/SYSTRAN/faster-whisper> | **MIT** ✅ | ❌ host | STT rápido + VAD Silero, int8 CPU | **Directa** | Python + CTranslate2/Torch | 🟢 |
| 11 | **whisper.cpp** | <https://github.com/ggml-org/whisper.cpp> | **MIT** ✅ (URL ⚠️) | ❌ host | Binario único + servidor STT del gateway | **Directa** | Sin diarización ni word-align | 🟢 |
| 12 | **WhisperX** | <https://github.com/m-bain/whisperX> | **BSD-2-Clause** ✅ | ❌ host | Word-level timestamps + diarización | **Adaptación** | Stack pesado; modelos gated | 🟡 |
| 13 | **Vosk** | <https://alphacephei.com/vosk> | **Apache-2.0** ✅ (modelos ⚠️ mixtos) | ❌ host | Streaming ligero, buen español | **Adaptación** | WER peor que Whisper; modelos grandes | 🟡 |
| 14 | **sherpa-onnx** | <https://github.com/k2-fsa/sherpa-onnx> | **Apache-2.0** ✅ | ❓ No verificado en ESP32 | VAD+KWS+TTS+diarización offline | **Adaptación** | Port a C6 sin confirmar | 🟡 |
| 15 | **Moonshine** | <https://github.com/moonshine-ai/moonshine> | **MIT (EN)** ✅ / **Community (ES)** ✅ | ❌ hoy; futuro con PSRAM | Modelos tiny ~1 MB, baja latencia | **Referencia** | **Licencia ES condicionada** (<1 US$ M/año); no corre en C6 | 🟡 |
| 16 | **pyannote.audio** | <https://github.com/pyannote/pyannote-audio> | **MIT** ✅ (modelos gated) | ❌ | Diarización SOTA | **Adaptación** | Torch + host potente | 🟡 |
| 17 | **Obsidian Local REST API** | <https://github.com/coddingtonbear/obsidian-local-rest-api> | **MIT** ✅ | n/a | PUT/POST notas, search, comandos (HTTPS 27124, Bearer) | **Directa** | Requiere plugin en el vault | 🟢 |
| 18 | **Obsidian URI** | `obsidian://new?vault=&file=&content=` (doc oficial ❓ pendiente) | nativo Obsidian | n/a | Crear nota sin servidor | **Directa** | URL-encoded, sin binarios | 🟢 |
| 19 | **mcp-obsidian** | <https://github.com/MarkusPfundstein/mcp-obsidian> | ❓ pendiente | n/a | Puente MCP↔Obsidian | **Referencia** | Solo con agente MCP | 🟡 |
| 20 | **llama.cpp / Ollama** | <https://github.com/ggml-org/llama.cpp> | ⚠️ MIT no re-verificado | ❌ | Post-proceso a Markdown local | **Adaptación** | RAM de host; calidad según modelo | 🟡 |
| 21 | **ASR/LLM por API** | ❓ URLs y precios pendientes | ❓ Propietaria | ❌ | Calidad máxima sin instalar | **Adaptación** | Coste + datos fuera de la casa | 🔴 (por ahora) |
| 22 | **STT on-device (cualquiera)** | — | — | 🔴 | — | **Referencia futura** | 512 KB RAM sin PSRAM | 🔴 |
| 23 | **atomic14/esp32-i2s-wav-recorder** | <https://github.com/atomic14/esp32-i2s-wav-recorder> | ⚠️ MIT no re-verificado | Portable | WAV writer (RIFF + seek-back) | **Adaptación** (Arduino→IDF) | Sin cola/FATFS robusto | 🟡 |
| 24 | **mem0** (memoria personal) | <https://github.com/mem0ai/mem0> | ⚠️ Apache-2.0 no re-verificado | ❌ host | Capa de memoria para búsqueda futura | **Referencia** | Post-R8 | 🟡 |
| 25 | **obsidian-git** | <https://github.com/Vinzent03/obsidian-git> | ⚠️ MIT | n/a | Sync versionado del vault | **Directa** | Requiere Git en vault | 🟢 |

---

## 17. RECOMENDACIÓN (SÍNTESIS)

1. **Grabar WAV estéreo (2 canales ES7210) @16 kHz, archivos de 60 s, streaming a `/sdcard` con `manifest.json`** — sin dependencias nuevas ni CPU extra.
2. **Transcribir fuera del device:** **faster-whisper (MIT)** por defecto · **whisper.cpp (MIT)** si el gateway debe ser binario único · **WhisperX (BSD-2)** sólo con diarización/word-timestamps.
3. **Publicar en Obsidian vía `obsidian://` URI primero, Local REST API (MIT) después.**
4. **Cola offline-first idempotente** (`sha1(audio)`) con estados visibles en la UI del pet.
5. **Arquitectura A** ahora, **C híbrida** en R7, **B cloud** solo opcional; **on-device ASR descartada** por hardware.
6. **Nada de audio/texto a la nube sin opt-in por nota.**

**Porcentaje de reutilización:**
- **Firmware:** ~60–70% (pipeline I2S/codec/estados/MCP/UI de XiaoZhi; faltan WAV writer + cola + modo).
- **Host/pipeline completo:** **~80–90%** (STT, LLM, VAD, Obsidian: software existente y probado).
- **Global: ~75–85% reutilizable.**

**Qué hay que desarrollar (lo único):**
1. Modo GRABADORA (estados + exclusión con modo asistente) — pequeño.
2. WAV writer mínimo en ESP-IDF (~150–200 líneas) — **el único código firmware significativo**.
3. Cola de subida/lectura (`manifest.json` + reintentos) — mediano.
4. Orquestador host (script/docker: STT → LLM → `.md`) — ~200–400 líneas.
5. Plantilla Markdown + prompt del LLM — texto, no código.

---

## 18. ROADMAP R0–R10 (solo investigación/validación; no altera las FASEs 1–10 del maestro)

| Ref | Tarea | Entregable | Bloqueo |
|---|---|---|---|
| **R0** | PoC captura: ES7210 → I2S → WAV en SD, botón + LED "grabando", prueba de **2 mics** (¿estéreo útil?) | 1 WAV reproducible desde PC | Pinout SD (wiki) |
| **R1** | Streaming robusto: bloques 4–16 KB, archivos 60 s, `manifest.json`, seek-back header WAV | Corte de batería no rompe archivos | — |
| **R2** | Arbitraje XiaoZhi ↔ RECORDER (dueño único del codec) + **ver driver de audio real del repo local** | FSM con `RECORDING` sin romper asistente | Comprobar driver |
| **R3** | Benchmark Opus vs WAV en 160 MHz (CPU% y MB/h) | Decisión de formato | — |
| **R4** | Pipeline host: SD → `faster-whisper` → texto crudo (+ VAD) | CLI que transcribe una clase de 45 min | — |
| **R5** | Post-IA → **plantilla Markdown Hornerito**, con y sin LLM local | `.md` aceptable sin edición manual | Elección/licencia LLM local |
| **R6** | Publicación Obsidian por **URI** | Nota creada con 1 comando | Doc URI oficial ⚠️ |
| **R7** | Cola offline + **subida Wi-Fi** (Arquitectura C híbrida) + **Local REST API** | Notas automáticas sin sacar la SD | RAM TLS en C6 ⚠️ |
| **R8** | Diarización opcional (WhisperX/pyannote) + "dual-mic pobre" | `Speaker_1/2` en el `.md` | GPU/host |
| **R9** | Gates: opt-in por nota, retención, LED, **verificación legal de grabación en aula**, pricing de APIs si se usan | Checklist firmado | ⚠️ legal pendiente |
| **R10** | Documentación: cerrar matriz de licencias (todos los ⚠️), ADR A/B/C, README RECORDER | Trazabilidad completa | Todos los ⚠️ |

**Ajuste respecto de la secuencia inicial propuesta:** se separó explícitamente **R0/R1 (captura + robustez)** antes de cualquier subida, porque la investigación mostró que la captura depende de validar hardware (pinout microSD pendiente) — verificar el cuello de botella real primero (Ponytail: confirmar el problema antes de construir sobre él).

---

## 19. GAPS EXPLÍCITOS (cerrar antes de implementar)

- ⚠️ Licencias **no re-verificadas** en esta sesión: `esp_codec_dev`, `gmf_examples`, ESP-IDF/FatFS, libopus, esp-sr, llama.cpp/Ollama, mcp-obsidian, pesos Whisper, silero-vad, repo Waveshare, atomic14, ESP8266Audio, mem0, obsidian-git.
- ❓ **Pinout microSD** del board (SDSPI vs SDMMC).
- ❓ **Driver de audio** usado por xiaozhi-esp32 local (`esp_audio_codec` vs `esp_codec_dev`).
- ❓ **Compatibilidad ESP32-C6** de sherpa-onnx.
- ⚠️ **Touch IC:** FASE 1 dice `CST9217`, wiki Waveshare dice `CST9220`.
- ⚠️ **Precios** de APIs cloud y licencia comercial de Obsidian.
- ⚠️ **Marco legal** de grabación de clases.
- ⚠️ **RAM/TLS** en C6 con Wi-Fi + BLE simultáneos (Arquitectura C).
- ⚠️ **Soporte español de Moonshine** (licencia Community condicionada).

---

## 20. CONCLUSIÓN ESPECÍFICA

**¿Podemos convertir Hornerito en una grabadora inteligente con transcripción y memoria personal reutilizando software existente?**

**Sí.** El dispositivo necesita solo **captura (I2S ya existente) + WAV writer (pocas líneas) + cola en FATFS (manifest.json)**. Todo lo pesado (STT, LLM, Obsidian) ocurre **fuera del ESP32** en software maduro y libre (faster-whisper MIT, llama.cpp/Ollama, Local REST API MIT).

**¿Qué porcentaje reutilizaríamos?** **~75–85% global** (firmware 60–70%, host 80–90%).

**¿Qué hay que desarrollar?** Modo GRABADORA, WAV writer (~150–200 líneas), cola offline, orquestador host, plantilla Markdown. **Nada más.**

**Arquitectura recomendada:** **A (DUMP & SYNC)** como base → **C (HÍBRIDA/EDGE GATEWAY)** en R7. **B (CLOUD)** solo opcional. **On-device ASR 🔴 descartada** (512 KB RAM sin PSRAM).

---

## 21. RESUMEN DE CIERRE

1. **FASE 0.5 completada** (documentación de la investigación existente + 9 búsquedas puntuales de verificación).
2. **Proyectos/repositorios documentados:** 25 en la matriz + servicios cloud + proyectos similares ≈ **30+**.
3. **Búsquedas adicionales:** 9 puntuales (licencias concretas, wiki Waveshare, drivers ES7210/`esp_codec_dev`). Ninguna exhaustiva nueva.
4. **Pendiente de verificación:** pinout microSD, driver de audio del repo local, compat C6 de sherpa-onnx, discrepancia CST9217/CST9220, precios cloud, licencia comercial Obsidian, marco legal de grabación en aula, RAM/TLS con Wi-Fi+BLE, licencias menores (§19).
5. **Archivo modificado:** solo `HORNERITO_REUTILIZACION_FASE05.md`.
6. **No se modificó código ni firmware.**
