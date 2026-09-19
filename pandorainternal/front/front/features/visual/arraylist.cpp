#include "../features.hpp"
#include "backends/imgui.h"
#include "backends/imgui_internal.h"
#include <GL/gl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>
#include <cstring>
#include <atomic>
#include "../../helper/blur/gl_blur.hpp"
#include "../../../back/misc/imgui/fonts/font_manager.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#define ICON_FA_pandoraFLAKE "\xef\x8b\x9c"
#include "../../../../w_imgui_port/render/stb/stb_image.hh"

#include <GL/gl.h>

extern bool g_AltManagerMode;

// ============================================================
// Variables GUI (definidas en pandora_main.cpp)
// ============================================================
extern std::string g_CachedPlayerName;
extern std::string g_CachedServerIP;
extern ImFont* g_ArrayListFont;
extern ImFont* g_ArrayListBoldFont;
extern ImFont* g_ArrayListPixelFont;
extern ImFont* g_Kamerik;
extern bool  gui_arraylist_enabled;
extern bool  gui_watermark_enabled;
extern float gui_watermark_pos_x;
extern float gui_watermark_pos_y;
extern bool  gui_watermark_show_player;
extern bool  gui_watermark_show_server;
extern bool  gui_watermark_show_fps;
extern bool  gui_watermark_show_name;
extern bool  gui_watermark_show_time;
extern bool  gui_watermark_background;
extern bool  gui_watermark_blur;
extern float gui_watermark_blur_opacity;
extern float gui_watermark_color[3];
extern float gui_watermark_color_b[3];
extern int   gui_watermark_color_mode;
extern float gui_arraylist_color_b[3];
extern float gui_arraylist_bar_width;
extern bool  gui_arraylist_bracket_flags;
namespace font { extern ImFont* default_icon; }
extern bool  gui_watermark_text_shadow;
extern bool  gui_noslow_enabled;
extern bool  gui_nojumpdelay_enabled;
extern bool  gui_noitemrelease_enabled;
extern bool  gui_sprint_enabled;
extern float gui_tracers_thickness;
extern float gui_blink_timer_limit;

extern float gui_arraylist_scale;
extern float gui_arraylist_speed;
extern float gui_arraylist_pos_x;
extern float gui_arraylist_pos_y;
extern float gui_arraylist_pad_x;
extern float gui_arraylist_pad_y;
extern float gui_arraylist_radius;
extern bool  gui_arraylist_background;
extern bool  gui_arraylist_colorbar;
extern bool  gui_arraylist_watermark;   // BUG 1 FIX: antes leia features::visual::arraylist::watermark
// Removed gui_arraylist_notifications
extern float gui_arraylist_color[3];
extern float gui_arraylist_wave_saturation;
extern float gui_arraylist_info_color[3];
extern int   gui_arraylist_alignment;
extern int   gui_arraylist_color_mode;

// ============================================================
// NUEVAS VARIABLES DE EFECTO
// Variables compartidas con pandora_main.cpp:
//   int   gui_arraylist_anim_style = 0; // 0=Slide, 1=Cascade, 2=Wave-Ola
//   bool  gui_arraylist_rainbow_bar = false;
//   bool  gui_arraylist_glow = false;
// Y en el menu ImGui del ArrayList:
//   ImGui::Combo("Anim Style", &gui_arraylist_anim_style, "Slide\0Cascade\0Wave-Ola\0");
//   ImGui::Checkbox("Rainbow Bar", &gui_arraylist_rainbow_bar);
//   ImGui::Checkbox("Glow", &gui_arraylist_glow);
// ============================================================
extern int  gui_arraylist_anim_style;   // 0=Slide 1=Cascade 2=Wave-Ola
extern bool gui_arraylist_rainbow_bar;  // barra lateral arcoiris independiente del color mode
extern bool gui_arraylist_glow;
extern bool gui_arraylist_show_info;         // brillo/halo detras de cada item activo
extern float gui_arraylist_bg_color_4[4];
extern int   gui_arraylist_font;
extern bool  gui_arraylist_lowercase;
extern bool  gui_arraylist_shadows;
extern bool  gui_arraylist_background_shadow;
extern ImFont* g_Kamerik;

// WhipClient ported variables
extern bool  gui_arraylist_show_title;
extern float gui_arraylist_title_color[3];
extern bool  gui_arraylist_blur;
extern float gui_arraylist_blur_opacity;
extern float gui_arraylist_flow_color[3];
extern float gui_arraylist_fade_color[3];
extern bool  g_MenuVisible;
extern std::atomic<bool> g_PlayerInGui;
extern std::atomic<bool> g_MinecraftWorldLoaded;
extern float g_AccentColor[3];

// Hidden modules set (defined in pandora_main.cpp)
#include <set>
extern std::set<std::string> g_arraylist_hidden_modules;

// ============================================================
// VARIABLES NUEVOS MODULOS (definidas en pandora_main.cpp)
// ============================================================
extern bool  gui_nohitdelay_enabled;
extern bool  gui_blockhit_enabled;
extern bool  gui_autoarmor_enabled;
extern bool  gui_macros_enabled;
extern int   gui_macros_mode;
extern bool  gui_armorswitcher_enabled;
extern int   gui_armorswitcher_kit;
extern bool  gui_fastplace_enabled;
extern bool  gui_autotool_enabled;
extern bool  gui_esp_enabled;
extern int   gui_whip_esp_render_mode;
extern bool  gui_friends_enabled;
extern bool gui_config_just_loaded;
extern int   gui_velo_mode;
extern float gui_velo_lag_ms;

// Shared with RenderNotifications() so system toasts stack above arraylist toasts
float g_arraylist_toast_height = 0.0f;
extern float g_system_toast_height;  // defined in pandora_main.cpp
namespace features::visual::arraylist
{
    struct __feature_data
    {
        const char* name;
        bool* is_enabled;
        float       animation_progress = 0.f;

        // --- NUEVOS CAMPOS para efectos avanzados ---
        float cascade_progress = 0.f; // 0..1, sube con delay segun posicion
        float wave_phase = 0.f; // fase individual para wave-ola
        bool  was_enabled = false;
        float cascade_reset = 0.f; // timer para reiniciar cascade al togglear

        std::function<std::string()> get_value;
        int  sort_priority = 99; // menor numero = aparece primero (0=AutoClicker, 1=AimAssist, 99=resto)
    };

