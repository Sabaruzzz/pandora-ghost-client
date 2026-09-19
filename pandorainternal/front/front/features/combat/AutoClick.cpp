#include "../features.hpp"
#include "sdk.hpp"
#include <windows.h>
extern bool g_MenuVisible;
#include <mmsystem.h>
#include <random>
#include <thread>
#include <atomic>
#include <cmath>
#include <intrin.h>

#pragma comment(lib, "winmm.lib")

// ============================================================
// PANDORA ENGINE v2 — Complete rewrite, zero legacy code
// Click Methods: Normal, Jitter, Butterfly
// Timing: HPC + NtSetTimerResolution sub-ms precision
// ============================================================

namespace features::combat::auto_click
{
    // ============================================================
    // THREAD STATE
    // ============================================================
    static std::atomic<bool>   g_thread_running{ false };
    static std::atomic<bool>   g_should_click{ false };
    static std::atomic<double> g_min_cps{ 12.0 };
    static std::atomic<double> g_max_cps{ 14.0 };
    static std::atomic<void*>  g_hwnd{ nullptr };
    static std::atomic<int>    g_click_counter{ 0 };
    static std::atomic<bool>   g_world_ready{ false };
    static std::atomic<bool>   g_in_gui{ false };
    static std::atomic<int>    g_click_method{ 0 };
    static std::thread         g_click_thread;

    // ============================================================
    // HIGH PERFORMANCE TIMER
    // ============================================================
    static inline double hpc_now() {
        static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return ((double)now.QuadPart / (double)freq.QuadPart) * 1000.0;
    }

