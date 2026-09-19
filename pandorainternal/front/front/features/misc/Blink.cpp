#include "../features.hpp"
#include "sdk.hpp" 
#include "backends/imgui.h"
#define NOMINMAX
#include <windows.h>
#include <vector>
#include <cmath>
#include <gl/GL.h>

extern bool gui_blink_enabled;

namespace features::misc::blink
{
    bool is_blinking = false;
    static ULONGLONG start_time = 0;
    static std::vector<mapper::__vec3> path_coords;

    static double saved_rX = 0.0;
    static double saved_rY = 0.0;
    static double saved_rZ = 0.0;

    // ============================================================
    // MODOS:
    //   0 = SMOOTH: El mundo se ve normal (jugadores se mueven,
    //       bloques caen, etc.) pero el servidor te ve quieto.
    //       Usa pulsos rapidos (80ms freeze / 40ms open) para
    //       recibir paquetes del servidor frecuentemente, pero
    //       tus paquetes de posicion se envian con delay.
    //
    //   1 = FREEZE: Congela Netty por completo. Todo se congela
    //       en tu pantalla. Cuando sueltas, te teletransportas.
    //       Usa pulsos lentos (450ms freeze / 35ms open) solo
    //       para mantener la conexion viva (anti-timeout).
    // ============================================================

    // Constantes para cada modo
    static const float HARD_CAP_SECONDS = 3.0f;

    // Smooth: pulsos rapidos para recibir datos del servidor
    static const ULONGLONG SMOOTH_FREEZE_MS  = 80;
    static const ULONGLONG SMOOTH_PULSE_MS   = 40;

    // Freeze: pulsos lentos solo para anti-timeout
    static const ULONGLONG FREEZE_FREEZE_MS  = 450;
    static const ULONGLONG FREEZE_PULSE_MS   = 35;

    static ULONGLONG last_pulse_time = 0;
    static bool pulse_active = false;

    // Removed unused Netty thread suspension methods. True Blink is handled in send.cpp

    void run(mapper::__minecraft& minecraft)
    {
        auto world = minecraft.get_world();
        if (world.object == nullptr) {
            is_blinking = false;
            pulse_active = false;
            return;
        }

        if (!enabled) {
            if (is_blinking) {
                is_blinking = false;
                pulse_active = false;
            }
            path_coords.clear();
            return;
        }

        if (!is_blinking) {
            is_blinking = true;
            pulse_active = false;
            start_time = GetTickCount64();
            last_pulse_time = start_time;
            path_coords.clear();
        }

        ULONGLONG now = GetTickCount64();

        // Hard cap: maximo 3 segundos o timer_limit (el menor)
        float effective_limit = timer_limit;
        if (effective_limit > HARD_CAP_SECONDS) effective_limit = HARD_CAP_SECONDS;

        if (now - start_time > (ULONGLONG)(effective_limit * 1000.0f)) {
            is_blinking = false;
            pulse_active = false;
            enabled = false;
            gui_blink_enabled = false;
            path_coords.clear();
            return;
        }

        // Guardar path visual
        auto local_player = minecraft.get_local_player();
        if (local_player.object != nullptr) {
            mapper::__vec3 current_pos = local_player.get_position();
            if (path_coords.empty() || path_coords.back().get_distance_to_vec3(current_pos) > 0.1) {
                path_coords.push_back(current_pos);
            }
        }

        // Guardar render position para OpenGL path
        auto render_manager = minecraft.get_render_manager();
        if (render_manager.object != nullptr && sdk::jni != nullptr) {
            jclass rm_cls = sdk::jni->GetObjectClass(render_manager.object);
            if (rm_cls) {
                jfieldID rx = sdk::jni->GetFieldID(rm_cls, "renderPosX", "D");
                if (!rx) { sdk::jni->ExceptionClear(); rx = sdk::jni->GetFieldID(rm_cls, "field_78725_b", "D"); }
                if (!rx) { sdk::jni->ExceptionClear(); rx = sdk::jni->GetFieldID(rm_cls, "o", "D"); }
                if (!rx) { sdk::jni->ExceptionClear(); rx = sdk::jni->GetFieldID(rm_cls, "h", "D"); }

                jfieldID ry = sdk::jni->GetFieldID(rm_cls, "renderPosY", "D");
                if (!ry) { sdk::jni->ExceptionClear(); ry = sdk::jni->GetFieldID(rm_cls, "field_78726_c", "D"); }
                if (!ry) { sdk::jni->ExceptionClear(); ry = sdk::jni->GetFieldID(rm_cls, "p", "D"); }
                if (!ry) { sdk::jni->ExceptionClear(); ry = sdk::jni->GetFieldID(rm_cls, "i", "D"); }

                jfieldID rz = sdk::jni->GetFieldID(rm_cls, "renderPosZ", "D");
                if (!rz) { sdk::jni->ExceptionClear(); rz = sdk::jni->GetFieldID(rm_cls, "field_78723_d", "D"); }
                if (!rz) { sdk::jni->ExceptionClear(); rz = sdk::jni->GetFieldID(rm_cls, "q", "D"); }
                if (!rz) { sdk::jni->ExceptionClear(); rz = sdk::jni->GetFieldID(rm_cls, "j", "D"); }

                if (rx && ry && rz) {
                    saved_rX = sdk::jni->GetDoubleField(render_manager.object, rx);
                    saved_rY = sdk::jni->GetDoubleField(render_manager.object, ry);
                    saved_rZ = sdk::jni->GetDoubleField(render_manager.object, rz);
                }
                sdk::jni->DeleteLocalRef(rm_cls);
            }
        }
    }