    static std::vector<__feature_data> feature_data_array = {
        // COMBAT
        { "Aim Assist",    &features::combat::aim_assist::enabled,    0.f,0.f,0.f,false,0.f, []() {
            int m = features::combat::aim_assist::mode;
            switch (m) {
            case 1: return std::string("Aim-Lock");
            case 2: return std::string("Vertical");
            default: return std::string("Smooth");
            }
        }, 1 },
        { "Auto Clicker",  &features::combat::auto_click::enabled,    0.f,0.f,0.f,false,0.f, []() {
            extern float gui_min_cps;
            extern float gui_max_cps;
            char b[64]; sprintf_s(b, "%.1f-%.1f", gui_min_cps, gui_max_cps); return std::string(b);
        }, 0 },
        { "Reach",         &features::combat::reach::enabled,         0.f,0.f,0.f,false,0.f, []() {
            extern float gui_reach_min_distance;
            extern float gui_reach_max_distance;
            char b[32]; sprintf_s(b, "%.1f-%.1f", gui_reach_max_distance, gui_reach_min_distance); return std::string(b);
        }},
        { "Velocity",      &features::combat::velocity::enabled,      0.f,0.f,0.f,false,0.f, []() {
            extern float gui_velo_horizontal;
            if (::gui_velo_mode == 1) return std::string("Lag");
            char b[16]; sprintf_s(b, "%.0f%%", gui_velo_horizontal); return std::string(b);
        }},
        { "Refill",        &features::combat::refill::enabled,        0.f,0.f,0.f,false,0.f, nullptr },
        { "BlockHit",      &gui_blockhit_enabled,                     0.f,0.f,0.f,false,0.f, []() {
            extern int gui_blockhit_mode;
            static const char* modes[] = {"Manual", "Predict", "Auto", "Lag"};
            int m = (gui_blockhit_mode >= 0 && gui_blockhit_mode < 4) ? gui_blockhit_mode : 0;
            return std::string(modes[m]);
        }},
        { "NoHitDelay",   &gui_nohitdelay_enabled,                    0.f,0.f,0.f,false,0.f, nullptr },
        // MOVEMENT
        { "Sprint",        &gui_sprint_enabled,                       0.f,0.f,0.f,false,0.f, nullptr },
        { "NoSlowdown",    &gui_noslow_enabled,                       0.f,0.f,0.f,false,0.f, nullptr },
        { "NoJumpDelay",   &gui_nojumpdelay_enabled,                  0.f,0.f,0.f,false,0.f, nullptr },
        { "NoItemRelease", &gui_noitemrelease_enabled,                 0.f,0.f,0.f,false,0.f, nullptr },
        { "FastPlace",     &gui_fastplace_enabled,                    0.f,0.f,0.f,false,0.f, nullptr },
        // ESP unificado (pandora Client)
        { "ESP",           &gui_esp_enabled,                          0.f,0.f,0.f,false,0.f, []() {
            static const char* styles[] = { "2D", "3D" };
            int idx = (gui_whip_esp_render_mode >= 0 && gui_whip_esp_render_mode < 2) ? gui_whip_esp_render_mode : 0;
            return std::string(styles[idx]);
        }},
        { "Tracers",       &features::visual::tracers::enabled,       0.f,0.f,0.f,false,0.f, []() {
            char b[16]; sprintf_s(b, "%.1fpx", gui_tracers_thickness); return std::string(b);
        }},
        { "Nametags",      &features::visual::nametags::enabled,      0.f,0.f,0.f,false,0.f, nullptr },
        { "Hit Markers",    &features::visual::hit_markers::enabled,   0.f,0.f,0.f,false,0.f, []() {
            return features::visual::hit_markers::mode == 0 ? std::string("2D") : std::string("3D");
        }},
        // MISC
        { "Blink",         &features::misc::blink::enabled,           0.f,0.f,0.f,false,0.f, []() {
            char b[16]; sprintf_s(b, "%.0fs", gui_blink_timer_limit); return std::string(b);
        }},
        // PLAYER
        { "AutoArmor",     &gui_autoarmor_enabled,                    0.f,0.f,0.f,false,0.f, nullptr },
        { "Macros",        &gui_macros_enabled,                       0.f,0.f,0.f,false,0.f, []() {
            static const char* modes[] = {"Bow", "Fireball", "Gap", "Pot"};
            int m = (gui_macros_mode >= 0 && gui_macros_mode < 4) ? gui_macros_mode : 0;
            return std::string(modes[m]);
        }},
        { "ArmorSwitcher", &gui_armorswitcher_enabled,                 0.f,0.f,0.f,false,0.f, []() {
            static const char* kits[] = {"Diamond", "Iron", "Gold", "Chain", "Leather"};
            int k = (gui_armorswitcher_kit >= 0 && gui_armorswitcher_kit < 5) ? gui_armorswitcher_kit : 0;
            return std::string(kits[k]);
        }},
        { "Friends",       &gui_friends_enabled,                      0.f,0.f,0.f,false,0.f, nullptr },
    };

