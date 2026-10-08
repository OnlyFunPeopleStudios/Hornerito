#include "hornerito/calendar/hornerito_ics_parser.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <ctime>

namespace hornerito_ics {
namespace {

constexpr size_t kMaxPhysical = 1024;   // tope de línea física (fold)
constexpr size_t kMaxLogical = 2048;    // tope de línea lógica
constexpr size_t kMaxTitle = 80;        // truncado de títulos
constexpr int kWindowDays = 32;         // horizonte de la agenda
constexpr int kMaxEvents = 80;          // eventos finales en caché
constexpr int kMaxRaw = 500;            // techo antes de ordenar
constexpr int kIterCap = 8000;          // paseos de RRULE máximos
constexpr int kMaxPerRule = 60;         // ocurrencias añadidas por RRULE

// Días desde 1970-01-01 (algoritmo de Hinnant; inverso CivilFromDays).
int64_t DaysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2u) / 5u + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

void CivilFromDays(int64_t z, int* y, unsigned* m, unsigned* d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yy = static_cast<int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = static_cast<int>((*m <= 2) ? yy + 1 : yy);
}

int DaysInMonth(int y, unsigned m) {
    static const unsigned kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) {
        return 0;
    }
    if (m == 2 && (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))) {
        return 29;
    }
    return static_cast<int>(kDays[m - 1]);
}

//0=domingo ... 6=sábado (1970-01-01 fue jueves).
int Weekday0(int64_t days) {
    return static_cast<int>(((days % 7) + 4 + 7) % 7);
}

// "20261006" | "20261006T090000" | "20261006T090000Z" -> wall.
// Sin 'T' es fecha pura (all_day). TZID van en parámetros y se ignoran.
bool ParseDt(const std::string& v, int64_t* out, bool* all_day) {
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
    if (v.size() < 8 || sscanf(v.c_str(), "%4d%2d%2d", &y, &mo, &d) != 3) {
        return false;
    }
    if (mo < 1 || mo > 12 || d < 1 || d > 31) {
        return false;
    }
    if (v.size() > 8 && v[8] == 'T') {
        *all_day = false;
        if (v.size() >= 15) {
            sscanf(v.c_str() + 9, "%2d%2d%2d", &h, &mi, &s);
        }
        if (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) {
            h = mi = s = 0;
        }
    } else {
        *all_day = true;
    }
    *out = DaysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d)) * 86400 +
           h * 3600 + mi * 60 + s;
    return true;
}

std::string Unescape(const std::string& v) {
    std::string out;
    out.reserve(v.size());
    for (size_t i = 0; i < v.size(); i++) {
        char c = v[i];
        if (c == '\\' && i + 1 < v.size()) {
            char n = v[++i];
            out.push_back((n == 'n' || n == 'N') ? ' ' : n);
        } else if (c == '\n' || c == '\r') {
            out.push_back(' ');
        } else {
            out.push_back(c);
        }
    }
    return out;
}

void TruncateTitle(std::string* t) {
    if (t->size() <= kMaxTitle) {
        return;
    }
    size_t n = kMaxTitle;
    while (n > 0 && (static_cast<unsigned char>((*t)[n]) & 0xC0) == 0x80) {
        n--;  // no cortar a mitad de un UTF-8
    }
    t->resize(n);
}

// "MO"/"SA"... en (i, i+1) -> 0=domingo..6=sábado; -1 si no.
int DayCode(const std::string& s, size_t i) {
    static const char* kNames[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    if (i + 1 >= s.size()) {
        return -1;
    }
    for (int d = 0; d < 7; d++) {
        if (s.compare(i, 2, kNames[d]) == 0) {
            return d;
        }
    }
    return -1;
}

struct Wall {
    int y = 0;
    unsigned m = 0;
    unsigned d = 0;
    int tod = 0;  // segundos del día
};

Wall SecToWall(int64_t s) {
    Wall w;
    CivilFromDays(s / 86400, &w.y, &w.m, &w.d);
    w.tod = static_cast<int>(s % 86400);
    return w;
}

}  // namespace

int64_t WallNow() {
    time_t t = time(nullptr);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    return DaysFromCivil(tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday) * 86400 +
           tmv.tm_hour * 3600 + tmv.tm_min * 60 + tmv.tm_sec;
}

int64_t StartOfDay(int64_t wall) {
    return (wall / 86400) * 86400;  // wall >= 0 siempre (post-1970)
}