    // ============================================================
    // NtSetTimerResolution — sub-ms system timer (Sapphire technique)
    // ============================================================
    typedef LONG(NTAPI* pNtSetTimerResolution)(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
    static bool g_timer_resolution_set = false;

    static void set_high_timer_resolution() {
        if (g_timer_resolution_set) return;
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        if (ntdll) {
            auto fn = (pNtSetTimerResolution)GetProcAddress(ntdll, "NtSetTimerResolution");
            if (fn) {
                ULONG current = 0;
                fn(5000, TRUE, &current); // 0.5ms resolution
                g_timer_resolution_set = true;
            }
        }
    }

    // ============================================================
    // WINDOW FOCUS CHECK
    // ============================================================
    static bool game_window_has_focus(HWND hwnd) {
        if (!hwnd || !IsWindow(hwnd)) return false;
        const HWND foreground = GetForegroundWindow();
        if (!foreground) return false;
        return foreground == hwnd ||
            GetAncestor(foreground, GA_ROOT) == GetAncestor(hwnd, GA_ROOT);
    }

    // ============================================================
    // RANDOM ENGINE — Mersenne Twister, seeded once
    // ============================================================
    static std::mt19937 g_rng(std::random_device{}());

    static double rand_double(double lo, double hi) {
        std::uniform_real_distribution<double> dist(lo, hi);
        return dist(g_rng);
    }

    static int rand_int(int lo, int hi) {
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(g_rng);
    }

    // ============================================================
    // CLICK METHOD: NORMAL
    // Sapphire-inspired numerator/deviation randomization.
    // Smooth, consistent timing with micro-adjustments.
    // ============================================================
    struct NormalState {
        double drift = 0.0; // slow CPS drift within range
    };

    static double compute_normal_interval(double cps, NormalState& state) {
        // Sapphire-style: numerator with distribution window, but scaled correctly for 1000ms base
        int distribution = (int)(features::combat::auto_click::randomization ? 
            rand_int(30, 70) : 0);
        
        // Use 1000 as the base numerator so interval = 1000 / cps is accurate.
        int numerator = features::combat::auto_click::randomization ?
            rand_int(1000 - distribution, 1000 + distribution) : 1000;

        int deviation = features::combat::auto_click::randomization ? 2 : 0;
        int effective_cps = (int)cps;
        if (effective_cps < 1) effective_cps = 1;

        double interval;
        // 5% chance of slight deviation in target CPS
        if (rand_int(0, 99) < 5) {
            interval = (double)numerator / (double)effective_cps;
        } else {
            int cps_varied = rand_int(
                (std::max)(1, effective_cps - deviation),
                effective_cps + deviation
            );
            if (cps_varied < 1) cps_varied = 1;
            interval = (double)numerator / (double)cps_varied;
        }

        // Outlier spikes (3% chance) — mimics human hesitation
        if (features::combat::auto_click::randomization && rand_int(0, 99) < 3) {
            interval = rand_double(100.0, 150.0);
        }

        // Spike chance from slider
        if (features::combat::auto_click::randomization &&
            rand_int(0, 999) < (int)(features::combat::auto_click::spike_chance * 10)) {
            interval *= rand_double(0.55, 0.75); // burst spike — faster click
        }

        // Drop chance from slider
        if (features::combat::auto_click::randomization &&
            rand_int(0, 999) < (int)(features::combat::auto_click::drop_chance * 10)) {
            interval += rand_double(30.0, 70.0); // human drop — missed beat
        }

        return interval;
    }

    static int compute_normal_hold(double interval) {
        // Hold time: 30-45% of interval with micro-variation
        double hold_ratio = rand_double(0.30, 0.45);
        int hold = (int)(interval * hold_ratio);
        if (hold < 2) hold = 2;
        return hold;
    }

    // ============================================================
    // CLICK METHOD: JITTER
    // Erratic, shaky timing. Short choppy holds, high variance.
    // Simulates physical hand tremor on the mouse button.
    // ============================================================
    struct JitterState {
        double tremor_accumulator = 0.0;
        int burst_counter = 0;
    };

    static double compute_jitter_interval(double cps, JitterState& state) {
        double base_interval = 1000.0 / cps;

        // Tremor: accumulates random micro-offsets
        state.tremor_accumulator += rand_double(-3.5, 3.5);
        state.tremor_accumulator *= 0.7; // decay
        if (state.tremor_accumulator > 12.0) state.tremor_accumulator = 12.0;
        if (state.tremor_accumulator < -8.0) state.tremor_accumulator = -8.0;

        // Jitter variance: ±25% of base interval
        double jitter = rand_double(-0.25, 0.25) * base_interval;

        // Micro-bursts: every 4-8 clicks, do 2-3 very fast ones
        state.burst_counter++;
        double burst_modifier = 0.0;
        if (state.burst_counter >= rand_int(4, 8)) {
            burst_modifier = -base_interval * rand_double(0.15, 0.30);
            state.burst_counter = 0;
        }

        double interval = base_interval + jitter + state.tremor_accumulator + burst_modifier;

        // Drop chance
        if (features::combat::auto_click::randomization &&
            rand_int(0, 999) < (int)(features::combat::auto_click::drop_chance * 10)) {
            interval += rand_double(25.0, 55.0);
        }

        // Spike chance
        if (features::combat::auto_click::randomization &&
            rand_int(0, 999) < (int)(features::combat::auto_click::spike_chance * 10)) {
            interval *= rand_double(0.5, 0.7);
        }

        if (interval < 8.0) interval = 8.0; // physical minimum
        return interval;
    }

    static int compute_jitter_hold(double interval) {
        // Jitter hold: very short and choppy, 15-35% of interval
        double hold_ratio = rand_double(0.15, 0.35);
        int hold = (int)(interval * hold_ratio);
        // Add micro-stutter
        hold += rand_int(-1, 2);
        if (hold < 1) hold = 1;
        return hold;
    }

    // ============================================================
    // CLICK METHOD: BUTTERFLY
    // Two-finger alternation. Fast-slow-fast pattern.
    // Each "finger" has different press characteristics.
    // ============================================================
    struct ButterflyState {
        bool second_finger = false;   // alternates between fingers
        double finger1_fatigue = 0.0; // fatigue accumulator per finger
        double finger2_fatigue = 0.0;
        int pair_counter = 0;         // counts finger pairs
    };

    static double compute_butterfly_interval(double cps, ButterflyState& state) {
        // Butterfly does TWO clicks per cycle.
        // If we want 20 total CPS, we need 10 pairs per second.
        // So the interval for a PAIR is 1000 / (cps / 2) = 2000 / cps.
        double pair_interval = 2000.0 / cps;

        double interval;
        if (!state.second_finger) {
            // First finger: hits, second finger follows VERY quickly
            // Gap between fingers is tiny (20-40% of the pair interval)
            double gap_ratio = rand_double(0.20, 0.40);
            interval = pair_interval * gap_ratio;

            // Finger 1 fatigue
            state.finger1_fatigue += rand_double(0.0, 1.5);
            if (state.finger1_fatigue > 8.0) state.finger1_fatigue = 8.0;
            interval += state.finger1_fatigue * 0.3;

            state.second_finger = true;
        } else {
            // Second finger: after both hit, longer gap before next pair
            // Gap is 60-80% of the pair interval (the "lift" phase)
            double gap_ratio = rand_double(0.60, 0.80);
            interval = pair_interval * gap_ratio;

            // Add pair variation every 3-6 pairs
            state.pair_counter++;
            if (state.pair_counter >= rand_int(3, 6)) {
                interval += rand_double(5.0, 20.0); // micro-rest between pair groups
                state.pair_counter = 0;
                // Partial fatigue recovery during rest
                state.finger1_fatigue *= 0.5;
                state.finger2_fatigue *= 0.5;
            }

            // Finger 2 fatigue
            state.finger2_fatigue += rand_double(0.0, 1.2);
            if (state.finger2_fatigue > 6.0) state.finger2_fatigue = 6.0;
            interval += state.finger2_fatigue * 0.25;

            state.second_finger = false;
        }

        // Asymmetric randomization — butterfly is naturally less consistent
        interval += rand_double(-2.0, 4.0);

        // Drop chance
        if (features::combat::auto_click::randomization &&
            rand_int(0, 999) < (int)(features::combat::auto_click::drop_chance * 10)) {
            interval += rand_double(20.0, 60.0);
            state.finger1_fatigue *= 0.3; // rest recovery
            state.finger2_fatigue *= 0.3;
        }

        // Spike chance
        if (features::combat::auto_click::randomization &&
            rand_int(0, 999) < (int)(features::combat::auto_click::spike_chance * 10)) {
            interval *= rand_double(0.45, 0.65);
        }

        if (interval < 5.0) interval = 5.0;
        return interval;
    }

    static int compute_butterfly_hold(double interval, bool second_finger) {
        if (!second_finger) {
            // First finger: very short tap (1-4ms)
            return rand_int(1, 4);
        } else {
            // Second finger: slightly longer hold (3-7ms)
            return rand_int(3, 7);
        }
    }

    // ============================================================
    // CPS DRIFT — smooth random walk within min/max range
    // ============================================================
    static double compute_effective_cps(double min_c, double max_c) {
        static double current_cps = -1.0;
        if (current_cps < 0.0) current_cps = (min_c + max_c) * 0.5;

        // Gentle random walk: ±0.4 CPS per tick
        current_cps += rand_double(-0.4, 0.4);

        // Clamp to range
        if (current_cps < min_c) current_cps = min_c;
        if (current_cps > max_c) current_cps = max_c;

        return current_cps;
    }

    // ============================================================
    // MAIN CLICK THREAD — completely new engine
    // ============================================================
    static void click_thread_func() {
        set_high_timer_resolution();

        double next_click = hpc_now();
        bool was_clicking = false;

        NormalState normal_state;
        JitterState jitter_state;
        ButterflyState butterfly_state;

        while (g_thread_running.load(std::memory_order_relaxed)) {

            if (!g_should_click.load(std::memory_order_relaxed)) {
                Sleep(1);
                was_clicking = false;
                // Reset method states when not clicking
                normal_state = {};
                jitter_state = {};
                butterfly_state = {};
                continue;
            }

            HWND hwnd = (HWND)g_hwnd.load(std::memory_order_relaxed);
            if (!hwnd) { Sleep(1); continue; }
            if (!game_window_has_focus(hwnd)) {
                g_should_click.store(false, std::memory_order_relaxed);
                was_clicking = false;
                Sleep(1);
                continue;
            }

            double min_c = g_min_cps.load(std::memory_order_relaxed);
            double max_c = g_max_cps.load(std::memory_order_relaxed);
            int method = g_click_method.load(std::memory_order_relaxed);

            // Effective CPS with drift
            double effective_cps = g_in_gui.load(std::memory_order_relaxed)
                ? features::combat::auto_click::inventory_cps
                : compute_effective_cps(min_c, max_c);

            if (effective_cps < 1.0) effective_cps = 1.0;

            // Compute interval and hold based on click method
            double interval;
            int hold_delay;

            switch (method) {
            case 1: // Jitter
                interval = compute_jitter_interval(effective_cps, jitter_state);
                hold_delay = compute_jitter_hold(interval);
                break;
            case 2: // Butterfly
                interval = compute_butterfly_interval(effective_cps, butterfly_state);
                hold_delay = compute_butterfly_hold(interval, butterfly_state.second_finger);
                break;
            default: // Normal (0)
                interval = compute_normal_interval(effective_cps, normal_state);
                hold_delay = compute_normal_hold(interval);
                break;
            }

            if (!was_clicking) {
                next_click = hpc_now();
                was_clicking = true;
            }

            // Precision wait using HPC spin-wait
            double now = hpc_now();
            if (now < next_click) {
                double diff = next_click - now;
                if (diff > 16.0) {
                    Sleep((DWORD)(diff - 16.0));
                }
                while (hpc_now() < next_click) {
                    std::this_thread::yield();
                }
            }

            // Validate state before sending click
            if (!game_window_has_focus(hwnd) ||
                ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) || g_MenuVisible ||
                !g_world_ready.load(std::memory_order_relaxed)) {
                g_should_click.store(false, std::memory_order_relaxed);
                was_clicking = false;
                continue;
            }

            // Send mouse down
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            LPARAM lParam = MAKELPARAM(pt.x, pt.y);

            PostMessageA(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lParam);

            // Hold
            if (hold_delay > 0) Sleep(hold_delay);

            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            lParam = MAKELPARAM(pt.x, pt.y);

            PostMessageA(hwnd, WM_LBUTTONUP, 0, lParam);
            g_click_counter.fetch_add(1, std::memory_order_relaxed);

            // Schedule next click
            next_click += interval;

            // Drift correction: if we fell too far behind, catch up
            double current_time = hpc_now();
            if (current_time > next_click && (current_time - next_click) > 50.0) {
                next_click = current_time;
            }
        }
    }

