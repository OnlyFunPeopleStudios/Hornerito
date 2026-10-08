#include "hornerito/games/hornerito_pet.h"

#include <cstring>

// Sprite original incrustado (pet_sprite_data.cc, generado desde
// ESP32-TamaPetchi/data/sprites/baby_idle.spr, MIT).
extern const uint8_t kPetBabyIdle[];
extern const unsigned kPetBabyIdleLen;

namespace {

// Geometría (content = 480x416).
constexpr int32_t kSpriteX = 20;      // sprite 64x64 a escala x3 = 192x192
constexpr int32_t kSpriteY = 10;
constexpr int32_t kSpriteScale = 3 * 256;  // LVGL: 256 = 100%
constexpr int32_t kStatsX = 228;      // columna de stats a la derecha
constexpr int32_t kStatsY0 = 8;
constexpr int32_t kStatsRow = 36;
constexpr int32_t kStatusY = 206;
constexpr int32_t kInfoY = 244;

constexpr int32_t kBtnW = 154;
constexpr int32_t kBtnH = 56;
// Fila 1: Comer, Jugar, Limpiar. Fila 2: Dormir, Curar (centrados).
constexpr int32_t kBtnX[5] = {1, 163, 325, 82, 244};
constexpr int32_t kBtnY[5] = {288, 288, 288, 352, 352};
constexpr const char* kBtnTexts[5] = {"Comer", "Jugar", "Limpiar",
                                      "Dormir", "Curar"};

// Temporización.
constexpr int kAnimMs = 83;           // 12 fps (baby_idle_durations original)
constexpr int kStatMs = 60000;        // PET_UPDATE_INTERVAL original
constexpr int kReactionTicks = 36;    // 36 x 83 ms ≈ 3 s
constexpr int kIdleFrames = 8;        // frames 0..7 = idle loop original
constexpr int kEatPose = 8;           // baby_eat_frames empieza en 8

constexpr uint32_t kColorStatus = 0x5BC8F5;
constexpr uint32_t kColorMuted = 0x9AB2C8;

}  // namespace

HorneritoPetGame& HorneritoPetGame::GetInstance() {
    static HorneritoPetGame instance;
    return instance;
}

// ---------------------------------------------------------------- sprite ---

bool HorneritoPetGame::ParseSprite() {
    const uint8_t* b = kPetBabyIdle;
    const unsigned len = kPetBabyIdleLen;
    if (len < 62 + 16 * 4) {
        return false;
    }
    if (b[0] != 'S' || b[1] != 'P' || b[2] != 'R' || b[3] != 0) {
        return false;
    }
    fw_ = static_cast<uint16_t>(b[4] | (b[5] << 8));
    fh_ = static_cast<uint16_t>(b[6] | (b[7] << 8));
    fc_ = static_cast<uint8_t>(b[8] | (b[9] << 8));
    if (b[10] != 4) {  // solo 4bpp paletado
        return false;
    }
    if (fw_ == 0 || fh_ == 0 || fw_ * fh_ > 64 * 64) {
        return false;  // el buffer estático es 64x64
    }
    if (fc_ == 0 || fc_ > 16) {
        return false;
    }
    std::memcpy(pal_, b + 30, sizeof(pal_));
    for (uint8_t i = 0; i < fc_; i++) {
        const unsigned o = 62 + static_cast<unsigned>(i) * 4;
        off_[i] = static_cast<uint32_t>(b[o]) |
                  (static_cast<uint32_t>(b[o + 1]) << 8) |
                  (static_cast<uint32_t>(b[o + 2]) << 16) |
                  (static_cast<uint32_t>(b[o + 3]) << 24);
        if (off_[i] + 4 > len) {
            return false;
        }
    }
    return true;
}

