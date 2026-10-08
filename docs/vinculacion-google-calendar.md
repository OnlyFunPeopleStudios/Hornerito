# Google Calendar en HORNERITO — vinculación sin backend

> FASE 0.5 (2026-10-06): desmontaje de la implementación anterior (Supabase)
> + nueva solución. Este documento reemplaza a `google-calendar-pairing.md`.

## 1. Arquitectura actual: QR → página del propio equipo → URL secreta iCal

**Cero backend permanente. Cero OAuth. Cero tokens. Cero secretos en firmware.**

```
 Configuración → Calendario → "Vincular Google Calendar"
        │
        ▼
 Pantalla QR del equipo ──── QR = http://<ip-del-equipo>:8080/pair
        │                            │ (misma Wi-Fi)
        ▼                            ▼
 Celular escanea ──────────▶ página servida POR EL EQUIPO (LAN)
                                    │
                                    │ 1) link a Google Calendar → Ajustes
                                    │ 2) calendario → "Dirección secreta
                                    │    en formato iCal" → Copiar
                                    │ 3) Pegar + Guardar
                                    ▼
                            POST /pair_set ──▶ NVS (clave "url")
                                                │
                                                ▼
                                     SyncTask iCal existente
                                     (GET HTTPS + parser VEVENT + caché)
```

- **Qué es la URL secreta iCal**: mecanismo oficial de exportación de Google
  Calendar (`https://calendar.google.com/calendar/ical/<email>/private-<token>/basic.ics`).
  Da lectura de los eventos de ese calendario — lo mismo que ofrecía el backend
  anterior, sin intermediarios.
- **El QR no lleva secretos**: solo la dirección LAN del equipo.
- **La URL secreta** se trata como contraseña: va solo en NVS, nunca se imprime
  por UART ni en logs (solo códigos de estado y tamaños).
- **Fallback manual**: *Configuración → Calendario → Configurar iCal* (teclado
  del equipo, para quien prefiera o no tenga celular a mano).

### Endpoints (servidor HTTP del propio equipo, :8080)

| Método | Ruta | Qué hace |
| --- | --- | --- |
| GET  | `/pair` | Página móvil con los 3 pasos (`?ok=1` banner de éxito, `?err=1` de error). |
| POST | `/pair_set` | `u=<url>` (form-urlencoded). Valida `https?://` y ≤500 car., guarda con `SetUrl` + `RequestSync`, responde 303 → `/pair?ok=1`. |

- Tope de cuerpo 1600 bytes; decodificación `%XX`; sin JS necesario.
- Ambos servidos por `hornerito/transfer/hornerito_transfer.cc` (mismo servidor
  de subida de archivos; arranca en boot).

### Seguridad

- Accesible solo desde la LAN del equipo (como la subida de archivos ya existente).
- La URL iCal viaja celular → equipo directamente, nunca por servicios de terceros.
- Sin client_id/client_secret/tokens en el firmware; sin tablas en nube.

## 2. Por qué NO se usó OAuth (investigación, fuente oficial)

