// Núcleo del pet — PORT DIRECTO (MIT) de CyberXcyborg/ESP32-TamaPetchi.
// Lógica idéntica a src/Pet_v2.cpp con las constantes de src/config_v2.h
// (valores sin tocar: decays, umbrales, gains y multiplicadores por etapa).
#include "hornerito/games/pet_engine.h"

#include <string.h>

namespace pet {
namespace {

// config_v2.h — valores originales verbatim.
constexpr int kStatMax = 100;

constexpr int kHungerDecayNormal = 5;
constexpr int kHappinessDecayNormal = 3;
constexpr int kEnergyDecayNormal = 2;
constexpr int kCleanlinessDecayNormal = 4;

constexpr int kHungerDecaySleep = 2;
constexpr int kHappinessDecaySleep = 1;
constexpr int kEnergyRegenSleep = 10;
constexpr int kCleanlinessDecaySleep = 2;

constexpr int kHealthDecayThreshold = 20;
constexpr int kHealthDecayAmount = 5;
constexpr int kCriticalHealthMax = 30;   // <= 30 => sick
constexpr int kDyingHealthMax = 10;

constexpr int kFeedHungerGain = 20;
constexpr int kFeedEnergyCost = 5;
constexpr int kFeedHealthBonus = 5;
constexpr int kPlayHappinessGain = 15;
constexpr int kPlayEnergyCost = 15;
constexpr int kPlayHungerCost = 10;
constexpr int kPlayEnergyMin = 10;
constexpr int kCleanHealthBonus = 5;

// Evolución (minutos): BABY 60, CHILD 480, ADULT 1440.
constexpr uint32_t kBabyMaxMinutes = 60;
constexpr uint32_t kChildMaxMinutes = 480;
constexpr uint32_t kAdultMaxMinutes = 1440;

constexpr int kBabyDecayMult = 80;
constexpr int kChildDecayMult = 100;
constexpr int kAdultDecayMult = 110;
constexpr int kElderDecayMult = 130;

// Salvaguardas equivalentes a los #error de config_v2.h.
static_assert(kBabyMaxMinutes < kChildMaxMinutes, "stage order");
static_assert(kChildMaxMinutes < kAdultMaxMinutes, "stage order");
static_assert(kCriticalHealthMax < kStatMax, "sick threshold");
static_assert(kDyingHealthMax < kCriticalHealthMax, "dying < critical");

uint8_t ClampStatAdd(uint8_t v, int add) {
    const int r = v + add;
    return r > kStatMax ? kStatMax : static_cast<uint8_t>(r);
}

uint8_t ClampStatSub(uint8_t v, int sub) {
    return v > sub ? static_cast<uint8_t>(v - sub) : 0;
}

}  // namespace

void Engine::Init() {
    memset(&d_, 0, sizeof(d_));
    d_.hunger = 80;
    d_.happiness = 70;
    d_.energy = 90;
    d_.cleanliness = 85;
    d_.health = 100;
    d_.age_minutes = 0;
    d_.stage = kBaby;
    d_.state = kIdle;
}

void Engine::Update() {
    if (d_.state == kDead) {
        return;
    }
    DecayStats();
    CheckHealth();
    Evolve();
    d_.age_minutes++;
}

void Engine::DecayStats() {
    const uint8_t mult = DecayMult();
    const bool sleeping = (d_.state == kSleeping);

    const int hunger_decay =
        sleeping ? (kHungerDecaySleep * mult / 100)
                 : (kHungerDecayNormal * mult / 100);
    d_.hunger = ClampStatSub(d_.hunger, hunger_decay);

    const int happy_decay =
        sleeping ? (kHappinessDecaySleep * mult / 100)
                 : (kHappinessDecayNormal * mult / 100);
    d_.happiness = ClampStatSub(d_.happiness, happy_decay);

    if (sleeping) {
        d_.energy = ClampStatAdd(d_.energy, kEnergyRegenSleep * mult / 100);
    } else {
        d_.energy = ClampStatSub(d_.energy, kEnergyDecayNormal * mult / 100);
    }

    const int clean_decay =
        sleeping ? (kCleanlinessDecaySleep * mult / 100)
                 : (kCleanlinessDecayNormal * mult / 100);
    d_.cleanliness = ClampStatSub(d_.cleanliness, clean_decay);
}

void Engine::CheckHealth() {
    if (d_.hunger < kHealthDecayThreshold ||
        d_.cleanliness < kHealthDecayThreshold) {
        d_.health = ClampStatSub(d_.health, kHealthDecayAmount);
    }

    if (d_.health == 0) {
        d_.state = kDead;
    } else if (d_.health <= kDyingHealthMax) {
        d_.state = kSick;  // "dying" reutiliza SICK (como el original)
    } else if (d_.health <= kCriticalHealthMax) {
        d_.state = kSick;
    }
    // Nota: el original no devuelve SICK -> IDLE solo por tener salud:
    // Heal()/Wake() de las acciones restauran el estado.
}

void Engine::Evolve() {
    if (d_.age_minutes > kAdultMaxMinutes) {
        d_.stage = kElder;
    } else if (d_.age_minutes > kChildMaxMinutes) {
        d_.stage = kAdult;
    } else if (d_.age_minutes > kBabyMaxMinutes) {
        d_.stage = kChild;
    }
}

uint8_t Engine::DecayMult() const {
    switch (d_.stage) {
        case kBaby:
            return kBabyDecayMult;
        case kChild:
            return kChildDecayMult;
        case kAdult:
            return kAdultDecayMult;
        case kElder:
            return kElderDecayMult;
        default:
            return 100;
    }
}

void Engine::Feed() {
    if (d_.state == kDead || d_.state == kSleeping) {
        return;
    }
    d_.state = kEating;
    d_.hunger = ClampStatAdd(d_.hunger, kFeedHungerGain);
    // Original: if (energy > cost) energy -= cost; (sin else: si energy <=
    // cost no paga). Se respeta tal cual.
    if (d_.energy > kFeedEnergyCost) {
        d_.energy -= kFeedEnergyCost;
    }
    if (d_.hunger < kHealthDecayThreshold) {
        d_.health = ClampStatAdd(d_.health, kFeedHealthBonus);
    }
}

void Engine::Play() {
    if (d_.state == kDead || d_.state == kSleeping) {
        return;
    }
    if (d_.energy < kPlayEnergyMin) {
        return;
    }
    d_.state = kPlaying;
    // Original: energy -= cost SIN clamp. Con energy en [10,14] eso desborda
    // uint8 (10-15 → 251) y rompe el stat: única desviación del port, se
    // clampa a 0 en lugar de envolver.
    d_.energy = ClampStatSub(d_.energy, kPlayEnergyCost);
    // Original: if (hunger > cost) hunger -= cost; (sin else) — tal cual.
    if (d_.hunger > kPlayHungerCost) {
        d_.hunger -= kPlayHungerCost;
    }
    d_.happiness = ClampStatAdd(d_.happiness, kPlayHappinessGain);
}

void Engine::Clean() {
    if (d_.state == kDead) {
        return;
    }
    d_.cleanliness = kStatMax;
    d_.health = ClampStatAdd(d_.health, kCleanHealthBonus);
}

void Engine::Sleep() {
    if (d_.state == kDead) {
        return;
    }
    d_.state = kSleeping;
}

void Engine::Wake() {
    if (d_.state == kSleeping) {
        d_.state = kIdle;
    }
}

void Engine::Heal() {
    if (d_.state == kDead) {
        return;
    }
    d_.health = kStatMax;
    d_.state = kIdle;
}

}  // namespace pet