bool HorneritoPetGame::DecodeFrame(uint8_t idx) {
    if (!sprite_ok_ || idx >= fc_) {
        return false;
    }
    const uint8_t* b = kPetBabyIdle;
    const unsigned len = kPetBabyIdleLen;
    const uint32_t o = off_[idx];
    const uint32_t csz = static_cast<uint32_t>(b[o]) |
                         (static_cast<uint32_t>(b[o + 1]) << 8) |
                         (static_cast<uint32_t>(b[o + 2]) << 16) |
                         (static_cast<uint32_t>(b[o + 3]) << 24);
    const uint8_t* p = b + o + 4;
    if (csz == 0 || o + 4 + csz > len) {
        return false;
    }
    const uint8_t* end = p + csz;

    // RLE idéntico al SpriteLoader original: bit7 = repeat (count-1 + byte),
    // bit7 limpio = literal (count-1 + índices). Salida directa a RGB565.
    const uint32_t n = static_cast<uint32_t>(fw_) * fh_;
    uint32_t d = 0;
    while (p < end && d < n) {
        const uint8_t token = *p++;
        if (token & 0x80) {
            if (p >= end) {
                return false;
            }
            const uint16_t color = pal_[*p++ & 0x0F];
            uint8_t count = static_cast<uint8_t>((token & 0x7F) + 1);
            while (count-- > 0 && d < n) {
                frame_buf_[d++] = color;
            }
        } else {
            uint8_t count = static_cast<uint8_t>(token + 1);
            while (count-- > 0 && d < n && p < end) {
                frame_buf_[d++] = pal_[*p++ & 0x0F];
            }
        }
    }
    if (d != n) {
        return false;
    }
    if (pet_ != nullptr) {
        lv_obj_invalidate(pet_);
    }
    return true;
}

// ------------------------------------------------------------------ UI -----

void HorneritoPetGame::Refresh() {
    const pet::Data& d = engine_.data();

    lv_label_set_text_fmt(stats_lbl_[0], "Hambre: %d", d.hunger);
    lv_label_set_text_fmt(stats_lbl_[1], "Felicidad: %d", d.happiness);
    lv_label_set_text_fmt(stats_lbl_[2], "Energía: %d", d.energy);
    lv_label_set_text_fmt(stats_lbl_[3], "Limpieza: %d", d.cleanliness);
    lv_label_set_text_fmt(stats_lbl_[4], "Salud: %d", d.health);

    const char* text;
    if (reaction_ != nullptr) {
        text = reaction_;
    } else if (d.state == pet::kDead) {
        text = "";
    } else if (d.state == pet::kSleeping) {
        text = "Durmiendo... Zzz";
    } else if (d.state == pet::kSick) {
        text = "Enfermo... pulsa Curar";
    } else if (d.happiness >= 60) {
        text = "¡Estoy feliz!";
    } else if (d.happiness >= 30) {
        text = "Normal... pasa el día";
    } else {
        text = "Triste... ¿jugamos?";
    }
    lv_label_set_text(status_, text);

    static const char* kStages[4] = {"Bebé", "Niño", "Adulto", "Anciano"};
    lv_label_set_text_fmt(info_, "Etapa %s · %lu min", kStages[d.stage & 3],
                          static_cast<unsigned long>(d.age_minutes));

    if (btn_sleep_label_ != nullptr) {
        lv_label_set_text(btn_sleep_label_,
                          engine_.is_sleeping() ? "Despertar" : "Dormir");
    }
}

void HorneritoPetGame::Reaction(const char* text, int32_t pose_frame) {
    reaction_ = text;
    reaction_pose_ = pose_frame;
    reaction_ticks_ = kReactionTicks;
    Refresh();
}

void HorneritoPetGame::AnimTick() {
    if (reaction_ticks_ > 0) {
        reaction_ticks_--;
        if (reaction_ticks_ == 0) {
            reaction_ = nullptr;
            reaction_pose_ = 255;
            Refresh();
        }
    }

    uint8_t want;
    if (reaction_ != nullptr && reaction_pose_ != 255) {
        want = static_cast<uint8_t>(reaction_pose_);  // pose fija (comer)
    } else {
        if (engine_.is_sleeping()) {
            // 4 fps al dormir (baby_sleep_durations = 250 ms).
            sleep_div_ = static_cast<uint8_t>((sleep_div_ + 1) % 3);
            if (sleep_div_ != 0) {
                return;
            }
        }
        frame_ = static_cast<uint8_t>((frame_ + 1) % kIdleFrames);
        want = frame_;
    }
    if (want == shown_) {
        return;
    }
    if (DecodeFrame(want)) {
        shown_ = want;
    }
}