void CivilDate(int64_t wall, int* y, int* m, int* d) {
    unsigned mm = 0, dd = 0;
    CivilFromDays(wall / 86400, y, &mm, &dd);
    *m = static_cast<int>(mm);
    *d = static_cast<int>(dd);
}

Parser::Parser(int64_t today00)
    : today00_(StartOfDay(today00)), window_end_(today00_ + kWindowDays * 86400) {}

void Parser::Feed(const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\n') {
            FlushPhysicalLine();
            physical_.clear();
        } else if (c != '\r' && physical_.size() < kMaxPhysical) {
            physical_.push_back(c);
        }
    }
}

void Parser::Finish() {
    if (!physical_.empty()) {
        FlushPhysicalLine();
        physical_.clear();
    }
    if (!logical_.empty()) {
        OnLogicalLine(logical_);
        logical_.clear();
    }
}

void Parser::FlushPhysicalLine() {
    if (!physical_.empty() && (physical_[0] == ' ' || physical_[0] == '\t')) {
        if (logical_.size() < kMaxLogical) {
            logical_.append(physical_, 1, std::string::npos);  // plegado
        }
        return;
    }
    if (!logical_.empty()) {
        OnLogicalLine(logical_);
    }
    logical_ = physical_;
}

void Parser::OnLogicalLine(const std::string& line) {
    if (line == "BEGIN:VEVENT") {
        cur_ = Pending{};
        in_event_ = true;
        has_start_ = false;
        saw_vevent_ = true;
        return;
    }
    if (line == "END:VEVENT") {
        if (in_event_ && has_start_) {
            Emit();
        }
        in_event_ = false;
        return;
    }
    if (!in_event_) {
        return;  // VTIMEZONE, BEGIN:VCALENDAR... no interesan
    }
    size_t colon = line.find(':');
    if (colon == std::string::npos) {
        return;
    }
    std::string prop = line.substr(0, colon);
    std::string val = line.substr(colon + 1);
    std::string name = prop.substr(0, prop.find(';'));
    if (name == "DTSTART") {
        bool all_day = false;
        if (ParseDt(val, &cur_.start, &all_day)) {
            cur_.all_day = all_day;
            has_start_ = true;
        }
    } else if (name == "DTEND") {
        bool all_day = false;
        int64_t e = 0;
        if (ParseDt(val, &e, &all_day)) {
            cur_.end = e;
        }
    } else if (name == "RRULE") {
        cur_.rrule = val;
    } else if (name == "SUMMARY") {
        cur_.title = Unescape(val);
        TruncateTitle(&cur_.title);
    }
}

void Parser::Emit() {
    int64_t end = cur_.end > cur_.start ? cur_.end : cur_.start;
    if (cur_.rrule.empty()) {
        AddOccurrence(cur_.start, end, cur_.all_day, cur_.title);
    } else {
        ExpandRule(cur_.rrule, cur_.start, end, cur_.all_day, cur_.title);
    }
}

void Parser::AddOccurrence(int64_t start, int64_t end, bool all_day,
                           const std::string& title) {
    if (end <= today00_) {
        return;  // terminó antes de hoy
    }
    if (start >= window_end_) {
        return;  // fuera del horizonte de32 días
    }
    if (events_.size() >= kMaxRaw) {
        return;
    }
    events_.push_back(Event{start, end, all_day, title});
}

