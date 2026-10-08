# HORNERITO
## Visión, funcionalidades y roadmap del proyecto

**Proyecto:** Hornerito  
**Hardware:** Waveshare ESP32-C6 Touch AMOLED 2.16"  
**Firmware base:** XiaoZhi / xiaozhi-esp32  
**Repositorio local:** `C:\OpenCode-Proyectos\Hornerito\xiaozhi-esp32`

---

# 1. ¿QUÉ ES HORNERITO?

Hornerito es un **asistente personal físico**, construido sobre una placa ESP32-C6 con pantalla AMOLED táctil, micrófonos, audio, almacenamiento, Wi-Fi, sensores y batería.

No queremos construir simplemente "un ESP32 con ChatGPT".

La idea es crear un pequeño dispositivo que tenga:

- personalidad;
- presencia física;
- voz;
- pantalla;
- memoria;
- almacenamiento;
- música;
- mascota virtual;
- herramientas;
- conectividad;
- interacción táctil;
- interacción por movimiento;
- capacidad de aprender del uso que hacemos de él.

Hornerito debe sentirse como un **objeto personal**, no como una placa de desarrollo.

---

# 2. PRINCIPIO GENERAL DEL PROYECTO

## Reutilizar antes que reinventar

La prioridad del desarrollo es reutilizar:

1. XiaoZhi.
2. ESP-IDF.
3. Componentes oficiales de Espressif.
4. Drivers existentes.
5. Librerías open source.
6. Proyectos existentes compatibles.

El código propio debe concentrarse principalmente en:

- integración;
- arquitectura;
- UI;
- estados;
- experiencia de usuario;
- grabadora;
- gestión de archivos;
- memoria;
- funcionalidades específicas de Hornerito.

No debemos desarrollar desde cero aquello que ya esté correctamente resuelto.

---

# 3. HARDWARE

## Placa

**Waveshare ESP32-C6 Touch AMOLED 2.16"**

Características conocidas:

- ESP32-C6 RISC-V
- single core
- hasta 160 MHz
- 16 MB Flash
- 512 KB HP SRAM
- 16 KB LP SRAM
- sin PSRAM externa
- Wi-Fi 6
- Bluetooth LE
- AMOLED 2.16"
- resolución 480×480
- pantalla táctil
- ES8311 audio codec
- ES7210 ADC / frontend de micrófonos
- dos micrófonos
- QMI8658 IMU
- AXP2101 PMIC
- microSD / TF

### Importante

El ESP32-C6 **no tiene Bluetooth Classic/A2DP**.

Por lo tanto:

- no se utilizará Bluetooth A2DP para reproducir música;
- la música estará almacenada localmente en microSD;
- en el futuro podría utilizarse hardware externo si se necesita Bluetooth Classic.

---

# 4. XIAOZHI COMO BASE

El firmware actual de referencia es:

`xiaozhi-esp32`

Repositorio:

`https://github.com/78/xiaozhi-esp32`

XiaoZhi será la base del asistente de voz.

No queremos reemplazarlo.

Queremos evolucionarlo hacia Hornerito.

La arquitectura conceptual es:

```text
                 HORNERITO
                     │
        ┌────────────┴────────────┐
        │                         │
   XiaoZhi Core              Hornerito Core
        │                         │
   Voz / IA                  UI / Pet / SD
   TTS / STT                  Recorder
   MCP                       Music
   Wi-Fi                     IMU
                             Files
                             Screensaver
```

La implementación real deberá respetar las estructuras existentes de XiaoZhi y extenderlas sin duplicar drivers ni propietarios de hardware.

---

# 5. ASISTENTE DE VOZ

Hornerito debe poder funcionar como asistente conversacional.

Funciones:

- activación por voz;
- wake word;
- escucha;
- reconocimiento de voz;
- envío a backend;
- LLM;
- respuesta;
- TTS;
- reproducción de respuesta;
- estados visuales durante la conversación.

Estados conceptuales:

```text
IDLE
  ↓
LISTENING
  ↓
THINKING
  ↓
SPEAKING
  ↓
IDLE
```