void HorneritoPetGame::StatTick() {
    engine_.Update();
    if (!engine_.is_alive()) {
        ShowDeath();
        return;
    }
    Refresh();
}

void HorneritoPetGame::ShowDeath() {
    if (overlay_ != nullptr || content_ == nullptr) {
        return;
    }
    reaction_ = nullptr;
    reaction_pose_ = 255;
    reaction_ticks_ = 0;
    overlay_ = GameCreateOverlay(content_, "Ha muerto",
                                 "Toca para empezar de nuevo");
    lv_obj_add_flag(overlay_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay_, OverlayThunk, LV_EVENT_CLICKED, this);
}

void HorneritoPetGame::AnimThunk(lv_timer_t* timer) {
    static_cast<HorneritoPetGame*>(lv_timer_get_user_data(timer))->AnimTick();
}

void HorneritoPetGame::StatThunk(lv_timer_t* timer) {
    static_cast<HorneritoPetGame*>(lv_timer_get_user_data(timer))->StatTick();
}

void HorneritoPetGame::BtnThunk(lv_event_t* event) {
    auto* self = static_cast<HorneritoPetGame*>(lv_event_get_user_data(event));
    auto* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
    if (!self->engine_.is_alive()) {
        return;  // la overlay de muerte cubre el contenido
    }
    const bool sleeping = self->engine_.is_sleeping();

    if (target == self->btn_[0]) {  // Comer
        if (sleeping) {
            self->Reaction("Zzz... está durmiendo", 255);
            return;
        }
        self->engine_.Feed();
        self->Reaction("¡Ñam ñam!", kEatPose);
    } else if (target == self->btn_[1]) {  // Jugar
        if (sleeping) {
            self->Reaction("Zzz... está durmiendo", 255);
            return;
        }
        if (!self->engine_.can_play()) {
            self->Reaction("Sin energía... descansa", 255);
            return;
        }
        self->engine_.Play();
        self->Reaction("¡A jugar!", 255);
    } else if (target == self->btn_[2]) {  // Limpiar (permitido durmiendo)
        self->engine_.Clean();
        self->Reaction("¡Qué limpio!", 255);
    } else if (target == self->btn_[3]) {  // Dormir / Despertar
        if (sleeping) {
            self->engine_.Wake();
            self->Reaction("¡Buenos días!", 255);
        } else {
            self->engine_.Sleep();
            self->Reaction("Buenas noches", 255);
        }
    } else if (target == self->btn_[4]) {  // Curar (despierta, como original)
        self->engine_.Heal();
        self->Reaction("¡Me siento mejor!", 255);
    }
    self->Refresh();
}

void HorneritoPetGame::OverlayThunk(lv_event_t* event) {
    auto* self = static_cast<HorneritoPetGame*>(lv_event_get_user_data(event));
    self->engine_.Init();  // reinicio (resetPet del original)
    self->reaction_ = nullptr;
    self->reaction_pose_ = 255;
    self->reaction_ticks_ = 0;
    self->frame_ = 0;
    self->sleep_div_ = 0;
    if (self->overlay_ != nullptr) {
        // Borrado diferido: evita eliminar el objeto dentro de su evento.
        lv_obj_delete_async(self->overlay_);
        self->overlay_ = nullptr;
    }
    self->shown_ = 255;
    if (self->DecodeFrame(0)) {
        self->shown_ = 0;
    }
    self->Refresh();
}

// -------------------------------------------------------------- lifecycle --

