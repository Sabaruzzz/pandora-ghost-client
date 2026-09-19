#include <winsock2.h>
#include "hooks.hpp"
#include "../../../sdk.hpp"
#include "../features/features.hpp"
#include <MinHook.h>
#include <gl/GL.h>
#include <iostream>
#include <cmath>

extern std::atomic<bool> g_PlayerInGui;
extern bool g_MenuVisible;
extern "C" void pandoraLog(const char* format, ...);
namespace hooks
{
    bool capture_render_frame_java_state()
    {
        JNIEnv* env = nullptr;
        if (!sdk::jvm || sdk::jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || !env)
            return false;
        auto& mc_class = mapper::classes["Minecraft"];
        auto& rm_class = mapper::classes["RenderManager"];
        auto& timer_class = mapper::classes["Timer"];
        mapper::__field mc_f = mc_class.get_field("theMinecraft", mc_class.signature);
        if (!mc_f.identifier) mc_f = mc_class.get_field("field_71432_P", mc_class.signature);
        if (!mc_f.identifier) mc_f = mc_class.get_field("S", mc_class.signature);
        if (!mc_f.identifier) return false;
        jobject mc = env->GetStaticObjectField(mc_class.klass, mc_f.identifier);
        if (!mc) return false;

        mapper::__field rm_f = mc_class.get_field("renderManager", rm_class.signature);
        if (!rm_f.identifier) rm_f = mc_class.get_field("field_175616_W", rm_class.signature);
        if (!rm_f.identifier) rm_f = mc_class.get_field("aa", rm_class.signature);
        mapper::__field timer_f = mc_class.get_field("timer", timer_class.signature);
        if (!timer_f.identifier) timer_f = mc_class.get_field("field_71428_T", timer_class.signature);
        if (!timer_f.identifier) timer_f = mc_class.get_field("Y", timer_class.signature);
        jobject rm = rm_f.identifier ? env->GetObjectField(mc, rm_f.identifier) : nullptr;
        jobject timer = timer_f.identifier ? env->GetObjectField(mc, timer_f.identifier) : nullptr;

        mapper::__field fx = rm_class.get_field("renderPosX", "D");
        if (!fx.identifier) fx = rm_class.get_field("field_78725_b", "D");
        if (!fx.identifier) fx = rm_class.get_field("o", "D");
        mapper::__field fy = rm_class.get_field("renderPosY", "D");
        if (!fy.identifier) fy = rm_class.get_field("field_78726_c", "D");
        if (!fy.identifier) fy = rm_class.get_field("p", "D");
        mapper::__field fz = rm_class.get_field("renderPosZ", "D");
        if (!fz.identifier) fz = rm_class.get_field("field_78723_d", "D");
        if (!fz.identifier) fz = rm_class.get_field("q", "D");
        mapper::__field fp = timer_class.get_field("renderPartialTicks", "F");
        if (!fp.identifier) fp = timer_class.get_field("field_74281_c", "F");
        if (!fp.identifier) fp = timer_class.get_field("c", "F");

        const bool ok = rm && timer && fx.identifier && fy.identifier && fz.identifier && fp.identifier;
        if (ok) {
            features::visual::render_camera_x = env->GetDoubleField(rm, fx.identifier);
            features::visual::render_camera_y = env->GetDoubleField(rm, fy.identifier);
            features::visual::render_camera_z = env->GetDoubleField(rm, fz.identifier);
            features::visual::render_partial_ticks = env->GetFloatField(timer, fp.identifier);

            // ActiveRenderInfo is filled immediately after
            // setupCameraTransform. Reading it at SwapBuffers gives the exact
            // matrices used for the entities in this frame; glClear happens
            // before camera setup and therefore contains the previous frame.
            auto& ari = mapper::classes["ActiveRenderInfo"];
            static jclass float_buffer_class = nullptr;
            static jmethodID float_buffer_get = nullptr;
            if (!float_buffer_class) {
                jclass local = env->FindClass("java/nio/FloatBuffer");
                if (local) {
                    float_buffer_class = (jclass)env->NewGlobalRef(local);
                    float_buffer_get = env->GetMethodID(float_buffer_class, "get", "(I)F");
                    env->DeleteLocalRef(local);
                }
            }
            auto read_matrix = [&](const char* named, const char* srg,
                                   const char* obf, double* output) -> bool {
                mapper::__field field = ari.get_field(named, "Ljava/nio/FloatBuffer;");
                if (!field.identifier) field = ari.get_field(srg, "Ljava/nio/FloatBuffer;");
                if (!field.identifier) field = ari.get_field(obf, "Ljava/nio/FloatBuffer;");
                if (!field.identifier || !float_buffer_get) return false;
                jobject buffer = env->GetStaticObjectField(ari.klass, field.identifier);
                if (!buffer) return false;
                for (int i = 0; i < 16; ++i)
                    output[i] = (double)env->CallFloatMethod(buffer, float_buffer_get, i);
                env->DeleteLocalRef(buffer);
                return !env->ExceptionCheck();
            };
            const bool matrices_ok = ari.klass &&
                read_matrix("MODELVIEW", "field_78726_a", "a", features::visual::model_view_matrix) &&
                read_matrix("PROJECTION", "field_78725_b", "b", features::visual::projection_matrix);
            if (matrices_ok)
                features::visual::matrix_capture_tick =
                    static_cast<unsigned long long>(GetTickCount64());
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (timer) env->DeleteLocalRef(timer);
        if (rm) env->DeleteLocalRef(rm);
        env->DeleteLocalRef(mc);
        return ok;
    }

    std::atomic<unsigned long> active_callbacks{ 0 };

    void wait_for_callbacks()
    {
        while (active_callbacks.load(std::memory_order_acquire) != 0)
            Sleep(1);
    }
}
#ifndef GL_COMBINE
#define GL_COMBINE                        0x8570
#define GL_COMBINE_RGB                    0x8571
#define GL_COMBINE_ALPHA                  0x8572
#define GL_SOURCE0_RGB                    0x8580
#define GL_SOURCE1_RGB                    0x8581
#define GL_SOURCE0_ALPHA                  0x8588
#define GL_SOURCE1_ALPHA                  0x8589
#define GL_OPERAND0_RGB                   0x8590
#define GL_OPERAND1_RGB                   0x8591
#define GL_OPERAND0_ALPHA                 0x8598
#define GL_OPERAND1_ALPHA                 0x8599
#define GL_PRIMARY_COLOR                  0x8577
#endif

namespace network_hooks {
    typedef int(WSAAPI* WSASend_t)(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, LPWSAOVERLAPPED, LPWSAOVERLAPPED_COMPLETION_ROUTINE);
    extern WSASend_t original_WSASend;
    int WSAAPI hooked_WSASend(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount, LPDWORD lpNumberOfBytesSent, DWORD dwFlags, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine);
}

namespace hooks
{
    std::atomic<bool> frame_matrices_captured{ false };
    std::atomic<DWORD> main_thread_id{ 0 };

