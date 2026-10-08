// Núcleo del pet — PORT DIRECTO (MIT) de CyberXcyborg/ESP32-TamaPetchi.
//
// Equivalencias con el original (V2.3.0 §Pet, pedido de usuario):
//   pet_engine.h/.cc  <- src/Pet_v2.h + src/Pet_v2.cpp + src/config_v2.h
// Mismos estados, umbrales, decaimientos, acciones y etapas de evolución;
// se elimina solo lo que no aplica a Hornerito (Arduino String, JSON de
// persistencia y millis()): el bucle lo gobierna el llamador con el
// PET_UPDATE_INTERVAL original de 60 s por Update().
#ifndef HORNERITO_GAMES_PET_ENGINE_H_
#define HORNERITO_GAMES_PET_ENGINE_H_

#include <stdint.h>

namespace pet {

// Etapas de evolución (PetStage del original).
enum Stage : uint8_t {
    kBaby = 0,
    kChild,
    kAdult,
    kElder,
};

// Estados (PetState del original).
enum State : uint8_t {
    kIdle = 0,
    kEating,
    kPlaying,
    kSleeping,
    kSick,
    kDead,
};

struct Data {
    uint8_t hunger;       // 0..100 (STAT_MAX)
    uint8_t happiness;    // 0..100
    uint8_t energy;       // 0..100
    uint8_t cleanliness;  // 0..100
    uint8_t health;       // 0..100
    uint32_t age_minutes;
    uint8_t stage;        // Stage
    uint8_t state;        // State
};

class Engine {
 public:
    void Init();    // valores por defecto del original (80/70/90/85/100)
    void Update();  // llamar cada 60 s (PET_UPDATE_INTERVAL original)

    // Acciones (idénticas a Pet_v2.cpp: mismas guardas y efectos).
    void Feed();
    void Play();
    void Clean();
    void Sleep();
    void Wake();
    void Heal();

    const Data& data() const { return d_; }
    bool is_alive() const { return d_.state != kDead; }
    bool is_sleeping() const { return d_.state == kSleeping; }
    // PLAY_ENERGY_MIN del original: Play() no hace nada por debajo.
    bool can_play() const { return d_.energy >= 10; }

 private:
    Data d_{};

    void DecayStats();
    void CheckHealth();
    void Evolve();
    uint8_t DecayMult() const;
};

}  // namespace pet

#endif  // HORNERITO_GAMES_PET_ENGINE_H_
