#include "../features.hpp"
#include <string>
#include <imgui.h>
#include <cfloat>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>

extern float gui_whip_esp_healthbar_bg[4];
extern float gui_whip_esp_healthbar_full[4];
extern float gui_whip_esp_healthbar_low[4];
extern float gui_whip_esp_healthbar_width;
extern float gui_whip_esp_healthbar_offset;

void* features::visual::window = nullptr;
double features::visual::model_view_matrix[16];
double features::visual::projection_matrix[16];
int features::visual::view_port[4];
double features::visual::render_camera_x = 0.0;
double features::visual::render_camera_y = 0.0;
double features::visual::render_camera_z = 0.0;
float features::visual::render_partial_ticks = 0.0f;
bool features::visual::render_frame_snapshot_valid = false;

auto features::visual::world_to_screen(mapper::__vec3 data, bool can_reverse, bool ignore_z) -> mapper::__vec2
{
    auto multiply_matrix = [&](mapper::__vec4 vec4, double matrix[16]) -> mapper::__vec4
        {
            mapper::__vec4 res;
            res.x = vec4.x * matrix[0] + vec4.y * matrix[4] + vec4.z * matrix[8] + vec4.w * matrix[12];
            res.y = vec4.x * matrix[1] + vec4.y * matrix[5] + vec4.z * matrix[9] + vec4.w * matrix[13];
            res.z = vec4.x * matrix[2] + vec4.y * matrix[6] + vec4.z * matrix[10] + vec4.w * matrix[14];
            res.w = vec4.x * matrix[3] + vec4.y * matrix[7] + vec4.z * matrix[11] + vec4.w * matrix[15];
            return res;
        };

    mapper::__vec4 initial_vec;
    initial_vec.x = data.x;
    initial_vec.y = data.y;
    initial_vec.z = data.z;
    initial_vec.w = 1.f;

    auto clipped_space_position = multiply_matrix(multiply_matrix(initial_vec, features::visual::model_view_matrix), features::visual::projection_matrix);
    mapper::__vec2 out_fail;
    out_fail.x = FLT_MAX;
    out_fail.y = FLT_MAX;

    if (!std::isfinite(clipped_space_position.x) ||
        !std::isfinite(clipped_space_position.y) ||
        !std::isfinite(clipped_space_position.z) ||
        !std::isfinite(clipped_space_position.w))
        return out_fail;

    constexpr double min_w = 0.0001;
    if ((!can_reverse && clipped_space_position.w <= min_w) ||
        (can_reverse && std::abs(clipped_space_position.w) <= min_w))
        return out_fail;

    mapper::__vec3 space_position;
    space_position.x = clipped_space_position.x / clipped_space_position.w;
    space_position.y = clipped_space_position.y / clipped_space_position.w;
    space_position.z = clipped_space_position.z / clipped_space_position.w;

    if (!std::isfinite(space_position.x) ||
        !std::isfinite(space_position.y) ||
        !std::isfinite(space_position.z))
        return out_fail;

    // Valores NDC desmedidos aparecen al cruzar el plano de la camara. No son
    // coordenadas dibujables y generaban cajas que ocupaban toda la pantalla.
    if (!can_reverse && (std::abs(space_position.x) > 32.0 ||
                         std::abs(space_position.y) > 32.0))
        return out_fail;

    if ((space_position.z < -1.f || space_position.z > 1.f) && !can_reverse && !ignore_z)
        return out_fail;

    const float viewport_x = static_cast<float>(features::visual::view_port[0]);
    const float viewport_y = static_cast<float>(features::visual::view_port[1]);
    const float viewport_w = static_cast<float>(features::visual::view_port[2]);
    const float viewport_h = static_cast<float>(features::visual::view_port[3]);
    if (viewport_w <= 0.0f || viewport_h <= 0.0f)
        return out_fail;

    mapper::__vec2 out_success;
    if (can_reverse && clipped_space_position.w < 0.0)
    {
        out_success.x = viewport_x + viewport_w - (((space_position.x + 1.f) * 0.5f) * viewport_w);
        out_success.y = viewport_y + viewport_h - (((1.f - space_position.y) * 0.5f) * viewport_h);
        return out_success;
    }
    else
    {
        out_success.x = viewport_x + (((space_position.x + 1.f) * 0.5f) * viewport_w);
        out_success.y = viewport_y + (((1.f - space_position.y) * 0.5f) * viewport_h);
        return out_success;
    }
}

