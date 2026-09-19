#include "../features.hpp"
#include "sdk.hpp"
#define NOMINMAX
#include <windows.h>
extern bool g_MenuVisible;
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <random>
#include <chrono>

// Compilado fuera: evita preparar mensajes de diagnostico en el bucle de targets.
#define aim_debug(...) ((void)0)

static float get_precise_time_ms() {
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<float, std::milli>(now - start_time).count();
}

// Vinculamos las variables del menÃº
extern bool gui_friends_enabled;

// Notification system export
extern void TriggerNotification(const char* title, const char* body = "", const char* tag = "SYSTEM");

namespace features::friends {
    extern std::vector<std::string>* list;
}

namespace features::combat::aim_assist
{
    static ULONGLONG last_click_time = 0;
    static std::mt19937 s_rng(std::random_device{}());

    // Variables para la adherencia de objetivo (Stickiness)
    static mapper::__vec3 s_last_target_pos = { 0, 0, 0 };
    static bool s_has_target = false;

    // --- AIM-LOCK: Entity ID del jugador lockeado ---
    static int  s_locked_entity_id = -1;
    static bool s_lock_active = false;
    // Helper: leer entityId directo del campo int (instantÃ¡neo, sin excepciones)
    static int get_entity_id(jobject entity) {
        if (entity == nullptr || sdk::jni == nullptr) return -1;

        jclass cls = sdk::jni->GetObjectClass(entity);
        if (cls == nullptr) return -1;

        // Buscar entityId en la clase real del objeto (hereda de Entity)
        jfieldID fid = sdk::jni->GetFieldID(cls, "entityId", "I");
        if (fid == nullptr) { sdk::jni->ExceptionClear(); fid = sdk::jni->GetFieldID(cls, "field_70157_k", "I"); }
        if (fid == nullptr) { sdk::jni->ExceptionClear(); fid = sdk::jni->GetFieldID(cls, "d", "I"); }

        if (fid == nullptr) {
            sdk::jni->ExceptionClear();
            sdk::jni->DeleteLocalRef(cls);
            return -1;
        }

        int id = sdk::jni->GetIntField(entity, fid);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); id = -1; }
        sdk::jni->DeleteLocalRef(cls);
        return id;
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (::g_MenuVisible) return;

        bool is_clicking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (is_clicking) last_click_time = GetTickCount64();

        // ============================================================
        // SISTEMA DE TEAMS
        // ============================================================
        // Friend key handling is centralized in features::friends::run().

        // ============================================================
        // RESET: Para Aim-Lock, solo resetear cuando SUELTAS el click
        // Para otros modos, usar el timer de 1ms
        // ============================================================
        bool should_deactivate = false;
        if (mode == 1) {
            // Aim-Lock: solo resetear si el botón NO está presionado
            should_deactivate = !is_clicking;
        }
        else {
            should_deactivate = (GetTickCount64() - last_click_time > 1);
        }

        if (!enabled || should_deactivate || minecraft.get_current_screen().object != nullptr) {
            static int s_skip_log = 0;
            if (s_skip_log++ < 5) aim_debug("[AA] SKIP: enabled=%d deact=%d screen=%d clicking=%d", enabled, should_deactivate, minecraft.get_current_screen().object != nullptr, is_clicking);
            s_has_target = false;
            angle_x_changes = 0.f;
            angle_y_changes = 0.f;
            s_locked_entity_id = -1;
            s_lock_active = false;
            return;
        }

        if (sdk::jni != nullptr && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        auto timer = minecraft.get_timer();
        if (timer.object == nullptr) return;
        float timer_partial_ticks = timer.get_partial_ticks();

        auto world = minecraft.get_world();
        auto local_player = minecraft.get_local_player();

        if (world.object == nullptr || local_player.object == nullptr) {
            aim_debug("[AA] world=%p local=%p - EXIT", world.object, local_player.object);
            s_has_target = false;
            angle_x_changes = 0.f;
            angle_y_changes = 0.f;
            s_locked_entity_id = -1;
            s_lock_active = false;
            return;
        }
        aim_debug("[AA] ACTIVE: world=%p local=%p weapons=%d axe=%d mode=%d", world.object, local_player.object, weapons_only, axe_only, mode);

        if (weapons_only || axe_only || food_only) {
            auto held_stack = local_player.get_held_item_stack();
            if (held_stack.object == nullptr) {
                if (sdk::jni != nullptr && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                angle_x_changes = 0.f; angle_y_changes = 0.f; s_lock_active = false; s_locked_entity_id = -1;
                return;
            }
            auto item = held_stack.get_item();
            if (item.object == nullptr) {
                if (sdk::jni != nullptr && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                angle_x_changes = 0.f; angle_y_changes = 0.f; s_lock_active = false; s_locked_entity_id = -1;
                return;
            }

            bool is_sword = item.is_sword();
            bool is_axe = item.is_axe();
            bool is_food = item.is_food() || item.is_potion() || item.is_soup();
            bool valid_item = false;

            if (weapons_only && is_sword) valid_item = true;
            if (axe_only && is_axe) valid_item = true;
            if (food_only && is_food) valid_item = true;

            if (!valid_item) {
                if (sdk::jni != nullptr && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                angle_x_changes = 0.f; angle_y_changes = 0.f; s_lock_active = false; s_locked_entity_id = -1;
                return;
            }
        }



        if (break_blocks) {
            auto object_mouse_over = minecraft.get_object_mouse_over();
            if (object_mouse_over.object != nullptr && object_mouse_over.get_type_of_hit() == 1) {
                s_has_target = false;
                return;
            }
        }

        mapper::__vec2 local_player_view_angles = local_player.get_view_angles();
        mapper::__vec3 local_player_view_position = local_player.get_view_position(timer_partial_ticks);
        local_player_view_position.y += 1.62f;

        float best_score = FLT_MAX;
        mapper::__vec2 closest_entity_diff = { FLT_MAX, FLT_MAX };
        jobject best_target = nullptr;
        mapper::__vec3 new_target_pos = { 0, 0, 0 };
        int best_entity_id = -1;  // Entity ID leÃ­do dentro del loop

        // ============================================================
        // AIM-LOCK: BÃºsqueda dedicada por Entity ID
        // Si tenemos un ID lockeado, SOLO buscamos esa entidad.
        // Ignora TODOS los filtros (HP, distancia, FOV, visibilidad).
        // ============================================================
        bool lock_found_this_tick = false;

        if (mode == 1 && s_lock_active && s_locked_entity_id >= 0)
        {
            for (auto& player : world.get_players())
            {
                if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                if (sdk::jni->IsSameObject(local_player.object, player.object)) continue;

                int eid = get_entity_id(player.object);
                if (eid < 0 || eid != s_locked_entity_id) continue;

                if (features::friends::is_teammate(player, local_player)) {
                    s_locked_entity_id = -1;
                    s_lock_active = false;
                    s_has_target = false;
                    angle_x_changes = 0.f;
                    angle_y_changes = 0.f;
                    return;
                }

                mapper::__vec3 p_pos = player.get_view_position(timer_partial_ticks);
                double dist_to = local_player_view_position.get_distance_to_vec3(p_pos);
                if (dist_to > distance) {
                    s_locked_entity_id = -1;
                    s_lock_active = false;
                    s_has_target = false;
                    angle_x_changes = 0.f;
                    angle_y_changes = 0.f;
                    return; // Break lock if out of range
                }

                // ¡Encontrado y en rango! Calcular ángulos
                float diff_x = local_player_view_position.get_angle_x_difference_to_vec3(p_pos, local_player_view_angles.x);

                closest_entity_diff = { diff_x, 0.0f };
                best_target = player.object;
                new_target_pos = p_pos;
                lock_found_this_tick = true;
                break; // Solo hay uno con ese ID
            }

            // Si no lo encontramos (se fue del server/muriÃ³), mantenemos el lock
            // pero no hacemos nada este tick. NO reseteamos el lock.
            if (!lock_found_this_tick) {
                angle_x_changes = 0.f;
                angle_y_changes = 0.f;
                if (sdk::jni != nullptr && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                return;
            }
        }

        // ============================================================
        // BÃšSQUEDA NORMAL (solo cuando NO hay lock activo)
        // ============================================================

        float actual_dist = (float)distance;
        float actual_fov = (float)fov;

        if (!lock_found_this_tick)
        {
            auto players_list = world.get_players();
            aim_debug("[AA] SEARCH: %d players, dist=%.1f fov=%.1f", (int)players_list.size(), actual_dist, actual_fov);
            for (auto& player : players_list)
            {
                if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                if (sdk::jni->IsSameObject(local_player.object, player.object)) continue;

                float hp = player.get_health();
                if (!std::isfinite(hp) || hp <= 0.f) { aim_debug("[AA]   player hp=%.1f SKIP", hp); continue; }

                mapper::__vec3 p_pos = player.get_view_position(timer_partial_ticks);

                double dist_to = local_player_view_position.get_distance_to_vec3(p_pos);
                if (dist_to > actual_dist) { aim_debug("[AA]   player hp=%.1f dist=%.1f > %.1f SKIP", hp, dist_to, actual_dist); continue; }
                if (ignore_walls && !local_player.can_entity_be_seen(player)) { aim_debug("[AA]   player WALL SKIP"); continue; }
                if (ignore_invisible && player.is_invisible()) { aim_debug("[AA]   player INVISIBLE SKIP"); continue; }
                if (features::friends::is_teammate(player, local_player)) continue;

                float diff_x = local_player_view_position.get_angle_x_difference_to_vec3(p_pos, local_player_view_angles.x);
                float diff_y = 0.0f;

                // En el modo Head (2), calculamos tambiÃ©n la diferencia en Y (Pitch) hacia la cabeza
                if (mode == 2) {
                    mapper::__vec3 head_pos = p_pos;
                    head_pos.y += 1.55f; // Altura a los ojos de un jugador normal
                    diff_y = local_player_view_position.get_angle_y_difference_to_vec3(head_pos, local_player_view_angles.y);
                }

                float score = std::abs(diff_x);

                if (s_has_target) {
                    double dist_to_last = p_pos.get_distance_to_vec3(s_last_target_pos);
                    if (dist_to_last < 2.5) {
                        score *= 0.001f;
                    }
                }

                if (score < best_score)
                {
                    best_score = score;
                    closest_entity_diff = { diff_x, diff_y };
                    best_target = player.object;
                    new_target_pos = p_pos;
                    // Leer entity ID AQUÃ  donde player.object aÃºn estÃ¡ vivo
                    if (mode == 1) best_entity_id = get_entity_id(player.object);
                }
            }
        }

        // ============================================================
        // APLICAR MOVIMIENTO
        // ============================================================
        if (best_target != nullptr && closest_entity_diff.x != FLT_MAX)
        {
            s_has_target = true;
            s_last_target_pos = new_target_pos;

            // --- AIM-LOCK: Guardar Entity ID la primera vez ---
            if (mode == 1 && !s_lock_active && best_entity_id >= 0) {
                s_locked_entity_id = best_entity_id;
                s_lock_active = true;
            }

            float move_x = 0.0f;
            float move_y = 0.0f;

            switch (mode)
            {
            case 0: // ===== SMOOTH =====
            {
                float safe_speed = std::clamp((float)speed, 1.0f, 100.0f);

                float base_smooth = std::pow(safe_speed, 1.5f) / 10000.0f;
                float current_time = get_precise_time_ms();
                float time_wave = std::sin(current_time * 0.003f) * 0.10f;
                float final_smooth = base_smooth * (1.0f + time_wave);

                // Offset sinusoidal fluido (el original)
                float offset_x = std::sin(current_time * 0.0015f) * 1.2f;
                float target_diff_x = closest_entity_diff.x + offset_x;

                float abs_dist = std::abs(target_diff_x);
                if (abs_dist < 2.0f) {
                    final_smooth *= std::clamp(abs_dist / 2.0f, 0.1f, 1.0f);
                }

                move_x = target_diff_x * final_smooth;
                move_y = closest_entity_diff.y * final_smooth;

                float max_turn = 0.5f + (safe_speed / 20.0f);
                move_x = std::clamp(move_x, -max_turn, max_turn);
                move_y = std::clamp(move_y, -max_turn, max_turn);
                break;
            }

            case 1: // ===== AIM-LOCK =====
            {
                float strength = lock_strength / 100.0f;

                float pull = strength * 0.35f;
                move_x = closest_entity_diff.x * pull;
                move_y = closest_entity_diff.y * pull * 0.4f;

                std::uniform_real_distribution<float> jitter_dist(-0.1f, 0.1f);
                move_x += jitter_dist(s_rng);

                float max_turn = 2.0f + (strength * 3.0f);
                move_x = std::clamp(move_x, -max_turn, max_turn);
                move_y = std::clamp(move_y, -max_turn * 0.5f, max_turn * 0.5f);
                break;
            }

            case 2: // ===== VERTICAL (Head) =====
                // Modo dedicado a apuntar directamente a la cabeza del oponente.
                // Combina un seguimiento horizontal fluido con una correcciÃ³n vertical precisa.
            {
                float target_diff_x = closest_entity_diff.x;
                float target_diff_y = closest_entity_diff.y; // Ya estÃ¡ calculado hacia la cabeza en este modo

                float strength = flick_strength / 100.0f; // 0.0 a 1.0

                // Suavizado dinÃ¡mico segÃºn la distancia (mÃ¡s cerca = mÃ¡s rÃ¡pido)
                float abs_diff_x = std::abs(target_diff_x);
                float abs_diff_y = std::abs(target_diff_y);

                // Smooth interpolation — softer pull, never snaps
                float current_time = get_precise_time_ms();
                float time_wave = std::sin(current_time * 0.002f) * 0.08f;
                float adaptive_speed_x = strength * 0.15f * (1.0f + time_wave);
                float adaptive_speed_y = strength * 0.20f * (1.0f + time_wave);

                // Slow down as we get closer (prevents overshoot/jitter at target)
                if (abs_diff_x < 3.0f) adaptive_speed_x *= std::clamp(abs_diff_x / 3.0f, 0.15f, 1.0f);
                if (abs_diff_y < 3.0f) adaptive_speed_y *= std::clamp(abs_diff_y / 3.0f, 0.15f, 1.0f);

                move_x = target_diff_x * adaptive_speed_x;
                move_y = target_diff_y * adaptive_speed_y;

                // PequeÃ±o jitter orgÃ¡nico para no ser 100% robÃ³tico
                std::uniform_real_distribution<float> jitter(-0.05f, 0.05f);
                move_x += jitter(s_rng);
                move_y += jitter(s_rng) * 0.5f;

                float max_turn = 0.4f + (strength * 1.2f);
                move_x = std::clamp(move_x, -max_turn, max_turn);
                move_y = std::clamp(move_y, -max_turn * 0.6f, max_turn * 0.6f);

                break;
            }

            default:
                break;
            }

            // Aplicar rotaciÃ³n
            if (std::abs(move_x) > 0.01f || std::abs(move_y) > 0.01f)
            {
                if (silent)
                {
                    angle_x_changes = move_x;
                    angle_y_changes = move_y;
                }
                else
                {
                    angle_x_changes = 0.f;
                    angle_y_changes = 0.f;
                    local_player.set_old_view_angles(local_player_view_angles);
                    local_player.set_view_angles({
                        local_player_view_angles.x + move_x,
                        local_player_view_angles.y + move_y
                        });
                }
            }
            else
            {
                angle_x_changes = 0.f;
                angle_y_changes = 0.f;
            }
        }
        else
        {
            // No se encontrÃ³ objetivo en bÃºsqueda normal
            // (El aim-lock ya retornÃ³ arriba si no encontrÃ³ su target)
            s_has_target = false;
            angle_x_changes = 0.f;
            angle_y_changes = 0.f;
        }

        if (sdk::jni != nullptr && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    }
}
