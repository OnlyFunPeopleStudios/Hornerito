#include "hornerito/ui/hornerito_ui.h"

#include <esp_app_desc.h>
#include <esp_timer.h>

#include <cstdio>
#include <string>

#include "application.h"
#include "board.h"
#include "display.h"
#include "hornerito/games/game_manager.h"
#include "lvgl_font.h"
#include "lvgl_theme.h"
#include "material_symbols.h"
#include "power_save_timer.h"
#include "settings.h"

namespace {

const char* DeviceStateText(DeviceState state) {
    switch (state) {
        case kDeviceStateStarting:
            return "Iniciando...";
        case kDeviceStateWifiConfiguring:
            return "Configurando Wi-Fi...";
        case kDeviceStateIdle:
            return "En espera";
        case kDeviceStateConnecting:
            return "Conectando...";
        case kDeviceStateListening:
            return "Escuchando...";
        case kDeviceStateSpeaking:
            return "Hablando...";
        case kDeviceStateNotifying:
            return "Aviso";
        case kDeviceStateUpgrading:
            return "Actualizando...";
        case kDeviceStateActivating:
            return "Activando...";
        case kDeviceStateAudioTesting:
            return "Probando audio...";
        case kDeviceStatePlayingGame:
            return "Jugando...";
        default:
            return "Desconocido";
    }
}

void ApplyFont(lv_obj_t* obj, const lv_font_t* font) {
    if (font != nullptr) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
}

void StyleDarkButton(lv_obj_t* button) {
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2E4058), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_radius(button, 16, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
}

}  // namespace

const lv_font_t* HorneritoTextFont() {
    auto* display = Board::GetInstance().GetDisplay();
    if (display == nullptr || display->GetTheme() == nullptr) {
        return nullptr;
    }
    auto font = display->GetTheme()->GetTextFont();
    return font ? font->font() : nullptr;
}

const lv_font_t* HorneritoIconFont() {
    auto* display = Board::GetInstance().GetDisplay();
    if (display == nullptr || display->GetTheme() == nullptr) {
        return nullptr;
    }
    auto font = static_cast<LvglTheme*>(display->GetTheme())->icon_font();
    return font ? font->font() : nullptr;
}

HorneritoUi& HorneritoUi::GetInstance() {
    static HorneritoUi instance;
    return instance;
}

void HorneritoUi::Start() {
    if (started_) {
        return;
    }
    auto* display = Board::GetInstance().GetDisplay();
    if (display == nullptr) {
        return;
    }

    {
        DisplayLockGuard lock(display);
        // La pantalla activa en este momento es la de XiaoZhi: la guardamos
        // como pantalla "Asistente" y le añadimos un botón de menú flotante.
        assistant_screen_ = lv_screen_active();
        BuildHome();
        BuildStatus();
        BuildGames();
        BuildSettings();
        music_ui_.Build();
        files_ui_.Build();
        wifi_ui_.Build();
        // calendar_ui_.Build() NO aquí: se construye al primer uso
        // (ShowCalendar) para no consumir heap en el boot.
        BuildAssistantEntry();
        timer_ = lv_timer_create(TimerCb, 500, this);
        started_ = true;
    }

    // Escaneo inicial de la microSD en background (no bloquea la UI).
    HorneritoMusicPlayer::GetInstance().RequestRefresh();
}