auto features::visual::render_nametag(std::string name, mapper::__vec3 vec3, mapper::__vec4 color, bool draw_health, float health, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void
{
    auto min_x = vec3.x - .35, min_y = vec3.y, min_z = vec3.z - .35, max_x = vec3.x + .35, max_y = vec3.y + 1.85, max_z = vec3.z + .35;

    mapper::__vec3 bounding_box[] = {
        { min_x, min_y, min_z }, { min_x, max_y, min_z }, { max_x, max_y, min_z }, { max_x, min_y, min_z },
        { max_x, max_y, max_z }, { min_x, max_y, max_z }, { min_x, min_y, max_z }, { max_x, min_y, max_z }
    };

    // SOLUCIÃ“N AL ERROR C2440 Y C3536: AsignaciÃ³n manual
    mapper::__vec4 bounding_box_screen_position;
    bounding_box_screen_position.x = DBL_MAX;
    bounding_box_screen_position.y = DBL_MAX;
    bounding_box_screen_position.z = DBL_MIN;
    bounding_box_screen_position.w = DBL_MIN;

    for (auto x = 0; x < 8; ++x)
    {
        auto screen_position = features::visual::world_to_screen({ bounding_box[x].x, bounding_box[x].y, bounding_box[x].z });
        if (screen_position.x == FLT_MAX || screen_position.y == FLT_MAX) continue;

        bounding_box_screen_position.x = min((double)screen_position.x, bounding_box_screen_position.x);
        bounding_box_screen_position.y = min((double)screen_position.y, bounding_box_screen_position.y);
        bounding_box_screen_position.z = max((double)screen_position.x, bounding_box_screen_position.z);
        bounding_box_screen_position.w = max((double)screen_position.y, bounding_box_screen_position.w);
    }

    if (bounding_box_screen_position.x == DBL_MAX || bounding_box_screen_position.y == DBL_MAX || bounding_box_screen_position.z == DBL_MIN || bounding_box_screen_position.w == DBL_MIN)
        return;

    mapper::__vec2 screen_position;
    screen_position.x = (float)(bounding_box_screen_position.x + (bounding_box_screen_position.z - bounding_box_screen_position.x) * .5);
    screen_position.y = (float)bounding_box_screen_position.y;

    auto name_size = ImGui::CalcTextSize(name.c_str());

    ImGui::GetBackgroundDrawList()->AddRectFilled(
        { screen_position.x - name_size.x * .5f - 4.f, screen_position.y - name_size.y - 7.f },
        { screen_position.x + name_size.x * .5f + 4.f, screen_position.y - 5.f },
        ImGui::ColorConvertFloat4ToU32({ .1175f, .1175f, .1175f, .8f })
    );


    if (draw_hurt_time) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { screen_position.x - name_size.x * .5f - 4.f, screen_position.y - name_size.y - 7.f },
            { screen_position.x + name_size.x * .5f + 4.f, screen_position.y - 5.f },
            ImGui::ColorConvertFloat4ToU32({ .8f, .35f, .35f, .5f * (float)hurt_time / 10.f })
        );
    }

    ImGui::GetBackgroundDrawList()->AddText(
        { (float)screen_position.x - name_size.x * .5f, (float)screen_position.y - name_size.y - 7.f },
        ImGui::ColorConvertFloat4ToU32({ (float)color.x, (float)color.y, (float)color.z, (float)color.w }),
        name.c_str()
    );

    if (draw_health) {
        mapper::__vec2 minimun_position;
        minimun_position.x = screen_position.x - name_size.x * .5f - 4.f;
        minimun_position.y = screen_position.y - 5.f;

        mapper::__vec2 maximun_position;
        maximun_position.x = screen_position.x + name_size.x * .5f + 4.f;
        maximun_position.y = screen_position.y - 2.f;

        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { minimun_position.x, minimun_position.y }, { maximun_position.x, maximun_position.y },
            ImGui::ColorConvertFloat4ToU32({ .1175f, .1175f, .1175f, .8f })
        );

        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { minimun_position.x + 1.f, minimun_position.y + 1.f },
            { minimun_position.x + (maximun_position.x - minimun_position.x) * health / 20.f - 1.f, maximun_position.y - 1.f },
            ImGui::ColorConvertFloat4ToU32({ (20.f - health) / 20.f, (health < 5.f ? 5.f : health) / 20.f, .35f, 1.f })
        );
    }

    if (draw_distance) {
        char fixed_distance[32];
        snprintf(fixed_distance, sizeof(fixed_distance), "%.2f", distance);
        auto fixed_distance_size = ImGui::CalcTextSize(fixed_distance);

        ImGui::GetBackgroundDrawList()->AddRectFilled(
            { (float)screen_position.x - fixed_distance_size.x * .5f - 4.f, (float)screen_position.y - name_size.y - fixed_distance_size.y - 15.f },
            { (float)screen_position.x + fixed_distance_size.x * .5f + 4.f, (float)screen_position.y - name_size.y - 13.f },
            ImGui::ColorConvertFloat4ToU32({ .1175f, .1175f, .1175f, .8f })
        );

        ImGui::GetBackgroundDrawList()->AddText(
            { (float)screen_position.x - fixed_distance_size.x * .5f, (float)screen_position.y - name_size.y - fixed_distance_size.y - 7.f },
            ImGui::ColorConvertFloat4ToU32({ (2.f - ((float)distance < 5. ? 5.f : (float)distance) / 20.f), ((float)distance < 5. ? 5.f : (float)distance) / 20.f, 0.f, (float)color.w }),
            fixed_distance
        );
    }
}