    // ============================================================
    // HELPERS: Block/Entity detection (condition checks)
    // ============================================================
    static bool safe_is_looking_at_block(JNIEnv* env, jobject mc_obj) {
        if (!env || !mc_obj) return false;

        mapper::__minecraft mc_wrapper;
        mc_wrapper.object = mc_obj;
        auto mop = mc_wrapper.get_object_mouse_over();
        mc_wrapper.object = nullptr;

        if (!mop.object) return false;

        int type = mop.get_type_of_hit();
        return (type == 1); // 0=MISS, 1=BLOCK, 2=ENTITY
    }

    static bool safe_is_looking_at_entity(JNIEnv* env, jobject mc_obj) {
        if (!env || !mc_obj) return false;

        mapper::__minecraft mc_wrapper;
        mc_wrapper.object = mc_obj;
        auto mop = mc_wrapper.get_object_mouse_over();
        mc_wrapper.object = nullptr;

        if (!mop.object) return false;

        int type = mop.get_type_of_hit();
        return (type == 2); // 0=MISS, 1=BLOCK, 2=ENTITY
    }

    // ============================================================
    // PUBLIC API
    // ============================================================
    int  get_click_count() { return g_click_counter.load(); }
    void reset_click_count() { g_click_counter.store(0); }

    void shutdown() {
        g_thread_running.store(false);
        g_should_click.store(false);
        g_world_ready.store(false);
        if (g_click_thread.joinable()) {
            g_click_thread.join();
        }
    }
}

