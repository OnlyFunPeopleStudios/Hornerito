# HORNERITO — Medición y caracterización del movimiento (QMI8658)

**Fecha:** 2026-10-04 · **Firmware:** App version 2.2.0 (base V2.2.0 intacta)
**Alcance:** MEDICIÓN/characterización. **Calibración NO realizada** — esta es la
captura de datos reales y una **propuesta de umbrales NO aplicada**, pendiente de aprobación.

---

## 1. Matriz PASS/FAIL

| Ítem | Resultado |
|---|---|
| Instrumentación (`kCalibrationLog`, línea MOTION cada 250 ms + dump `MOTION regs`) | **PASS** |
| Build `idf.py build`: 0 errores / 0 warnings | **PASS** |
| `App version: 2.2.0` verificado en consola | **PASS** |
| Flash SIN erase, offsets de `build\flasher_args.json`, COM4 → **5/5 Hash of data verified** | **PASS** |
| Fix `CTRL1=0x40` efectivo en hardware (ADDR_AI=1) | **PASS** |
| Captura de reposo válida (14 min, datos reales) | **PASS** |
| Captura de los tests 2–5 (tilt X, tilt Y, movimiento, shake) | **PASS** |
| Shake detectado por el firmware actual (`kShakeDeltaG=1.2`) | **FAIL** — 0 eventos en toda la sesión |
| `tilt_x`/`tilt_y` utilizables en reposo | **FAIL** — saturados en ±1.00 |
| Calibración aplicada | **N/A** — no solicitada (solo propuesta, §6) |

**ESTADO: REQUIERE CORRECCIÓN** — la medición está completa, pero el firmware actual
no detecta shake y entrega tilt saturado. Correcciones propuestas en §6, **NO aplicadas**.

---

## 2. Setup de medición

- Muestreo real: ODR 125 Hz / poll 20 ms (50 Hz); línea resumida `MOTION` cada 250 ms.
- Una línea por ventana: raw (g, dps) + filtrado EMA (α=0.2) + `tilt_x/tilt_y` + `shake` + `regs` (bytes crudos + CTRL1/2/3/7/ST0).
- Causa raíz del dato basura original corregida y verificada: **CTRL1 (0x02) nunca se escribía → ADDR_AI=0 → ráfaga I2C devolvía el mismo byte repetido → todos los ejes = `0xBBBB`** (evidencia: sesión `motion_measure.log`, 1491 líneas con ejes idénticos). Fix: `WriteReg(kRegCtrl1, 0x40)` tras el reset.
- Verificado en silicio: `ctrl1=40 ctrl2=16 ctrl3=76 ctrl7=03 st0=03`, 6 bytes distintos por ráfaga (ej. `acc[35]=03 00 4E FC F5 DC` → ax=+0.0004, ay=−0.1155, az=−1.095 g ✓).

**Logs:**
| Archivo | Tamaño | Contenido |
|---|---|---|
| `%TEMP%\opencode\motion_measure.log` | 672.660 B | Sesión pre-fix: datos basura `0xBBBB` (evidencia) |
| `%TEMP%\opencode\motion_measure2.log` | 1.108.631 B | **Test 1 — reposo**, 14 min válidos (3286 líneas) |
| `%TEMP%\opencode\motion_measure5.log` | 353.832 B | **Tests 2–5**, 1012 líneas, t=36.9–300.2 s |
| `motion_measure3/4.log` | ~20 KB | Ventanas vacías: reinicio sin rearmar (Pet) — dato de flujo |

---

## 3. Tabla de resultados

### 3.1 Test 1 — Reposo (sesión 2, minutos 2–13, ~12 min estables)

| Magnitud | Valor medido |
|---|---|
| ax | **−0.03 … +0.03 g** (ruido ±0.01) |
| ay | **−0.13 … −0.11 g** (offset estable −0.12 ≈ 6–7° o offset de sensor) |
| az | **−1.13 … −1.06 g** (gravedad en **−Z**; \|g\| aparente 1.09–1.10 → **escala +9–10% sin calibrar**) |
| gx | **−7.3 … −5.4 dps** (bias ≈ −7, sin deriva apreciable en 14 min) |
| gy | **+2.2 … +4.6 dps** (bias ≈ +3.5) |
| gz | ≈ +2.1 … +2.5 dps |
| ‖a‖ filtrada | **1.09 … 1.12** |
| tilt_x / tilt_y | **−1.00 / −1.00 SATURADOS** (por az<0, ver §5) |
| shake (firmware) | **0 eventos** en 14 min ✓ |

Sesión 5 corrobora: `fnrm` reposo 1.09–1.13, az −1.19…−1.04, gx −14…+12 (picos de bump táctil), gy estable.

