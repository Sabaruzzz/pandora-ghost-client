#include <unordered_map>
#include "../features.hpp"
extern float gui_whip_esp_hurt_color[4];
#include <vector>
#include <mutex>
#include <cmath>
#include "backends/imgui.h"

struct ESP2D_Entry
{
    mapper::__vec3 position{};
    mapper::__vec3 old_position{};
    float          health = 0.f;
    float          max_health = 20.f;
    __int32        hurt_time = 0;
    int            entity_id = -1;
    bool           friendly = false;
};

extern std::atomic<bool> g_PlayerInGui;
extern bool g_MenuVisible;
extern bool gui_whip_esp_hide_friends;

static std::vector<ESP2D_Entry> g_esp2d_buffer;
static std::mutex               g_esp2d_mutex;

// Posicion de la camara (RenderManager) capturada junto con el buffer
// para sincronizar la conversion a screen coords
static double g_esp2d_cam_x = 0.0;
static double g_esp2d_cam_y = 0.0;
static double g_esp2d_cam_z = 0.0;
static float  g_esp2d_partial_ticks = 0.0f;

// Helpers
static mapper::__vec4 HsvToRgb2D(float h, float s, float v, float a)
{
    float c = v * s, x = c * (1.f - fabsf(fmodf(h / 60.f, 2.f) - 1.f)), m = v - c;
    float r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    return { r + m, g + m, b + m, a };
}

auto features::visual::player_esp_2d::run(mapper::__minecraft& minecraft) -> void
{
    auto clear_buffer = [&]() {
        std::lock_guard<std::mutex> lock(g_esp2d_mutex);
        g_esp2d_buffer.clear();
        };

    if (!features::visual::player_esp_2d::enabled) { clear_buffer(); return; }

    auto timer = minecraft.get_timer();
    if (timer.object == nullptr) return;
    float partial_ticks = timer.get_partial_ticks();

    auto world = minecraft.get_world();
    auto local_player = minecraft.get_local_player();
    if (world.object == nullptr || local_player.object == nullptr) { clear_buffer(); return; }

    auto world_players = world.get_players();
    // A transient empty Java list can occur during collection updates. Keep the
    // previous frame instead of flashing every ESP element off for one frame.
    if (world_players.empty()) return;
    // Capturar la posicion del RenderManager (camara de OpenGL)
    // para sincronizar la conversion a screen coords en render()
    auto rm = minecraft.get_render_manager();
    double cam_x = 0.0, cam_y = 0.0, cam_z = 0.0;
    if (rm.object) {
        auto local_pos = local_player.get_view_position(partial_ticks);
        cam_x = rm.get_render_pos_x();
        cam_y = rm.get_render_pos_y();
        cam_z = rm.get_render_pos_z();
    }

    std::vector<ESP2D_Entry> temp;
    temp.reserve(world_players.size());

    for (auto& player : world_players)
    {
        if (player.object == nullptr) continue;
        if (sdk::jni->IsSameObject(local_player.object, player.object)) continue;
        const bool friendly = features::friends::is_friend(player.get_name()) ||
            features::friends::is_teammate(player, local_player);

        float health = player.get_health();
        if (health <= 0.f) continue;
        if (!features::visual::player_esp_2d::draw_invisible_players && player.get_flag(5)) continue;

        const auto position = player.get_position();
        const auto old_position = player.get_old_position();
        mapper::__vec3 player_pos{
            old_position.x + (position.x - old_position.x) * partial_ticks,
            old_position.y + (position.y - old_position.y) * partial_ticks,
            old_position.z + (position.z - old_position.z) * partial_ticks
        };

        // Usar camara para distancia (mas preciso que local_pos)
        double dx = player_pos.x - cam_x;
        double dy = player_pos.y - cam_y;
        double dz = player_pos.z - cam_z;
        if (sqrt(dx*dx + dy*dy + dz*dz) > 255.0) continue;

        ESP2D_Entry entry;
        // Keep tick endpoints. Interpolation is deferred until render(), where
        // it can use the camera and partial tick from the exact matrix frame.
        entry.position = position;
        entry.old_position = old_position;
        entry.health = health;
        entry.max_health = player.get_max_health();
        entry.hurt_time = features::visual::player_esp_2d::draw_hurt_time ? player.get_hurt_time() : 0;
        entry.entity_id = player.get_entity_id();
        entry.friendly = friendly;
        temp.push_back(std::move(entry));
    }

    // Lock minimo: solo el swap del vector + camara
    {
        std::lock_guard<std::mutex> lock(g_esp2d_mutex);
        g_esp2d_buffer = std::move(temp);
        g_esp2d_cam_x = cam_x;
        g_esp2d_cam_y = cam_y;
        g_esp2d_cam_z = cam_z;
        g_esp2d_partial_ticks = partial_ticks;
    }
}

