// Parser ICS (RFC 5545) mínimo para la agenda de HORNERITO.
//
// Extrae VEVENT con DTSTART/DTEND/SUMMARY y RRULE básico
// (DAILY/WEEKLY/MONTHLY + INTERVAL/COUNT/UNTIL/BYDAY). Ignora VTIMEZONE,
// EXDATE y reglas avanzadas (BYSETPOS, BYMONTHDAY...): techo consciente,
// ampliar solo si un calendario real lo necesita.
//
// Sin LVGL ni headers de ESP: C++ puro para poder testearlo en host.
// Los tiempos van en dominio "wall": los componentes DTSTART tal cual
// (TZID = hora local del calendario) comparados con la hora local del
// equipo; los sufijos Z/TZID se ignoran a propósito. ponytail: si un
// calendario en UTC muestra horas desplazadas2 h, añadir offset de zona.
#ifndef HORNERITO_ICS_PARSER_H_
#define HORNERITO_ICS_PARSER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hornerito_ics {

struct Event {
    int64_t start = 0;  // segundos "wall" desde 1970 (componentes locales)
    int64_t end = 0;    // mismo dominio; == start cuando no hay DTEND
    bool all_day = false;
    std::string title;  // SUMMARY sin escapados, truncado
};

// Reloj "wall" del equipo (el arranque ya lo sincroniza con la OTA) y
// medianoche del día actual. En host se comportan igual (UTC).
int64_t WallNow();
int64_t StartOfDay(int64_t wall);
// Descompone una fecha "wall" en año/mes/día (para pintar dd/MM).
void CivilDate(int64_t wall, int* y, int* m, int* d);

// Parsea por streaming (la respuesta HTTPS llega en trozos; no se guarda el
// cuerpo entero en RAM). Ventana: conserva eventos con end > today00 y
// start < today00 + 32 días, ordenados por start, como mucho 80.
class Parser {
public:
    explicit Parser(int64_t today00);

    void Feed(const char* data, size_t len);
    void Finish();  // procesa la última línea pendiente al cerrar la respuesta

    bool saw_vevent() const { return saw_vevent_; }

    // Mueve los eventos (ordenados, recortados a la ventana) al caller.
    std::vector<Event> Take();

private:
    void FlushPhysicalLine();
    void OnLogicalLine(const std::string& line);
    void Emit();
    void AddOccurrence(int64_t start, int64_t end, bool all_day,
                       const std::string& title);
    void ExpandRule(const std::string& rule, int64_t start, int64_t end,
                    bool all_day, const std::string& title);

    int64_t today00_;
    int64_t window_end_;
    std::string physical_;   // línea física en curso (unfold)
    std::string logical_;    // línea lógica completa (con continuaciones)
    bool in_event_ = false;
    bool has_start_ = false;
    bool saw_vevent_ = false;
    struct Pending {
        int64_t start = 0;
        int64_t end = 0;
        bool all_day = false;
        std::string title;
        std::string rrule;
    } cur_;
    std::vector<Event> events_;
};

}  // namespace hornerito_ics

#endif  // HORNERITO_ICS_PARSER_H_
