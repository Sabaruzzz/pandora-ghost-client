#include "hooks.hpp"
#include "../features/features.hpp"
#include "../sdk.hpp"

extern bool g_MenuVisible;
extern std::atomic<bool> g_Running;

struct JNIFrame {
    JNIEnv* env;
    JNIFrame(JNIEnv* e, int cap) : env(e) { if (env) env->PushLocalFrame(cap); }
    ~JNIFrame() { if (env) env->PopLocalFrame(nullptr); }
};

namespace hooks
{
    auto get_time(JNIEnv* jni, jclass klass) -> __int64
    {
        if (!g_Running) return ((jlong(__stdcall*)(JNIEnv*, jclass))original_get_time)(jni, klass);

        if (jni != nullptr && klass != nullptr && sdk::jni != nullptr)
        {
            JNIEnv* backup_jni = sdk::jni;
            sdk::jni = jni; // Usamos el JNI del hilo principal del juego

            mapper::__minecraft minecraft;
            if (minecraft.is_valid())
            {
                // JNIFrame para evitar el agotamiento de Local References (causa de los parpadeos del ESP)
                // Usamos una capacidad alta (1024) porque recorrer entidades genera muchas referencias.
                JNIFrame frame(sdk::jni, 1024);

                // ============================================================
                // TICK PRINCIPAL: Ejecuta TODOS los modulos de combate,
                // movimiento, misc y visuales desde features.cpp
                // ============================================================
                features::run_on_run_tick(minecraft);

                // ============================================================
                // VISUALES: Recoleccion de datos JNI para ESP/Nametags/etc.
                // Se ejecuta aqui porque get_time() corre en el hilo de
                // renderizado de Minecraft con JNI valido.
                // El DIBUJO real ocurre en swap_buffers -> render()
                // ============================================================
                features::visual::nametags::run(minecraft);
                
                features::visual::player_esp_2d::run(minecraft);
                features::visual::player_esp_3d::run(minecraft);
                features::visual::tracers::run(minecraft);
                
                // Removed chams from here because rendering in get_time crashes/breaks matrix
                features::misc::blink::render_world(minecraft);
            }
            else
            {
                // Mundo no listo (cargando, menu, desconectando):
                // Decirle al hilo de AutoClick que NO dispare clicks
                features::combat::auto_click::shutdown();
            }

            sdk::jni = backup_jni;
        }

        // Devolvemos el control al reloj original del juego
        return ((jlong(__stdcall*)(JNIEnv*, jclass))original_get_time)(jni, klass);
    }
}