| Vía | Resultado |
| --- | --- |
| **OAuth Device Flow** (TV/limited-input) | ❌ Scopes permitidos SOLO: `email`, `openid`, `profile`, `drive.appdata`, `drive.file`, `youtube`, `youtube.readonly`. **`calendar.readonly` no está en la lista** — [Allowed scopes](https://developers.google.com/identity/protocols/oauth2/limited-input-device#allowedscopes) (doc. actualizada 2026-09-14). |
| **OAuth web (code flow)** | ❌ El canje de código exige `client_secret` → obliga a un **backend permanente** que guarde el secreto y los refresh tokens (justo lo que se quiere eliminar). El secret en firmware es extraíble del flash. |
| **Google Identity Services (JS en el celular)** | ❌ Entrega solo `access_token` de 1 h, **sin refresh_token** → la sincronización periódica desde el equipo sería imposible. |
| **Feed iCal secreto de Google** | ✅ Oficial, sin backend, sin límites de OAuth, compatible 1:1 con el pipeline iCal existente. |

**Conclusión**: la dirección secreta iCal es la única vía que cumple "sin backend
permanente" para esta API. Si en el futuro se quiere login OAuth de verdad, el
camino es un backend mínimo *stateless* (p. ej. un worker serverless) que retenga
`client_secret` + refresh tokens — requiere crear un cliente OAuth nuevo y
mantener hosting, y se decidió no hacerlo por ahora.

## 3. Desmontaje de la implementación anterior (FASE 0.5)

### Eliminado

| Recurso | Estado |
| --- | --- |
| Tablas `hornerito_config`, `hornerito_sessions`, `hornerito_links` | ✅ `DROP TABLE` (verificado: 0 tablas `hornerito%`; sin FKs/vistas/funciones que las referenciaran; eran exclusivas: creadas por la migración propia). |
| Migración `hornerito_google_pairing` (20261006223202) | ✅ Fila borrada de `supabase_migrations.schema_migrations` (también borró los INSERT con credenciales que contenía). Las otras 5 migraciones del proyecto son de ProfeOS: **no se tocaron**. |
| Edge Function `hornerito-pair` (única del proyecto profeos, creada para esta implementación) | ⚠️ **Código y credenciales eliminados**: redespuesta como tumba que responde `410 {"ok":false,"gone":true}` (verificado con curl). **Borrado definitivo pendiente** porque mi token local no tiene `edge_functions_write` (403) y no hay sesión en el Dashboard. *Acción del dueño*: Dashboard → profeos → Edge Functions → `hornerito-pair` → Delete, o CLI con token que tenga ese permiso. |
| `docs/google-calendar-pairing.md` | ✅ Borrado (reemplazado por este). |
| Sección README del sistema viejo | ✅ Reescrita. |
| Firmware: `kPairUrl`, task `cal_pair`/`PairTask`/`PairPost`, estados `PairState/PairOp/PairResult`, NVS `gkey`/`gnames`, pantallas QR+Vincular+Desvincular del sistema viejo | ✅ Extraídos; `CalendarManager`, parser iCal, sync iCal y configuración manual **conservados intactos**. |

### Verificación (9 chequeos, grep sobre TODO el repo incl. build regenerado)

`hornerito-pair` · `nrflmjybqifwarokugim` · `hornerito_google_pairing` ·
`hornerito_config` · `hornerito_sessions` · `hornerito_links` ·
`364421740199` / `mi-gestion-507222` · `refresh_token|access_token|client_secret|gkey` ·
URL vieja de la Edge Function → **0 coincidencias funcionales**
(única mención residual: comentario "Sin client_secret…" en el header iCal, que
describe lo que NO se usa).

### Credenciales Google — tratar como comprometidas (NO reutilizar)

- **Cliente OAuth web `364421740199-…` (proyecto `mi-gestion-507222`)**: usado por
  la implementación vieja → **comprometido para este proyecto**. NO reutilizar en
  ninguna arquitectura nueva. Recomendación al dueño: **rotar el client_secret y
  revocar el cliente** en Google Cloud Console → Credentials. *No se borró el JSON
  local* (`C:\Users\onlyf\Downloads\client_secret_364421740199-….json`): su
  redirect URI registrado apunta al Auth de **otro** proyecto Supabase
  (`edkidxiexvvrfkhknywk`) → posible uso compartido → regla: ante duda, no borrar.
- **Cliente `1085887510365-…` (JSON del 28/7/2026)**: no referenciado en ningún
  lado del proyecto; origen indeterminado → no se tocó.
- Los valores nunca se imprimieron en logs ni en este documento; el secreto que
  estaba en `hornerito_config`/migración murió con esas filas.

### Qué NO se tocó (regla de seguridad)

ProfeOS (tablas, Edge Functions, Auth, migraciones), VIBE LAB, proyecto vendi,
cualquier otro recurso del Supabase `nrflmjybqifwarokugim` o `lxmzyurogzyraohbcfsv`.

## 4. Regresiones diagnosticadas en el dispositivo (causa raíz)

- `abort() PC 0x421b1119` → `__wrap___cxa_allocate_exception` → `std::bad_alloc`
  en `Application::Alert → AudioService::PlaySound → make_unique<OggDemuxer>`
  cuando el heap quedaba agotado → **reinicio**.
- El drenaje era del pairing viejo: task `cal_pair` (12 KB) + intento TLS fallido
  (`alloc(4770) failed`), agravado por red inestable; el QR "invisible" y los
  juegos que no arrancaban eran la misma escasez de heap.
- Tras la limpieza: **5 min de serie sin crashes, heap estable ~49 KB libres**
  (antes colapsaba a 4–13 KB y rebootaba a los 61–84 s).
- **2026-10-06 — apertura de la nueva pantalla QR (regresión propia, ya
  corregida)**: `qr_buffer_.resize(~10 KB)` con el heap fragmentado (bloque
  máx. 8.7 KB durante conversación con el asistente) → `bad_alloc` →
  `__wrap___cxa_allocate_exception` (el build va **sin**
  `CONFIG_COMPILER_CXX_EXCEPTIONS`: todo `throw` aborta) → reinicio.
  Corregido: `malloc` con chequeo NULL (mensaje en pantalla en vez de abortar),
  buffer devuelto al salir de la pantalla QR (`LV_EVENT_SCREEN_UNLOADED`) y
  temporales de `DrawQr` con `new (std::nothrow)`. Verificado en serie: 300 s
  sin crashes + E2E `GET /pair` → `POST /pair_set` → sync `CAL: sync ok: 2 eventos`.
