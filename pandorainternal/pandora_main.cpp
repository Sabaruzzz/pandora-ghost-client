#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <cmath>
#include <atomic>
#include <mutex>
#include <jvmti.h>
#include <commdlg.h>
#include <winhttp.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "winhttp.lib")
#include <gl/GL.h>
#include <propsys.h>
#include <propkey.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

#include "sdk.hpp"
#include "mapper.hpp"
#include "features.hpp"
#include "front/front/hooks/hooks.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui.h"
#include "front/back/misc/imgui/misc/freetype/imgui_freetype.h"
#include "alt_auth.hh"
#include "backends/imgui_internal.h"
#include "backends/imgui_impl_opengl3.h"
#include "w_imgui_port/includes.hh"
#include "backends/imgui_impl_win32.h"

#include "front/back/misc/imgui/fonts/font_manager.h"

#include "front/back/misc/security/security.hpp"
#include "front/back/misc/imgui/fonts.hpp"
#include "front/back/misc/imgui/img.hpp"
#include "front/back/misc/imgui/binary_icon_font.hpp"
#include "front/back/misc/imgui/imgui_settings.h"
#include "assets/fonts/pandora_poppins_semibold_compressed.hpp"
#include "assets/fonts/inter_regular_data.hpp"
#include "assets/fonts/menu_sf_pro.hpp"
#include "assets/fonts/sf_pro_bold.hh"
#include "assets/pandoralogo_png.hpp"
#include "w_imgui_port/render/stb/stb_image.hh"
#include "resource.h"

GLuint g_PandoraLogoTexture = 0;
int g_PandoraLogoWidth = 0;
int g_PandoraLogoHeight = 0;

namespace mapper { extern jvmtiEnv* jvmti; }

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dwmapi.lib")

HMODULE myModule = NULL;
HWND    g_GameWindow = NULL;

// ============================================================
// ANTI-CRACK / ANTI-DEBUG TRICKS
// ============================================================
void ErasePEHeader(HMODULE hModule) {
    // Borrar la cabecera PE de la DLL de la memoria para que no puedan dumpearla con Scylla o Process Hacker.
    DWORD oldProtect;
    if (VirtualProtect(hModule, 4096, PAGE_READWRITE, &oldProtect)) {
        SecureZeroMemory(hModule, 4096);
        VirtualProtect(hModule, 4096, oldProtect, &oldProtect);
    }
}

void AntiDebugChecks() {
    // Cierra el juego si hay un depurador (Cheat Engine, x64dbg) adjuntado.
    if (IsDebuggerPresent()) {
        ExitProcess(0);
    }
    BOOL remoteDebugger = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger);
    if (remoteDebugger) {
        ExitProcess(0);
    }
}

void AntiCrackTimeSync() {
    // 1. Verificacion de hipervisor/depurador por tiempo (RDTSC) - (Tiempo real en ciclos de CPU)
    unsigned __int64 start = __rdtsc();
    int volatile dummy = 0;
    for (int i = 0; i < 100; i++) {
        dummy += i;
    }
    unsigned __int64 end = __rdtsc();
    
    // Si la ejecucion tomo demasiado, el hilo fue interceptado (Debuggeado o en VM ralentizada)
    // Aumentado a 500,000,000 para evitar falsos positivos al entrar a servidores (lag de carga)
    if ((end - start) > 500000000) {
        ExitProcess(0);
    }

    // 2. Comprobacion de alteracion del reloj local en tiempo real (Time Spoofing Detection)
    // Se asegura de que el tiempo transcurrido en el juego coincida con la CPU
    static SYSTEMTIME last_st = {0};
    
    if (last_st.wYear == 0) {
        GetSystemTime(&last_st);
        return;
    }
    
    SYSTEMTIME current_st;
    GetSystemTime(&current_st);
    
    // Verificar si el reloj de Windows ha retrocedido repentinamente mientras el juego esta abierto
    FILETIME ftLast, ftCurrent;
    SystemTimeToFileTime(&last_st, &ftLast);
    SystemTimeToFileTime(&current_st, &ftCurrent);
    
    ULARGE_INTEGER uLast, uCurrent;
    uLast.LowPart = ftLast.dwLowDateTime; uLast.HighPart = ftLast.dwHighDateTime;
    uCurrent.LowPart = ftCurrent.dwLowDateTime; uCurrent.HighPart = ftCurrent.dwHighDateTime;
    
    // Si el tiempo del sistema viajo hacia atras mas de 2 segundos de repente (el cracker retrocedio la hora)
    if (uCurrent.QuadPart < uLast.QuadPart && (uLast.QuadPart - uCurrent.QuadPart) > 20000000ULL) {
        ExitProcess(0);
    }
    
    last_st = current_st;
}
// ============================================================