### 3.2 Tests 2–5 (sesión 5, `motion_measure5.log`)

| Estado | Ventana (t) | ax (g) | ay (g) | az (g) | ‖a‖ raw máx | ‖a‖ filtrada | shake fw |
|---|---|---|---|---|---|---|---|
| Tilt X — izquierda | ~46.2 s | **−1.02** | −0.12 | −0.13 | 1.13 | 0.98–1.11 | 0 |
| Tilt X — derecha | ~49.8 s | **+1.01** | −0.13 | −0.11 | " | " | 0 |
| Tilt Y — adelante | ~51.6 s | −0.03 | **−1.14** | −0.16 | 1.41 | 0.87–1.23 | 0 |
| Tilt Y — atrás | ~54.0 s | 0.00 | **+0.84** | −0.29 | " | " | 0 |
| Movimiento normal (2 tramos: vigoroso 55–68 s / suave 68–79 s*) | 55–79 s | −1.47…1.01 | −1.13…1.48 | −2.27…−0.10 | **2.44** / 1.27 | 0.90–2.14 | 0 |
| **Shake intencional** | 79–85 s | **−3.35**…0.80 | −0.70…1.86 | −1.86…−0.26 | **4.26** | **0.89–1.28** | **0** ✗ |

\* Las ventanas 55–79 s incluyen la repetición de los tests de tilt (segundo pase) y un tramo de
manejo vigoroso; la separación exacta test4/test5 no es deducible sin marcas de tiempo del operador.
El shake (79–85 s) es inequívoco: pico crudo 4.26 g con cambios de signo rápidos.

### 3.3 Signos de ejes (según orden de ejecución indicado)

| Eje | Signo | Evidencia |
|---|---|---|
| X | **ax − = inclinar izquierda**, **ax + = derecha** | −1.02 (46.2 s) / +1.01 (49.8 s) |
| Y | **ay − = adelante**, **ay + = atrás** | −1.14 (51.6 s) / +0.84 (54.0 s) |
| Z | **az = −1.10 en reposo** → gravedad en −Z (chip Z hacia abajo) | minutos 2–13 |
| Giro | tilt X ↔ **gy** (roll sobre Y), tilt Y ↔ **gx** (pitch sobre X) | parejas observadas coherentes |

### 3.4 Ruido y deriva

- **Aceleración:** ruido ±0.01 g (ax/ay) y ±0.02–0.03 g (az); **deriva ≈ 0** en 14 min (az −1.09/−1.10 constante).
- **Offset sistemático:** ay0 = −0.12 g; escala Z aparente 1.09–1.10 g (+9–10%).
- **Giroscopio:** bias gx ≈ −7 dps, gy ≈ +3.5 dps; deriva lenta ±2–5 dps en minutos (típico MEMS sin calibrar). Gyro no lo usan los juegos.

---

## 4. Umbral actual de shake (vigente, NO modificado)

```cpp
// hornerito_motion.cc:65-67, 250
constexpr float kShakeDeltaG = 1.2f;          // |‖a‖ − 1g| > 1.2 g
constexpr uint32_t kShakeRefractoryMs = 400;
constexpr float kEmaAlpha = 0.2f;             // EMA sobre acc_, 50 Hz
if (std::fabs(norm - 1.0f) > kShakeDeltaG && now - last_shake_tick_ > kShakeRefractoryMs)
```

Se aplica sobre **‖a‖ de la EMA** (α=0.2 @ 50 Hz). Requiere **‖a‖_filtrada > 2.2 g**.

**Simulación sobre la sesión completa (refractario 400 ms):**

| delta sobre filtrada | reposo | tilt X | tilt Y | movimiento | shake | total |
|---|---|---|---|---|---|---|
| 0.3 – 0.7 | 0 | 0 | 0 | 3 (FP) | **0** | 3 |
| 0.8 | 0 | 0 | 0 | 2 (FP) | **0** | 2 |
| 1.0 – 1.14 | 0 | 0 | 0 | 1 (FP) | **0** | 1 |
| **1.2 (actual)** | 0 | 0 | 0 | 0 | **0** | **0** |

**Conclusión dura:** la EMA de α=0.2 aplasta el shake (pico crudo 4.26 g → filtrado 1.28 g;
|Δ| = 0.28) mientras el reposo vive en |Δ| = 0.09–0.13. **Ningún delta sobre la EMA actual
distingue shake de reposo con margen útil** — el umbral 1.2 jamás se alcanza (máximo global
filtrado = 2.14, y ocurrió en el manejo violento, no en el shake).