El procesamiento pesado no debe realizarse en el ESP32-C6.

---

# 6. ARQUITECTURA DE IA

El ESP32 funciona principalmente como:

**cuerpo + interfaz + captura + reproducción + conectividad.**

El procesamiento pesado queda fuera.

```text
HORNERITO
   │
   │ Wi-Fi
   ▼
GATEWAY / BACKEND
   │
   ├── STT
   ├── LLM
   ├── TTS
   ├── Google Calendar
   ├── ProfeOS
   ├── Obsidian
   └── otros servicios
```

Esto permite cambiar de modelo o proveedor sin tener que modificar el firmware principal.

---

# 7. PANTALLA AMOLED

La pantalla no será solamente un display de información.

Será parte de la personalidad de Hornerito.

Debe mostrar:

- Hornerito;
- estados emocionales;
- animaciones;
- respuestas;
- reloj;
- música;
- grabación;
- batería;
- Wi-Fi;
- errores;
- configuración;
- juegos;
- información contextual.

La interfaz deberá diseñarse específicamente para 480×480 y touch.

---

# 8. PROTECTOR DE PANTALLA / RELOJ

## Función oficial de Hornerito

Cuando Hornerito esté inactivo durante un período configurable:

```text
IDLE
 ↓
SCREENSAVER
```

El protector de pantalla será un **reloj visual**, no una pantalla negra.

### Concepto

Un reloj grande y elegante acompañado por Hornerito.

Puede incluir:

- hora;
- fecha;
- Hornerito;
- animación;
- estado de batería;
- Wi-Fi;
- pequeños indicadores.

### Personalidad

Hornerito puede:

- estar sentado;
- dormir;
- mirar la hora;
- hacer pequeños movimientos;
- cambiar de expresión;
- reaccionar al movimiento;
- tener diferentes estados según el horario.

### Despertar

El protector puede finalizar mediante:

- touch;
- movimiento;
- interacción por voz;
- botón/evento físico si corresponde.

Arquitectura:

```text
IDLE
  │
  │ timeout
  ▼
SCREENSAVER_CLOCK
  │
  ├── touch
  ├── movimiento
  └── voz
  │
  ▼
WAKE
  │
  ▼
IDLE
```

El protector de pantalla debe ser una parte normal de la FSM del firmware.

---

# 9. TOUCH Y GESTOS

Queremos utilizar el touch como parte central de la UX.

La navegación deberá ser sencilla.

Gestos previstos:

- tap;
- swipe izquierda;
- swipe derecha;
- swipe arriba;
- swipe abajo.

Especialmente:

```text
← → 
```

para desplazarse entre funciones.

La idea es evitar menús profundos.

Hornerito debe poder manejarse con pocos gestos.

---

# 10. MÚSICA

Hornerito tendrá reproductor de música local.

Los archivos estarán en la microSD.

Formato inicial:

```text
/SD/Music/
```

Ejemplo:

```text
Music/
├── canción1.mp3
├── canción2.mp3
├── álbum/
│   ├── tema01.mp3
│   └── tema02.mp3
└── ...
```

Funciones:

- explorar canciones;
- reproducir;
- pausar;
- continuar;
- siguiente;
- anterior;
- volumen;
- reproducción desde SD;
- mostrar información básica;
- interfaz visual.

---

# 11. ¿CÓMO CARGAMOS MÚSICA?

Queremos solamente **dos métodos**.

## Método 1 — USB

Conectar Hornerito mediante cable USB a la PC.

La PC deberá poder reconocer el almacenamiento y permitir administrar la microSD.

Idealmente:

```text
Windows
   │
   │ USB
   ▼
HORNERITO
   │
   └── microSD
       └── Music/
```

El objetivo es poder:

- conectar;
- abrir el almacenamiento;
- arrastrar MP3;
- desconectar;
- escuchar.

No queremos depender de sacar físicamente la microSD.

La implementación exacta de USB Mass Storage deberá verificarse técnicamente antes de desarrollarla.