void HorneritoUi::ShowHome() {
    if (home_ == nullptr) {
        return;
    }
    // Si había una conversación abierta (asistente), salir al inicio la
    // cierra: OnTick se encarga de abortar/cerrar hasta volver a idle.
    auto app_state = Application::GetInstance().GetDeviceState();
    if (app_state == kDeviceStateListening || app_state == kDeviceStateSpeaking ||
        app_state == kDeviceStateConnecting) {
        leaving_assistant_ = true;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    lv_screen_load(home_);
    home_shown_ = true;
    last_state_ = kDeviceStateUnknown;  // refresca etiqueta y mascota
    UpdatePet(Application::GetInstance().GetDeviceState());
}

void HorneritoUi::ShowAssistant() {
    // Entrar a mano (o por wake word) reactiva la auto-escucha.
    leaving_assistant_ = false;
    // La conversación y la música comparten el mismo codec y el heap: al
    // entrar en el asistente se DETIENE la reproducción y se liberan decoder
    // y remuestreador. Con el heap partido en bloques de 3776 B el TLS no
    // conseguía 4437 B y el equipo abortaba al conectarse.
    HorneritoMusicPlayer::GetInstance().Stop();
    if (assistant_screen_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    lv_screen_load(assistant_screen_);
}

void HorneritoUi::ShowMusic() {
    if (music_ui_.screen() == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    music_ui_.OnShow();
    lv_screen_load(music_ui_.screen());
}

void HorneritoUi::ShowFiles() {
    if (files_ui_.screen() == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    files_ui_.OnShow();
    lv_screen_load(files_ui_.screen());
}

void HorneritoUi::ShowStatus() {
    if (status_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    lv_screen_load(status_);
    UpdateStatusInfo(Application::GetInstance().GetDeviceState());
}

void HorneritoUi::ShowGames() {
    if (games_menu_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    lv_screen_load(games_menu_);
}

void HorneritoUi::ShowSettings() {
    if (settings_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    // Refresca el slider: el brillo puede haber cambiado por otro camino
    // (asistente/MCP o la restauración al salir del ahorro de energía).
    if (settings_brightness_ != nullptr) {
        if (auto* backlight = Board::GetInstance().GetBacklight()) {
            lv_slider_set_value(settings_brightness_, backlight->brightness(), LV_ANIM_OFF);
        }
    }
    lv_screen_load(settings_);
}

void HorneritoUi::ShowWifi() {
    if (wifi_ui_.screen() == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    lv_screen_load(wifi_ui_.screen());
    wifi_ui_.OnShow();  // tras cargar: Refresh pinta de inmediato (active()==true)
}

void HorneritoUi::ShowCalendar() {
    // Entrada: menú Calendario (Estado / Vincular / iCal / ...); "Ver
    // agenda" y el éxito del vinculado llevan a la pantalla de agenda.
    // Build perezoso: el primer gesto lo construye (ahorra ~13 KB de heap
    // en el boot, margen para la wake word al entrar a idle).
    if (calendar_ui_.entry() == nullptr) {
        calendar_ui_.Build();
    }
    if (calendar_ui_.entry() == nullptr) {
        return;
    }
    DisplayLockGuard lock(Board::GetInstance().GetDisplay());
    lv_screen_load(calendar_ui_.entry());
    calendar_ui_.OnShowEntry();
}

void HorneritoUi::TimerCb(lv_timer_t* timer) {
    auto* self = static_cast<HorneritoUi*>(lv_timer_get_user_data(timer));
    if (self == nullptr || !self->started_) {
        return;
    }
    // Cualquier toque reinicia el temporizador de apagado: sin esto la
    // pantalla se atenúa a los N s aunque se esté usando, porque solo los
    // botones físicos llamaban WakeUp().
    for (auto* indev = lv_indev_get_next(nullptr); indev != nullptr;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER &&
            lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
            if (auto* pst = Board::GetInstance().GetPowerSaveTimer()) {
                pst->WakeUp();
            }
            break;
        }
    }
    self->OnTick(Application::GetInstance().GetDeviceState());
}

void HorneritoUi::OnTick(DeviceState state) {
    // --- Navegación según el estado del dispositivo ---
    if (state == kDeviceStateWifiConfiguring) {
        // Solo al ENTRAR en configuración mostramos el asistente (una única
        // vez). Si se hiciera en cada tick, con el portal activo sería
        // imposible llegar a Configuración -> Wi-Fi desde el dispositivo.
        if (!was_configuring_) {
            was_configuring_ = true;
            if (!wifi_ui_.active() && lv_screen_active() != assistant_screen_) {
                ShowAssistant();
            }
        }
    } else if (state == kDeviceStateIdle) {
        leaving_assistant_ = false;
        if (was_configuring_) {
            // Vuelve de configurar Wi-Fi: mostramos la pantalla principal.
            was_configuring_ = false;
            if (!wifi_ui_.active()) {
                ShowHome();
            }
        } else if (!home_shown_) {
            // Primer arranque correcto: la pantalla principal manda.
            ShowHome();
        } else if (lv_screen_active() == assistant_screen_) {
            // En la pantalla del asistente se escucha SOLO: sin tocar la
            // pantalla. Si el canal falla vuelve a idle y reintenta a los
            // 10 s (ponytail: cooldown para no bombardear el servidor).
            int64_t now = esp_timer_get_time();
            if (now - last_auto_listen_us_ >= 10000000) {
                last_auto_listen_us_ = now;
                Application::GetInstance().ToggleChatState();
            }
        }
    } else if (state == kDeviceStateListening || state == kDeviceStateSpeaking) {
        if (leaving_assistant_) {
            // El usuario salió al inicio con la conversación abierta: cerrar
            // (speaking aborta a listening y el siguiente tick cierra el
            // canal) sin rebotar de vuelta al asistente.
            Application::GetInstance().ToggleChatState();
        } else if (lv_screen_active() != assistant_screen_) {
            ShowAssistant();
        }
    }

    // --- La música se pausa si el asistente sale de reposo ---
    auto& player = HorneritoMusicPlayer::GetInstance();
    if (state != kDeviceStateIdle && player.state() == HorneritoMusicState::kPlaying) {
        player.TogglePause();
    }

    // --- Refresco de las pantallas propias visibles ---
    if (lv_screen_active() == home_) {
        UpdatePet(state);
    } else if (lv_screen_active() == status_) {
        UpdateStatusInfo(state);
    }
}

void HorneritoUi::UpdatePet(DeviceState state) {
    HorneritoEmoji emoji;
    switch (state) {
        case kDeviceStateIdle:
            emoji = HorneritoEmoji::kIdle;
            break;
        case kDeviceStateListening:
            emoji = HorneritoEmoji::kListening;
            break;
        case kDeviceStateSpeaking:
            emoji = HorneritoEmoji::kSpeaking;
            break;
        case kDeviceStateUnknown:
            emoji = HorneritoEmoji::kError;
            break;
        default:
            emoji = HorneritoEmoji::kThinking;
            break;
    }

    if (pet_ != nullptr && emoji != last_emoji_) {
        lv_image_set_src(pet_, HorneritoEmojiImage(emoji));
        last_emoji_ = emoji;
    }
    if (state_label_ != nullptr && state != last_state_) {
        lv_label_set_text(state_label_, DeviceStateText(state));
        last_state_ = state;
    }
}

void HorneritoUi::UpdateStatusInfo(DeviceState state) {
    char buffer[96];

    if (status_state_ != nullptr) {
        snprintf(buffer, sizeof(buffer), "Estado: %s", DeviceStateText(state));
        lv_label_set_text(status_state_, buffer);
    }

    if (status_battery_ != nullptr) {
        int level = 0;
        bool charging = false;
        bool discharging = false;
        if (Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
            snprintf(buffer, sizeof(buffer), "Bateria: %d%%%s", level,
                     charging ? " (cargando)" : "");
        } else {
            snprintf(buffer, sizeof(buffer), "Bateria: --");
        }
        lv_label_set_text(status_battery_, buffer);
    }

    if (status_storage_ != nullptr) {
        auto& player = HorneritoMusicPlayer::GetInstance();
        auto count = player.SongCount();
        switch (player.state()) {
            case HorneritoMusicState::kNotMounted:
                snprintf(buffer, sizeof(buffer), "microSD: sin tarjeta");
                break;
            case HorneritoMusicState::kNoSongs:
                snprintf(buffer, sizeof(buffer), "microSD: sin canciones");
                break;
            case HorneritoMusicState::kError:
                snprintf(buffer, sizeof(buffer), "microSD: error");
                break;
            default:
                snprintf(buffer, sizeof(buffer), "microSD: %u canciones",
                         static_cast<unsigned>(count));
                break;
        }
        lv_label_set_text(status_storage_, buffer);
    }
}

void HorneritoUi::BuildHome() {
    // En esta versión de LVGL una pantalla se crea como objeto sin padre.
    home_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(home_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(home_, LV_OPA_COVER, 0);

    lv_obj_t* title = lv_label_create(home_);
    lv_label_set_text(title, "HORNERITO V2.2");
    ApplyFont(title, HorneritoTextFont());
    lv_obj_set_style_text_color(title, lv_color_hex(0x5BC8F5), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    pet_ = lv_image_create(home_);
    lv_image_set_src(pet_, HorneritoEmojiImage(HorneritoEmoji::kIdle));
    lv_obj_align(pet_, LV_ALIGN_TOP_MID, 0, 58);

    state_label_ = lv_label_create(home_);
    lv_label_set_text(state_label_, "En espera");
    ApplyFont(state_label_, HorneritoTextFont());
    lv_obj_set_width(state_label_, 448);
    lv_label_set_long_mode(state_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(state_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(state_label_, LV_ALIGN_TOP_MID, 0, 250);

    // Menú de la pantalla principal (launcher estilo firmware de fábrica):
    // rejilla 2x3 -> Asistente / Música / Archivos / Estado / Juegos / Configuración
    lv_obj_t* menu = lv_obj_create(home_);
    lv_obj_set_size(menu, 448, 172);
    lv_obj_align(menu, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_opa(menu, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menu, 0, 0);
    lv_obj_set_style_radius(menu, 0, 0);
    lv_obj_set_style_pad_all(menu, 0, 0);
    lv_obj_set_scrollbar_mode(menu, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(menu, LV_FLEX_FLOW_ROW_WRAP);
    // Con 6 accesos quedan 3 filas de 2 botones (la de 2 se centra sola).
    lv_obj_set_flex_align(menu, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(menu, 8, 0);
    lv_obj_set_style_pad_column(menu, 8, 0);

    const char* labels[] = {"Asistente", "M\u00fasica", "Archivos", "Estado",
                            "Juegos", "Configuraci\u00f3n"};
    for (intptr_t code = 0; code < 6; code++) {
        lv_obj_t* button = lv_button_create(menu);
        lv_obj_set_size(button, 220, 52);
        StyleDarkButton(button);
        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, labels[code]);
        ApplyFont(label, HorneritoTextFont());
        lv_obj_center(label);
        lv_obj_add_event_cb(
            button,
            [](lv_event_t* event) {
                auto code = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
                auto& ui = HorneritoUi::GetInstance();
                if (code == 0) {
                    ui.ShowAssistant();
                } else if (code == 1) {
                    ui.ShowMusic();
                } else if (code == 2) {
                    ui.ShowFiles();
                } else if (code == 3) {
                    ui.ShowStatus();
                } else if (code == 4) {
                    ui.ShowGames();
                } else {
                    ui.ShowSettings();
                }
            },
            LV_EVENT_CLICKED, reinterpret_cast<void*>(code));
    }
}

void HorneritoUi::BuildStatus() {
    status_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(status_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(status_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(status_, 16, 0);

    lv_obj_t* back = lv_button_create(status_);
    lv_obj_set_size(back, 56, 56);
    StyleDarkButton(back);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(
        back, [](lv_event_t* event) {
            (void)event;
            HorneritoUi::GetInstance().ShowHome();
        },
        LV_EVENT_CLICKED, nullptr);

    lv_obj_t* heading = lv_label_create(status_);
    lv_label_set_text(heading, "Estado");
    ApplyFont(heading, HorneritoTextFont());
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* content = lv_obj_create(status_);
    lv_obj_set_size(content, 448, 340);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(content, 14, 0);

    lv_obj_t* identity = lv_label_create(content);
    lv_label_set_text(identity, "HORNERITO V2.2");
    ApplyFont(identity, HorneritoTextFont());
    lv_obj_set_style_text_color(identity, lv_color_hex(0x5BC8F5), 0);

    lv_obj_t* version = lv_label_create(content);
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Version: %s", esp_app_get_description()->version);
    lv_label_set_text(version, buffer);
    ApplyFont(version, HorneritoTextFont());

    status_state_ = lv_label_create(content);
    lv_label_set_text(status_state_, "Estado: --");
    ApplyFont(status_state_, HorneritoTextFont());
    lv_obj_set_width(status_state_, 448);
    lv_label_set_long_mode(status_state_, LV_LABEL_LONG_SCROLL_CIRCULAR);

    status_battery_ = lv_label_create(content);
    lv_label_set_text(status_battery_, "Bateria: --");
    ApplyFont(status_battery_, HorneritoTextFont());

    status_storage_ = lv_label_create(content);
    lv_label_set_text(status_storage_, "microSD: --");
    ApplyFont(status_storage_, HorneritoTextFont());
    lv_obj_set_width(status_storage_, 448);
    lv_label_set_long_mode(status_storage_, LV_LABEL_LONG_SCROLL_CIRCULAR);
}

void HorneritoUi::BuildGames() {
    if (games_menu_ != nullptr) {
        return;
    }

    games_menu_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(games_menu_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(games_menu_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(games_menu_, 16, 0);

    lv_obj_t* back = lv_button_create(games_menu_);
    lv_obj_set_size(back, 56, 56);
    StyleDarkButton(back);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(
        back, [](lv_event_t* event) {
            (void)event;
            HorneritoUi::GetInstance().ShowHome();
        },
        LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_icon = lv_label_create(back);
    lv_label_set_text(back_icon, MATERIAL_SYMBOLS_ARROW_BACK);
    ApplyFont(back_icon, HorneritoIconFont());
    lv_obj_center(back_icon);

    lv_obj_t* heading = lv_label_create(games_menu_);
    lv_label_set_text(heading, "Juegos");
    ApplyFont(heading, HorneritoTextFont());
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* list = lv_obj_create(games_menu_);
    lv_obj_set_size(list, 448, 340);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 10, 0);

    const char* labels[] = {
        "Hornerito Pet", "Snake",           "Pong",
        "Breakout",      "Flappy",          "Tetris",
        "Space Invaders", "Maze Runner",    "Piedra Papel Tijera",
        "Memory",        "Catch the Egg"};
    for (intptr_t code = 0;
         code < static_cast<intptr_t>(sizeof(labels) / sizeof(labels[0]));
         code++) {
        lv_obj_t* button = lv_button_create(list);
        lv_obj_set_size(button, 448, 60);
        StyleDarkButton(button);
        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, labels[code]);
        ApplyFont(label, HorneritoTextFont());
        lv_obj_center(label);
        lv_obj_add_event_cb(
            button,
            [](lv_event_t* event) {
                auto id = static_cast<GameId>(
                    reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
                GameManager::GetInstance().Start(id);
            },
            LV_EVENT_CLICKED, reinterpret_cast<void*>(code));
    }
}

void HorneritoUi::BuildSettings() {
    if (settings_ != nullptr) {
        return;
    }

    settings_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(settings_, lv_color_hex(0x101418), 0);
    lv_obj_set_style_bg_opa(settings_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(settings_, 16, 0);

    lv_obj_t* back = lv_button_create(settings_);
    lv_obj_set_size(back, 56, 56);
    StyleDarkButton(back);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(
        back, [](lv_event_t* event) {
            (void)event;
            HorneritoUi::GetInstance().ShowHome();
        },
        LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_icon = lv_label_create(back);
    lv_label_set_text(back_icon, MATERIAL_SYMBOLS_ARROW_BACK);
    ApplyFont(back_icon, HorneritoIconFont());
    lv_obj_center(back_icon);

    lv_obj_t* heading = lv_label_create(settings_);
    lv_label_set_text(heading, "Configuraci\u00f3n");
    ApplyFont(heading, HorneritoTextFont());
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* list = lv_obj_create(settings_);
    lv_obj_set_size(list, 448, 340);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list, 10, 0);

    const char* labels[] = {"Wi-Fi", "Calendario"};
    for (intptr_t code = 0;
         code < static_cast<intptr_t>(sizeof(labels) / sizeof(labels[0]));
         code++) {
        lv_obj_t* button = lv_button_create(list);
        lv_obj_set_size(button, 448, 60);
        StyleDarkButton(button);
        lv_obj_t* label = lv_label_create(button);
        lv_label_set_text(label, labels[code]);
        ApplyFont(label, HorneritoTextFont());
        lv_obj_center(label);
        lv_obj_add_event_cb(
            button,
            [](lv_event_t* event) {
                auto code = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
                auto& ui = HorneritoUi::GetInstance();
                if (code == 0) {
                    ui.ShowWifi();
                } else if (code == 1) {
                    ui.ShowCalendar();
                }
            },
            LV_EVENT_CLICKED, reinterpret_cast<void*>(code));
    }

    // --- Brillo (sin sensor de luz: control manual) ---
    lv_obj_t* bright_row = lv_obj_create(list);
    lv_obj_set_size(bright_row, 448, 72);
    lv_obj_set_style_bg_opa(bright_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bright_row, 0, 0);
    lv_obj_set_style_radius(bright_row, 0, 0);
    lv_obj_set_style_pad_all(bright_row, 0, 0);
    lv_obj_set_scrollbar_mode(bright_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(bright_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bright_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bright_row, 16, 0);

    lv_obj_t* bright_label = lv_label_create(bright_row);
    lv_label_set_text(bright_label, "Brillo");
    ApplyFont(bright_label, HorneritoTextFont());

    lv_obj_t* slider = lv_slider_create(bright_row);
    lv_obj_set_width(slider, 300);
    lv_slider_set_range(slider, 10, 100);
    lv_obj_add_event_cb(
        slider,
        [](lv_event_t* event) {
            auto* backlight = Board::GetInstance().GetBacklight();
            if (backlight != nullptr) {
                backlight->SetBrightness(
                    static_cast<uint8_t>(lv_slider_get_value(
                        static_cast<lv_obj_t*>(lv_event_get_target(event)))), true);
            }
        },
        LV_EVENT_VALUE_CHANGED, nullptr);
    if (auto* backlight = Board::GetInstance().GetBacklight()) {
        lv_slider_set_value(slider, backlight->brightness(), LV_ANIM_OFF);
    }
    settings_brightness_ = slider;

    // --- Apagado de pantalla: inactividad antes de atenuarse/apagarse ---
    lv_obj_t* off_btn = lv_button_create(list);
    lv_obj_set_size(off_btn, 448, 60);
    StyleDarkButton(off_btn);
    lv_obj_t* off_title = lv_label_create(off_btn);
    lv_label_set_text(off_title, "Apagado de pantalla");
    ApplyFont(off_title, HorneritoTextFont());
    lv_obj_align(off_title, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_t* off_value = lv_label_create(off_btn);
    ApplyFont(off_value, HorneritoTextFont());
    lv_obj_align(off_value, LV_ALIGN_RIGHT_MID, -20, 0);
    settings_off_label_ = off_value;
    UpdateScreenOffLabel();
    lv_obj_add_event_cb(off_btn, ScreenOffCb, LV_EVENT_CLICKED, nullptr);
}

void HorneritoUi::UpdateScreenOffLabel() {
    auto* label = GetInstance().settings_off_label_;
    if (label == nullptr) {
        return;
    }
    int32_t secs = Settings("display").GetInt("screen_off", 120);
    char buf[16];
    if (secs < 0) {
        snprintf(buf, sizeof(buf), "Nunca");
    } else {
        snprintf(buf, sizeof(buf), "%ld min", static_cast<long>(secs / 60));
    }
    lv_label_set_text(label, buf);
}

void HorneritoUi::ScreenOffCb(lv_event_t* event) {
    (void)event;
    // 1 / 2 / 5 / 10 / 20 minutos / Nunca (en NVS "display"/screen_off).
    static const int32_t options[] = {60, 120, 300, 600, 1200, -1};
    constexpr size_t kCount = sizeof(options) / sizeof(options[0]);
    int32_t secs = Settings("display").GetInt("screen_off", 120);
    size_t idx = kCount;  // valor actual fuera de la lista
    for (size_t i = 0; i < kCount; ++i) {
        if (options[i] == secs) {
            idx = i;
            break;
        }
    }
    int32_t next = (idx < kCount) ? options[(idx + 1) % kCount] : options[0];
    Settings("display", true).SetInt("screen_off", next);
    if (auto* pst = Board::GetInstance().GetPowerSaveTimer()) {
        // El equipo se apaga 20 min después de la pantalla; con "Nunca",
        // nunca (mismos valores que lee el board en InitializePowerSaveTimer).
        pst->SetTimeouts(next, next == -1 ? -1 : next + 1200);
    }
    UpdateScreenOffLabel();
}

void HorneritoUi::BuildAssistantEntry() {
    if (assistant_screen_ == nullptr) {
        return;
    }
    // Botón flotante sobre la pantalla de XiaoZhi para volver al menú.
    // Ocupa la esquina superior izquierda (x < 60: el status bar de XiaoZhi
    // deja ese hueco libre con pad_left = 60).
    lv_obj_t* button = lv_button_create(assistant_screen_);
    lv_obj_set_size(button, 54, 44);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1E2836), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2E4058), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_align(button, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_add_event_cb(
        button, [](lv_event_t* event) {
            (void)event;
            HorneritoUi::GetInstance().ShowHome();
        },
        LV_EVENT_CLICKED, nullptr);

    lv_obj_t* icon = lv_label_create(button);
    lv_label_set_text(icon, MATERIAL_SYMBOLS_HOME);
    ApplyFont(icon, HorneritoIconFont());
    lv_obj_center(icon);
}
