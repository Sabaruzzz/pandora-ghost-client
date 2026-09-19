#pragma once
#include "mapper.hpp"
#include <string>
#include <utility>
#include <vector>
#include <atomic>

namespace features
{
    void run_on_run_tick(mapper::__minecraft& minecraft);
    void run_on_render_world(mapper::__minecraft& minecraft, unsigned __int64* last_render_time);
}
namespace features::combat::weapons
{
    inline bool sword = true;
}

namespace features::combat::reach
{
    inline bool enabled = false;
    inline float min_distance = 3.0f;
    inline float max_distance = 3.10f;
    inline bool hitbox_enabled = false;
    inline float hitbox_size = 0.20f;
    inline float chance = 100.0f;
    inline bool ground_only = false;
    inline bool weapon_only = false;
    inline bool liquid_check = false;
    inline bool combo_mode = false;

    void run(mapper::__minecraft& minecraft);
}
namespace features::combat::velocity
{
    inline bool  enabled = false;
    inline int   mode = 0;               
    inline bool  air_only = false;
    inline bool  moving_only = false;
    inline bool  weapon_only = false;
    inline bool  push_back = false;
    inline bool  clicking_only = false;
    inline bool  universocraft_bypass = false; // Bypass para GrimAC/Vulcan
    inline float horizontal = 100.0f;
    inline float vertical = 100.0f;
    inline float chance = 100.0f;
    inline float delay = 0.0f;

    void run(mapper::__minecraft& minecraft);

    // ESP: true while the celeste self-ESP should render
    extern bool lag_esp_active;
}

// --- NO HIT DELAY ---
namespace features::combat::no_hit_delay {
    extern bool enabled;
    void run(mapper::__minecraft& mc);
}
// --- BLOCKHIT ---
namespace features::combat::block_hit {
    void run(mapper::__minecraft& minecraft);
}

// --- MACROS ---
namespace features::combat::macros {
    void run(mapper::__minecraft& minecraft);
}
// --- ARMOR SWITCHER ---
namespace features::combat::armor_switcher {
    void run(mapper::__minecraft& minecraft);
}

// --- COMBATE ---
namespace features::combat::auto_click
{
    inline bool enabled = false;
    inline double min_cps = 12.0;
    inline double max_cps = 14.0;
    inline bool inventory_enabled = true;
    inline double inventory_cps = 20.0; // CPS for inventory fill (fast but server-safe)
    inline float jitter_intensity = 0.5f;
    inline bool no_hit_delay_enabled = false;
    inline bool weapons_only = false;
    inline bool break_blocks = false;

    // Click Method: 0=Normal, 1=Jitter, 2=Butterfly
    inline int click_method = 0;

    // Conditions
    inline bool target_only = false;
    inline bool blockhit_sync = false;
    inline bool randomization = false;
    inline float drop_chance = 5.0f;   // 0 - 100%
    inline float spike_chance = 2.0f;  // 0 - 100%

    // Click counter (W TAP)
    int get_click_count();
    void reset_click_count();
    void shutdown();

    void run(mapper::__minecraft& minecraft);
}
namespace features::combat::aim_assist
{
    inline bool enabled = false;
    inline int mode = 0;              // 0=Smooth, 1=Legit, 2=Aim-Lock, 3=Vertical
    inline double distance = 4.0;
    inline double fov = 90.0;
    inline double speed = 5.0;
    inline bool clicking_only = false;
    inline bool lock_target = false;
    inline bool weapons_only = false;
    inline bool axe_only = false;
    inline bool food_only = false;
    inline bool break_blocks = false;
    inline bool ignore_walls = false;
    inline bool ignore_invisible = false;
    inline bool stick = false;  // ---- NUEVO: Anti-Switch
    inline float angle_x_changes = 0.f;  // rotacion horizontal calculada
    inline float angle_y_changes = 0.f;  // rotacion vertical calculada
    inline bool  silent = true;          // si no la tenes ya
    
    // Mode-specific settings
    inline float lock_strength = 70.0f;    // Aim-Lock: fuerza de adherencia (%)
    inline float flick_strength = 40.0f;   // Head: intensidad del micro-ajuste hacia la cabeza (%)

