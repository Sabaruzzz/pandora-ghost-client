#pragma once
#include <windows.h>
#include <mutex>
#include <atomic>
#include <jni.h>

struct ImGuiContext; // forward declaration

namespace hooks
{
    // Refresh camera and ActiveRenderInfo matrices on the render thread, after
    // Minecraft has completed the current world frame.
    bool capture_render_frame_java_state();
    // Every callback that can execute code from this DLL participates in the
    // unload barrier. MinHook/JNI detachment prevents new entries; the final
    // unload then waits until all callbacks that were already in flight leave.
    extern std::atomic<unsigned long> active_callbacks;
    struct callback_guard {
        callback_guard() { active_callbacks.fetch_add(1, std::memory_order_acq_rel); }
        ~callback_guard() { active_callbacks.fetch_sub(1, std::memory_order_acq_rel); }
        callback_guard(const callback_guard&) = delete;
        callback_guard& operator=(const callback_guard&) = delete;
    };
    extern void wait_for_callbacks();

    extern auto initialize() -> __int32;
    extern auto uninitialize_jni() -> void;
    extern auto uninitialize() -> __int32;

    // --- SWAP BUFFERS ---
    inline void* original_swap_buffers = nullptr;
    extern auto swap_buffers(HDC hdc) -> __int32;

    // --- ROTATE (aim assist silent) ---
    inline void* original_rotate = nullptr;
    extern auto rotate(JNIEnv_* jni, _jclass* klass, float angle, float x, float y, float z, unsigned __int64 function) -> void;

    inline void* original_gl_ortho = nullptr;
    extern auto gl_ortho_hook(double left, double right, double bottom, double top, double zNear, double zFar) -> void;

    // --- GL CLEAR (MinHook directo sobre opengl32.glClear) ---
    // Captura matrices 3D (ModelView + Projection) cuando Minecraft
    // limpia el depth buffer antes de renderizar entidades.
    // CRITICO: sin este hook world_to_screen() siempre devuelve FLT_MAX
    // y ningun ESP ni nametag se dibuja.
    inline void* original_gl_clear = nullptr;
    extern std::atomic<bool> frame_matrices_captured;
    extern std::atomic<DWORD> main_thread_id;
    extern auto gl_clear_hook(unsigned int mask) -> void;

    inline void* original_glu_project = nullptr;
    extern auto glu_project_hook(double objx, double objy, double objz,
        const double modelMatrix[16], const double projMatrix[16],
        const int viewport[4], double* winx, double* winy, double* winz) -> int;

    // --- CLEAR JNI (reservado para RegisterNatives si se necesita) ---
    inline void* original_clear = nullptr;
    extern auto clear(JNIEnv* jni, jclass klass, __int32 mask, __int64 function) -> void;

    // --- GET TIME (RegisterNatives sobre Sys.ngetTime) ---
    inline void* original_get_time = nullptr;
    extern auto get_time(JNIEnv* jni, jclass klass) -> __int64;

    // Mutex global render
    inline std::recursive_mutex render_mutex;

    // Early ESP rendering (behind inventory GUI)
    extern std::atomic<bool> esp_rendered_this_frame;
    void render_esp_behind_gui();

    // ImGui cleanup flag for destruct synchronization
    extern std::atomic<bool> g_ImGuiCleanupDone;
    extern ImGuiContext* g_GameImGuiContext;
    void abandon_imgui_after_render_timeout();
}