    // ----------------------------------------------------------------
    // get_theme_color
    //
    // Each color mode is exclusive. Legacy chroma state must never override
    // the currently selected mode.
    // ----------------------------------------------------------------
    static ImU32 get_whip_color(int idx, float time, float alpha)
    {
        float r, g, b;
        float index_ratio = idx * 0.08f;

        const float safeSpeed = (std::max)(0.10f, (std::min)(gui_arraylist_speed, 3.0f));
        if (gui_arraylist_color_mode == 1)
        {
            // 1 = Rainbow (HSV ciclico)
            float hue = fmodf(time * (0.12f + safeSpeed * 0.32f) + index_ratio * 0.25f, 1.0f);
            ImGui::ColorConvertHSVtoRGB(hue, 0.85f, 1.0f, r, g, b);
        }
        else if (gui_arraylist_color_mode == 2)
        {
            // 2 = Fade (transicion entre dos colores seleccionados)
            float t = (sinf(time * (0.8f + safeSpeed * 2.0f) - idx * 0.4f) + 1.0f) * 0.5f;
            r = gui_arraylist_color[0] + (gui_arraylist_fade_color[0] - gui_arraylist_color[0]) * t;
            g = gui_arraylist_color[1] + (gui_arraylist_fade_color[1] - gui_arraylist_color[1]) * t;
            b = gui_arraylist_color[2] + (gui_arraylist_fade_color[2] - gui_arraylist_color[2]) * t;
        }
        else if (gui_arraylist_color_mode == 3)
        {
            // 3 = Flow (oscilacion entre Main Color y Flow Color)
            float t = (sinf(time * (0.7f + safeSpeed * 1.8f) - idx * 0.45f) + 1.0f) * 0.5f;
            r = gui_arraylist_color[0] + (gui_arraylist_flow_color[0] - gui_arraylist_color[0]) * t;
            g = gui_arraylist_color[1] + (gui_arraylist_flow_color[1] - gui_arraylist_color[1]) * t;
            b = gui_arraylist_color[2] + (gui_arraylist_flow_color[2] - gui_arraylist_color[2]) * t;
        }
        else if (gui_arraylist_color_mode == 4)
        {
            // 4 = GUI Based (Color del acento de pandora Client con ligera onda de brillo)
            float t = (sinf(time * 3.0f - idx * 0.35f) + 1.0f) * 0.5f;
            float factor = 0.80f + 0.20f * t;
            r = g_AccentColor[0] * factor;
            g = g_AccentColor[1] * factor;
            b = g_AccentColor[2] * factor;
        }
        else
        {
            // 0 = Static
            r = gui_arraylist_color[0];
            g = gui_arraylist_color[1];
            b = gui_arraylist_color[2];
        }

        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, alpha));
    }

    // Rainbow bar independiente del color del texto (siempre HSV completo)
    static ImU32 get_rainbow_bar_color(float index_ratio, float time, float alpha)
    {
        float r, g, b;
        float hue = fmodf(time * 0.4f + index_ratio * 0.22f, 1.0f);
        ImGui::ColorConvertHSVtoRGB(hue, 0.9f, 1.0f, r, g, b);
        return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, alpha));
    }

    // ----------------------------------------------------------------
    // Toast system
    // ----------------------------------------------------------------
    struct Toast {
        std::string text = {};
        bool        enabled_state = false;
        int         keybind = 0;
        float       alpha = 0.f;
        float       slide = 0.f;
        float       life = 0.f;
        float       text_w = 0.f;
        float       slideY = 0.f;
    };

    static std::vector<Toast>    g_toasts;

    struct WatchEntry { bool* ptr; bool prev; const char* name; };
    static std::vector<WatchEntry> g_watch;
    static bool g_watch_init = false;

    static void render_toasts(ImDrawList* dl, ImFont* font, float sw, float sh, float dt);
    static void check_and_push(ImFont* font, float fsz);
    static void init_watch();
    void PushToastEvent(const std::string& txt, bool st);

    static void render_whip_watermark(ImDrawList* dl, ImFont* font, float sw, float sh)
    {
        if (!gui_watermark_enabled) return;

        if (!font) {
            font = font_manager::FontManager::get().get_watermark_font();
            if (!font) font = ImGui::GetFont();
        }

        static int cachedFps = 0;
        static int fpsFrameCount = 0;
        static double fpsTimer = 0.0;
        double now = ImGui::GetTime();
        fpsFrameCount++;
        if (now - fpsTimer >= 1.0) {
            cachedFps = fpsFrameCount;
            fpsFrameCount = 0;
            fpsTimer = now;
        }

        if (gui_watermark_pos_x > 0.98f || gui_watermark_pos_x < 0.0f) gui_watermark_pos_x = 0.015f;
        if (gui_watermark_pos_y > 0.98f || gui_watermark_pos_y < 0.0f) gui_watermark_pos_y = 0.015f;

        const float s  = 1.35f;
        float fontSz   = 13.0f * s;
        float padX     = 11.0f * s;
        float padY     = 7.0f  * s;
        float totalH   = fontSz + (padY * 2.0f);
        float rounding = totalH * 0.5f; // Pill-shape rounding

        // Watermark color is independent from Array List modes.  This prevents
        // a saved Rainbow/Flow list mode from changing the watermark remotely.
        ImU32 cWhite  = IM_COL32(244, 244, 247, 250);
        ImU32 cText   = IM_COL32(194, 195, 204, 242);
        ImU32 cBar    = IM_COL32(
            (int)(g_AccentColor[0] * 255.0f),
            (int)(g_AccentColor[1] * 255.0f),
            (int)(g_AccentColor[2] * 255.0f), 145);
        ImU32 cAccent = IM_COL32(
            (int)(g_AccentColor[0] * 255.0f),
            (int)(g_AccentColor[1] * 255.0f),
            (int)(g_AccentColor[2] * 255.0f), 250);

        struct Seg { std::string text; ImU32 color; bool is_icon = false; ImTextureID texture = nullptr; };
        std::vector<Seg> segs;

        auto addInfo = [&](const std::string& text, ImU32 color, bool is_icon = false) {
            if (!segs.empty() && !is_icon) segs.push_back({ " | ", cWhite }); // Restore pipe separator for matching photo
            segs.push_back({ text, color, is_icon });
        };

        if (gui_watermark_show_name) {
            float t = (float)ImGui::GetTime();
            float r, g, b;
            if (gui_watermark_color_mode == 1) { // Gradient Mode
                const float blend = (sinf(t * 1.35f) + 1.0f) * 0.5f;
                const float smooth = blend * blend * (3.0f - 2.0f * blend);
                r = gui_watermark_color[0] + (gui_watermark_color_b[0] - gui_watermark_color[0]) * smooth;
                g = gui_watermark_color[1] + (gui_watermark_color_b[1] - gui_watermark_color[1]) * smooth;
                b = gui_watermark_color[2] + (gui_watermark_color_b[2] - gui_watermark_color[2]) * smooth;
            } else { // Shadow Mode (white base with shadow color accent)
                r = gui_watermark_color[0]; g = gui_watermark_color[1]; b = gui_watermark_color[2];
            }
            ImU32 cBlend = IM_COL32((int)(r * 255.f), (int)(g * 255.f), (int)(b * 255.f), 250);
            
            segs.push_back({ "pandora", cBlend, false });
        }

        if (gui_watermark_show_fps) {
            char fpsBuf[32];
            snprintf(fpsBuf, sizeof(fpsBuf), "%d fps", cachedFps);
            addInfo(fpsBuf, cText);
        }
        if (gui_watermark_show_server) {
            std::string server = g_CachedServerIP.empty() ? "Singleplayer" : g_CachedServerIP;
            addInfo(server, cText);
        }
        if (gui_watermark_show_player) {
            std::string user = g_CachedPlayerName.empty() ? "Player" : g_CachedPlayerName;
            addInfo(user, cText);
        }
        // Real-time clock (HH:MM)
        if (gui_watermark_show_time) {
            time_t tNow = ::time(nullptr);
            tm tmNow;
            localtime_s(&tmNow, &tNow);
            char timeBuf[16];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tmNow.tm_hour, tmNow.tm_min);
            addInfo(timeBuf, cText);
        }

        if (segs.empty()) {
            segs.push_back({ "Watermark", cAccent, false });
        }

        float totalTextW = 0.0f;
        for (const auto& sg : segs) {
            ImFont* f = sg.is_icon ? FONT_MANAGER.get_icon_font() : font;
            if (!f) f = font;
            totalTextW += f->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, sg.text.c_str()).x;
        }

        float totalW = totalTextW + (padX * 2.0f);

        float barX = gui_watermark_pos_x * sw;
        float barY = gui_watermark_pos_y * sh;

        barX = (std::max)(5.0f, (std::min)(barX, sw - totalW - 5.0f));
        barY = (std::max)(5.0f, (std::min)(barY, sh - totalH - 5.0f));

        if (g_MenuVisible && !g_AltManagerMode) {
            static bool   wmDragging = false;
            static ImVec2 wmDragOff;

            const ImVec2 mouse = ImGui::GetMousePos();
            const bool   down  = ImGui::IsMouseDown(0);
            const bool   clicked = ImGui::IsMouseClicked(0);

            const ImVec2 editMin(barX - 4.0f, barY - 4.0f);
            const ImVec2 editMax(barX + totalW + 4.0f, barY + totalH + 4.0f);

            const bool inBox = mouse.x >= editMin.x && mouse.x <= editMax.x && mouse.y >= editMin.y && mouse.y <= editMax.y;
            if (clicked) {
                if (inBox) {
                    wmDragging = true;
                    wmDragOff = ImVec2(barX - mouse.x, barY - barY - mouse.y);
                }
            }
            if (!down) {
                wmDragging = false;
            }
            static float wmDragAnim = 0.0f;
            wmDragAnim = ImLerp(wmDragAnim, wmDragging ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 15.0f);

            if (wmDragging && down) {
                float newX = mouse.x + wmDragOff.x;
                float newY = mouse.y + wmDragOff.y;

                float centerX = newX + totalW * 0.5f;
                float centerY = newY + totalH * 0.5f;

                float snapPointsX[] = { sw * 0.3333f, sw * 0.5f, sw * 0.6666f };
                float snapPointsY[] = { sh * 0.3333f, sh * 0.5f, sh * 0.6666f };

                for (float sp : snapPointsX) {
                    if (std::abs(centerX - sp) < 15.0f) { centerX = sp; newX = centerX - totalW * 0.5f; break; }
                }
                for (float sp : snapPointsY) {
                    if (std::abs(centerY - sp) < 15.0f) { centerY = sp; newY = centerY - totalH * 0.5f; break; }
                }

                gui_watermark_pos_x = newX / sw;
                gui_watermark_pos_y = newY / sh;
                barX = newX;
                barY = newY;

                for (float sp : snapPointsX) {
                    bool isSnapped = std::abs(centerX - sp) < 1.0f;
                    int alpha = (int)(80 * wmDragAnim);
                    if (isSnapped) alpha = (int)(255 * wmDragAnim);
                    dl->AddLine(ImVec2(sp, 0), ImVec2(sp, sh), IM_COL32(255, 255, 255, alpha), isSnapped ? 2.0f : 1.0f);
                }
                for (float sp : snapPointsY) {
                    bool isSnapped = std::abs(centerY - sp) < 1.0f;
                    int alpha = (int)(80 * wmDragAnim);
                    if (isSnapped) alpha = (int)(255 * wmDragAnim);
                    dl->AddLine(ImVec2(0, sp), ImVec2(sw, sp), IM_COL32(255, 255, 255, alpha), isSnapped ? 2.0f : 1.0f);
                }
            }

            if (wmDragAnim > 0.01f) {
                dl->AddRect(ImVec2(barX - 2.0f * wmDragAnim, barY - 2.0f * wmDragAnim), ImVec2(barX + totalW + 2.0f * wmDragAnim, barY + totalH + 2.0f * wmDragAnim), IM_COL32(255, 255, 255, (int)(100 * wmDragAnim)), 5.0f, 0, 2.0f);
            }

        }

        ImVec2 bMin(barX, barY);
        ImVec2 bMax(barX + totalW, barY + totalH);
        if (gui_watermark_background) {
            float bgAlpha = (gui_watermark_blur ? gui_watermark_blur_opacity : 0.88f);
            
            // Shadow
            dl->AddRectFilled(ImVec2(bMin.x - 2, bMin.y - 2), ImVec2(bMax.x + 2, bMax.y + 2), IM_COL32(0, 0, 0, (int)(70 * bgAlpha)), rounding);
            
            // Main dark background
            dl->AddRectFilled(bMin, bMax, IM_COL32(20, 20, 20, (int)(255 * bgAlpha)), rounding);
        }

        float cx = barX + padX;
        float cy = barY + (totalH - fontSz) * 0.5f;

        for (const auto& sg : segs) {
            ImFont* f = sg.is_icon && font::default_icon ? font::default_icon : font;
            
            // Text Shadow (Black)
            if (gui_watermark_text_shadow && gui_watermark_color_mode != 0) // if not shadow mode (0)
                dl->AddText(f, fontSz, ImVec2(cx + 1.0f, cy + 1.0f), IM_COL32(0, 0, 0, 180), sg.text.c_str());
                
            // Custom Shadow Mode Effect
            if (gui_watermark_color_mode == 0) {
                float shadowOffset = 1.0f * s; // Adjust shadow distance
                // Shadow Color Layer
                dl->AddText(f, fontSz, ImVec2(cx + shadowOffset, cy + shadowOffset), sg.color, sg.text.c_str());
                // White Top Layer
                dl->AddText(f, fontSz, ImVec2(cx, cy), cWhite, sg.text.c_str());
            } else {
                dl->AddText(f, fontSz, ImVec2(cx, cy), sg.color, sg.text.c_str());
            }
            
            cx += f->CalcTextSizeA(fontSz, FLT_MAX, 0.0f, sg.text.c_str()).x;
        }

        if (g_MenuVisible && !g_AltManagerMode) {
            const char* hint = "hold to move";
            const float hintSize = 11.5f * s;
            ImVec2 hintExtent = font->CalcTextSizeA(
                hintSize, FLT_MAX, 0.0f, hint);
            ImVec2 hintPos(
                bMin.x + (totalW - hintExtent.x) * 0.5f,
                bMax.y + 6.0f * s);
            dl->AddText(
                font, hintSize, ImVec2(hintPos.x + 1.0f, hintPos.y + 1.0f),
                IM_COL32(0, 0, 0, 220), hint);
            dl->AddText(
                font, hintSize, hintPos,
                IM_COL32(235, 235, 239, 245), hint);
        }
    }

    // ----------------------------------------------------------------
    // run()
    // ----------------------------------------------------------------
    void run()
    {
        GLBlur::Init();
        float dt = ImGui::GetIO().DeltaTime;
        float elapsed = dt * 1000.f;
        float time = (float)ImGui::GetTime();

        __int32 vp[4] = {};
        glGetIntegerv(GL_VIEWPORT, vp);
        float sw = (float)vp[2];
        float sh = (float)vp[3];

        // Notifications and Watermark use the menu's font (Poppins Bold)
        ImFont* notifFont = FONT_MANAGER.get_watermark_font();
        if (!notifFont) notifFont = ImGui::GetFont();

        // Array List usa Kamerik grande
        ImFont* arrayFont = FONT_MANAGER.get_arraylist_font();
        if (!arrayFont) arrayFont = ImGui::GetFont();

        // Front-end/title and multiplayer screens have no loaded world.
        const bool showHud = g_MinecraftWorldLoaded.load(std::memory_order_acquire) ||
            g_MenuVisible;

        if (showHud)
            render_whip_watermark(ImGui::GetBackgroundDrawList(), notifFont, sw, sh);

        if (features::misc::notifications::enabled)
            render_toasts(ImGui::GetBackgroundDrawList(), notifFont, sw, sh, dt);
        else {
            if (g_watch_init)
                for (auto& w : g_watch) w.prev = *w.ptr;
            g_toasts.clear();
        }

        if (!showHud) return;

        enabled = ::gui_arraylist_enabled;
        if (!enabled) return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();

        if (gui_arraylist_scale < 0.4f || gui_arraylist_scale > 3.5f) gui_arraylist_scale = 1.0f;

        float scale = gui_arraylist_scale;
        float fontSize = 20.0f * scale;
        float padX = gui_arraylist_pad_x * scale;
        float padY = gui_arraylist_pad_y * scale;
        float barW = gui_arraylist_bar_width * scale;
        float rounding = gui_arraylist_radius * scale;
        float waveSpeed = gui_arraylist_speed;

        // ============================================================
        // USAR KAMERIK PARA EL ARRAY LIST
        // ============================================================
        float spaceW = arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, " ").x;
        float lerpSpeed = 7.0f * dt;

        // Module color and secondary color (from settings)
        ImVec4 modColorA(gui_arraylist_color[0], gui_arraylist_color[1], gui_arraylist_color[2], 1.0f);
        ImVec4 modColorB(gui_arraylist_color_b[0], gui_arraylist_color_b[1], gui_arraylist_color_b[2], 1.0f);

        // Anchor: draggable position (stored as normalized 0-1)
        static bool alDragging = false;
        static ImVec2 alDragOff;

        float anchorX = gui_arraylist_pos_x * sw;
        float startY  = gui_arraylist_pos_y * sh;

        // Keep the selected edge stable. The previous fallback moved every
        // position below 10px to the opposite side of the screen.
        anchorX = (std::max)(0.0f, (std::min)(anchorX, sw));
        startY = (std::max)(0.0f, (std::min)(startY, sh));

        // Auto-flip: if anchor is on left half, render left-aligned
        bool isLeftSide = (gui_arraylist_pos_x < 0.5f);

        struct ItemToDraw {
            std::string name;
            std::string flags;
            float totalW;
            ImVec2 nameSz;
            ImVec2 flagSz;
            float anim;
            int index;
        };
        std::vector<ItemToDraw> activeList;

        for (int i = 0; i < (int)feature_data_array.size(); i++) {
            __feature_data& fd = feature_data_array[i];
            const bool hiddenByCurrentName = g_arraylist_hidden_modules.count(fd.name) != 0;
            const bool hiddenByLegacyName = strcmp(fd.name, "NoSlowdown") == 0
                && g_arraylist_hidden_modules.count("NoSlow") != 0;
            bool st = (fd.is_enabled && *fd.is_enabled) && !hiddenByCurrentName && !hiddenByLegacyName;
            float targetAnim = st ? 1.0f : 0.0f;

            fd.animation_progress += (targetAnim - fd.animation_progress) * lerpSpeed;
            if (fd.animation_progress < 0.01f && !st) { fd.animation_progress = 0.0f; continue; }

            std::string nameStr = fd.name;
            std::string flagStr = "";
            if (gui_arraylist_show_info && fd.get_value) {
                flagStr = fd.get_value();
                if (!flagStr.empty() && gui_arraylist_bracket_flags) {
                    flagStr = "[" + flagStr + "]";
                }
            }
            if (gui_arraylist_lowercase) {
                for (auto& c : nameStr) c = (char)std::tolower((unsigned char)c);
                for (auto& c : flagStr) c = (char)std::tolower((unsigned char)c);
            }

            ImVec2 nSz = arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, nameStr.c_str());
            ImVec2 fSz = flagStr.empty() ? ImVec2(0,0) : arrayFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, flagStr.c_str());
            float tW   = nSz.x + (flagStr.empty() ? 0.0f : (spaceW + fSz.x)) + (padX * 2.0f) + (gui_arraylist_colorbar ? barW : 0.0f);

            activeList.push_back({ nameStr, flagStr, tW, nSz, fSz, fd.animation_progress, i });
        }

        // Sort by total width, widest first (raid0 style)
        std::sort(activeList.begin(), activeList.end(), [](const ItemToDraw& a, const ItemToDraw& b) {
            return a.totalW > b.totalW;
        });

        float currentY = startY;

        // Perform the OpenGL blur pass ONCE for the entire list
        if (gui_arraylist_background && gui_arraylist_blur) {
            GLBlur::AddBlurPass(dl, 3.0f);
        }

        // Trace the real stepped silhouette formed by all row backgrounds.
        // This is deliberately not a bounding box: every width transition is
        // part of the outer contour while row-to-row joins remain internal.
        if (false && gui_arraylist_background && gui_arraylist_background_shadow && !activeList.empty()) {
            struct ShadowRow { float left, top, right, bottom, anim; };
            std::vector<ShadowRow> shadowRows;
            shadowRows.reserve(activeList.size());
            float boundsY = startY;
            float shadowAnim = 0.0f;

            for (const ItemToDraw& item : activeList) {
                if (item.anim < 0.01f) continue;

                const float itemH = item.nameSz.y + (padY * 2.0f);
                const float itemW = item.totalW;
                float boxX = isLeftSide
                    ? anchorX - (1.0f - item.anim) * (itemW + 10.0f)
                    : anchorX - itemW + (1.0f - item.anim) * (itemW + 10.0f);
                boxX = std::round(boxX);
                const float boxY = std::round(boundsY);

                shadowRows.push_back({ boxX, boxY, boxX + itemW, boxY + itemH, item.anim });
                shadowAnim = (std::max)(shadowAnim, item.anim);
                boundsY += itemH * item.anim;
            }

            if (!shadowRows.empty()) {
                std::vector<ImVec2> contour;
                contour.reserve(shadowRows.size() * 4 + 4);

                // Top edge, then the complete right-hand staircase.
                contour.push_back(ImVec2(shadowRows.front().left, shadowRows.front().top));
                contour.push_back(ImVec2(shadowRows.front().right, shadowRows.front().top));
                for (size_t i = 0; i < shadowRows.size(); ++i) {
                    const float edgeY = (i + 1 < shadowRows.size())
                        ? shadowRows[i + 1].top : shadowRows[i].bottom;
                    contour.push_back(ImVec2(shadowRows[i].right, edgeY));
                    if (i + 1 < shadowRows.size())
                        contour.push_back(ImVec2(shadowRows[i + 1].right, edgeY));
                }

                // Bottom edge, then the complete left-hand staircase upward.
                contour.push_back(ImVec2(shadowRows.back().left, shadowRows.back().bottom));
                for (int i = (int)shadowRows.size() - 1; i >= 0; --i) {
                    const float edgeY = (i > 0) ? shadowRows[i].top : shadowRows.front().top;
                    contour.push_back(ImVec2(shadowRows[i].left, edgeY));
                    if (i > 0)
                        contour.push_back(ImVec2(shadowRows[i - 1].left, edgeY));
                }

                // Low-alpha layers are placed fully outside the clockwise
                // contour. Their inner edge touches the background edge, so
                // no shadow darkens the translucent background itself.
                static const float shadowThickness[] = { 11.0f, 8.0f, 5.5f, 3.5f, 1.8f };
                static const int shadowAlpha[] = { 8, 11, 15, 21, 30 };

                // Remove duplicate staircase vertices before calculating joins.
                std::vector<ImVec2> cleanContour;
                cleanContour.reserve(contour.size());
                for (const ImVec2& point : contour) {
                    if (cleanContour.empty() ||
                        std::fabs(point.x - cleanContour.back().x) > 0.5f ||
                        std::fabs(point.y - cleanContour.back().y) > 0.5f)
                        cleanContour.push_back(point);
                }
                if (cleanContour.size() > 2 &&
                    std::fabs(cleanContour.front().x - cleanContour.back().x) <= 0.5f &&
                    std::fabs(cleanContour.front().y - cleanContour.back().y) <= 0.5f)
                    cleanContour.pop_back();

                // Round the real silhouette first. Offsetting this sampled
                // curve by its tangent normal cannot create concave miter
                // spikes or self-crossing diagonal segments.
                std::vector<ImVec2> roundedBase;
                roundedBase.reserve(cleanContour.size() * 5);
                const float targetRadius = (std::max)(0.0f, rounding);
                for (size_t point = 0; point < cleanContour.size(); ++point) {
                        const ImVec2 prev = cleanContour[(point + cleanContour.size() - 1) % cleanContour.size()];
                        const ImVec2 current = cleanContour[point];
                        const ImVec2 next = cleanContour[(point + 1) % cleanContour.size()];
                        float toPrevX = prev.x - current.x;
                        float toPrevY = prev.y - current.y;
                        float toNextX = next.x - current.x;
                        float toNextY = next.y - current.y;
                        const float prevLength = std::sqrt(toPrevX * toPrevX + toPrevY * toPrevY);
                        const float nextLength = std::sqrt(toNextX * toNextX + toNextY * toNextY);
                        if (prevLength < 0.5f || nextLength < 0.5f || targetRadius <= 0.0f) {
                            roundedBase.push_back(current);
                            continue;
                        }
                        toPrevX /= prevLength; toPrevY /= prevLength;
                        toNextX /= nextLength; toNextY /= nextLength;
                        const float cut = (std::min)(targetRadius,
                            (std::min)(prevLength, nextLength) * 0.45f);
                        const ImVec2 start(current.x + toPrevX * cut, current.y + toPrevY * cut);
                        const ImVec2 end(current.x + toNextX * cut, current.y + toNextY * cut);
                        for (int sample = 0; sample <= 4; ++sample) {
                            const float t = sample * 0.25f;
                            const float inv = 1.0f - t;
                            roundedBase.push_back(ImVec2(
                                inv * inv * start.x + 2.0f * inv * t * current.x + t * t * end.x,
                                inv * inv * start.y + 2.0f * inv * t * current.y + t * t * end.y));
                        }
                }

                for (int shadowLayer = 0; shadowLayer < 5; ++shadowLayer) {
                    const int alpha = (int)(shadowAlpha[shadowLayer] * shadowAnim);
                    const float thickness = shadowThickness[shadowLayer] * scale;
                    const float halfThickness = thickness * 0.5f;
                    std::vector<ImVec2> offsetContour;
                    offsetContour.reserve(roundedBase.size());

                    for (size_t point = 0; point < roundedBase.size(); ++point) {
                        const ImVec2 prev = roundedBase[(point + roundedBase.size() - 1) % roundedBase.size()];
                        const ImVec2 current = roundedBase[point];
                        const ImVec2 next = roundedBase[(point + 1) % roundedBase.size()];
                        float tangentX = next.x - prev.x;
                        float tangentY = next.y - prev.y;
                        const float tangentLength = std::sqrt(tangentX * tangentX + tangentY * tangentY);
                        if (tangentLength < 0.5f) {
                            offsetContour.push_back(current);
                            continue;
                        }
                        tangentX /= tangentLength;
                        tangentY /= tangentLength;
                        offsetContour.push_back(ImVec2(
                            current.x + tangentY * halfThickness,
                            current.y - tangentX * halfThickness));
                    }

                    dl->AddPolyline(offsetContour.data(), (int)offsetContour.size(),
                        IM_COL32(0, 0, 0, alpha), ImDrawFlags_Closed, thickness);
                }
            }
        }

        // --- Render Active Modules (raid0 style) ---
        for (int i = 0; i < (int)activeList.size(); i++) {
            ItemToDraw& item = activeList[i];
            float anim = item.anim;
            if (anim < 0.01f) continue;

            float itemH = item.nameSz.y + (padY * 2.0f);
            float itemW = item.totalW;

            // Position: flip based on which side of screen
            float boxX;
            if (isLeftSide) {
                // Left-aligned: items grow from anchor to the right
                boxX = anchorX - (1.0f - anim) * (itemW + 10.0f);
            } else {
                // Right-aligned: items grow from anchor to the left
                boxX = anchorX - itemW + (1.0f - anim) * (itemW + 10.0f);
            }
            // Keep the geometry on physical pixels. Fractional origins made
            // the atlas sample between texels and visibly softened the text.
            boxX = std::round(boxX);
            float boxY = std::round(currentY);

            ImVec2 iMin(boxX, boxY);
            ImVec2 iMax(boxX + itemW, boxY + itemH);

            // Wave color based on Mode
            ImVec4 waveColor;
            int waveDelay = -(i * 12);
            if (gui_arraylist_color_mode == 0) {
                // Single
                waveColor = modColorA;
                waveColor.w = anim;
            } else if (gui_arraylist_color_mode == 1) {
                // Rainbow
                float hue = fmodf(time * waveSpeed + (float)waveDelay * 0.01f, 1.0f);
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(hue, 0.6f, 0.9f, r, g, b);
                waveColor = ImVec4(r, g, b, anim);
            } else if (gui_arraylist_color_mode == 2) {
                // Fade (smooth transition between Color 1 and Color 2)
                float wave = (std::sin(time * waveSpeed * 6.28f + (float)waveDelay * 0.055f) + 1.0f) * 0.5f;
                waveColor = ImVec4(
                    modColorA.x + (modColorB.x - modColorA.x) * wave,
                    modColorA.y + (modColorB.y - modColorA.y) * wave,
                    modColorA.z + (modColorB.z - modColorA.z) * wave,
                    anim);
            } else if (gui_arraylist_color_mode == 3) {
                // Flow (faster oscillation between Color 1 and Color 2, per-item offset)
                float t = (std::sin(time * waveSpeed * 4.0f + (float)i * 0.45f) + 1.0f) * 0.5f;
                waveColor = ImVec4(
                    modColorA.x + (modColorB.x - modColorA.x) * t,
                    modColorA.y + (modColorB.y - modColorA.y) * t,
                    modColorA.z + (modColorB.z - modColorA.z) * t,
                    anim);
            } else {
                // GUI Based (uses accent color with subtle brightness wave)
                float t = (std::sin(time * 3.0f - (float)i * 0.35f) + 1.0f) * 0.5f;
                float factor = 0.80f + 0.20f * t;
                waveColor = ImVec4(
                    g_AccentColor[0] * factor,
                    g_AccentColor[1] * factor,
                    g_AccentColor[2] * factor,
                    anim);
            }
            ImU32 itemCol = ImGui::ColorConvertFloat4ToU32(waveColor);

            // Dark box background with rounding on outer edge
            if (gui_arraylist_background) {
                ImDrawFlags corners = isLeftSide ? ImDrawFlags_RoundCornersBottomRight : ImDrawFlags_RoundCornersBottomLeft;

                // Draw the pre-calculated blur texture for this item's area
                if (gui_arraylist_blur)
                    GLBlur::DrawBlurredImage(dl, iMin, iMax, anim, rounding, corners);

                // Semi-transparent tinted overlay on top of the blur
                ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    gui_arraylist_bg_color_4[0],
                    gui_arraylist_bg_color_4[1],
                    gui_arraylist_bg_color_4[2],
                    gui_arraylist_bg_color_4[3] * anim));
                dl->AddRectFilled(iMin, iMax, bgCol, rounding, corners);
            }

            // Color bar on outer edge
            if (gui_arraylist_colorbar) {
                if (isLeftSide) {
                    ImVec2 bMin(iMin.x, iMin.y);
                    ImVec2 bMax(iMin.x + barW, iMax.y);
                    dl->AddRectFilled(bMin, bMax, itemCol);
                } else {
                    ImVec2 bMin(iMax.x - barW, iMin.y);
                    ImVec2 bMax(iMax.x, iMax.y);
                    dl->AddRectFilled(bMin, bMax, itemCol);
                }
            }

            // Text position
            float tx = std::round(iMin.x + padX + (isLeftSide && gui_arraylist_colorbar ? barW : 0.0f));
            float ty = std::round(boxY + padY);

            // Module name with text shadow
            if (gui_arraylist_shadows) {
                dl->AddText(arrayFont, fontSize, ImVec2(tx + 1, ty + 1), IM_COL32(0, 0, 0, (int)(220 * anim)), item.name.c_str());
            }
            dl->AddText(arrayFont, fontSize, ImVec2(tx, ty), itemCol, item.name.c_str());

            // Flag text in dimmer color (static alpha)
            if (!item.flags.empty()) {
                float fx = std::round(tx + item.nameSz.x + spaceW);
                float flagAlpha = 1.f * anim;
                ImU32 flagCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                    gui_arraylist_info_color[0],
                    gui_arraylist_info_color[1],
                    gui_arraylist_info_color[2],
                    flagAlpha));
                if (gui_arraylist_shadows) {
                    dl->AddText(arrayFont, fontSize, ImVec2(fx + 1, ty + 1), IM_COL32(0, 0, 0, (int)(180 * anim)), item.flags.c_str());
                }
                dl->AddText(arrayFont, fontSize, ImVec2(fx, ty), flagCol, item.flags.c_str());
            }

            currentY += itemH * anim;
        }

        // --- hold to move (only when menu is open) ---
        if (g_MenuVisible && !g_AltManagerMode && !activeList.empty()) {
            float listTop = gui_arraylist_pos_y * sh;
            float listW = activeList[0].totalW; // widest item
            float listH = currentY - listTop;
            float listLeft, listRight;
            if (isLeftSide) {
                listLeft = anchorX;
                listRight = anchorX + listW;
            } else {
                listRight = anchorX;
                listLeft = anchorX - listW;
            }

            ImVec2 dragMin(listLeft - 8.0f, listTop - 8.0f);
            ImVec2 dragMax(listRight + 8.0f, listTop + listH + 8.0f);

            const ImVec2 mouse = ImGui::GetMousePos();
            const bool down = ImGui::IsMouseDown(0);
            const bool clicked = ImGui::IsMouseClicked(0);
            const bool inBox = mouse.x >= dragMin.x && mouse.x <= dragMax.x && mouse.y >= dragMin.y && mouse.y <= dragMax.y;

            if (clicked && inBox) {
                alDragging = true;
                alDragOff = ImVec2(anchorX - mouse.x, listTop - mouse.y);
            }
            if (!down) alDragging = false;

            static float alDragAnim = 0.0f;
            alDragAnim = ImLerp(alDragAnim, alDragging ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 15.0f);

            if (alDragging && down) {
                float newX = mouse.x + alDragOff.x;
                float newY = mouse.y + alDragOff.y;

                float centerX = isLeftSide ? (newX + listW * 0.5f) : (newX - listW * 0.5f);
                float centerY = newY + listH * 0.5f;

                float snapPointsX[] = { sw * 0.3333f, sw * 0.5f, sw * 0.6666f };
                float snapPointsY[] = { sh * 0.3333f, sh * 0.5f, sh * 0.6666f };

                for (float sp : snapPointsX) {
                    if (std::abs(centerX - sp) < 15.0f) { centerX = sp; newX = isLeftSide ? (centerX - listW * 0.5f) : (centerX + listW * 0.5f); break; }
                }
                for (float sp : snapPointsY) {
                    if (std::abs(centerY - sp) < 15.0f) { centerY = sp; newY = centerY - listH * 0.5f; break; }
                }

                gui_arraylist_pos_x = newX / sw;
                gui_arraylist_pos_y = newY / sh;
                gui_arraylist_pos_x = (std::max)(0.0f, (std::min)(gui_arraylist_pos_x, 1.0f));
                gui_arraylist_pos_y = (std::max)(0.0f, (std::min)(gui_arraylist_pos_y, 0.95f));

                for (float sp : snapPointsX) {
                    bool isSnapped = std::abs(centerX - sp) < 1.0f;
                    int alpha = (int)(80 * alDragAnim);
                    if (isSnapped) alpha = (int)(255 * alDragAnim);
                    dl->AddLine(ImVec2(sp, 0), ImVec2(sp, sh), IM_COL32(255, 255, 255, alpha), isSnapped ? 2.0f : 1.0f);
                }
                for (float sp : snapPointsY) {
                    bool isSnapped = std::abs(centerY - sp) < 1.0f;
                    int alpha = (int)(80 * alDragAnim);
                    if (isSnapped) alpha = (int)(255 * alDragAnim);
                    dl->AddLine(ImVec2(0, sp), ImVec2(sw, sp), IM_COL32(255, 255, 255, alpha), isSnapped ? 2.0f : 1.0f);
                }
            }

            if (alDragAnim > 0.01f) {
                dl->AddRect(ImVec2(listLeft - 2.0f * alDragAnim, listTop - 2.0f * alDragAnim), ImVec2(listRight + 2.0f * alDragAnim, listTop + listH + 2.0f * alDragAnim), IM_COL32(255, 255, 255, (int)(100 * alDragAnim)), 5.0f, 0, 2.0f);
            }

            // "hold to move" hint text
            const char* hint = "hold to move";
            float hintSz = 11.5f * scale;
            ImVec2 hintExt = arrayFont->CalcTextSizeA(hintSz, FLT_MAX, 0.0f, hint);
            float hintX = isLeftSide ? listLeft : (listRight - hintExt.x);
            float hintY = listTop + listH + 4.0f;
            dl->AddText(arrayFont, hintSz, ImVec2(hintX + 1, hintY + 1), IM_COL32(0, 0, 0, 200), hint);
            dl->AddText(arrayFont, hintSz, ImVec2(hintX, hintY), IM_COL32(200, 200, 210, 220), hint);
        }
    }

    // ----------------------------------------------------------------
    // Sistema toast (sin cambios)
    // ----------------------------------------------------------------
    static void init_watch()
    {
        static const struct { bool* ptr; const char* name; } entries[] = {
            { &features::combat::auto_click::enabled,     "AutoClick"     },
            { &features::combat::aim_assist::enabled,     "Aim Assist"    },
            { &features::combat::reach::enabled,          "Reach"         },
            { &features::combat::velocity::enabled,       "Velocity"      },
            { &features::combat::refill::enabled,         "Refill"        },
            { &gui_nohitdelay_enabled,                    "NoHitDelay"    },
            { &gui_blockhit_enabled,                      "BlockHit"      },
            { &gui_friends_enabled,                       "Friends"       },
            { &features::movement::sprint::enabled,       "Sprint"        },
            { &features::movement::no_jump_delay::enabled,"NoJumpDelay"   },
            { &gui_fastplace_enabled,                     "FastPlace"     },
            { &gui_esp_enabled,                           "ESP"           },
            { &features::visual::nametags::enabled,       "Nametags"      },
            { &features::visual::tracers::enabled,        "Tracers"       },
            { &features::visual::hit_markers::enabled,    "Hit Markers"   },
            { &features::visual::arraylist::enabled,      "ArrayList"     },
            { &features::misc::blink::enabled,            "Blink"         },
            { &gui_autotool_enabled,                      "AutoTool"      },
            { &gui_autoarmor_enabled,                     "AutoArmor"     },
            { &gui_macros_enabled,                        "Macros"        },
            { &gui_armorswitcher_enabled,                 "ArmorSwitcher" },
        };
        int n = (int)(sizeof(entries) / sizeof(entries[0]));
        g_watch.resize(n);
        for (int i = 0; i < n; i++)
            g_watch[i] = { entries[i].ptr, *entries[i].ptr, entries[i].name };
        g_watch_init = true;
    }

    static void check_and_push(ImFont* font, float fsz)
    {
        if (!g_watch_init) { init_watch(); return; }
        
        if (::gui_config_just_loaded) {
            for (auto& w : g_watch) w.prev = *w.ptr;
            ::gui_config_just_loaded = false;
            return;
        }

        for (auto& w : g_watch) {
            w.prev = *w.ptr;
        }
    }

    void PushToastEvent(const std::string& txt, bool st, int kb = 0)
    {
        Toast t;
        t.text = txt;
        t.enabled_state = st;
        t.keybind = kb;
        t.alpha = 0.f;
        t.slide = 0.f;
        t.life = 2.6f;
        t.text_w = 140.f;
        if (g_toasts.size() >= 20) g_toasts.erase(g_toasts.begin());
        g_toasts.push_back(t);
    }

    static void render_toasts(ImDrawList* dl, ImFont* font, float sw, float sh, float dt)
    {
        float fsz_title = 18.f;
        float fsz_state = 16.f;
        float pad_x = 18.f;
        float pad_y = 13.f;
        float margin_x = 20.f;
        float margin_y = 20.f;
        float spacing = 8.f;
        float rounding = 6.f;

        if (g_system_toast_height > 0.f)
            margin_y = g_system_toast_height;

        int pos_mode = features::misc::notifications::position;
        bool isTop = (pos_mode == 0 || pos_mode == 1);
        bool isLeft = (pos_mode == 0 || pos_mode == 2);

        float line_h1 = font->CalcTextSizeA(fsz_title, FLT_MAX, 0.f, "A").y;
        float line_h2 = font->CalcTextSizeA(fsz_state, FLT_MAX, 0.f, "A").y;
        float H_box = pad_y * 2.f + line_h1 + line_h2 + 6.f;

        check_and_push(font, fsz_title);

        float curTargetY = isTop ? margin_y : (sh - margin_y);

        for (int i = (int)g_toasts.size() - 1; i >= 0; i--) {
            Toast& t = g_toasts[i];
            t.life -= dt;

            float want = (t.life > 0.35f) ? 1.f : 0.f;
            
            float slide_speed = 18.0f;
            float alpha_speed = 15.0f;
            t.slide = want + (t.slide - want) * std::exp(-slide_speed * dt);
            t.alpha = want + (t.alpha - want) * std::exp(-alpha_speed * dt);

            if (t.life <= 0.f && t.alpha < 0.01f) {
                g_toasts.erase(g_toasts.begin() + i);
                continue;
            }

            float a = t.alpha;
            if (a < 0.01f) continue;

            std::string top_text = t.text;
            std::string body_text = t.enabled_state ? "Enabled" : "Disabled";

            ImVec2 title_sz = font->CalcTextSizeA(fsz_title, FLT_MAX, 0.f, top_text.c_str());
            ImVec2 body_sz = font->CalcTextSizeA(fsz_state, FLT_MAX, 0.f, body_text.c_str());
            float max_text_w = (title_sz.x > body_sz.x) ? title_sz.x : body_sz.x;

            float box_w = 280.f;
            if (max_text_w + (pad_x * 2.f) > box_w) box_w = max_text_w + (pad_x * 2.f);

            float H_total = H_box + 12.0f; // box + gap + bar

            if (isTop) {
                if (t.slideY == 0.0f) t.slideY = curTargetY - 20.f;
                float y_speed = 25.0f;
                t.slideY = curTargetY + (t.slideY - curTargetY) * std::exp(-y_speed * dt);
                curTargetY += H_total + spacing;
            } else {
                curTargetY -= H_total;
                if (t.slideY == 0.0f) t.slideY = curTargetY + 20.f;
                float y_speed = 25.0f;
                t.slideY = curTargetY + (t.slideY - curTargetY) * std::exp(-y_speed * dt);
                curTargetY -= spacing;
            }

            float slide_off = (1.f - t.slide) * (box_w + margin_x + 4.f);
            float bx = isLeft ? (margin_x - slide_off) : (sw - margin_x - box_w + slide_off);
            float by = t.slideY;

            ImVec2 bmin = { bx, by };
            ImVec2 bmax = { bx + box_w, by + H_box };

            dl->AddRectFilled(bmin, bmax,
                IM_COL32(15, 15, 18, (int)(255 * a)), rounding);


            // 3. Text + icon rendering
            float tx = bx + pad_x;
            float ty1 = by + pad_y - 4.f;
            float ty2 = ty1 + line_h1 + 2.f;

            // Draw keybind icon next to module name if it was a keybind toggle
            if (font::default_icon && t.keybind > 0) {
                const char* kbIcon = "\xEF\x84\x9C"; // fa-keyboard
                ImVec2 iconSz = font::default_icon->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, kbIcon);
                dl->AddText(font::default_icon, 14.0f, ImVec2(tx, ty1 + 1.0f), IM_COL32(160, 160, 170, (int)(255 * a)), kbIcon);
                tx += iconSz.x + 6.0f;
            }

            ImU32 title_col = ImGui::ColorConvertFloat4ToU32({ 1.0f, 1.0f, 1.0f, a });
            ImU32 body_col = t.enabled_state
                ? ImGui::ColorConvertFloat4ToU32({ 0.45f, 0.85f, 0.50f, a })
                : ImGui::ColorConvertFloat4ToU32({ 0.85f, 0.45f, 0.50f, a });
            
            dl->AddText(font, fsz_title, { tx, ty1 }, title_col, top_text.c_str());
            dl->AddText(font, fsz_state, { tx, ty2 }, body_col, body_text.c_str());

            float progress = t.life / 2.6f;
            if (progress < 0.f) progress = 0.f;
            if (progress > 1.f) progress = 1.f;
            float barY = bmax.y + 6.0f;
            float barMinX = bmin.x + 4.0f;
            float barMaxX = bmax.x - 4.0f;
            float barFullW = barMaxX - barMinX;
            float prog_w = barFullW * progress;
            // Bar track
            dl->AddRectFilled(ImVec2(barMinX, barY), ImVec2(barMaxX, barY + 6.0f),
                IM_COL32(35, 35, 40, (int)(180 * a)), 3.0f);
            if (prog_w > 0.01f) {
                float* bc = features::misc::notifications::bar_color;
                ImU32 prog_color = IM_COL32((int)(bc[0]*255), (int)(bc[1]*255), (int)(bc[2]*255), (int)(255 * a));
                dl->AddRectFilled(ImVec2(barMinX, barY), ImVec2(barMinX + prog_w, barY + 6.0f),
                    prog_color, 3.0f);
            }

        }
    }
}

void PushModuleToast(const std::string& name, bool state, int keybind = 0) {
    features::visual::arraylist::PushToastEvent(name, state, keybind);
}