extern int gui_whip_esp_render_mode;
extern int gui_whip_esp_mode2d;
extern bool gui_whip_esp_show_healthbar;
extern float gui_whip_esp_healthbar_bg[4];
extern float gui_whip_esp_healthbar_full[4];
extern float gui_whip_esp_healthbar_low[4];
extern float gui_whip_esp_healthbar_width;
extern float gui_whip_esp_healthbar_offset;
extern float gui_whip_esp_outline2d_color[4];
extern float gui_whip_esp_outline2d_width;
extern float gui_whip_esp_max_distance;

auto features::visual::player_esp_2d::render() -> void
{
    // Whip behavior: Hide ESP when opening Inventory or Cheat Menu
    if (!features::visual::player_esp_2d::enabled || g_PlayerInGui || g_MenuVisible) return;

    std::vector<ESP2D_Entry> local_buf;
    double rm_x, rm_y, rm_z;
    float frame_partial;
    {
        std::lock_guard<std::mutex> lock(g_esp2d_mutex);
        if (g_esp2d_buffer.empty()) return;
        local_buf = g_esp2d_buffer;
        rm_x = g_esp2d_cam_x;
        rm_y = g_esp2d_cam_y;
        rm_z = g_esp2d_cam_z;
        frame_partial = g_esp2d_partial_ticks;
    }

    if (features::visual::render_frame_snapshot_valid) {
        rm_x = features::visual::render_camera_x;
        rm_y = features::visual::render_camera_y;
        rm_z = features::visual::render_camera_z;
        frame_partial = features::visual::render_partial_ticks;
    }
    frame_partial = (std::clamp)(frame_partial, 0.0f, 1.0f);

    float t = (float)ImGui::GetTime();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (auto& e : local_buf)
    {
        // Calculate relative position to camera
        float rel_x = (float)(e.old_position.x +
            (e.position.x - e.old_position.x) * frame_partial - rm_x);
        float rel_y = (float)(e.old_position.y +
            (e.position.y - e.old_position.y) * frame_partial - rm_y);
        float rel_z = (float)(e.old_position.z +
            (e.position.z - e.old_position.z) * frame_partial - rm_z);

        // Distance check
        float dist = sqrtf(rel_x * rel_x + rel_y * rel_y + rel_z * rel_z);
        if (dist > gui_whip_esp_max_distance) continue;

        // Native player volume centered on the interpolated entity position.
        // This keeps the reference implementation's frame synchronization
        // without reading a Java AABB field through an incompatible subclass.
        const double interp_x = e.old_position.x +
            (e.position.x - e.old_position.x) * frame_partial;
        const double interp_y = e.old_position.y +
            (e.position.y - e.old_position.y) * frame_partial;
        const double interp_z = e.old_position.z +
            (e.position.z - e.old_position.z) * frame_partial;
        constexpr double half_width = 0.30;
        constexpr double body_height = 1.80;
        const float min_bx = (float)(interp_x - half_width - rm_x);
        const float min_by = (float)(interp_y - rm_y);
        const float min_bz = (float)(interp_z - half_width - rm_z);
        const float max_bx = (float)(interp_x + half_width - rm_x);
        const float max_by = (float)(interp_y + body_height - rm_y);
        const float max_bz = (float)(interp_z + half_width - rm_z);
        const mapper::__vec3 corners[8] = {
            { min_bx, min_by, min_bz }, { max_bx, min_by, min_bz },
            { max_bx, min_by, max_bz }, { min_bx, min_by, max_bz },
            { min_bx, max_by, min_bz }, { max_bx, max_by, min_bz },
            { max_bx, max_by, max_bz }, { min_bx, max_by, max_bz }
        };

        float min_x = FLT_MAX, min_y = FLT_MAX;
        float max_x = -FLT_MAX, max_y = -FLT_MAX;
        bool projection_valid = true;
        for (const auto& corner : corners) {
            const auto screen = features::visual::world_to_screen(corner);
            if (screen.x == FLT_MAX || screen.y == FLT_MAX) {
                projection_valid = false;
                break;
            }
            min_x = (std::min)(min_x, (float)screen.x);
            min_y = (std::min)(min_y, (float)screen.y);
            max_x = (std::max)(max_x, (float)screen.x);
            max_y = (std::max)(max_y, (float)screen.y);
        }
        if (!projection_valid || min_x >= max_x || min_y >= max_y) continue;

        // Small distance-independent screen padding keeps limbs inside the box
        // and makes camera turns look stable without creating an oversized ESP.
        const float projected_height = max_y - min_y;
        const float padding = (std::clamp)(projected_height * 0.0125f, 1.0f, 3.5f);
        float x = min_x - padding;
        float y = min_y - padding;
        float w = (max_x - min_x) + padding * 2.0f;
        float h = projected_height + padding * 2.0f;

        float hurt_alpha = (e.hurt_time > 0) ? 1.0f : 0.85f;
        const float friendColor[4] = { 0.10f, 1.0f, 0.25f, 1.0f };
        const float* entityColor = e.friendly ? friendColor : gui_whip_esp_outline2d_color;
        ImU32 col_out = IM_COL32((int)(entityColor[0] * 255),
                                 (int)(entityColor[1] * 255),
                                 (int)(entityColor[2] * 255),
                                 (int)(entityColor[3] * 255 * hurt_alpha));
        if (e.hurt_time > 0 && !e.friendly) col_out = ImGui::ColorConvertFloat4ToU32(ImVec4(
            gui_whip_esp_hurt_color[0], gui_whip_esp_hurt_color[1],
            gui_whip_esp_hurt_color[2], gui_whip_esp_hurt_color[3]));

        ImU32 col_fill = IM_COL32((int)(entityColor[0] * 255),
                                  (int)(entityColor[1] * 255),
                                  (int)(entityColor[2] * 255),
                                  (int)(60 * hurt_alpha));
        if (e.hurt_time > 0 && !e.friendly) col_fill = IM_COL32(
            (int)(gui_whip_esp_hurt_color[0] * 255),
            (int)(gui_whip_esp_hurt_color[1] * 255),
            (int)(gui_whip_esp_hurt_color[2] * 255),
            (int)(gui_whip_esp_hurt_color[3] * 90));

        // Mode 2D (0=Outline, 1=Fill, 2=Both)
        if (gui_whip_esp_mode2d == 1 || gui_whip_esp_mode2d == 2) {
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), col_fill, 0.f);
        }

        if (gui_whip_esp_mode2d == 0 || gui_whip_esp_mode2d == 2) {
            float thick = gui_whip_esp_outline2d_width;
            dl->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1), IM_COL32(0, 0, 0, 180), 0.f, 0, thick + 1.0f);
            dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), col_out, 0.f, 0, thick);
        }

        // Whip Health Bar (Interpolated Low to Full)
        if (gui_whip_esp_show_healthbar) {
            float max_hp = e.max_health > 0 ? e.max_health : 20.f;
            float hp_pct = e.health / max_hp;
            if (hp_pct > 1.f) hp_pct = 1.f;
            if (hp_pct < 0.f) hp_pct = 0.f;

            float bar_h = h * hp_pct;
            float bar_w = gui_whip_esp_healthbar_width;
            float bar_x = x - gui_whip_esp_healthbar_offset - bar_w;

            // Background
            ImU32 bg_col = IM_COL32((int)(gui_whip_esp_healthbar_bg[0] * 255),
                                    (int)(gui_whip_esp_healthbar_bg[1] * 255),
                                    (int)(gui_whip_esp_healthbar_bg[2] * 255),
                                    (int)(gui_whip_esp_healthbar_bg[3] * 255));
            dl->AddRectFilled(ImVec2(bar_x - 1, y - 1), ImVec2(bar_x + bar_w + 1, y + h + 1), bg_col);

            // Interpolate color from Low to Full
            float r = gui_whip_esp_healthbar_low[0] + (gui_whip_esp_healthbar_full[0] - gui_whip_esp_healthbar_low[0]) * hp_pct;
            float g = gui_whip_esp_healthbar_low[1] + (gui_whip_esp_healthbar_full[1] - gui_whip_esp_healthbar_low[1]) * hp_pct;
            float b = gui_whip_esp_healthbar_low[2] + (gui_whip_esp_healthbar_full[2] - gui_whip_esp_healthbar_low[2]) * hp_pct;
            float a = gui_whip_esp_healthbar_low[3] + (gui_whip_esp_healthbar_full[3] - gui_whip_esp_healthbar_low[3]) * hp_pct;

            ImU32 hp_col = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), (int)(a * 255));
            dl->AddRectFilled(ImVec2(bar_x, y + h - bar_h), ImVec2(bar_x + bar_w, y + h), hp_col);
            dl->AddRect(ImVec2(bar_x - 1, y - 1), ImVec2(bar_x + bar_w + 1, y + h + 1), IM_COL32(0, 0, 0, 200), 0.f, 0, 1.0f);
        }
    }
}