auto features::visual::render_2d_bounding_box(mapper::__vec3 vec3, mapper::__vec4 outline_color, mapper::__vec4 line_color, mapper::__vec4 fill_color, bool draw_corners, bool draw_health, float health, bool draw_hurt_time, __int32 hurt_time) -> void
{
    auto min_x = vec3.x - .35, min_y = vec3.y, min_z = vec3.z - .35, max_x = vec3.x + .35, max_y = vec3.y + 1.85, max_z = vec3.z + .35;
    mapper::__vec3 bounding_box[] = { { min_x, min_y, min_z }, { min_x, max_y, min_z }, { max_x, max_y, min_z }, { max_x, min_y, min_z }, { max_x, max_y, max_z }, { min_x, max_y, max_z }, { min_x, min_y, max_z }, { max_x, min_y, max_z } };

    // SOLUCIÃ“N AL ERROR C2440 Y C3536
    mapper::__vec4 bounding_box_screen_position;
    bounding_box_screen_position.x = DBL_MAX;
    bounding_box_screen_position.y = DBL_MAX;
    bounding_box_screen_position.z = DBL_MIN;
    bounding_box_screen_position.w = DBL_MIN;

    for (auto x = 0; x < 8; ++x) {
        auto screen_position = features::visual::world_to_screen({ bounding_box[x].x, bounding_box[x].y, bounding_box[x].z });
        if (screen_position.x == FLT_MAX || screen_position.y == FLT_MAX) return;
        bounding_box_screen_position.x = min((double)screen_position.x, bounding_box_screen_position.x);
        bounding_box_screen_position.y = min((double)screen_position.y, bounding_box_screen_position.y);
        bounding_box_screen_position.z = max((double)screen_position.x, bounding_box_screen_position.z);
        bounding_box_screen_position.w = max((double)screen_position.y, bounding_box_screen_position.w);
    }

    if (bounding_box_screen_position.x == DBL_MAX || bounding_box_screen_position.y == DBL_MAX || bounding_box_screen_position.z == DBL_MIN || bounding_box_screen_position.w == DBL_MIN) return;

    if (draw_corners) {
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.y + ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.x + ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.y }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.y + ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.x + ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.y }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.w - ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.x + ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.w }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.w - ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.x + ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.w }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.y + ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.z - ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.y }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.y + ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.z - ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.y }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.w - ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.z - ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.w }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }), 3.f);
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.w - ((float)bounding_box_screen_position.w - (float)bounding_box_screen_position.y) * .33f }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddLine({ (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.w }, { (float)bounding_box_screen_position.z - ((float)bounding_box_screen_position.z - (float)bounding_box_screen_position.x) * .33f, (float)bounding_box_screen_position.w }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
    }
    else {
        ImGui::GetBackgroundDrawList()->AddRect({ (float)bounding_box_screen_position.x - 1.f, (float)bounding_box_screen_position.y - 1.f }, { (float)bounding_box_screen_position.z + 1.f, (float)bounding_box_screen_position.w + 1.f }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }));
        ImGui::GetBackgroundDrawList()->AddRect({ (float)bounding_box_screen_position.x, (float)bounding_box_screen_position.y }, { (float)bounding_box_screen_position.z, (float)bounding_box_screen_position.w }, ImGui::ColorConvertFloat4ToU32({ (float)line_color.x, (float)line_color.y, (float)line_color.z, (float)line_color.w }));
        ImGui::GetBackgroundDrawList()->AddRect({ (float)bounding_box_screen_position.x + 1.f, (float)bounding_box_screen_position.y + 1.f }, { (float)bounding_box_screen_position.z - 1.f, (float)bounding_box_screen_position.w - 1.f }, ImGui::ColorConvertFloat4ToU32({ (float)outline_color.x, (float)outline_color.y, (float)outline_color.z, (float)outline_color.w }));
    }

    ImGui::GetBackgroundDrawList()->AddRectFilled({ (float)bounding_box_screen_position.x + 1.f, (float)bounding_box_screen_position.y + 1.f }, { (float)bounding_box_screen_position.z - 1.f, (float)bounding_box_screen_position.w - 1.f }, ImGui::ColorConvertFloat4ToU32({ (float)fill_color.x, (float)fill_color.y, (float)fill_color.z, (float)fill_color.w }));

    if (draw_health) {
        // Keep the 3D box perspective-correct, but make its health bar a stable
        // screen-space element using the projected body center. Extrema can jump
        // between box corners while rotating and made the old bar stretch.
        const auto feet_center = features::visual::world_to_screen({ vec3.x, min_y, vec3.z });
        const auto head_center = features::visual::world_to_screen({ vec3.x, max_y, vec3.z });
        if (feet_center.x == FLT_MAX || head_center.x == FLT_MAX) return;
        float y = (std::min)((float)feet_center.y, (float)head_center.y);
        float h = fabsf((float)feet_center.y - (float)head_center.y);
        if (h < 1.0f) return;
        float bar_w = gui_whip_esp_healthbar_width;
        float bar_x = (float)bounding_box_screen_position.x - gui_whip_esp_healthbar_offset - bar_w;
        float hp_pct = health / 20.f;
        if (hp_pct > 1.f) hp_pct = 1.f;
        if (hp_pct < 0.f) hp_pct = 0.f;
        float bar_h = h * hp_pct;
        ImU32 bg_col = ImGui::ColorConvertFloat4ToU32(ImVec4(
            gui_whip_esp_healthbar_bg[0], gui_whip_esp_healthbar_bg[1],
            gui_whip_esp_healthbar_bg[2], gui_whip_esp_healthbar_bg[3]));
        const float r = gui_whip_esp_healthbar_low[0] + (gui_whip_esp_healthbar_full[0] - gui_whip_esp_healthbar_low[0]) * hp_pct;
        const float g = gui_whip_esp_healthbar_low[1] + (gui_whip_esp_healthbar_full[1] - gui_whip_esp_healthbar_low[1]) * hp_pct;
        const float b = gui_whip_esp_healthbar_low[2] + (gui_whip_esp_healthbar_full[2] - gui_whip_esp_healthbar_low[2]) * hp_pct;
        const float a = gui_whip_esp_healthbar_low[3] + (gui_whip_esp_healthbar_full[3] - gui_whip_esp_healthbar_low[3]) * hp_pct;
        ImGui::GetBackgroundDrawList()->AddRectFilled({ bar_x - 1.f, y - 1.f }, { bar_x + bar_w + 1.f, y + h + 1.f }, bg_col);
        ImU32 hp_col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
        ImGui::GetBackgroundDrawList()->AddRectFilled({ bar_x, y + h - bar_h }, { bar_x + bar_w, y + h }, hp_col);
    }

    if (draw_hurt_time) {
        ImGui::GetBackgroundDrawList()->AddRectFilled({ (float)bounding_box_screen_position.x + 1.f, (float)bounding_box_screen_position.y + 1.f }, { (float)bounding_box_screen_position.z - 1.f, (float)bounding_box_screen_position.w - 1.f }, ImGui::ColorConvertFloat4ToU32({ .8f, .35f, .35f, (float)fill_color.w * (float)hurt_time / 10.f * .5f }));
    }
}

