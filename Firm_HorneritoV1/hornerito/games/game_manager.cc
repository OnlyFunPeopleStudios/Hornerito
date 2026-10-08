#include "hornerito/games/game_manager.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>

#include "application.h"
#include "board.h"
#include "display.h"
#include "hornerito/games/breakout.h"
#include "hornerito/games/catch_the_egg.h"
#include "hornerito/games/flappy.h"
#include "hornerito/games/hornerito_pet.h"
#include "hornerito/games/maze_runner.h"
#include "hornerito/games/memory.h"
#include "hornerito/games/pong.h"
#include "hornerito/games/rock_paper_scissors.h"
#include "hornerito/games/snake.h"
#include "hornerito/games/space_invaders.h"
#include "hornerito/games/tetris.h"
#include "hornerito/motion/hornerito_motion.h"
#include "hornerito/ui/hornerito_ui.h"
#include "material_symbols.h"

#define TAG "GameManager"

namespace {

// Geometría de la sesión: barra superior 64 px + content 480x416.
constexpr int kContentHeight = 416;

void ExitCb(lv_event_t* event) {
    (void)event;
    GameManager::GetInstance().Exit();
}

}  // namespace

GameManager& GameManager::GetInstance() {
    static GameManager instance;
    return instance;
}

HorneritoGame* GameManager::Lookup(GameId id) {
    switch (id) {
        case GameId::kPet:
            return &HorneritoPetGame::GetInstance();
        case GameId::kSnake:
            return &SnakeGame::GetInstance();
        case GameId::kPong:
            return &PongGame::GetInstance();
        case GameId::kBreakout:
            return &BreakoutGame::GetInstance();
        case GameId::kFlappy:
            return &FlappyGame::GetInstance();
        case GameId::kTetris:
            return &TetrisGame::GetInstance();
        case GameId::kSpaceInvaders:
            return &SpaceInvadersGame::GetInstance();
        case GameId::kMazeRunner:
            return &MazeRunnerGame::GetInstance();
        case GameId::kRockPaperScissors:
            return &RockPaperScissorsGame::GetInstance();
        case GameId::kMemory:
            return &MemoryGame::GetInstance();
        case GameId::kCatchTheEgg:
            return &CatchTheEggGame::GetInstance();
        default:
            return nullptr;
    }
}

void GameManager::BuildSession() {
    session_ = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(session_, lv_color_hex(0x0A0E12), 0);
    lv_obj_set_style_bg_opa(session_, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(session_, 0, 0);

    lv_obj_t* back = lv_button_create(session_);
    lv_obj_set_size(back, 56, 56);
    GameStyleDarkButton(back);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_add_event_cb(back, ExitCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* back_icon = lv_label_create(back);
    lv_label_set_text(back_icon, MATERIAL_SYMBOLS_ARROW_BACK);
    lv_obj_set_style_text_font(back_icon, HorneritoIconFont(), 0);
    lv_obj_center(back_icon);

    lv_obj_t* title = lv_label_create(session_);
    lv_label_set_text(title, current_->Name());
    GameApplyFont(title);
    lv_obj_set_style_text_color(title, lv_color_hex(0x5BC8F5), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 14);

    content_ = lv_obj_create(session_);
    lv_obj_set_size(content_, 480, kContentHeight);
    lv_obj_align(content_, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_bg_opa(content_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
}

bool GameManager::Start(GameId id) {
    if (current_ != nullptr) {
        // Sesión huérfana (p. ej. un evento externo forzó idle): autolimpieza
        // para poder reentrar sin reiniciar Hornerito.
        Exit();
    }

    auto& app = Application::GetInstance();
    if (app.GetDeviceState() != kDeviceStateIdle) {
        ESP_LOGW(TAG, "Start rechazado (estado no idle)");
        return false;
    }

    HorneritoGame* game = Lookup(id);
    if (game == nullptr) {
        ESP_LOGW(TAG, "Juego no disponible");
        return false;
    }
    if (!app.SetDeviceState(kDeviceStatePlayingGame)) {
        ESP_LOGW(TAG, "No se pudo entrar en PLAYING_GAME");
        return false;
    }

    current_ = game;
    {
        // Mismo patrón que ShowHome/ShowMusic: construir y cargar bajo el
        // lock del display (el mutex es recursivo: seguro desde eventos LVGL).
        DisplayLockGuard lock(Board::GetInstance().GetDisplay());
        BuildSession();
        game->Enter(content_);
        lv_screen_load(session_);
    }
    ESP_LOGI(TAG, "%s iniciado. Heap libre: %u, mayor bloque: %u",
             game->Name(), static_cast<unsigned>(esp_get_free_heap_size()),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
    return true;
}

void GameManager::Exit() {
    if (current_ == nullptr) {
        return;
    }

    HorneritoGame* game = current_;
    current_ = nullptr;
    game->Exit();
    HorneritoMotion::GetInstance().Disable();

    lv_obj_t* session = session_;
    session_ = nullptr;
    content_ = nullptr;

    // Primero carga el menú (la sesión deja de ser la pantalla activa) y
    // después la borra diferida: los timers/evts del juego ya están fuera.
    HorneritoUi::GetInstance().ShowGames();
    if (session != nullptr) {
        lv_obj_delete_async(session);
    }

    Application::GetInstance().SetDeviceState(kDeviceStateIdle);
    ESP_LOGI(TAG, "%s salido. Heap libre: %u, mayor bloque: %u",
             game->Name(), static_cast<unsigned>(esp_get_free_heap_size()),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}
