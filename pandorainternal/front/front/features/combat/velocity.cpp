#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <random>

extern int gui_velo_mode;
extern float gui_velo_lag_ms;

namespace features::combat::velocity
{
    static std::mt19937 s_rng(std::random_device{}());

    // Variables de la Máquina de Estados (Estilo Drip Lite)
    static bool s_should_apply = false;
    static int  s_target_tick = 10;
    static int  s_last_hurt = 0;

    // === LAG MODE STATE ===
    bool       lag_esp_active = false;       // true while self-ESP should show
    ULONGLONG  lag_spike_time = 0;           // timestamp of last lag spike
    static int s_lag_last_hurt = 0;

    // ============================================================
    // DEFAULT MODE (mode 0) — Comportamiento original sin cambios
    // ============================================================
    static void run_default(mapper::__minecraft& minecraft)
    {
        if (minecraft.get_current_screen().object != nullptr) return;

        // 1. Clic solo (Clicking Only)
        if (clicking_only && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

        JNIEnv* env = sdk::jni;
        if (!env) return;
        if (env->ExceptionCheck()) env->ExceptionClear();

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        // 2. Solo armas (Weapons Only)
        if (weapon_only) {
            auto hs = local_player.get_held_item_stack();
            if (!hs.object) return;
            auto it = hs.get_item();
            if (!it.object) return;
            if (!it.is_sword() && !it.is_axe() && !it.is_pickaxe() && !it.is_shovel()) return;
        }

        // --- Anti-Log: Solo aplicar velocity si estamos sprinteando (en combate real) ---
        bool is_sprinting = local_player.get_flag(3);
        if (!is_sprinting) return;

        int hurt = local_player.get_hurt_time();
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

        // LÓGICA DE DRIP LITE
        if (hurt == 10 && s_last_hurt != 10)
        {
            std::uniform_real_distribution<float> dist(0.0f, 100.0f);
            if (dist(s_rng) <= chance)
            {
                s_should_apply = true;
                int safe_delay = std::clamp((int)std::round(delay), 0, 9);
                s_target_tick = 10 - safe_delay;
            }
            else
            {
                s_should_apply = false;
            }
        }

        s_last_hurt = hurt;

        // APLICACIÓN DEL VELOCITY
        bool is_valid_tick = false;
        if (universocraft_bypass) {
            is_valid_tick = (hurt == 9);
        } else {
            is_valid_tick = (hurt == s_target_tick);
        }

        if (s_should_apply && is_valid_tick)
        {
            jclass player_class = env->GetObjectClass(local_player.object);
            if (!player_class) return;

            // 3. Solo en el Aire (Air Only)
            if (air_only)
            {
                jfieldID onGround_fid = env->GetFieldID(player_class, "onGround", "Z");
                if (!onGround_fid) { env->ExceptionClear(); onGround_fid = env->GetFieldID(player_class, "field_70122_E", "Z"); }
                if (!onGround_fid) { env->ExceptionClear(); onGround_fid = env->GetFieldID(player_class, "C", "Z"); }

                if (onGround_fid) {
                    bool on_ground = env->GetBooleanField(local_player.object, onGround_fid);
                    if (on_ground) {
                        env->DeleteLocalRef(player_class);
                        s_should_apply = false;
                        return;
                    }
                }
            }

            // 4. Solo en movimiento (Moving Only)
            if (moving_only)
            {
                jfieldID moveForward_fid = env->GetFieldID(player_class, "moveForward", "F");
                if (!moveForward_fid) { env->ExceptionClear(); moveForward_fid = env->GetFieldID(player_class, "field_70701_bs", "F"); }
                if (!moveForward_fid) { env->ExceptionClear(); moveForward_fid = env->GetFieldID(player_class, "ba", "F"); }
                if (!moveForward_fid) { env->ExceptionClear(); moveForward_fid = env->GetFieldID(player_class, "be", "F"); }

                jfieldID moveStrafing_fid = env->GetFieldID(player_class, "moveStrafing", "F");
                if (!moveStrafing_fid) { env->ExceptionClear(); moveStrafing_fid = env->GetFieldID(player_class, "field_70702_br", "F"); }
                if (!moveStrafing_fid) { env->ExceptionClear(); moveStrafing_fid = env->GetFieldID(player_class, "aZ", "F"); }
                if (!moveStrafing_fid) { env->ExceptionClear(); moveStrafing_fid = env->GetFieldID(player_class, "bd", "F"); }

                bool is_moving = false;
                if (moveForward_fid && moveStrafing_fid) {
                    float mf = env->GetFloatField(local_player.object, moveForward_fid);
                    float ms = env->GetFloatField(local_player.object, moveStrafing_fid);
                    if (std::abs(mf) > 0.01f || std::abs(ms) > 0.01f) {
                        is_moving = true;
                    }
                }

                if (!is_moving) {
                    env->DeleteLocalRef(player_class);
                    s_should_apply = false;
                    return;
                }
            }
            env->DeleteLocalRef(player_class);

            // 5. Aplicar la reducción matemática
            auto motion = local_player.get_motion();

            double hm = (double)horizontal / 100.0;
            double vm = universocraft_bypass ? 1.0 : ((double)vertical / 100.0);
            double dir = push_back ? -1.0 : 1.0;

            local_player.set_motion({
                (float)(motion.x * hm * dir),
                (float)(motion.y * vm),
                (float)(motion.z * hm * dir)
                });

            s_should_apply = false;
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ============================================================
    // MODO 1: Jump Reset (Kite Velocity)
    // En vez de congelar el juego con Sleep() (que arruina los FPS),
    // este modo detecta el tick exacto del daño y emula un salto 
    // fisico (Jump Reset). 
    // En Minecraft 1.8.9, saltar al recibir daño anula gran parte
    // del knockback horizontal y te permite hacer combos (Kiting)
    // de forma 100% Legit y sin que el Anticheat lo note.
    // ============================================================
    static void run_jump_reset(mapper::__minecraft& minecraft)
    {
        ULONGLONG now = GetTickCount64();

        // Mantenemos el efecto visual (Self-ESP al mitigar)
        if (lag_esp_active && (now - lag_spike_time > 500))
            lag_esp_active = false;

        if (minecraft.get_current_screen().object != nullptr) {
            lag_esp_active = false;
            return;
        }

        JNIEnv* env = sdk::jni;
        if (!env) return;
        if (env->ExceptionCheck()) env->ExceptionClear();

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        // Solo armas
        if (weapon_only) {
            auto hs = local_player.get_held_item_stack();
            if (!hs.object) return;
            auto it = hs.get_item();
            if (!it.object) return;
            if (!it.is_sword() && !it.is_axe() && !it.is_pickaxe() && !it.is_shovel()) return;
        }

        // Solo clickeando (en combate activo)
        if (clicking_only && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

        int hurt = local_player.get_hurt_time();
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }

        // Detectar golpe nuevo (transicion a hurt_time == 10)
        if (hurt == 10 && s_lag_last_hurt != 10)
        {
            // Chance check
            std::uniform_real_distribution<float> dist(0.0f, 100.0f);
            if (dist(s_rng) <= chance)
            {
                // Solo podemos hacer Jump Reset si estamos tocando el piso
                jclass player_class = env->GetObjectClass(local_player.object);
                if (player_class) {
                    jfieldID onGround_fid = env->GetFieldID(player_class, "onGround", "Z");
                    if (!onGround_fid) { env->ExceptionClear(); onGround_fid = env->GetFieldID(player_class, "field_70122_E", "Z"); }
                    if (!onGround_fid) { env->ExceptionClear(); onGround_fid = env->GetFieldID(player_class, "C", "Z"); }

                    bool on_ground = false;
                    if (onGround_fid) {
                        on_ground = env->GetBooleanField(local_player.object, onGround_fid);
                    }
                    env->DeleteLocalRef(player_class);

                    if (on_ground) {
                        lag_esp_active = true;
                        lag_spike_time = now;

                        // Emulamos el salto fisico presionando espacio por un instante
                        keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, 0), 0, 0);
                        
                        // Never leave detached code executing inside the DLL:
                        // an unload while that lambda sleeps would jump into
                        // unmapped memory. The short pulse is safe inline.
                        Sleep(30);
                        keybd_event(VK_SPACE, MapVirtualKey(VK_SPACE, 0), KEYEVENTF_KEYUP, 0);
                    }
                }
            }
        }
        s_lag_last_hurt = hurt;

        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    // ============================================================
    // ENTRY POINT — Despacha al modo correcto
    // ============================================================
    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled) return;

        // Sincronizar mode desde la variable del menu
        mode = gui_velo_mode;

        switch (mode)
        {
        case 1:  run_jump_reset(minecraft); break;
        default: run_default(minecraft);    break;
        }
    }
}