extern int gui_whip_esp_mode3d;
extern float gui_whip_esp_neutral_color[4];

auto features::visual::render_3d_bounding_box(mapper::__vec3 vec3, mapper::__vec4 color, bool draw_health, float health, bool draw_hurt_time, __int32 hurt_time, int entity_id) -> void
{
    auto min_x = vec3.x - .35, min_y = vec3.y, min_z = vec3.z - .35, max_x = vec3.x + .35, max_y = vec3.y + 1.85, max_z = vec3.z + .35;
    auto bottom_0 = features::visual::world_to_screen({ min_x, min_y, min_z }); if (bottom_0.x == FLT_MAX) return;
    auto bottom_1 = features::visual::world_to_screen({ max_x, min_y, min_z }); if (bottom_1.x == FLT_MAX) return;
    auto bottom_2 = features::visual::world_to_screen({ max_x, min_y, max_z }); if (bottom_2.x == FLT_MAX) return;
    auto bottom_3 = features::visual::world_to_screen({ min_x, min_y, max_z }); if (bottom_3.x == FLT_MAX) return;
    auto top_0 = features::visual::world_to_screen({ min_x, max_y, min_z }); if (top_0.x == FLT_MAX) return;
    auto top_1 = features::visual::world_to_screen({ max_x, max_y, min_z }); if (top_1.x == FLT_MAX) return;
    auto top_2 = features::visual::world_to_screen({ max_x, max_y, max_z }); if (top_2.x == FLT_MAX) return;
    auto top_3 = features::visual::world_to_screen({ min_x, max_y, max_z }); if (top_3.x == FLT_MAX) return;

    ImU32 fillCol = ImGui::ColorConvertFloat4ToU32({ (float)color.x, (float)color.y, (float)color.z, (float)color.w * 0.35f });
    ImU32 outlineCol = ImGui::ColorConvertFloat4ToU32({ (float)color.x, (float)color.y, (float)color.z, 1.0f });

    // Mode 3D: 0 = Outline, 1 = Fill, 2 = Both
    if (gui_whip_esp_mode3d == 1 || gui_whip_esp_mode3d == 2) {
        ImGui::GetBackgroundDrawList()->AddQuadFilled({ bottom_0.x, bottom_0.y }, { bottom_1.x, bottom_1.y }, { bottom_2.x, bottom_2.y }, { bottom_3.x, bottom_3.y }, fillCol);
        ImGui::GetBackgroundDrawList()->AddQuadFilled({ top_0.x, top_0.y }, { top_1.x, top_1.y }, { top_2.x, top_2.y }, { top_3.x, top_3.y }, fillCol);
        ImGui::GetBackgroundDrawList()->AddQuadFilled({ bottom_0.x, bottom_0.y }, { top_0.x, top_0.y }, { top_1.x, top_1.y }, { bottom_1.x, bottom_1.y }, fillCol);
        ImGui::GetBackgroundDrawList()->AddQuadFilled({ bottom_1.x, bottom_1.y }, { top_1.x, top_1.y }, { top_2.x, top_2.y }, { bottom_2.x, bottom_2.y }, fillCol);
        ImGui::GetBackgroundDrawList()->AddQuadFilled({ bottom_2.x, bottom_2.y }, { top_2.x, top_2.y }, { top_3.x, top_3.y }, { bottom_3.x, bottom_3.y }, fillCol);
        ImGui::GetBackgroundDrawList()->AddQuadFilled({ bottom_3.x, bottom_3.y }, { top_3.x, top_3.y }, { top_0.x, top_0.y }, { bottom_0.x, bottom_0.y }, fillCol);
    }

    if (gui_whip_esp_mode3d == 0 || gui_whip_esp_mode3d == 2) {
        // Draw 12 3D box edges
        ImDrawList* bgDraw = ImGui::GetBackgroundDrawList();
        bgDraw->AddLine({ bottom_0.x, bottom_0.y }, { bottom_1.x, bottom_1.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ bottom_1.x, bottom_1.y }, { bottom_2.x, bottom_2.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ bottom_2.x, bottom_2.y }, { bottom_3.x, bottom_3.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ bottom_3.x, bottom_3.y }, { bottom_0.x, bottom_0.y }, outlineCol, 1.5f);

        bgDraw->AddLine({ top_0.x, top_0.y }, { top_1.x, top_1.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ top_1.x, top_1.y }, { top_2.x, top_2.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ top_2.x, top_2.y }, { top_3.x, top_3.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ top_3.x, top_3.y }, { top_0.x, top_0.y }, outlineCol, 1.5f);

        bgDraw->AddLine({ bottom_0.x, bottom_0.y }, { top_0.x, top_0.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ bottom_1.x, bottom_1.y }, { top_1.x, top_1.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ bottom_2.x, bottom_2.y }, { top_2.x, top_2.y }, outlineCol, 1.5f);
        bgDraw->AddLine({ bottom_3.x, bottom_3.y }, { top_3.x, top_3.y }, outlineCol, 1.5f);
    }

    if (draw_health) {
        // Use one of the already projected vertical box edges. The health bar
        // therefore shares the exact same camera transform as the 3D box and
        // cannot drift independently when the view rotates.
        const mapper::__vec2 bottoms[4] = { bottom_0, bottom_1, bottom_2, bottom_3 };
        const mapper::__vec2 tops[4] = { top_0, top_1, top_2, top_3 };
        int edgeIndex = 0;
        float leftMost = ((float)bottoms[0].x + (float)tops[0].x) * 0.5f;
        for (int i = 1; i < 4; ++i) {
            const float edgeX = ((float)bottoms[i].x + (float)tops[i].x) * 0.5f;
            if (edgeX < leftMost) { leftMost = edgeX; edgeIndex = i; }
        }

        ImVec2 edgeBottom((float)bottoms[edgeIndex].x, (float)bottoms[edgeIndex].y);
        ImVec2 edgeTop((float)tops[edgeIndex].x, (float)tops[edgeIndex].y);
        const ImVec2 edgeVector(edgeTop.x - edgeBottom.x, edgeTop.y - edgeBottom.y);
        const float edgeLength = sqrtf(edgeVector.x * edgeVector.x + edgeVector.y * edgeVector.y);
        if (edgeLength < 1.0f) return;

        ImVec2 outward(-edgeVector.y / edgeLength, edgeVector.x / edgeLength);
        const ImVec2 boxCenter(
            ((float)bottom_0.x + (float)bottom_1.x + (float)bottom_2.x + (float)bottom_3.x +
             (float)top_0.x + (float)top_1.x + (float)top_2.x + (float)top_3.x) / 8.0f,
            ((float)bottom_0.y + (float)bottom_1.y + (float)bottom_2.y + (float)bottom_3.y +
             (float)top_0.y + (float)top_1.y + (float)top_2.y + (float)top_3.y) / 8.0f);
        const ImVec2 edgeCenter((edgeBottom.x + edgeTop.x) * 0.5f, (edgeBottom.y + edgeTop.y) * 0.5f);
        if (outward.x * (edgeCenter.x - boxCenter.x) + outward.y * (edgeCenter.y - boxCenter.y) < 0.0f) {
            outward.x = -outward.x;
            outward.y = -outward.y;
        }

        const float barWidth = gui_whip_esp_healthbar_width;
        const float offset = gui_whip_esp_healthbar_offset + barWidth * 0.5f;
        edgeBottom.x += outward.x * offset; edgeBottom.y += outward.y * offset;
        edgeTop.x += outward.x * offset; edgeTop.y += outward.y * offset;

        const float hpPct = (std::clamp)(health / 20.0f, 0.0f, 1.0f);
        const ImVec2 healthEnd(
            edgeBottom.x + (edgeTop.x - edgeBottom.x) * hpPct,
            edgeBottom.y + (edgeTop.y - edgeBottom.y) * hpPct);
        const ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
            gui_whip_esp_healthbar_bg[0], gui_whip_esp_healthbar_bg[1],
            gui_whip_esp_healthbar_bg[2], gui_whip_esp_healthbar_bg[3]));
        const ImVec4 hpColor(
            gui_whip_esp_healthbar_low[0] + (gui_whip_esp_healthbar_full[0] - gui_whip_esp_healthbar_low[0]) * hpPct,
            gui_whip_esp_healthbar_low[1] + (gui_whip_esp_healthbar_full[1] - gui_whip_esp_healthbar_low[1]) * hpPct,
            gui_whip_esp_healthbar_low[2] + (gui_whip_esp_healthbar_full[2] - gui_whip_esp_healthbar_low[2]) * hpPct,
            gui_whip_esp_healthbar_low[3] + (gui_whip_esp_healthbar_full[3] - gui_whip_esp_healthbar_low[3]) * hpPct);
        ImGui::GetBackgroundDrawList()->AddLine(edgeBottom, edgeTop, bgColor, barWidth + 2.0f);
        ImGui::GetBackgroundDrawList()->AddLine(edgeBottom, healthEnd,
            ImGui::ColorConvertFloat4ToU32(hpColor), barWidth);
    }

    if (draw_hurt_time) features::visual::render_3d_bounding_box(vec3, { .8f, .35f, .35f, (float)color.w * (float)hurt_time / 10.f }, false, 0.f, false, 0, entity_id);
}

