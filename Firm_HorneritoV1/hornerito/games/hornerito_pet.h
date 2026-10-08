// Hornerito Pet (V2.3.0): mascota virtual con el NÚCLEO ORIGINAL.
//
// Port pedido explícitamente por usuario (V2.3.0) del proyecto CyberXcyborg/
// ESP32-TamaPetchi (MIT, elegido como referencia en FASE0 §9):
//   - pet_engine.{h,cc}: motor Pet_v2 (hambre/felicidad/energía/limpieza/
//     salud, decaimientos, estados, acciones y evolución por etapas) portado
//     verbatim de src/Pet_v2.* + config_v2.h.
//   - pet_sprite_data.cc: su sprite original baby_idle.spr (64x64, 9 frames,
//     RLE 4bpp) incrustado en flash — sin depender de microSD.
// Reversa la decisión V2.2 de no reutilizar assets del original (ahora sí,
// por orden de usuario): la UI sigue siendo nuestra (LVGL 9, 480x416).
//
// Comportamiento:
//   - Update() del motor cada 60 s (PET_UPDATE_INTERVAL original): los
//     cambios de stats son lentos a propósito (mascota de horas, no de
//     segundos). Las acciones dan feedback instantáneo.
//   - Sin persistencia: el pet vive durante la sesión (reinicio al reboot).
//   - Sin Motion: interacción por botones (patrón del original).
//   - Ciclo de vida vía GameManager: Exit() borra timers y callbacks.
#ifndef HORNERITO_GAMES_HORNERITO_PET_H_
#define HORNERITO_GAMES_HORNERITO_PET_H_

#include <lvgl.h>

#include "hornerito/games/game_common.h"
#include "hornerito/games/pet_engine.h"

class HorneritoPetGame : public HorneritoGame {
public:
    static HorneritoPetGame& GetInstance();

    const char* Name() const override { return "Hornerito Pet"; }
    void Enter(lv_obj_t* content) override;
    void Exit() override;

private:
    HorneritoPetGame() = default;

    // Sprite .spr incrustado: parseo de cabecera/paleta/offsets y decode RLE
    // de un frame directo a RGB565 (frame_buf_).
    bool ParseSprite();
    bool DecodeFrame(uint8_t idx);

    void Refresh();   // etiquetas de estado/stats según el motor
    void Reaction(const char* text, int32_t pose_frame);  // 255 = anim normal
    void AnimTick();  // 83 ms: frames + cuenta de reacción
    void StatTick();  // 60 s: motor Update + comprobación de muerte
    void ShowDeath();

    static void AnimThunk(lv_timer_t* timer);
    static void StatThunk(lv_timer_t* timer);
    static void BtnThunk(lv_event_t* event);
    static void OverlayThunk(lv_event_t* event);

    lv_obj_t* content_ = nullptr;
    lv_obj_t* pet_ = nullptr;   // lv_image sobre &dsc_
    lv_obj_t* status_ = nullptr;
    lv_obj_t* info_ = nullptr;
    lv_obj_t* stats_lbl_[5] = {};
    lv_obj_t* btn_[5] = {};
    lv_obj_t* btn_sleep_label_ = nullptr;
    lv_obj_t* overlay_ = nullptr;
    lv_timer_t* anim_timer_ = nullptr;
    lv_timer_t* stat_timer_ = nullptr;

    // Frame actual decodificado (RGB565) + descriptor LVGL persistente.
    lv_image_dsc_t dsc_{};
    uint16_t frame_buf_[64 * 64] = {};

    // Datos del .spr embebido.
    uint16_t pal_[16] = {};
    uint32_t off_[16] = {};
    uint16_t fw_ = 0;
    uint16_t fh_ = 0;
    uint8_t fc_ = 0;
    bool sprite_ok_ = false;

    uint8_t frame_ = 0;        // frame de la animación en curso
    uint8_t shown_ = 255;      // último frame decodificado (255 = ninguno)
    uint8_t sleep_div_ = 0;    // divide el anim a 4 fps mientras duerme
    const char* reaction_ = nullptr;
    int32_t reaction_pose_ = 255;  // frame fijo durante la reacción (8 = comer)
    int reaction_ticks_ = 0;       // ticks de 83 ms de reacción (36 ≈ 3 s)

    pet::Engine engine_;
};

#endif  // HORNERITO_GAMES_HORNERITO_PET_H_