---

# 12. CARGA DE MÚSICA POR WI-FI

También queremos poder administrar la música mediante Wi-Fi.

Hornerito ofrecerá una interfaz web sencilla.

Desde:

- PC;
- notebook;
- celular;
- tablet;

se podrá acceder a Hornerito y cargar archivos.

Conceptualmente:

```text
Dispositivo
     │
     │ Wi-Fi
     ▼
Hornerito Web UI
     │
     ▼
microSD /Music/
```

Funciones mínimas:

- subir MP3;
- listar canciones;
- eliminar canciones;
- organizar carpetas.

No queremos una aplicación móvil obligatoria.

---

# 13. ADMINISTRACIÓN DE ARCHIVOS

La microSD tendrá una estructura organizada.

Propuesta:

```text
/SD
├── Music/
├── Recordings/
├── Emojis/
├── Themes/
└── Config/
```

USB y Wi-Fi deberán utilizar una **misma capa lógica de gestión de archivos**, evitando tener dos sistemas independientes.

---

# 14. GRABADORA INTELIGENTE

Una de las funcionalidades principales de Hornerito será convertirse en una grabadora inteligente.

Puede utilizarse para:

- reuniones;
- clases;
- conversaciones;
- ideas;
- notas de voz;
- proyectos;
- tareas;
- decisiones;
- recordatorios;
- información personal.

El dispositivo debe poder grabar sin depender de Internet.

Arquitectura:

```text
Micrófonos
    ↓
ES7210
    ↓
I2S
    ↓
Buffer / DMA
    ↓
WAV
    ↓
microSD
```

La investigación previa concluyó que el ESP32-C6 debe funcionar principalmente como **máquina de captura y almacenamiento**, mientras que la transcripción se realiza externamente.

---

# 15. FORMATO DE GRABACIÓN

Primera versión:

- PCM;
- 16 bit;
- 16 kHz;
- WAV;
- 2 canales / estéreo;
- almacenamiento en microSD.

No optimizar prematuramente.

Primero:

**WAV robusto y confiable.**

Posteriormente se puede evaluar Opus si resulta necesario.

---

# 16. SEGMENTACIÓN DE GRABACIONES

Las grabaciones deberán dividirse en segmentos.

Propuesta inicial:

```text
60 segundos por archivo
```

Esto permite:

- recuperación ante errores;
- procesamiento incremental;
- evitar archivos gigantes;
- reintentos;
- sincronización parcial.

---

# 17. COLA DE GRABACIONES

La grabadora utilizará una cola offline.

Directorios:

```text
/REC/pending/
/REC/processed/
/REC/failed/
```

Además:

```text
manifest.json
```

El manifest permitirá conocer:

- ID de grabación;
- fecha;
- duración;
- estado;
- archivo;
- hash;
- intentos;
- procesamiento.

Estados conceptuales:

```text
RECORDED
   ↓
PENDING
   ↓
PROCESSING
   ↓
PROCESSED
```

o:

```text
PENDING
   ↓
FAILED
   ↓
RETRY
```

Debe existir idempotencia para evitar procesar dos veces la misma grabación.

---

# 18. TRANSCRIPCIÓN

La transcripción NO se realizará en el ESP32-C6.

Opciones previstas:

### Primera opción

**faster-whisper**

### Alternativas

- whisper.cpp;
- WhisperX;
- Vosk;
- sherpa-onnx.

Primera versión:

**transcripción simple.**

No implementar diarización desde el principio.

---

# 19. DIARIZACIÓN

En una segunda etapa Hornerito podrá identificar diferentes hablantes.

Ejemplo:

```text
Hablante 1:
...

Hablante 2:
...

Hablante 1:
...
```

Posibles herramientas:

- WhisperX;
- pyannote.

Esto queda para una fase posterior.

---

# 20. PROCESAMIENTO CON IA

Una vez transcrito el audio:

```text
WAV
 ↓
STT
 ↓
TRANSCRIPT
 ↓
LLM
 ↓
ESTRUCTURA
 ↓
MARKDOWN
```