    void run(mapper::__minecraft& minecraft);
}
// --- REFILL (NUEVO MODULO) ---
namespace features::combat::refill
{
    inline bool enabled = false;
    inline int bind = 0;          // Tecla para activar el refill en combate
    inline int delay_ms = 80;     // Slider de milisegundos (Velocidad de refil)
    void run(mapper::__minecraft& minecraft);
}

// --- MISC / PLAYER ---
namespace features::misc::auto_armor
{
    void run(mapper::__minecraft& minecraft);
}

namespace features::misc::fastplace
{
    void run(mapper::__minecraft& minecraft);
}

// --- MISC / PLAYER ---
namespace features::misc::blink
{
    inline bool enabled = false;
    inline int  mode = 0;           // 0=Smooth, 1=Freeze
    inline bool show_path = true;
    inline bool show_timer = true;
    inline float path_color[3] = { 0.2f, 0.6f, 1.0f };
    inline float timer_limit = 5.0f; // in seconds
    inline int bind = 0; // Tecla para activar el Blink

    // Variable global que tu futuro Hook de red leera para cancelar paquetes
    extern bool is_blinking;

    void run(mapper::__minecraft& minecraft);
    void render_world(mapper::__minecraft& minecraft);
    void render_ui();
}
// --- MOVEMENT ---
namespace features::movement::sprint
{
    inline bool enabled = false;
    inline bool omni = false; // sprint en todas direcciones

    void run(mapper::__minecraft& minecraft);
}

namespace features::movement::no_slow
{
    inline bool enabled = false;

    void run(mapper::__minecraft& minecraft);
}

namespace features::movement::no_item_release
{
    inline bool enabled = false;
    inline bool food = false;

    void run(mapper::__minecraft& minecraft);
}

namespace features::movement::no_jump_delay
{
    inline bool enabled = false;

    void run(mapper::__minecraft& minecraft);
}
// --- MOVEMENT ---
// --- ARRAYLIST (HUD VISUAL) ---
namespace features::visual::arraylist
{
    inline bool enabled = true;
    inline bool watermark = true;
    inline bool rainbow = false;
    inline float color[3] = { 0.85f, 0.05f, 0.80f };
    void run();
}

// ============================================================================
// VISUALES / MOTOR GRAFICO
// ============================================================================

namespace features::visual
{
    extern void* window;

    // Flag: false cuando estamos en loading screen, menu, o sin mundo
    // Shared between the Minecraft/render hook and the swap-buffers thread.
    // Atomic prevents torn/racy visibility while a world is loaded/unloaded.
    inline std::atomic_bool render_valid{ false };
    // Ultimo frame en el que OpenGL entrego matrices sincronizadas con la camara.
    // El lector de Java se usa solamente como respaldo cuando esta captura falta.
    inline unsigned long long matrix_capture_tick = 0;

    // Matrices matematicas del juego
    extern double model_view_matrix[16];
    extern double projection_matrix[16];
    extern int view_port[4];
    extern double render_camera_x;
    extern double render_camera_y;
    extern double render_camera_z;
    extern float render_partial_ticks;
    extern bool render_frame_snapshot_valid;

