#include "../../features/features.hpp"
#include "../../mapper/mapper.hpp"
#include <jnihook.h>

namespace features::visual::nametags
{
    static jmethodID g_render_name_method = nullptr;
    static bool g_jnihook_initialized = false;
    static bool g_hook_attached = false;
    static bool g_attach_failed = false;

    static void JNICALL cancel_render_name(JNIEnv*, jobject, jobject, jdouble, jdouble, jdouble) {}

    static jmethodID find_render_name()
    {
        const mapper::__class& renderer = mapper::classes["RendererLivingEntity"];
        if (!renderer.klass || !sdk::jni) return nullptr;
        const std::string& minecraft_name = mapper::classes["Minecraft"].name;
        const char* name = "renderName";
        const char* sig = "(Lnet/minecraft/entity/EntityLivingBase;DDD)V";
        if (minecraft_name == "ave") { name = "a"; sig = "(Lpr;DDD)V"; }
        else if (minecraft_name == "bao") { name = "a"; sig = "(Lsv;DDD)V"; }
        jmethodID method = sdk::jni->GetMethodID(renderer.klass, name, sig);
        if (!method && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        return method;
    }

    void initialize_hook()
    {
        if (JNIHook_Init(sdk::jvm) != JNIHOOK_OK) return;
        g_jnihook_initialized = true;
        g_render_name_method = find_render_name();
        if (!g_render_name_method) {
            JNIHook_Shutdown();
            g_jnihook_initialized = false;
        }
    }

    void sync_hide_vanilla_hook(bool should_attach)
    {
        if (!g_jnihook_initialized || !g_render_name_method) return;
        if (should_attach == g_hook_attached) return;
        if (should_attach && g_attach_failed) return;
        const jnihook_result_t result = should_attach
            ? JNIHook_Attach(g_render_name_method, (void*)cancel_render_name, nullptr)
            : JNIHook_Detach(g_render_name_method);
        if (result == JNIHOOK_OK) {
            g_hook_attached = should_attach;
            g_attach_failed = false;
        } else if (should_attach) {
            g_attach_failed = true;
        }
    }

    void uninitialize_hook()
    {
        if (!g_jnihook_initialized) return;
        if (g_hook_attached) sync_hide_vanilla_hook(false);
        JNIHook_Shutdown();
        g_render_name_method = nullptr;
        g_jnihook_initialized = false;
        g_hook_attached = false;
        g_attach_failed = false;
    }
}