auto features::visual::render_tracer(mapper::__vec3 vec3, mapper::__vec4 color, bool draw_distance, double distance, bool draw_hurt_time, __int32 hurt_time) -> void
{
    auto screen_position = features::visual::world_to_screen(vec3, true);
    if (screen_position.x == FLT_MAX || screen_position.y == FLT_MAX) return;

    ImGui::GetBackgroundDrawList()->AddLine(
        { (float)features::visual::view_port[2] * .5f, (float)features::visual::view_port[3] * .5f },
        { screen_position.x, screen_position.y },
        ImGui::ColorConvertFloat4ToU32({ (float)color.x, (float)color.y, (float)color.z, (float)color.w })
    );

    if (draw_distance) {
        ImGui::GetBackgroundDrawList()->AddLine({ (float)features::visual::view_port[2] * .5f, (float)features::visual::view_port[3] * .5f }, { screen_position.x, screen_position.y }, ImGui::ColorConvertFloat4ToU32({ (2.f - ((float)distance < 5. ? 5.f : (float)distance) / 20.f), ((float)distance < 5. ? 5.f : (float)distance) / 20.f, 0.f, (float)color.w }), 2.f);
    }
    if (draw_hurt_time) {
        ImGui::GetBackgroundDrawList()->AddLine({ (float)features::visual::view_port[2] * .5f, (float)features::visual::view_port[3] * .5f }, { screen_position.x, screen_position.y }, ImGui::ColorConvertFloat4ToU32({ .8f, .35f, .35f, (float)color.w * (float)hurt_time / 10.f }), 2.f);
    }
}