    // Declaracion de las funciones de renderizado
    auto world_to_screen(mapper::__vec3 data, bool can_reverse = false, bool ignore_z = false) -> mapper::__vec2;
    auto render_2d_bounding_box(mapper::__vec3 vec3, mapper::__vec4 outline_color, mapper::__vec4 line_color, mapper::__vec4 fill_color, bool draw_corners, bool draw_health, float health, bool draw_hurt_time, __int32 hurt_time) -> void;
    auto render_3d_bounding_box(mapper::__vec3 vec3, mapper::__vec4 color, bool draw_health, float health, bool draw_hurt_time, __int32 hurt_time, int entity_id = -1) -> void;
    auto render_nametag(std::string name, mapper::__vec3 vec3, mapper::__vec4 color, bool draw_health, float health, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void;
    auto render_tracer(mapper::__vec3 vec3, mapper::__vec4 color, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void;

    // Helpers
    std::string get_formatted_name(void* env_ptr, void* player_obj);
    mapper::__vec4 extract_color_from_format(const std::string& formatted_name, mapper::__vec4 fallback_color);
}

// Declaracion de los modulos visuales para que el menu ImGui funcione
namespace features::visual::player_esp_2d
{
    inline bool enabled = false;
    inline bool draw_health = true;
    inline bool draw_hurt_time = true;
    inline bool draw_invisible_players = false;

    inline mapper::__vec4 outline_color = { 0.0f, 0.0f, 0.0f, 1.0f };
    inline mapper::__vec4 line_color = { 1.0f, 0.0f, 0.0f, 1.0f };
    inline mapper::__vec4 fill_color = { 0.0f, 0.0f, 0.0f, 0.2f };

    void run(mapper::__minecraft& minecraft);
    auto render() -> void;
}

namespace features::visual::player_esp_3d
{
    inline bool enabled = false;
    inline bool draw_health = true;
    inline bool draw_hurt_time = true;
    inline bool draw_invisible_players = false;
    inline mapper::__vec4 color = { 0.8f, 0.1f, 0.1f, 0.3f };

    void run(mapper::__minecraft& minecraft);
    auto render() -> void;
}

namespace features::visual::nametags
{
    void initialize_hook();
    void sync_hide_vanilla_hook(bool should_attach);
    void uninitialize_hook();
    void shutdown(mapper::__minecraft& minecraft);
    inline bool enabled = false;
    inline bool show_equipment = true;
    inline bool show_enchantments = true;
    inline bool draw_health = true;
    inline bool draw_distance = true;
    inline bool draw_hurt_time = true;
    inline bool draw_invisible_players = false;
    inline bool background = true;

    // use_fake_name: funcion independiente de hide_nickname
    inline bool use_fake_name = false;
    inline std::string fake_name = "Jugador";

    inline mapper::__vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    void run(mapper::__minecraft& minecraft);
    auto render() -> void;
    void render_equipment_native();
    void shutdown_render_resources();
    void abandon_render_resources_after_context_loss();
}

namespace features::visual::tracers
{
    inline bool enabled = false;
    inline bool draw_distance = false;
    inline bool draw_hurt_time = false;
    inline bool draw_invisible_players = false;
    inline float thickness = 1.5f;
    inline mapper::__vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

    void run(mapper::__minecraft& minecraft);
    auto render() -> void;
}

namespace features::visual::aguita
{
    inline bool enabled = false;
}

namespace features::misc::notifications
{
    inline bool enabled = true;
    inline int position = 3; // 0: Top Left, 1: Top Right, 2: Bottom Left, 3: Bottom Right
    inline float bar_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
}

// --- CONFIGURACION GLOBAL ---
namespace features::settings
{
}

// --- FRIENDS ---
namespace features::friends
{
    extern std::vector<std::string>* list;
    extern std::vector<std::string>* uuids;
    bool is_friend(const std::string& name);
    bool is_teammate(mapper::__player& player, mapper::__player& local_player);
    void clear();
    void remove_last();
    void remove_at(size_t index);
    std::vector<std::string> snapshot_names();
    std::vector<std::pair<std::string, std::string>> snapshot_entries();
    void replace_all(const std::vector<std::string>& names, const std::vector<std::string>& ids);
    void request_uuid(const std::string& name);
    void verify_uuid(const std::string& uuid);
    void shutdown();
    void run(mapper::__minecraft& minecraft);
}

namespace features::visual::hit_markers
{
    inline bool enabled = false;
    inline int mode = 0; // 0 = 2D, 1 = 3D
    inline float color[4] = { 1.f, 1.f, 1.f, 1.f };
    inline float size = 10.f;
    inline float line_width = 2.f;
    inline float duration = 0.5f;
    inline bool fade_out = true;
    inline bool scale_animation = true;
    inline float scale_amount = 1.5f;
    inline bool outline = true;
    inline float outline_color[4] = { 0.f, 0.f, 0.f, 1.f };
    inline float outline_width = 1.f;
    void run(mapper::__minecraft& minecraft);
    void render();
}