std::atomic<bool> g_Running{ true };
bool g_Init = false;
std::atomic<bool> g_ShouldDestruct{ false };
static std::atomic<bool> g_DestructStarted{ false };
static HANDLE g_LogicThreadHandle = nullptr;
static float g_DestructHoldProgress = 0.0f;

#include "gui/gui.h"
#include "userconfig/userconfig.h"
#include "altmanager/altmanager.h"
#include "gui/gui.cpp"

// ============================================================
// SELF DESTRUCT
// ============================================================
#include <stdarg.h>
extern "C" void pandoraLog(const char* format, ...) {
    FILE* f = nullptr;
    fopen_s(&f, "D:\\pandoraclient\\pandora_log.txt", "a");
    if (f) {
        va_list args;
        va_start(args, format);
        vfprintf(f, format, args);
        fprintf(f, "\n");
        va_end(args);
        fflush(f);
        fclose(f);
    }
}

DWORD WINAPI DestructThread(LPVOID) {
    pandoraLog("DestructThread: Started");

    // PASO 1: seÃƒÆ’Ã‚Â±al de parada ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â swap_buffers sale por el early bail
    g_Running = false;
	ShutdownAltAuthWorker();
    ShutdownPlayerHeadLoader();
    g_MenuVisible = false;
    g_ImGuiReady = false;
    MenuClose();
    if (g_LogicThreadHandle) {
        pandoraLog("DestructThread: Waiting for LogicThread...");
        WaitForSingleObject(g_LogicThreadHandle, INFINITE);
        pandoraLog("DestructThread: LogicThread joined.");
        CloseHandle(g_LogicThreadHandle);
        g_LogicThreadHandle = nullptr;
    }
    // All hooks and Java state were already released by LogicThread.
    g_OurImGuiCtx = nullptr;
    pandoraLog("DestructThread: Sleeping 1000ms...");
    Sleep(1000);

    pandoraLog("DestructThread: Calling FreeLibraryAndExitThread...");
    // Descargar DLL solo cuando todos los hilos propietarios terminaron.
    FreeLibraryAndExitThread(myModule, 0);
    return 0;
}
// LogicThread owns runtime work; the former empty GUI polling thread was removed.
    // DestructThread maneja toda la limpieza ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â este hilo solo espera y sale