// ============================================================
// RUN — Called every tick from features::run_on_run_tick
// Handles all condition checks, then signals the click thread
// ============================================================
namespace features::combat::auto_click
{
    void run(mapper::__minecraft& minecraft)
    {
        if (!g_thread_running.load()) {
            if (g_click_thread.joinable()) {
                g_click_thread.join();
            }
            g_thread_running.store(true);
            g_click_thread = std::thread(click_thread_func);
        }

        if (!enabled) { g_should_click.store(false); g_world_ready.store(false); return; }
        // Refill check removed
        if (g_MenuVisible) { g_should_click.store(false); return; }

        HWND gameWindow = static_cast<HWND>(features::visual::window);
        if (!game_window_has_focus(gameWindow)) {
            g_should_click.store(false);
            g_world_ready.store(false);
            g_in_gui.store(false);
            return;
        }

        g_world_ready.store(true);

        g_min_cps.store(min_cps > 0.0 ? min_cps : 1.0);
        g_max_cps.store(max_cps > 0.0 ? max_cps : 1.0);
        g_hwnd.store(gameWindow);
        g_click_method.store(click_method);

        bool is_clicking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        if (!is_clicking) { g_should_click.store(false); return; }

        // --- GUI detection (Inventory Fill) ---
        bool is_in_gui = false;
        if (minecraft.object != nullptr) {
            auto scr = minecraft.get_current_screen();
            if (sdk::jni && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
            if (scr.object != nullptr) is_in_gui = true;
        }

        if (is_in_gui) {
            if (inventory_enabled) {
                g_in_gui.store(true);
                if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) == 0) { g_should_click.store(false); return; }
            }
            else { g_should_click.store(false); g_in_gui.store(false); return; }
        } else {
            g_in_gui.store(false);
        }

