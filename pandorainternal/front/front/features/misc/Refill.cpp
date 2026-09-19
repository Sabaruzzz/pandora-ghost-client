#include "../features.hpp"
#include "sdk.hpp"
#include <windows.h>
#include <random>
#include <string>

namespace features::combat::refill
{
    static LARGE_INTEGER frequency;
    static bool timer_initialized = false;

    double get_time_in_ms() {
        if (!timer_initialized) {
QueryPerformanceFrequency(&frequency);
timer_initialized = true;
        }
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)(now.QuadPart) * 1000.0 / (double)(frequency.QuadPart);
    }

    // =====================================================================
    // BYPASS: jitter aleatorio en delays para que los tiempos no sean
    // perfectamente constantes  Polar/Watchdog detectan intervalos fijos
    // =====================================================================
    static std::mt19937 g_rng(std::random_device{}());

    int get_random_jitter(int min_v, int max_v) {
        std::uniform_int_distribution<> d(min_v, max_v);
        return d(g_rng);
    }

    // Delay humanizado: base + variacion gaussiana +-8%
    double humanized_delay(double base_ms) {
        std::normal_distribution<double> noise(0.0, base_ms * 0.08);
        double result = base_ms + noise(g_rng);
        if (result < 20.0) result = 20.0;
        return result;
    }

    static double last_click_time = 0.0;
    static int    macro_state = 0;
    static double state_timer = 0.0;
    static bool   was_enabled = false;



    void press_key_hardware(BYTE virtual_key) {
        UINT scan_code = MapVirtualKey(virtual_key, 0);
        keybd_event(virtual_key, scan_code, KEYEVENTF_EXTENDEDKEY | 0, 0);
        // Bypass: delay humanizado en keypress en vez de Sleep fijo
        Sleep((DWORD)humanized_delay(15.0));
        keybd_event(virtual_key, scan_code, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }



    // =====================================================================
    // CORREGIDO PARA 1.7.10: usar mapper::classes["Item"].signature
    // en vez de hardcodear "Lnet/minecraft/item/Item;" que solo funciona
    // en Forge. Ahora funciona en Forge, Lunar 1.8 y Lunar 1.7.10.
    // =====================================================================
    bool is_refillable_item(JNIEnv* env, jobject item_stack_obj) {
        if (!item_stack_obj || !env) return false;
        jobject stack_ref = env->NewLocalRef(item_stack_obj);
        if (!stack_ref) return false;

        mapper::__item_stack stack(stack_ref);
        mapper::__item item = stack.get_item();
        const int id = item.get_id();
        if (id == 282) return true;
        if (id != 373) return false;

        const int damage = stack.get_item_damage();
        return (damage & 16384) != 0;
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (sdk::jni && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

        // Si el modulo esta desactivado y habia un estado activo, resetear
        if (!enabled) {
            macro_state = 0;
            was_enabled = false;
            return;
        }

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        auto current_screen = minecraft.get_current_screen();
        bool is_gui_open = (current_screen.object != nullptr);

        // Gatillo: keybind O activacion directa del toggle
        // The key edge is already handled centrally by ProcessKeybinds().
        // Reading the held key here restarted Refill on every frame.
        bool should_trigger = enabled && !was_enabled && macro_state == 0;

        if (should_trigger && macro_state == 0) {
was_enabled = true;
bool needs_refill = false;
bool has_potions_in_bag = false;

for (int i = 0; i <= 8; i++) {
    auto stack = local_player.get_inventory_slot(i);
    if (stack.object == nullptr || sdk::jni->IsSameObject(stack.object, NULL)) {
        needs_refill = true; break;
    }
}
for (int i = 9; i <= 35; i++) {
    auto stack = local_player.get_inventory_slot(i);
    if (stack.object && is_refillable_item(sdk::jni, stack.object)) {
        has_potions_in_bag = true; break;
    }
}

if (needs_refill && has_potions_in_bag) {
    if (!is_gui_open) press_key_hardware(0x45);
    macro_state = 1;
    state_timer = get_time_in_ms();
}
else {
    // Nada que hacer  apagar el modulo
    macro_state = 0;
    enabled = false;
    was_enabled = false;
}
        }

        // Estado 1: esperar apertura normal
        if (macro_state == 1) {
            if (is_gui_open) {
                macro_state = 2;
                last_click_time = get_time_in_ms();
            }
            else if (get_time_in_ms() - state_timer > 800.0) {
                macro_state = 0; enabled = false;
            }
        }

        // Estado 2: mover pociones
        else if (macro_state == 2) {
            if (!is_gui_open) { macro_state = 0; enabled = false; return; }
            // Bypass: delay humanizado entre cada movimiento de item
            double base_delay = (double)delay_ms;
            double current_delay = humanized_delay(base_delay);

            // Bypass: occasional longer pause (simulates player hesitation)
            if (get_random_jitter(0, 100) < 15) {
                current_delay += get_random_jitter(40, 120);
            }

            if (get_time_in_ms() - last_click_time < current_delay) return;

            int pot_slot = -1, potion_count = 0;
            for (int i = 9; i <= 35; i++) {
                auto stack = local_player.get_inventory_slot(i);
                if (stack.object && is_refillable_item(sdk::jni, stack.object)) {
                    if (pot_slot == -1) pot_slot = i;
                    potion_count++;
                }
            }

            if (potion_count == 0) {
                press_key_hardware(0x45); macro_state = 3; state_timer = get_time_in_ms();
                return;
            }

            // Bypass: Use shift-click (mode 1) instead of hotbar swap (mode 2)
            // Shift-click is what real players use. Hotbar swap is flagged by strict AC.
            bool free_hotbar = false;
            for (int i = 0; i <= 8; i++) {
                auto stack = local_player.get_inventory_slot(i);
                if (stack.object == nullptr || sdk::jni->IsSameObject(stack.object, NULL)) {
                    free_hotbar = true;
                    break;
                }
            }

            if (pot_slot != -1 && free_hotbar) {
                // Shift-click: slot, button=0, mode=1 = identical to Shift+LMB
                minecraft.window_click(0, pot_slot, 0, 1, local_player);
                last_click_time = get_time_in_ms();
            }
            else {
                press_key_hardware(0x45); macro_state = 3; state_timer = get_time_in_ms();
            }
        }

        // Estado 3: esperar cierre normal
        else if (macro_state == 3) {
if (!is_gui_open || get_time_in_ms() - state_timer > 500.0) {
    macro_state = 0; enabled = false;
}
        }
    }
}