El LLM podrá extraer:

- resumen;
- temas;
- decisiones;
- tareas;
- fechas;
- ideas;
- preguntas;
- participantes;
- información importante.

---

# 21. MEMORIA PERSONAL

Una de las ideas centrales de Hornerito es que no sea solamente un asistente que responde.

Debe poder convertirse progresivamente en una **memoria personal**.

Ejemplos:

> "¿Qué hablé ayer con Juan?"

> "¿Qué idea tuve el martes?"

> "¿Qué tareas quedaron pendientes?"

> "¿Qué decidimos en la reunión?"

> "¿Cuándo dije que iba a hacer esto?"

La memoria no tiene que vivir necesariamente dentro del ESP32.

---

# 22. OBSIDIAN COMO MEMORIA

La capa de memoria personal estará basada en **Obsidian**.

Arquitectura:

```text
Hornerito
   ↓
Audio
   ↓
STT
   ↓
LLM
   ↓
Markdown
   ↓
Obsidian
```

Las notas pueden incluir:

```markdown
---
title:
date:
duration:
tags:
participants:
source:
---

# Resumen

# Temas

# Decisiones

# Tareas

# Fechas

# Ideas

# Preguntas

# Transcripción
```

Primera integración:

**Obsidian URI**

Segunda etapa:

**Obsidian Local REST API**

---

# 23. MODO OFFLINE-FIRST

Hornerito debe seguir siendo útil sin Internet.

Ejemplo:

```text
GRABAR
 ↓
GUARDAR EN SD
 ↓
ESPERAR
 ↓
CUANDO HAYA WI-FI
 ↓
PROCESAR
 ↓
GUARDAR RESULTADO
```

No perder una grabación porque no había conexión.

La conectividad debe ser una capacidad adicional, no un requisito para la captura.

---

# 24. WIFI

El Wi-Fi tendrá dos funciones principales.

## Uso normal

- asistente;
- STT;
- LLM;
- TTS;
- sincronización;
- servicios;
- calendario;
- ProfeOS.

## Configuración

Queremos que configurar Hornerito sea lo más sencillo posible.

La configuración inicial debe ser accesible incluso para alguien que no sabe nada de ESP32.

El Wi-Fi de configuración no debe convertirse en una experiencia de "placa de desarrollo".

---

# 25. BLUETOOTH

Bluetooth será BLE.

Usos posibles:

- configuración;
- comunicación con dispositivos compatibles;
- sensores;
- transferencia de información;
- futuras funcionalidades.

No se utilizará para A2DP porque el ESP32-C6 no soporta Bluetooth Classic.

---

# 26. CALENDARIO

Hornerito deberá poder integrarse con Google Calendar.

Funciones futuras:

- consultar próximos eventos;
- preguntar qué hay hoy;
- preguntar qué hay mañana;
- crear eventos;
- modificar eventos;
- recordatorios.

La lógica de Google Calendar deberá vivir principalmente fuera del ESP32.

El dispositivo utilizará herramientas/API/MCP para acceder a ella.

---

# 27. PROFEO​S

Hornerito deberá poder integrarse posteriormente con ProfeOS.

Posibles usos:

- consultar información;
- recordatorios docentes;
- tareas;
- clases;
- información de cursos;
- asistencia;
- calendario académico.

La integración deberá diseñarse como servicio externo y no cargar toda la lógica de ProfeOS dentro del firmware.

---

# 28. MCP

MCP será una de las capas para que Hornerito pueda utilizar herramientas.

Ejemplos conceptuales:

```text
calendar.*
profeos.*
recorder.*
music.*
memory.*
```

La herramienta:

```text
recorder.start
recorder.stop
recorder.status
```

puede permitir controlar la grabadora desde el asistente.

---

# 29. ESTADOS DEL SISTEMA

La arquitectura debe utilizar una FSM clara.

Estados iniciales previstos:

```text
BOOT
INIT
IDLE
LISTENING
THINKING
SPEAKING
RECORDING
PROCESSING
PLAYING
SCREENSAVER
OTA
ERROR
```