    static int clear_depth_count = 0;
    static jobject global_gs_obj = nullptr;
    static jfieldID hide_gui_fid = nullptr;
    static std::atomic<bool> minhook_initialized{ false };

    void toggle_f1(bool state) {
        // HIDE NAME IMPLICITO: Si el ESP de Nametags esta apagado, forzamos apagar la supresion
        if (!features::visual::nametags::enabled) {
            state = false;
        }
        
        // OBTENER JNIEnv ESPECIFICO DEL HILO DE RENDERIZADO
        // NUNCA usar sdk::jni aqui porque ese pertenece al LogicThread. Usarlo cruzado causa crasheos al inyectar.
        JNIEnv* env = nullptr;
        if (!sdk::jvm || sdk::jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK || !env) return;
        
        // Inicializar cache global UNA SOLA VEZ para evitar fugas de memoria
        if (!global_gs_obj || !hide_gui_fid) {
            jclass mc_class = mapper::classes["Minecraft"].klass;
            if (!mc_class) return;
            
            jfieldID mc_fid = env->GetStaticFieldID(mc_class, "theMinecraft", "Lnet/minecraft/client/Minecraft;");
            if (!mc_fid) { env->ExceptionClear(); mc_fid = env->GetStaticFieldID(mc_class, "field_71432_P", "Lnet/minecraft/client/Minecraft;"); }
            if (!mc_fid) { env->ExceptionClear(); mc_fid = env->GetStaticFieldID(mc_class, "S", "Lnet/minecraft/client/Minecraft;"); }
            if (!mc_fid) { env->ExceptionClear(); mc_fid = env->GetStaticFieldID(mc_class, "M", "Lnet/minecraft/client/Minecraft;"); }
            if (!mc_fid) { env->ExceptionClear(); return; }
            
            jobject mc_obj = env->GetStaticObjectField(mc_class, mc_fid);
            if (!mc_obj) return;
            
            const std::string& gs_sig = mapper::classes["GameSettings"].signature;
            if (gs_sig.empty()) { env->DeleteLocalRef(mc_obj); return; }
            
            jfieldID gs_fid = env->GetFieldID(mc_class, "gameSettings", gs_sig.c_str());
            if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "field_71474_y", gs_sig.c_str()); }
            if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "t", gs_sig.c_str()); }
            if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "N", gs_sig.c_str()); }
            
            if (gs_fid) {
                jobject local_gs = env->GetObjectField(mc_obj, gs_fid);
                if (local_gs) {
                    // Cachear GameSettings como Referencia Global (NUNCA DEBE MORIR)
                    global_gs_obj = env->NewGlobalRef(local_gs);
                    
                    jclass gs_class = env->GetObjectClass(local_gs);
                    if (gs_class) {
                        hide_gui_fid = env->GetFieldID(gs_class, "hideGUI", "Z");
                        if (!hide_gui_fid) { env->ExceptionClear(); hide_gui_fid = env->GetFieldID(gs_class, "field_74319_N", "Z"); }
                        if (!hide_gui_fid) { env->ExceptionClear(); hide_gui_fid = env->GetFieldID(gs_class, "aw", "Z"); } // 1.8 obf
                        if (!hide_gui_fid) { env->ExceptionClear(); hide_gui_fid = env->GetFieldID(gs_class, "af", "Z"); } // 1.7 obf
                        env->DeleteLocalRef(gs_class);
                    }
                    env->DeleteLocalRef(local_gs); // Eliminar ref local original
                }
            }
            env->DeleteLocalRef(mc_obj); // Eliminar ref local original
            if (!global_gs_obj || !hide_gui_fid) return;
        }
        
        // CERO ALOCACIONES: Leer y escribir usando la referencia global y primitives (jboolean)
        // PROTECCION DE EXCEPCIONES: Si un modulo de combate fallÃƒÂ³ al golpear, evitamos el crasheo de JNI.
        if (env->ExceptionCheck()) env->ExceptionClear();
        
        bool current_state = env->GetBooleanField(global_gs_obj, hide_gui_fid) == JNI_TRUE;
        if (env->ExceptionCheck()) env->ExceptionClear();
        
        if (current_state != state) {
            env->SetBooleanField(global_gs_obj, hide_gui_fid, state ? JNI_TRUE : JNI_FALSE);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    auto gl_clear_hook(unsigned int mask) -> void
    {
        callback_guard callback;
        typedef void(__stdcall* glClear_t)(unsigned int);

        // Si todavia no conocemos el hilo principal, o si este NO es el hilo principal,
        // no procesamos la captura de matrices ni Chams (previene crash por Worker Threads de Optifine).
        if (main_thread_id == 0 || GetCurrentThreadId() != main_thread_id)
        {
            return ((glClear_t)original_gl_clear)(mask);
        }

        static constexpr unsigned int DEPTH_BIT = 0x0100;
        static constexpr unsigned int COLOR_BIT = 0x4000;

        const bool has_depth = (mask & DEPTH_BIT) != 0;
        const bool has_color = (mask & COLOR_BIT) != 0;

        // A depth+color clear starts a new world frame. Reset before capturing
        // it; resetting after capture left the latch open and later hand/HUD
        // passes replaced the world matrices, which made overlays jitter.
        if (has_depth && has_color)
        {
            hooks::frame_matrices_captured = false;
        }

        // F1 control removed Ã¢â‚¬â€ user can use F1 freely

        // Capturar la primera matriz 3D del frame (el Mundo)
        if (has_depth && !frame_matrices_captured)
        {
            GLint vp[4] = {};
            glGetIntegerv(GL_VIEWPORT, vp);

            if (vp[2] > 0 && vp[3] > 0)
            {
                double proj[16] = {};
                glGetDoublev(GL_PROJECTION_MATRIX, proj);

                // Capturamos si es perspectiva 3D (proj[11] es -1.0)
                if (proj[11] < -0.5)
                {
                    features::visual::view_port[0] = vp[0];
                    features::visual::view_port[1] = vp[1];
                    features::visual::view_port[2] = vp[2];
                    features::visual::view_port[3] = vp[3];

                    glGetDoublev(GL_MODELVIEW_MATRIX, features::visual::model_view_matrix);
                    for (int i = 0; i < 16; ++i)
                        features::visual::projection_matrix[i] = proj[i];

                    features::visual::render_frame_snapshot_valid =
                        capture_render_frame_java_state();
                    features::visual::matrix_capture_tick =
                        static_cast<unsigned long long>(GetTickCount64());

                    // Bloqueamos para el resto del frame.
                    frame_matrices_captured = true;
                }
            }
        }

        // ============================================================
        // CHAMS RENDER: Se ejecuta UNA SOLA VEZ por frame.
        // glClear se llama multiples veces (sombras, agua, mundo, mano)
        return ((glClear_t)original_gl_clear)(mask);
    }

    // original_gl_ortho ya declarado en hooks.hpp
    auto gl_ortho_hook(double left, double right, double bottom, double top, double zNear, double zFar) -> void {
        callback_guard callback;
        typedef void(__stdcall* glOrtho_t)(double, double, double, double, double, double);
        if (GetCurrentThreadId() == main_thread_id) {
            // F1 control removed Ã¢â‚¬â€ user can use F1 freely
            
        }
        ((glOrtho_t)original_gl_ortho)(left, right, bottom, top, zNear, zFar);
    }

    auto initialize() -> __int32
    {
        g_ImGuiCleanupDone.store(false);
        if (MH_Initialize() != MH_OK) return 1;
        minhook_initialized.store(true);

        HMODULE opengl_module = GetModuleHandleA("opengl32.dll");
        if (opengl_module == nullptr) return 1;

        void* swap_addr = (void*)GetProcAddress(opengl_module, "wglSwapBuffers");
        MH_CreateHook(swap_addr, &hooks::swap_buffers, &hooks::original_swap_buffers);

        void* clear_addr = (void*)GetProcAddress(opengl_module, "glClear");
        void* ortho_addr = (void*)GetProcAddress(opengl_module, "glOrtho");
        MH_CreateHook(ortho_addr, &hooks::gl_ortho_hook, &hooks::original_gl_ortho);
        MH_CreateHook(clear_addr, &hooks::gl_clear_hook, &hooks::original_gl_clear);

        HMODULE ws2_module = GetModuleHandleA("ws2_32.dll");
        if (ws2_module) {
            void* wsasend_addr = (void*)GetProcAddress(ws2_module, "WSASend");
            if (wsasend_addr) {
                MH_CreateHook(wsasend_addr, (void*)&network_hooks::hooked_WSASend, (void**)&network_hooks::original_WSASend);
            }
        }

        if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
            return 1;
        return 0;
    }

    auto uninitialize_jni() -> void
    {
        // Forzar restauracion del F1 y liberar memoria JNI antes de salir
        if (global_gs_obj && hide_gui_fid && sdk::jvm) {
            JNIEnv* env = nullptr;
            bool attached = false;
            jint res = sdk::jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (res == JNI_EDETACHED) {
                if (sdk::jvm->AttachCurrentThread((void**)&env, nullptr) == JNI_OK) {
                    attached = true;
                }
            }
            if (env) {
                // Forzar apagado del hideGUI
                env->SetBooleanField(global_gs_obj, hide_gui_fid, JNI_FALSE);
                if (env->ExceptionCheck()) env->ExceptionClear();
                
                // Liberar la memoria global retenida
                env->DeleteGlobalRef(global_gs_obj);
                global_gs_obj = nullptr;
                hide_gui_fid = nullptr;
                
                if (attached) {
                    sdk::jvm->DetachCurrentThread();
                }
            }
        }
    }

    auto uninitialize() -> __int32
    {
        if (!minhook_initialized.exchange(false)) return 0;
        MH_DisableHook(MH_ALL_HOOKS);
        // Wait for a SwapBuffers callback that entered before DisableHook to
        // leave this DLL. A fixed Sleep could unload while rendering was still
        // executing and made the next injection crash.
        {
            std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);
        }
        wait_for_callbacks();
        MH_RemoveHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        return 0;
    }
}