void Parser::ExpandRule(const std::string& rule, int64_t start, int64_t end,
                        bool all_day, const std::string& title) {
    std::string freq;
    int interval = 1;
    int count = -1;
    int64_t until = -1;
    int byday[7];
    int nbyday = 0;

    size_t pos = 0;
    while (pos <= rule.size()) {
        size_t semi = rule.find(';', pos);
        std::string item =
            rule.substr(pos, semi == std::string::npos ? std::string::npos : semi - pos);
        size_t eq = item.find('=');
        if (eq != std::string::npos) {
            std::string k = item.substr(0, eq);
            std::string v = item.substr(eq + 1);
            if (k == "FREQ") {
                freq = v;
            } else if (k == "INTERVAL") {
                interval = atoi(v.c_str());
            } else if (k == "COUNT") {
                count = atoi(v.c_str());
            } else if (k == "UNTIL") {
                bool ad = false;
                int64_t u = 0;
                if (ParseDt(v, &u, &ad)) {
                    until = u;
                }
            } else if (k == "BYDAY") {
                for (size_t i = 0; i < v.size() && nbyday < 7;) {
                    int d = DayCode(v, i);
                    if (d >= 0) {
                        byday[nbyday++] = d;
                        i += 2;
                    } else {
                        i++;  // ordinales tipo "2MO": saltar el dígito
                    }
                }
            }
        }
        if (semi == std::string::npos) {
            break;
        }
        pos = semi + 1;
    }

    if (interval < 1) {
        interval = 1;
    }
    if (interval > 1000) {
        interval = 1000;
    }
    if (count == 0) {
        count = -1;
    }

    if (freq != "DAILY" && freq != "WEEKLY" && freq != "MONTHLY") {
        AddOccurrence(start, end, all_day, title);  // FREQ exótica: primera sola
        return;
    }

    int64_t dur = end > start ? end - start : 0;
    int64_t start_days = start / 86400;
    int tod = static_cast<int>(start % 86400);
    int wd0 = Weekday0(start_days);
    int added = 0;
    int occ_i = 0;
    bool done = false;

    // Cada ocurrencia generada, en orden: cuenta para COUNT, corta en
    // UNTIL/ventana y añade a la lista si solapa la ventana [hoy, +32 d).
    auto consider = [&](int64_t occ) {
        if (occ < start) {
            return true;  // antes del DTSTART: ni cuenta (RFC: empieza en DTSTART)
        }
        if (count >= 0 && occ_i >= count) {
            done = true;
            return false;
        }
        occ_i++;
        if (until >= 0 && occ > until) {
            done = true;
            return false;
        }
        if (occ > window_end_) {
            done = true;
            return false;
        }
        AddOccurrence(occ, occ + dur, all_day, title);
        added++;
        return added < kMaxPerRule;
    };

    if (freq == "DAILY") {
        for (int i = 0; i < kIterCap && !done; i++) {
            int64_t occ = start + static_cast<int64_t>(i) * interval * 86400;
            if (!consider(occ)) {
                break;
            }
        }
    } else if (freq == "WEEKLY") {
        int days[7];
        int nd = 0;
        if (nbyday > 0) {
            for (int i = 0; i < nbyday; i++) {
                days[nd++] = byday[i];
            }
        } else {
            days[nd++] = wd0;
        }
        // Orden ascendente por offset desde el lunes (evita cortes tempranos).
        for (int i = 1; i < nd; i++) {
            int key = days[i];
            int koff = (key + 6) % 7;
            int j = i - 1;
            while (j >= 0 && (days[j] + 6) % 7 > koff) {
                days[j + 1] = days[j];
                j--;
            }
            days[j + 1] = key;
        }
        int64_t week0 = start_days - ((wd0 + 6) % 7);  // lunes de la semana inicial
        for (int k = 0; k < kIterCap / 8 && !done; k++) {
            int64_t week = week0 + static_cast<int64_t>(k) * interval * 7;
            for (int j = 0; j < nd && !done; j++) {
                int64_t occ = (week + (days[j] + 6) % 7) * 86400 + tod;
                if (!consider(occ)) {
                    break;
                }
            }
            if (added >= kMaxPerRule) {
                break;
            }
        }
    } else {  // MONTHLY: mismo día del mes (BYDAY del mes se ignora: techo)
        Wall ws = SecToWall(start);
        int base = ws.y * 12 + (static_cast<int>(ws.m) - 1);
        for (int k = 0; k < 400 && !done; k++) {
            int total = base + k * interval;
            int y = total / 12;
            unsigned m = static_cast<unsigned>(total % 12) + 1;
            if (ws.d > static_cast<unsigned>(DaysInMonth(y, m))) {
                continue;  //31 en febrero: mes sin esa fecha
            }
            int64_t occ = DaysFromCivil(y, m, ws.d) * 86400 + tod;
            if (!consider(occ)) {
                break;
            }
        }
    }
}

std::vector<Event> Parser::Take() {
    std::sort(events_.begin(), events_.end(),
              [](const Event& a, const Event& b) {
                  if (a.start != b.start) return a.start < b.start;
                  return a.end < b.end;
              });
    if (events_.size() > static_cast<size_t>(kMaxEvents)) {
        events_.resize(kMaxEvents);
    }
    return std::move(events_);
}

}  // namespace hornerito_ics