// ============================================================
// LOGICTHREAD
// ============================================================
DWORD WINAPI LogicThread(LPVOID lpParam) {
    const bool sdkInitialized = sdk::init();
    if (!sdkInitialized) { pandoraLog("[LOGIC] [!] FATAL ERROR: Could not connect to the Java process.\n"); }
    else { 
        mapper::initialize(); 
        features::visual::nametags::initialize_hook();
    }
    if (hooks::initialize() != 0) { pandoraLog("[CLOUD] [!] Warning: ArrayList hooks could not be initialized.\n"); }

    // FIX: Signal the injector that initialization is complete.
    // The injector creates a Named Event before calling LoadLibraryA and waits
    // on it. We signal it here after JNI, mapper, and hooks are all ready.
    // This eliminates the race condition where modules like Refill wouldn't
    // work because the injector reported success before init finished.
    {
        std::string eventName = "Global\\pandoraClientReady_" + std::to_string(GetCurrentProcessId());
        HANDLE readyEvent = OpenEventA(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
        if (readyEvent) {
            SetEvent(readyEvent);
            CloseHandle(readyEvent);
            pandoraLog("[LOGIC] Signaled injector: init complete.");
        }
    }

    TriggerNotification("pandora", "Client loaded", "CLOUD");
    TriggerNotification("Keybind", "Insert to open menu", "KEYBIND");

    ULONGLONG lastIntegrityCheck = 0;
    ULONGLONG lastIdentityRefresh = 0;

    while (g_Running) {
        const ULONGLONG loopNow = GetTickCount64();
        if (loopNow - lastIntegrityCheck >= 1000) {
            lastIntegrityCheck = loopNow;
            AntiDebugChecks();
            AntiCrackTimeSync();
        }

        if (g_ShouldDestruct) {
            g_MenuVisible = false; // Close menu immediately
            
            // PASO 1: Enviar notificacion y dejar que OpenGL la dibuje
            TriggerNotification("Destruct successful", "Self Destruct.", "DESTRUCT");
            
            // Dormimos 2.5s para que el usuario VEA la notificacion de despedida.
            // Durante este tiempo, g_Running sigue true, OpenGL y JNI siguen vivos y no hay crash.
            Sleep(2500);
            
            // PASO 2: Detener ejecucion general (hace que swap_buffers limpie ImGui)
            pandoraLog("LogicThread: g_Running = false");
            g_Running = false;

            // ImGui/OpenGL and WndProc belong to the render thread. Never continue
            // with a partial unload: wait until SwapBuffers confirms complete cleanup.
            const ULONGLONG renderCleanupDeadline = GetTickCount64() + 3000ULL;
            while (!hooks::g_ImGuiCleanupDone.load(std::memory_order_acquire) &&
                   GetTickCount64() < renderCleanupDeadline) {
                Sleep(10);
            }
            const bool renderCleanupTimedOut =
                !hooks::g_ImGuiCleanupDone.load(std::memory_order_acquire);

            pandoraLog("LogicThread: ImGui cleanup finished/timed out");

            // No OpenGL/network callback may remain before Java global refs
            // or this DLL's executable memory are released.
            features::visual::render_valid.store(false, std::memory_order_release);
            features::visual::nametags::enabled = false;
            features::visual::player_esp_2d::enabled = false;
            features::visual::player_esp_3d::enabled = false;
            features::visual::tracers::enabled = false;

            // Stop and drain only the native hooks installed by
            // hooks::initialize(). JNIHook owns its independent JVM lifecycle
            // and is shut down later using its original, proven order.
            hooks::uninitialize();
            // We MUST always call abandon_imgui_after_render_timeout() because it handles
            // sending WM_NULL to remove the WndProc hook on the main thread, regardless
            // of whether OpenGL ImGui cleanup timed out or succeeded.
            hooks::abandon_imgui_after_render_timeout();


            // Shut down background threads BEFORE detaching JVM
            features::combat::auto_click::shutdown();
            features::friends::shutdown();
            
            // Forzar restauracion de nametags vanilla
            if (sdkInitialized) {
                mapper::__minecraft minecraft;
                if (minecraft.object) {
                    features::visual::nametags::shutdown(minecraft);
                }
            }
            
            // Apagar y desenganchar JNIHook para que la JVM no llame a memoria vacia
            if (sdkInitialized) {
                pandoraLog("LogicThread: Uninitializing JNIHook...");
                features::visual::nametags::uninitialize_hook();

                // Never leave Lunar's live Session object carrying an offline
                // name after this DLL unloads. Otherwise the next injection
                // captures that offline identity as if it were the premium
                // session and Restore can no longer know the real account.
                if (sdk::jni && !g_AltManagerPremiumStringFields.empty() &&
                    !g_AltManagerPremiumName.empty() &&
                    g_AltManagerPremiumName != "Unknown") {
                    mapper::__minecraft minecraft;
                    if (minecraft.object) {
                        jfieldID sessionField = nullptr;
                        jobject liveSession = nullptr;
                        if (AltManagerFindSession(sdk::jni, minecraft.object,
                                                  sessionField, liveSession) &&
                            liveSession) {
                            AltManagerRestorePremiumSession(sdk::jni, liveSession);
                            AltManagerSetSessionName(sdk::jni, liveSession,
                                g_AltManagerPremiumName.c_str());
                            sdk::jni->DeleteLocalRef(liveSession);
                        }
                    }
                }

                if (g_AltManagerPremiumSession && sdk::jni) {
                    sdk::jni->DeleteGlobalRef(g_AltManagerPremiumSession);
                    g_AltManagerPremiumSession = nullptr;
                }
                if (sdk::jni) {
                    for (auto& savedField : g_AltManagerPremiumStringFields) {
                        if (savedField.second) sdk::jni->DeleteGlobalRef(savedField.second);
                    }
                }
                g_AltManagerPremiumStringFields.clear();

                // Release every Java-owned reference before detaching this thread.
                pandoraLog("LogicThread: Uninitializing JNI mapping...");
                hooks::uninitialize_jni();
                mapper::uninitialize();
            }
            pandoraLog("LogicThread: Detaching JVM...");
            if (sdkInitialized && sdk::jvm) {
                sdk::jvm->DetachCurrentThread();
                sdk::jni = nullptr;
                sdk::jvmti = nullptr;
            }
            
            // PASO 3: Iniciar el hilo final de limpieza (descarga la DLL)
            pandoraLog("LogicThread: Spawning DestructThread...");
            HANDLE destructThread = CreateThread(
                nullptr, 0, DestructThread, nullptr, 0, nullptr);
            if (destructThread) CloseHandle(destructThread);
            return 0;
        }
        if (!g_GameWindow || !IsWindow(g_GameWindow)) {
            g_GameWindow = FindWindowA("LWJGL", nullptr);
            if (!g_GameWindow) g_GameWindow = FindWindowA("GLFW30", nullptr);
            features::visual::window = g_GameWindow;
        }

        features::visual::player_esp_2d::enabled = gui_esp_enabled && gui_whip_esp_render_mode == 0;
        features::visual::player_esp_2d::draw_health = gui_whip_esp_show_healthbar;
        features::visual::player_esp_2d::draw_invisible_players = true;

        features::visual::player_esp_3d::enabled = gui_esp_enabled && gui_whip_esp_render_mode == 1;
        features::visual::player_esp_3d::draw_health = gui_whip_esp_show_healthbar;
        features::visual::player_esp_3d::draw_invisible_players = true;
        features::visual::player_esp_3d::color = mapper::__vec4{ gui_whip_esp_neutral_color[0], gui_whip_esp_neutral_color[1], gui_whip_esp_neutral_color[2], gui_whip_esp_neutral_color[3] };
        
        features::visual::nametags::enabled = gui_nametags_enabled;
        features::visual::nametags::show_equipment = gui_nametags_show_equipment;
        features::visual::nametags::show_enchantments = gui_nametags_show_enchantments;
        features::visual::nametags::draw_health = gui_nametags_draw_health;
        features::visual::nametags::draw_distance = gui_nametags_draw_distance;
        features::visual::nametags::draw_hurt_time = gui_nametags_draw_hurt_time;
        features::visual::nametags::draw_invisible_players = true;
        features::visual::nametags::background = gui_nametags_background;        features::visual::nametags::use_fake_name = gui_nametags_use_fake_name;
        features::visual::nametags::fake_name = gui_nametags_fake_name;
        features::visual::nametags::color = mapper::__vec4{ gui_nametags_color[0],gui_nametags_color[1],gui_nametags_color[2],gui_nametags_color[3] };
        features::visual::tracers::enabled = gui_tracers_enabled;
        features::visual::tracers::draw_distance = gui_tracers_draw_distance;
        features::visual::tracers::draw_hurt_time = gui_tracers_draw_hurt_time;
        features::visual::tracers::draw_invisible_players = true;
        features::visual::tracers::thickness = gui_tracers_thickness;
        features::visual::tracers::color = mapper::__vec4{ gui_tracers_color_4[0],gui_tracers_color_4[1],gui_tracers_color_4[2],gui_tracers_color_4[3] };

        features::combat::auto_click::min_cps = (double)gui_min_cps;
        features::combat::auto_click::max_cps = (double)gui_max_cps;
        features::combat::auto_click::inventory_cps = (double)gui_inv_cps;
        features::combat::auto_click::break_blocks = gui_ac_break_blocks;
        features::combat::auto_click::click_method = gui_ac_click_method;
        features::combat::aim_assist::speed = (double)gui_aa_speed;
        features::combat::aim_assist::distance = (double)gui_aa_dist;
        features::combat::aim_assist::fov = (double)gui_aa_fov;
        features::combat::aim_assist::stick = gui_aa_stick;
        features::combat::aim_assist::silent = gui_aa_silent;
        features::combat::reach::enabled = gui_reach_enabled;
        features::combat::reach::min_distance = gui_reach_min_distance; features::combat::reach::max_distance = gui_reach_max_distance; features::combat::reach::hitbox_enabled = gui_reach_hitbox_enabled; features::combat::reach::hitbox_size = gui_reach_hitbox_size;
        features::combat::reach::chance = gui_reach_chance;
        features::combat::reach::ground_only = gui_reach_ground_only;
        features::combat::reach::weapon_only = gui_reach_weapon_only;
        features::combat::reach::liquid_check = gui_reach_liquid_check;
        features::combat::reach::combo_mode = gui_reach_combo_mode;
        features::combat::refill::delay_ms = (int)gui_refill_delay;
        features::combat::velocity::enabled = gui_velo_enabled;
        features::combat::velocity::air_only = gui_velo_air_only;
        features::combat::velocity::moving_only = gui_velo_moving_only;
        features::combat::velocity::weapon_only = gui_velo_weapon_only;
        features::combat::velocity::push_back = gui_velo_push_back;
        features::combat::velocity::universocraft_bypass = gui_velo_universocraft_bypass;
        features::combat::velocity::clicking_only = gui_velo_clicking_only;
        features::combat::velocity::horizontal = gui_velo_horizontal;
        features::combat::velocity::vertical = gui_velo_vertical;
        features::combat::velocity::chance = gui_velo_chance;
        features::combat::velocity::delay = gui_velo_delay;
        
        
        
        // -- NUEVO: sincronizar NoHitDelay ---------------------
        features::combat::no_hit_delay::enabled = gui_nohitdelay_enabled;


        features::movement::no_jump_delay::enabled = gui_nojumpdelay_enabled;
        features::movement::no_slow::enabled = gui_noslow_enabled;
        features::movement::no_item_release::enabled = gui_noitemrelease_enabled;
        features::movement::no_item_release::food = gui_noitemrelease_food;
        features::movement::sprint::enabled = gui_sprint_enabled;
        features::movement::sprint::omni = gui_sprint_omni;

        features::misc::blink::enabled = gui_blink_enabled;
        features::misc::blink::show_path = gui_blink_show_path;
        features::misc::blink::show_timer = gui_blink_show_timer;
        features::misc::blink::path_color[0] = gui_blink_path_color[0];
        features::misc::blink::path_color[1] = gui_blink_path_color[1];
        features::misc::blink::path_color[2] = gui_blink_path_color[2];
        features::misc::blink::timer_limit = gui_blink_timer_limit;
        features::visual::arraylist::enabled = gui_arraylist_enabled;
        features::visual::arraylist::watermark = gui_arraylist_watermark;
        for (auto& mod : modules) {
            if (mod.name == "Blink")         features::misc::blink::bind = mod.keybind;
            if (mod.name == "Refill")        features::combat::refill::bind = mod.keybind;
            if (mod.name == "ArmorSwitcher") gui_armorswitcher_bind = mod.keybind;
            // Sync g_KeybindMap for modules so SaveConfig captures latest binds
            g_KeybindMap[mod.name] = mod.keybind;
        }

        if (sdk::jni != nullptr) {
            mapper::__minecraft minecraft;
            // GUI/session state must be updated even when there is no loaded
            // world. minecraft.is_valid() intentionally requires a world and
            // local player, which is false on the Multiplayer screen.
            if (minecraft.object != nullptr) {
                auto menuScreen = minecraft.get_current_screen();
                AltManagerUpdateOnGameThread(sdk::jni, minecraft.object, menuScreen.object);
                g_PlayerInGui = menuScreen.object != nullptr;

                static bool rightShiftWasDownOutsideWorld = false;
                const bool gameHasFocus = g_GameWindow &&
                    GetForegroundWindow() == g_GameWindow;
                const bool rightShiftDownOutsideWorld = gameHasFocus &&
                    (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
                if (rightShiftDownOutsideWorld && !rightShiftWasDownOutsideWorld &&
                    g_OnMultiplayerScreen.load(std::memory_order_acquire) && !g_MenuVisible) {
                    g_AltManagerMode = true;
                    g_MenuVisible = true;
                    if (g_GameWindow) MenuOpen(g_GameWindow);
                }
                rightShiftWasDownOutsideWorld = rightShiftDownOutsideWorld;
                
                if (menuScreen.object != nullptr) {
                    sdk::jni->DeleteLocalRef(menuScreen.object);
                }
            } else {
                g_PlayerInGui = false;
            }

            const bool worldLoaded = minecraft.is_valid();
            g_MinecraftWorldLoaded.store(worldLoaded, std::memory_order_release);
            if (worldLoaded) {
                // LogicThread is the single owner of visual availability.
                // Keep it true for the whole loaded-world lifetime; toggling it
                // during a loop makes SwapBuffers skip complete frames.
                features::visual::render_valid.store(true, std::memory_order_release);
                // Verificar si estamos en un estado valido para renderizar
                {
                    if (loopNow - lastIdentityRefresh >= 1000) {
                        lastIdentityRefresh = loopNow;
                        auto lp = minecraft.get_local_player();
                        if (lp.object != nullptr) {
                            std::string pname = lp.get_name();
                            if (!pname.empty()) g_CachedPlayerName = pname;
                            std::string sip = minecraft.get_server_ip();
                            g_CachedServerIP = sip.empty() ? "Singleplayer" : sip;
                        }
                    }

                    // OpenGL entrega matrices sincronizadas con el frame. ActiveRenderInfo
                    // queda como respaldo para clientes donde esa captura no aparece; mezclar
                    // ambas fuentes en cada tick producia saltos al girar la camara.
                    const unsigned long long now =
                        static_cast<unsigned long long>(GetTickCount64());
                    const bool has_fresh_gl_matrices =
                        features::visual::matrix_capture_tick != 0 &&
                        now >= features::visual::matrix_capture_tick &&
                        now - features::visual::matrix_capture_tick <= 150;
                    if (features::visual::render_valid && !has_fresh_gl_matrices) {
                        auto mv = mapper::__active_render_info::get_model_view();
                        auto proj = mapper::__active_render_info::get_projection();
                        if (mv.size() == 16 && proj.size() == 16) {
                            for (int i = 0; i < 16; i++) {
                                features::visual::model_view_matrix[i] = mv[i];
                                features::visual::projection_matrix[i] = proj[i];
                            }
                            if ((features::visual::view_port[2] <= 0 ||
                                 features::visual::view_port[3] <= 0) &&
                                g_GameWindow != nullptr) {
                                RECT clientRect = {};
                                if (GetClientRect(g_GameWindow, &clientRect)) {
                                    features::visual::view_port[0] = 0;
                                    features::visual::view_port[1] = 0;
                                    features::visual::view_port[2] =
                                        clientRect.right - clientRect.left;
                                    features::visual::view_port[3] =
                                        clientRect.bottom - clientRect.top;
                                }
                            }
                        }
                    }
                }
                features::run_on_run_tick(minecraft);

                // Entity-list JNI scans do not benefit from the 1 ms combat
                // loop. Run them at 125 Hz while SwapBuffers still renders the
                // latest immutable snapshots at the full game frame rate.
                static ULONGLONG nextVisualScan = 0;
                if (loopNow >= nextVisualScan) {
                    nextVisualScan = loopNow + 8;
                    features::visual::nametags::run(minecraft);
                    features::visual::player_esp_2d::run(minecraft);
                    features::visual::player_esp_3d::run(minecraft);
                    features::visual::tracers::run(minecraft);
                    features::visual::hit_markers::run(minecraft);
                }
            } else {
                features::visual::render_valid.store(false, std::memory_order_release);
            }
        } else {
            g_MinecraftWorldLoaded.store(false, std::memory_order_release);
        }

        Sleep(1);
    }

    // DestructThread se encarga de toda la limpieza ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â LogicThread solo termina
    return 0;
}

// ============================================================
// LAUNCHER THREAD
// ============================================================
static DWORD WINAPI LauncherThread(LPVOID lpParam)
{
    g_LogicThreadHandle = CreateThread(nullptr, 0, LogicThread, lpParam, 0, nullptr);
    return 0;
}

// ============================================================
// DLLMAIN
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        myModule = hModule;
        // Keep the PE header intact: Windows needs valid module metadata for
        // reliable unwinding, FreeLibrary and later reinjection.
        HANDLE launcher = CreateThread(nullptr, 0, LauncherThread, hModule, 0, nullptr);
        if (launcher) CloseHandle(launcher);
    }
    return TRUE;
}