No queremos una colección de callbacks y flags globales sin estructura.

---

# 30. ARBITRAJE DEL AUDIO

XiaoZhi y la grabadora compartirán:

- ES8311;
- ES7210;
- I2S;
- DMA.

Por lo tanto deberá existir un único propietario del sistema de audio.

Conceptualmente:

```text
MODO_ASISTENTE
MODO_RECORDER
```

Posiblemente posteriormente:

```text
MODO_DUAL
```

pero el modo dual no debe asumirse funcional hasta probarlo.

Transición:

```text
ASISTENTE
 ↓
suspender audio
 ↓
configurar recorder
 ↓
RECORDING
 ↓
cerrar WAV
 ↓
liberar recorder
 ↓
restaurar audio
 ↓
ASISTENTE
```

---

# 31. IMU

El QMI8658 se utilizará para interacción física.

Usos futuros:

- detección de movimiento;
- gestos;
- juegos;
- mascota virtual;
- reacción de Hornerito;
- acciones contextuales.

Ejemplo:

Hornerito está dormido.

El usuario mueve el dispositivo.

```text
IMU
 ↓
detecta movimiento
 ↓
Hornerito despierta
 ↓
SCREENSAVER → IDLE
```

---

# 32. MASCOTA VIRTUAL

Hornerito no debe ser solamente un avatar estático.

Queremos una mascota virtual con:

- expresiones;
- animaciones;
- estados;
- sueño;
- alegría;
- atención;
- escucha;
- pensamiento;
- habla;
- sorpresa;
- aburrimiento;
- reacción al movimiento.

La mascota debe integrarse con:

- asistente;
- reloj;
- música;
- juegos;
- IMU;
- batería;
- hora del día.

---

# 33. EMOJIS Y ASSETS

Hornerito utilizará assets gráficos propios.

Los emojis y expresiones estarán organizados en la SD o en el firmware según convenga.

Estructura conceptual:

```text
Emojis/
├── basic/
├── master/
└── ...
```

Los assets deben poder evolucionar sin tener que reconstruir todo el firmware cuando sea posible.

---

# 34. MÚSICA + MASCOTA

El reproductor de música puede integrarse con la mascota.

Por ejemplo:

```text
PLAYING MUSIC

Hornerito
├── escucha
├── baila
├── mueve el cuerpo
└── reacciona al ritmo
```

No es prioridad para la primera versión, pero forma parte de la visión.

---

# 35. JUEGOS

Hornerito tendrá juegos simples.

La IMU permitirá utilizar movimiento como control.

Posibles categorías:

- juegos casuales;
- juegos de reacción;
- juegos de movimiento;
- mini juegos de mascota.

Los juegos deben ser livianos y adecuados al hardware.

---

# 36. OTA

El firmware debe poder actualizarse por OTA.

Objetivos:

- actualizar sin conectar cable;
- mantener una versión anterior recuperable cuando sea posible;
- detectar errores;
- mostrar progreso;
- evitar dejar el dispositivo inutilizable.

OTA será parte del hardening final.

---

# 37. CONFIGURACIÓN

La configuración debe ser accesible para una persona normal.

No queremos que el usuario tenga que:

- abrir un IDE;
- conocer ESP-IDF;
- configurar puertos serie;
- modificar archivos;
- editar JSON manualmente.

Las funciones avanzadas de desarrollador pueden quedar ocultas.

---

# 38. USB

El USB tendrá dos posibles usos principales:

### Usuario

- transferencia de archivos;
- administración de música;
- mantenimiento.

### Desarrollo

- serial;
- logs;
- actualización;
- debugging.

La interfaz deberá separar claramente ambos usos cuando sea necesario.

---

# 39. WEB UI

Hornerito tendrá una pequeña interfaz web accesible mediante Wi-Fi.

Funciones iniciales:

- cargar música;
- administrar archivos;
- configuración básica;
- información del dispositivo.

Funciones futuras:

- gestión de grabaciones;
- estado de sincronización;
- actualización;
- emojis/themes;
- configuración de servicios;
- diagnóstico.

---

# 40. ESTRUCTURA DE LA MICROSD

Propuesta:

```text
/
├── Music/
│   ├── ...
│
├── Recordings/
│   ├── pending/
│   ├── processed/
│   └── failed/
│
├── Emojis/
│   ├── basic/
│   └── master/
│
├── Themes/
│
└── Config/
```

La estructura definitiva deberá adaptarse a las necesidades reales del firmware.

---

# 41. PRIVACIDAD

La grabadora introduce una responsabilidad especial.

Principios:

- indicador visual claro cuando se está grabando;
- el usuario debe saber cuándo está grabando;
- procesamiento local cuando sea posible;
- cloud opcional;
- no subir automáticamente audio sin autorización;
- almacenamiento local como primera opción;
- posibilidad de eliminar grabaciones;
- evitar secretos permanentes dentro del firmware;
- conexiones externas mediante TLS.

La legalidad de grabar conversaciones/clases deberá analizarse específicamente antes de distribuir esta función públicamente.

---

# 42. ARQUITECTURA DE PRIVACIDAD

Preferencia:

```text
LOCAL
 ↓
SD
 ↓
PC / SERVIDOR LOCAL
 ↓
procesamiento
 ↓
Obsidian
```

Cloud:

```text
OPCIONAL
```

No debe ser obligatorio para que la grabadora funcione.

---

# 43. FUTURO HARDWARE DE AUDIO

A futuro se contempla una salida de audio de 3.5 mm.

No forma parte del hardware actual de la placa.

Por lo tanto:

- no asumir que existe;
- estudiar solución externa;
- mantener la arquitectura de audio preparada para una futura salida.

---

# 44. ARQUITECTURA GENERAL

La visión completa puede representarse así:

```text
                         HORNERITO
                              │
       ┌──────────────────────┼──────────────────────┐
       │                      │                      │
     INPUT                   UI                    AUDIO
       │                      │                      │
 ┌─────┴─────┐         ┌─────┴─────┐         ┌──────┴──────┐
 │ Mic       │         │ AMOLED    │         │ ES8311      │
 │ Touch     │         │ Touch     │         │ ES7210      │
 │ IMU       │         │ Pet       │         │ Speaker     │
 └───────────┘         │ Clock     │         └─────────────┘
                       │ Music     │
                       └───────────┘
                              │
                       ┌──────┴──────┐
                       │   CORE      │
                       │             │
                       │ FSM         │
                       │ XiaoZhi     │
                       │ Recorder    │
                       │ Music       │
                       │ Storage     │
                       │ BLE         │
                       │ Wi-Fi       │
                       │ OTA         │
                       └──────┬──────┘
                              │
                    ┌─────────┴─────────┐
                    │      microSD     │
                    └─────────┬─────────┘
                              │
                         Wi-Fi / USB
                              │
                    ┌─────────┴─────────┐
                    │   PC / BACKEND    │
                    │                   │

                    # Estructura unificada de almacenamiento

Toda la información administrada por Hornerito debe almacenarse dentro de una única raíz:

/hornerito/

Estructura inicial:

/hornerito/
├── music/
├── recordings/
│   ├── pending/
│   ├── processed/
│   └── failed/
├── emojis/
│   ├── basic/
│   └── master/
├── themes/
└── config/

## Reglas

- No crear rutas paralelas fuera de `/hornerito/`.
- USB y Wi-Fi deben utilizar exactamente esta misma estructura.
- La aplicación de música utiliza `/hornerito/music/`.
- La grabadora utiliza `/hornerito/recordings/`.
- La cola de procesamiento utiliza `pending`, `processed` y `failed`.
- Los assets gráficos utilizan `/hornerito/emojis/`.
- Los temas utilizan `/hornerito/themes/`.
- La configuración persistente utiliza `/hornerito/config/`.

La capa de gestión de archivos debe centralizar estas rutas para que ningún módulo del firmware tenga que definirlas independientemente.