void HorneritoPetGame::Enter(lv_obj_t* content) {
    content_ = content;

    sprite_ok_ = ParseSprite();
    if (sprite_ok_) {
        std::memset(&dsc_, 0, sizeof(dsc_));
        dsc_.header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc_.header.cf = LV_COLOR_FORMAT_RGB565;
        dsc_.header.flags = 0;
        dsc_.header.w = fw_;
        dsc_.header.h = fh_;
        dsc_.header.stride = static_cast<uint16_t>(fw_ * 2);
        dsc_.data_size = static_cast<uint32_t>(fw_) * fh_ * 2;
        dsc_.data = reinterpret_cast<const uint8_t*>(frame_buf_);

        pet_ = lv_image_create(content_);
        lv_obj_set_pos(pet_, kSpriteX, kSpriteY);
        lv_image_set_scale(pet_, kSpriteScale);
        shown_ = 255;
        if (DecodeFrame(frame_)) {
            shown_ = frame_;
        }
        lv_image_set_src(pet_, &dsc_);
    } else {
        // No debería pasar (asset en flash); al menos el resto sigue vivo.
        pet_ = nullptr;
    }

    // Columna de stats (5 líneas a la derecha del sprite).
    for (int i = 0; i < 5; i++) {
        stats_lbl_[i] = lv_label_create(content_);
        lv_obj_set_style_text_color(stats_lbl_[i], lv_color_hex(kColorMuted),
                                    0);
        GameApplyFont(stats_lbl_[i]);
        lv_obj_set_pos(stats_lbl_[i], kStatsX, kStatsY0 + kStatsRow * i);
    }

    status_ = lv_label_create(content_);
    lv_obj_set_style_text_color(status_, lv_color_hex(kColorStatus), 0);
    GameApplyFont(status_);
    lv_obj_align(status_, LV_ALIGN_TOP_MID, 0, kStatusY);

    info_ = lv_label_create(content_);
    lv_obj_set_style_text_color(info_, lv_color_hex(kColorMuted), 0);
    GameApplyFont(info_);
    lv_obj_align(info_, LV_ALIGN_TOP_MID, 0, kInfoY);

    // Acciones del motor original.
    for (int i = 0; i < 5; i++) {
        btn_[i] = lv_button_create(content_);
        lv_obj_set_size(btn_[i], kBtnW, kBtnH);
        lv_obj_set_pos(btn_[i], kBtnX[i], kBtnY[i]);
        GameStyleDarkButton(btn_[i]);
        lv_obj_t* label = lv_label_create(btn_[i]);
        lv_label_set_text(label, kBtnTexts[i]);
        GameApplyFont(label);
        lv_obj_center(label);
        lv_obj_add_event_cb(btn_[i], BtnThunk, LV_EVENT_CLICKED, this);
        if (i == 3) {
            btn_sleep_label_ = label;
        }
    }

    anim_timer_ = lv_timer_create(AnimThunk, kAnimMs, this);
    stat_timer_ = lv_timer_create(StatThunk, kStatMs, this);

    Refresh();
    if (!engine_.is_alive()) {
        ShowDeath();  // pet muerto de una sesión anterior
    }
}

void HorneritoPetGame::Exit() {
    if (anim_timer_ != nullptr) {
        lv_timer_delete(anim_timer_);
        anim_timer_ = nullptr;
    }
    if (stat_timer_ != nullptr) {
        lv_timer_delete(stat_timer_);
        stat_timer_ = nullptr;
    }

    // Los widgets mueren con la pantalla de sesión; engine_, frame_ y el
    // buffer persisten en la instancia para re-entrar y seguir viviendo.
    content_ = nullptr;
    pet_ = nullptr;
    status_ = nullptr;
    info_ = nullptr;
    for (lv_obj_t*& label : stats_lbl_) {
        label = nullptr;
    }
    for (lv_obj_t*& button : btn_) {
        button = nullptr;
    }
    btn_sleep_label_ = nullptr;
    overlay_ = nullptr;
    reaction_ = nullptr;
    reaction_pose_ = 255;
    reaction_ticks_ = 0;
}