        // --- Weapons Only ---
        if (weapons_only && !is_in_gui && minecraft.object != nullptr) {
            auto lp = minecraft.get_local_player();
            if (!lp.object) { g_should_click.store(false); return; }
            auto hs = lp.get_held_item_stack();
            if (!hs.object) { g_should_click.store(false); return; }
            auto it = hs.get_item();
            if (!it.object || !it.is_sword()) { g_should_click.store(false); return; }
        }

        // --- Break Blocks ---
        if (break_blocks && !is_in_gui && minecraft.object != nullptr) {
            if (safe_is_looking_at_block(sdk::jni, minecraft.object)) {
                g_should_click.store(false);
                return;
            }
        }

        // --- Target Only ---
        if (target_only && !is_in_gui && minecraft.object != nullptr) {
            if (!safe_is_looking_at_entity(sdk::jni, minecraft.object)) {
                g_should_click.store(false); return;
            }
        }

        // --- Blockhit Sync ---
        if (blockhit_sync && !is_in_gui && minecraft.object != nullptr) {
            if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0) {
                g_should_click.store(false); return;
            }
        }

        g_should_click.store(true);

        // --- Jitter condition (camera shake) tied to Jitter Method ---
        if (click_method == 1 && minecraft.object != nullptr) {
            auto lp = minecraft.get_local_player();
            if (lp.object) {
                std::uniform_real_distribution<float> jd(-jitter_intensity, jitter_intensity);
                auto ang = lp.get_view_angles();
                lp.set_old_view_angles(ang);
                lp.set_view_angles({ ang.x + jd(g_rng), ang.y + jd(g_rng) });
            }
        }
    }
}
