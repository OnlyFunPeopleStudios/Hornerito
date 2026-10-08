// Calendario Google de HORNERITO sin OAuth: el usuario pega una vez la URL
// secreta iCal/ICS de Google (Ajustes de Google Calendar -> "añadir
// calendario a otro app" -> dirección secreta en formato iCal):
//   https://calendar.google.com/calendar/ical/<email>/private-<token>/basic.ics
// El equipo hace GET HTTPS, parsea los VEVENT y cachea la agenda en NVS.
// Sin client_secret, sin refresh tokens, sin pantallas de consentimiento.
// La URL es un secreto tipo contraseña: NUNCA se imprime por UART ni se
// registra en logs (solo códigos de estado y recuentos).
//
// Módulo sin LVGL (la UI vive en hornerito/ui/hornerito_calendar_ui.*).
// Sincronización: task FreeRTOS propia; la UI consulta TakeSyncResult().
#ifndef HORNERITO_CALENDAR_H_
#define HORNERITO_CALENDAR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "hornerito/calendar/hornerito_ics_parser.h"

namespace hornerito_calendar {

using Event = hornerito_ics::Event;

// Estados de sincronización (los que ve el usuario en la pantalla).
enum Status {
    kOk = 0,
    kNoWifi = 1,
    kBadClock = 2,  // hora sin sincronizar (TLS/fecha inválidos)
    kHttp = 3,
    kBadIcs = 4,    // respuesta200 pero sin VEVENT (URL revocada/login)
    kNoUrl = 5,
};

// Caché en NVS (namespace "calendar": e0..e7 trozos + last + url).
std::vector<Event> LoadCache();
int64_t LastUpdate();  // "wall" de la última sync correcta;0 = nunca
std::string GetUrl();
void SetUrl(const std::string& url);  // si cambia, invalida LastUpdate

// Sync en background (task "cal_sync", no bloquea la UI). Noop si ya hay
// una en curso. Repost: la UI lo decide con LastUpdate()/IsSyncing().
void RequestSync();
bool IsSyncing();
bool TakeSyncResult(int* status);  // true si hay resultado nuevo

// Atajos del reloj "wall" del equipo (mismo dominio que los DTSTART).
int64_t WallNow();
int64_t StartOfDay(int64_t wall);

}  // namespace hornerito_calendar

#endif  // HORNERITO_CALENDAR_H_