**Normas crudas por estado** (la referencia que sí separa):

| reposo | tilt X | tilt Y | movimiento suave | manejo vigoroso | **shake** |
|---|---|---|---|---|---|
| ≤ 1.24 | ≤ 1.13 | ≤ 1.41 | ≤ 1.27 | 2.44 | **4.26** |

---

## 5. Conclusiones

1. **IMU sano tras el fix CTRL1:** bytes distintos, config correcta en silicio (`ctrl1=40`,
   `ctrl2=16` ±4 g/125 Hz, `ctrl3=76`, `ctrl7=03`, `st0=03`), valores físicamente coherentes.
2. **Shake NO funciona hoy:** 0 eventos en toda la medición; el umbral exige 2.2 g filtrados y
   el shake real alcanza solo 1.28 g filtrados (4.26 g crudos). Problema de **filtro + umbral**, no del sensor.
3. **Tilt NO funciona hoy:** en reposo `az<0` hace `atan2(ax,az)≈±180°` → `tilt_x/tilt_y`
   saturados en ±1.00 permanentemente (los juegos de Pong/Breakout recibirían full-deflection).
   La fórmula asume `az>0`; este montaje tiene el chip con Z hacia abajo.
4. **Ejes bien definidos** (§3.3) y ruido/deriva aptos: el sensor sirve; lo que falta es el mapeo.
5. **Hallazgo de flujo (no modificado):** solo Pet llama `Enable()` (`hornerito_pet.cc:163`);
   Pong/Breakout/Flappy leen `tilt_x()/tilt_y()` sin armar el muestreo en sesión limpia.
6. **Hallazgo anotado (no modificado):** `kGyroScale` con enum ambiguo entre referencias;
   sin impacto — los juegos no usan gyro.

---

## 6. Propuesta de umbrales — **SOLO RECOMENDACIÓN, NO APLICADA**

Aplicar solo tras aprobación explícita.

1. **Shake → usar la norma CRUDA (o EMA con α≥0.6 en la ruta de shake):**
   `|‖a‖raw − 1| ≥ 1.5` (es decir ‖a‖ ≥ **2.5 g**), refractario 400 ms conservado.
   - Shake observado dispara con holgura (4.26 vs 2.5); **cero falsos**: reposo ≤1.24, tilt ≤1.41,
     movimiento suave ≤1.27, manejo vigoroso ≤2.44.
   - *Alternativa más sensible:* `≥1.2` sobre cruda (‖a‖≥2.2) — acepta disparo en manejo vigoroso (2.44).
   - *Riesgo a validar:* margen manejo/shake = 2.44 vs 2.5 (estrecho); si en la fase de corrección
     un shake suave no llega a 2.5, bajar a 2.2 (delta 1.2 crudo).
2. **Tilt → referencia −az con autocalibración en reposo:**
   `tilt_x = atan2(ax, −az) / 45°`, `tilt_y = atan2(ay − ay0, −az) / 45°`, con `ay0 ≈ −0.12`
   (offset medido) y `az0 ≈ −1.10` (opcional: normalizar por \|g\| medido para corregir el +10% de escala).
   - Validar el sentido final con `kTiltXSign`/`kTiltYSign` (hoy `+1.0f`, `hornerito_motion.cc:61-62`)
     contra la lógica de Pong/Breakout.
   - Con los datos de §3.2 esto da tilt_x: 0 en reposo, ≈−1.0 izq, ≈+1.0 der; tilt_y: ≈−0.14 reposo, ≈−0.97 adelante, ≈+0.9 atrás.

---

## 7. Archivos modificados (instrumentación + fix, TEMPORAL)

| Archivo | Cambio |
|---|---|
| `hornerito/motion/hornerito_motion.cc` | Fix `kRegCtrl1=0x02` + `kCtrl1Value=0x40` en `ProbeAndConfigure`; `constexpr bool kCalibrationLog = true` (l.75); dump `MOTION regs` + línea `MOTION raw…` cada 250 ms en `PollOnce` (l.253, 264); costado `calib_shake_seen_`; `Disable()` conserva el timer con `kCalibrationLog` (l.193); dump temporal de bytes crudos |
| `hornerito/motion/hornerito_motion.h` | Miembros temporales `calib_last_log_tick_`, `calib_shake_tick_`, `calib_shake_seen_` (l.76 — **a quitar tras la fase de calibración**, junto con `kCalibrationLog`) |

No se tocó: umbrales de tilt/shake, lógica de juegos, UI, audio, Wi-Fi/TLS, driver QMI8658
(más allá del fix CTRL1 aprobado), nada borrado, sin `erase_flash`, SD intacta, V2.2.0 de base.
