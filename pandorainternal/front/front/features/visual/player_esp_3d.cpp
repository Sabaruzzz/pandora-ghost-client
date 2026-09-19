#include "../features.hpp"
extern float gui_whip_esp_hurt_color[4];
#include <vector>
#include <mutex>
#include <cmath>
#include "backends/imgui.h"

struct ESP3D_Entry
{
    mapper::__vec3 rel_pos = {};  // PIES del target, relativo al ojo local
    float          yaw = 0.f;
    float          health = 0.f;
    __int32        hurt_time = 0;

    // Estado del jugador para animar el stick figure
    float vel_xz = 0.f;  // velocidad horizontal (bloques/tick)
    float vel_y = 0.f;  // velocidad vertical   (bloques/tick)
    bool  is_attacking = false; // tiene hurt_time activo = acaba de golpear
    int   entity_id = -1;
    bool  friendly = false;
};

static std::vector<ESP3D_Entry> g_esp3d_buffer;
static std::mutex               g_esp3d_mutex;
extern std::atomic<bool> g_PlayerInGui;
extern bool g_MenuVisible;
extern bool gui_whip_esp_hide_friends;

auto features::visual::player_esp_3d::run(mapper::__minecraft& minecraft) -> void
{
    auto clear_buffer = [&]() {
        std::lock_guard<std::mutex> lock(g_esp3d_mutex);
        g_esp3d_buffer.clear();
        };

    if (!features::visual::player_esp_3d::enabled) { clear_buffer(); return; }


    auto timer = minecraft.get_timer();
    if (timer.object == nullptr) return;
    float partial_ticks = timer.get_partial_ticks();

    auto world = minecraft.get_world();
    auto local_player = minecraft.get_local_player();
    if (world.object == nullptr || local_player.object == nullptr) { clear_buffer(); return; }

    auto render_manager = minecraft.get_render_manager();
    if (!render_manager.object) { clear_buffer(); return; }

    mapper::__vec3 render_pos;
    auto local_pos = local_player.get_view_position(partial_ticks);
    render_pos.x = render_manager.get_render_pos_x();
    render_pos.y = render_manager.get_render_pos_y();
    render_pos.z = render_manager.get_render_pos_z();

    auto world_players = world.get_players();
    if (world_players.empty()) return; // FIX FLICKER: Evitar vaciado por excepcion Java

    std::vector<ESP3D_Entry> temp;
    temp.reserve(world_players.size());

    for (auto& player : world_players)
    {
        if (player.object == nullptr) continue;
        if (sdk::jni->IsSameObject(local_player.object, player.object)) continue;
        const bool friendly = features::friends::is_friend(player.get_name()) ||
            features::friends::is_teammate(player, local_player);

        float health = player.get_health();
        if (health <= 0.f) continue;
        if (!features::visual::player_esp_3d::draw_invisible_players && player.get_flag(5)) continue;

        // Interpolamos la posicion del target para movimiento suave.
        // get_view_position devuelve pies interpolados (igual que local_pos).
        auto player_pos = player.get_view_position(partial_ticks);

        if (local_pos.get_distance_to_vec3(player_pos) > 255.0) continue;

        ESP3D_Entry entry;
        entry.rel_pos.x = (float)(player_pos.x - render_pos.x);
        entry.rel_pos.y = (float)(player_pos.y - render_pos.y);
        entry.rel_pos.z = (float)(player_pos.z - render_pos.z);
        entry.yaw = player.get_view_angles().x;
        entry.health = health;
        entry.hurt_time = features::visual::player_esp_3d::draw_hurt_time ? player.get_hurt_time() : 0;

        // Velocidad: motionX/Y/Z siempre es 0 para jugadores remotos en MC 1.8.
        // En su lugar calculamos la velocidad con la diferencia pos - oldPos
        // que si se actualiza correctamente para todos los jugadores.
        auto cur_pos = player.get_position();
        auto old_pos = player.get_old_position();
        double dx = cur_pos.x - old_pos.x;
        double dy = cur_pos.y - old_pos.y;
        double dz = cur_pos.z - old_pos.z;
        entry.vel_xz = sqrtf((float)(dx * dx + dz * dz));
        entry.vel_y  = (float)dy;

        // is_attacking: usamos is_swing_in_progress() que detecta la animacion
        // de brazo cuando el jugador ataca (no cuando recibe dano).
        entry.is_attacking = player.is_swing_in_progress();
        entry.entity_id = player.get_entity_id();
        entry.friendly = friendly;
        temp.push_back(std::move(entry));
    }

    std::lock_guard<std::mutex> lock(g_esp3d_mutex);
    g_esp3d_buffer = std::move(temp);
}

extern float gui_whip_esp_max_distance;

auto features::visual::player_esp_3d::render() -> void
{
    if (!features::visual::player_esp_3d::enabled || g_PlayerInGui || g_MenuVisible) return;

    std::lock_guard<std::mutex> lock(g_esp3d_mutex);
    if (g_esp3d_buffer.empty()) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float t = (float)ImGui::GetTime();

    for (auto& e : g_esp3d_buffer)
    {
        float dist = sqrtf(e.rel_pos.x * e.rel_pos.x + e.rel_pos.y * e.rel_pos.y + e.rel_pos.z * e.rel_pos.z);
        if (dist > gui_whip_esp_max_distance) continue;

        mapper::__vec4 col = features::visual::player_esp_3d::color;
        if (e.friendly)
            col = { 0.10f, 1.0f, 0.25f, 1.0f };
        else if (e.hurt_time > 0)
            col = { gui_whip_esp_hurt_color[0], gui_whip_esp_hurt_color[1],
                    gui_whip_esp_hurt_color[2], gui_whip_esp_hurt_color[3] };

        features::visual::render_3d_bounding_box(
            e.rel_pos, col,
            features::visual::player_esp_3d::draw_health, e.health,
            false, 0, e.entity_id);
    }
}
