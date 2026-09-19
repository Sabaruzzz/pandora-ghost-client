#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>
#include <random>
#include <vector>
#include <string>

// ArmorSwitcher V4 - Estilo Video + Refill (E key + shift-click + swap)
// ==============================================================
// Se activa al PRENDER EL TOGGLE (igual que Refill) o al presionar bind.
// Flujo:
//   1. Abre inventario con tecla E (como Refill)
//   2. Busca piezas del kit seleccionado en el inventario
//   3. Shift-click cada pieza al hotbar
//   4. Swap (mode 2) desde hotbar al slot de armadura
//   5. Cierra inventario con tecla E
//   6. Se auto-desactiva al terminar (como Refill)

extern bool  gui_armorswitcher_enabled;
extern int   gui_armorswitcher_kit;       // 0=Diamond, 1=Iron, 2=Gold, 3=Chain, 4=Leather
extern int   gui_armorswitcher_bind;
extern float gui_armorswitcher_delay;
extern HWND  g_GameWindow;

namespace features::combat::armor_switcher
{
    // ============================================================
    // ESTADOS DE LA MAQUINA
    // ============================================================
    enum class State {
        IDLE,
        OPEN_INV,        // Esperando que el inventario se abra
        SHIFT_CLICK,     // Moviendo piezas nuevas del inventario al hotbar
        SWAP_EQUIP,      // Swapeando desde hotbar hacia los slots de armadura
        CLOSE_INV        // Cerrando inventario
    };

    static State       s_state = State::IDLE;
    static int         s_current_piece = 0;  // 0=helmet, 1=chest, 2=legs, 3=boots
    static bool        s_was_enabled = false; // Para detectar toggle ON (como Refill)
    static std::mt19937 s_rng(std::random_device{}());

    // Slots del inventario donde encontramos cada pieza nueva
    static int s_found_slots[4] = { -1, -1, -1, -1 };
    // Hotbar slots donde caen las piezas despues del shift-click
    static int s_hotbar_slots[4] = { -1, -1, -1, -1 };

    // ============================================================
    // TIMER DE ALTA RESOLUCION (copiado del Refill)
    // ============================================================
    static LARGE_INTEGER s_frequency;
    static bool s_timer_initialized = false;
    static double s_state_timer = 0.0;
    static double s_last_click_time = 0.0;