    void render_world(mapper::__minecraft& minecraft)
    {
        if (!enabled || !show_path || !is_blinking || path_coords.empty()) return;

        double rX = saved_rX;
        double rY = saved_rY;
        double rZ = saved_rZ;

        glPushMatrix();
        glEnable(GL_LINE_SMOOTH);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_TEXTURE_2D);
        glDepthMask(false);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        glLineWidth(2.5f);

        glColor4f(path_color[0], path_color[1], path_color[2], 1.0f);

        glBegin(GL_LINE_STRIP);
        for (const auto& pos : path_coords) {
            glVertex3d(pos.x - rX, pos.y - rY, pos.z - rZ);
        }
        glEnd();

        glDisable(GL_BLEND);
        glDepthMask(true);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_LINE_SMOOTH);
        glPopMatrix();
    }

    void render_ui()
    {
        if (!enabled || !show_timer || !is_blinking) return;

        ULONGLONG elapsed = GetTickCount64() - start_time;
        float effective_limit = timer_limit;
        if (effective_limit > HARD_CAP_SECONDS) effective_limit = HARD_CAP_SECONDS;
        float fraction = (float)elapsed / (effective_limit * 1000.0f);
        if (fraction > 1.0f) fraction = 1.0f;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (!draw) return;

        ImVec2 screen_center = ImVec2(ImGui::GetIO().DisplaySize.x / 2.0f, ImGui::GetIO().DisplaySize.y / 2.0f);

        float bar_width = 200.0f;
        float bar_height = 8.0f;
        ImVec2 bar_pos = ImVec2(screen_center.x - (bar_width / 2.0f), screen_center.y + 30.0f);

        ImU32 col_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.10f, 0.10f, 0.10f, 0.85f));
        ImU32 col_bar = ImGui::ColorConvertFloat4ToU32(ImVec4(path_color[0], path_color[1], path_color[2], 1.0f));

        draw->AddRectFilled(bar_pos, ImVec2(bar_pos.x + bar_width, bar_pos.y + bar_height), col_bg, 4.0f);
        draw->AddRectFilled(bar_pos, ImVec2(bar_pos.x + (bar_width * fraction), bar_pos.y + bar_height), col_bar, 4.0f);

        // Indicador de modo
        const char* mode_text = (mode == 0) ? "SMOOTH" : "FREEZE";
        ImVec2 textSz = ImGui::CalcTextSize(mode_text);
        draw->AddText(ImVec2(screen_center.x - textSz.x * 0.5f, bar_pos.y - 16.0f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(path_color[0], path_color[1], path_color[2], 0.8f)), mode_text);
    }
}