    static double get_time_in_ms() {
        if (!s_timer_initialized) {
            QueryPerformanceFrequency(&s_frequency);
            s_timer_initialized = true;
        }
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)(now.QuadPart) * 1000.0 / (double)(s_frequency.QuadPart);
    }

    // ============================================================
    // HELPERS (copiados del Refill para bypass anti-cheat)
    // ============================================================
    static double humanized_delay(double base_ms) {
        std::normal_distribution<double> noise(0.0, base_ms * 0.08);
        double result = base_ms + noise(s_rng);
        if (result < 20.0) result = 20.0;
        return result;
    }

    static int get_armor_id(int kit, int piece) {
        switch (kit) {
        case 0: return 310 + piece; // Diamond
        case 1: return 306 + piece; // Iron
        case 2: return 314 + piece; // Gold
        case 3: return 302 + piece; // Chain
        case 4: return 298 + piece; // Leather
        default: return 310 + piece;
        }
    }

    static int get_item_id_safe(JNIEnv* env, jobject item_obj) {
        if (!item_obj || !env) return -1;

        jclass item_class = env->GetObjectClass(item_obj);
        if (!item_class) return -1;

        std::string item_sig = mapper::classes["Item"].signature;
        std::string getId_sig = "(" + item_sig + ")I";

        jmethodID get_id = env->GetStaticMethodID(item_class, "getIdFromItem", getId_sig.c_str());
        if (!get_id) { env->ExceptionClear(); get_id = env->GetStaticMethodID(item_class, "func_150891_b", getId_sig.c_str()); }
        if (!get_id) { env->ExceptionClear(); get_id = env->GetStaticMethodID(item_class, "b", getId_sig.c_str()); }
        if (!get_id) { env->ExceptionClear(); get_id = env->GetStaticMethodID(item_class, "a", getId_sig.c_str()); }

        int id = -1;
        if (get_id) {
            id = env->CallStaticIntMethod(item_class, get_id, item_obj);
            if (env->ExceptionCheck()) { env->ExceptionClear(); id = -1; }
        }
        env->DeleteLocalRef(item_class);
        return id;
    }

    // ============================================================
    // ABRIR/CERRAR INVENTARIO (tecla E - copiado del Refill)
    // ============================================================
    static void press_key_hardware(BYTE virtual_key) {
        UINT scan_code = MapVirtualKey(virtual_key, 0);
        keybd_event(virtual_key, scan_code, KEYEVENTF_EXTENDEDKEY | 0, 0);
        Sleep((DWORD)humanized_delay(15.0));
        keybd_event(virtual_key, scan_code, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }

    // ============================================================
    // MOVER CURSOR VISUAL A SLOT DEL INVENTARIO
    // ============================================================
    static void move_cursor_to_slot(int container_slot, JNIEnv* env, jobject mc_obj) {
        if (!g_GameWindow || !IsWindow(g_GameWindow)) return;
        if (!mc_obj || !env) return;

        RECT rc;
        GetClientRect(g_GameWindow, &rc);
        int win_w = rc.right - rc.left;
        int win_h = rc.bottom - rc.top;
        if (win_w <= 0 || win_h <= 0) return;

        // Obtener guiScale
        int gui_scale = 2;
        jclass mc_class = env->GetObjectClass(mc_obj);
        if (mc_class) {
            std::string gs_sig = "L" + mapper::classes["GameSettings"].name + ";";
            jfieldID gs_fid = env->GetFieldID(mc_class, "gameSettings", gs_sig.c_str());
            if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "field_71474_y", gs_sig.c_str()); }
            if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "t", gs_sig.c_str()); }
            if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "N", gs_sig.c_str()); }

            if (gs_fid) {
                jobject gs_obj = env->GetObjectField(mc_obj, gs_fid);
                if (gs_obj) {
                    jclass gs_class = env->GetObjectClass(gs_obj);
                    jfieldID scale_fid = env->GetFieldID(gs_class, "guiScale", "I");
                    if (!scale_fid) { env->ExceptionClear(); scale_fid = env->GetFieldID(gs_class, "field_74335_Z", "I"); }
                    if (!scale_fid) { env->ExceptionClear(); scale_fid = env->GetFieldID(gs_class, "ah", "I"); }
                    if (!scale_fid) { env->ExceptionClear(); scale_fid = env->GetFieldID(gs_class, "ag", "I"); }
                    if (scale_fid) {
                        gui_scale = env->GetIntField(gs_obj, scale_fid);
                        if (env->ExceptionCheck()) { env->ExceptionClear(); gui_scale = 2; }
                    }
                    env->DeleteLocalRef(gs_class);
                    env->DeleteLocalRef(gs_obj);
                }
            }
            env->DeleteLocalRef(mc_class);
        }

        if (gui_scale <= 0) {
            gui_scale = 1;
            while (gui_scale < 4 && (win_w / (gui_scale + 1)) >= 320 && (win_h / (gui_scale + 1)) >= 240)
                gui_scale++;
        }

        int guiX = 0, guiY = 0;
        if (container_slot >= 5 && container_slot <= 8) {
            guiX = 8; guiY = 8 + (container_slot - 5) * 18;
        }
        else if (container_slot >= 9 && container_slot <= 35) {
            int idx = container_slot - 9;
            guiX = 8 + (idx % 9) * 18; guiY = 84 + (idx / 9) * 18;
        }
        else if (container_slot >= 36 && container_slot <= 44) {
            guiX = 8 + (container_slot - 36) * 18; guiY = 142;
        }
        guiX += 8; guiY += 8;

        int scaled_w = win_w / gui_scale;
        int scaled_h = win_h / gui_scale;
        int guiLeft = (scaled_w - 176) / 2;
        int guiTop = (scaled_h - 166) / 2;

        int px = (guiLeft + guiX) * gui_scale;
        int py = (guiTop + guiY) * gui_scale;

        POINT screen_pt = { px, py };
        ClientToScreen(g_GameWindow, &screen_pt);
        SetCursorPos(screen_pt.x, screen_pt.y);
    }

    // ============================================================
    // BUSCAR PIEZAS DE ARMADURA EN EL INVENTARIO
    // ============================================================
    // Busca en el inventario completo (hotbar + inventario principal)
    // cada pieza del kit seleccionado. NO verifica durabilidad para
    // simplificar y asegurar que siempre encuentre las piezas.
    static bool find_armor_pieces(JNIEnv* env, mapper::__player& local_player, int kit) {
        for (int p = 0; p < 4; p++) s_found_slots[p] = -1;

        int found_count = 0;
        for (int piece = 0; piece < 4; piece++) {
            int target_id = get_armor_id(kit, piece);

            // Buscar en inventario principal PRIMERO (slots 9-35)
            for (int slot = 9; slot <= 35; slot++) {
                auto stack = local_player.get_inventory_slot(slot);
                if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                if (!stack.object) continue;

                auto item = stack.get_item();
                if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                if (!item.object) continue;

                int item_id = get_item_id_safe(env, item.object);
                if (item_id == target_id) {
                    s_found_slots[piece] = slot;
                    found_count++;
                    break; // Encontrado, siguiente pieza
                }
            }

            // Si no esta en inventario, buscar en hotbar (slots 0-8)
            if (s_found_slots[piece] == -1) {
                for (int slot = 0; slot <= 8; slot++) {
                    auto stack = local_player.get_inventory_slot(slot);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (!stack.object) continue;

                    auto item = stack.get_item();
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (!item.object) continue;

                    int item_id = get_item_id_safe(env, item.object);
                    if (item_id == target_id) {
                        s_found_slots[piece] = slot;
                        found_count++;
                        break;
                    }
                }
            }
        }

        return found_count > 0;
    }

    // ============================================================
    // FUNCION PRINCIPAL: Maquina de estados
    // ============================================================
    void run(mapper::__minecraft& minecraft)
    {
        if (!sdk::jni) return;
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        JNIEnv* env = sdk::jni;

        // Si el modulo esta desactivado y habia un estado activo, resetear
        if (!gui_armorswitcher_enabled) {
            if (s_state != State::IDLE) {
                s_state = State::IDLE;
                s_current_piece = 0;
            }
            s_was_enabled = false;
            return;
        }

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) { s_state = State::IDLE; return; }

        auto current_screen = minecraft.get_current_screen();
        bool is_gui_open = (current_screen.object != nullptr);
        double now = get_time_in_ms();

        // ============================================================
        // TRIGGER: igual que Refill
        // Se activa cuando:
        //   1. El toggle se acaba de prender (enabled && !was_enabled)
        //   2. O se presiona el bind
        // ============================================================
        bool key_pressed = (gui_armorswitcher_bind != 0 && (GetAsyncKeyState(gui_armorswitcher_bind) & 0x8000));
        bool should_trigger = key_pressed || (gui_armorswitcher_enabled && !s_was_enabled && s_state == State::IDLE);

        if (should_trigger && s_state == State::IDLE) {
            s_was_enabled = true;

            // Buscar todas las piezas del set
            if (!find_armor_pieces(env, local_player, gui_armorswitcher_kit)) {
                // No hay piezas, apagar modulo
                gui_armorswitcher_enabled = false;
                s_was_enabled = false;
                return;
            }

            s_current_piece = 0;
            for (int i = 0; i < 4; i++) s_hotbar_slots[i] = -1;

            // Abrir inventario con E (estilo Refill)
            if (!is_gui_open) {
                press_key_hardware(0x45); // E key
                s_state = State::OPEN_INV;
                s_state_timer = now;
            }
            else {
                // Ya hay inventario abierto, ir directo
                s_state = State::SHIFT_CLICK;
                s_current_piece = 0;
                s_last_click_time = now;
            }
            return;
        }

        if (!key_pressed) s_was_enabled = true; // Evitar re-trigger continuo

        // ============================================================
        // ESTADO OPEN_INV - Esperando que el inventario se abra
        // ============================================================
        if (s_state == State::OPEN_INV) {
            if (is_gui_open) {
                s_state = State::SHIFT_CLICK;
                s_current_piece = 0;
                s_last_click_time = now;
            }
            else if ((now - s_state_timer) > 800.0) {
                // Timeout
                s_state = State::IDLE;
                gui_armorswitcher_enabled = false;
                s_was_enabled = false;
            }
            return;
        }

        // Si el inventario se cerro inesperadamente, abort
        if (s_state != State::IDLE && !is_gui_open) {
            s_state = State::IDLE;
            s_current_piece = 0;
            gui_armorswitcher_enabled = false;
            s_was_enabled = false;
            return;
        }

        // ============================================================
        // ESTADO SHIFT_CLICK - Mover piezas del inventario al hotbar
        // ============================================================
        if (s_state == State::SHIFT_CLICK) {
            double delay = humanized_delay((double)gui_armorswitcher_delay);
            if ((now - s_last_click_time) < delay) return;

            // Saltar piezas que no se encontraron
            while (s_current_piece < 4 && s_found_slots[s_current_piece] == -1) {
                s_current_piece++;
            }

            if (s_current_piece >= 4) {
                // Todas procesadas, pasar a swap
                s_current_piece = 0;
                s_state = State::SWAP_EQUIP;
                s_last_click_time = now;
                return;
            }

            int inv_slot = s_found_slots[s_current_piece];

            if (inv_slot >= 0 && inv_slot <= 8) {
                // Ya esta en el hotbar, recordar posicion y saltar
                s_hotbar_slots[s_current_piece] = inv_slot;
                s_current_piece++;
                s_last_click_time = now;
                return;
            }

            // En inventario principal (9-35): shift-click para mover al hotbar
            int container_slot = inv_slot;

            // Mover cursor visual al slot
            move_cursor_to_slot(container_slot, env, minecraft.object);

            // Shift-click (mode 1, button 0)
            minecraft.window_click(0, container_slot, 0, 1, local_player);
            if (env->ExceptionCheck()) env->ExceptionClear();

            // Buscar en que slot del hotbar cayo la pieza
            auto fresh_player = minecraft.get_local_player();
            if (fresh_player.object) {
                int target_id = get_armor_id(gui_armorswitcher_kit, s_current_piece);
                for (int h = 0; h <= 8; h++) {
                    auto stack = fresh_player.get_inventory_slot(h);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (!stack.object) continue;

                    auto item = stack.get_item();
                    if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
                    if (!item.object) continue;

                    int item_id = get_item_id_safe(env, item.object);
                    if (item_id == target_id) {
                        s_hotbar_slots[s_current_piece] = h;
                        break;
                    }
                }
            }

            s_current_piece++;
            s_last_click_time = now;
            return;
        }

        // ============================================================
        // ESTADO SWAP_EQUIP - Equipar armadura desde hotbar
        // ============================================================
        if (s_state == State::SWAP_EQUIP) {
            double delay = humanized_delay((double)gui_armorswitcher_delay);
            if ((now - s_last_click_time) < delay) return;

            // Saltar piezas sin hotbar slot
            while (s_current_piece < 4 && s_hotbar_slots[s_current_piece] == -1) {
                s_current_piece++;
            }

            if (s_current_piece >= 4) {
                // Todas equipadas, cerrar inventario
                s_state = State::CLOSE_INV;
                s_last_click_time = now;
                return;
            }

            int hotbar_idx = s_hotbar_slots[s_current_piece];
            int armor_container_slot = 5 + s_current_piece; // 5=helmet, 6=chest, 7=legs, 8=boots

            // Mover cursor visual al slot de armadura
            move_cursor_to_slot(armor_container_slot, env, minecraft.object);

            // Mode 2: swap hotbar slot con armor slot
            minecraft.window_click(0, armor_container_slot, hotbar_idx, 2, local_player);
            if (env->ExceptionCheck()) env->ExceptionClear();

            s_current_piece++;
            s_last_click_time = now;
            return;
        }

        // ============================================================
        // ESTADO CLOSE_INV - Cerrar inventario con E
        // ============================================================
        if (s_state == State::CLOSE_INV) {
            double delay = humanized_delay((double)gui_armorswitcher_delay * 0.5);
            if ((now - s_last_click_time) < delay) return;

            press_key_hardware(0x45);
            s_state = State::IDLE;
            s_current_piece = 0;

            // Auto-desactivar el modulo (como Refill)
            gui_armorswitcher_enabled = false;
            s_was_enabled = false;
            return;
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}
