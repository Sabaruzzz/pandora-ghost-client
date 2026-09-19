#include "../features.hpp"
#include "../sdk.hpp"
#define NOMINMAX
#include <windows.h>

namespace features::movement::sprint
{
    static void set_sprint_key_state(JNIEnv* env, jobject mc_obj, bool state)
    {
        if (!env || !mc_obj) return;

        // Usar el mapper para obtener firmas dinamicas
        const std::string& gs_sig = mapper::classes["GameSettings"].signature;
        const std::string& kb_sig = mapper::classes["KeyBinding"].signature;
        if (gs_sig.empty() || kb_sig.empty()) return;

        jclass mc_class = env->GetObjectClass(mc_obj);
        if (!mc_class) return;

        // Buscar gameSettings con firma del mapper
        jfieldID gs_fid = env->GetFieldID(mc_class, "gameSettings", gs_sig.c_str());
        if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "field_71474_y", gs_sig.c_str()); }
        if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "t", gs_sig.c_str()); } // 1.8 obf
        if (!gs_fid) { env->ExceptionClear(); gs_fid = env->GetFieldID(mc_class, "N", gs_sig.c_str()); } // 1.7 obf

        if (gs_fid) {
            jobject gs_obj = env->GetObjectField(mc_obj, gs_fid);
            if (gs_obj) {
                jclass gs_class = env->GetObjectClass(gs_obj);

                // Buscamos keyBindSprint con firma del mapper
                jfieldID kb_fid = env->GetFieldID(gs_class, "keyBindSprint", kb_sig.c_str());
                if (!kb_fid) { env->ExceptionClear(); kb_fid = env->GetFieldID(gs_class, "field_151444_V", kb_sig.c_str()); }
                if (!kb_fid) { env->ExceptionClear(); kb_fid = env->GetFieldID(gs_class, "A", kb_sig.c_str()); } // 1.8 obf

                // 1.7.10: keyBindSprint no existe! Sprint se hacia con doble-tap W
                // Fallback: usar setSprinting directamente en vez de la tecla
                if (kb_fid) {
                    jobject kb_obj = env->GetObjectField(gs_obj, kb_fid);
                    if (kb_obj) {
                        jclass kb_class = env->GetObjectClass(kb_obj);

                        jfieldID pr_fid = env->GetFieldID(kb_class, "pressed", "Z");
                        if (!pr_fid) { env->ExceptionClear(); pr_fid = env->GetFieldID(kb_class, "field_74513_e", "Z"); }
                        if (!pr_fid) { env->ExceptionClear(); pr_fid = env->GetFieldID(kb_class, "i", "Z"); }
                        if (!pr_fid) { env->ExceptionClear(); pr_fid = env->GetFieldID(kb_class, "h", "Z"); }
                        if (!pr_fid) { env->ExceptionClear(); pr_fid = env->GetFieldID(kb_class, "g", "Z"); } // 1.7.10

                        if (pr_fid) {
                            env->SetBooleanField(kb_obj, pr_fid, state);
                        }
                        env->DeleteLocalRef(kb_class);
                        env->DeleteLocalRef(kb_obj);
                    }
                }
                env->DeleteLocalRef(gs_class);
                env->DeleteLocalRef(gs_obj);
            }
        }
        env->DeleteLocalRef(mc_class);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    void run(mapper::__minecraft& minecraft)
    {
        if (!enabled) return;

        JNIEnv* env = sdk::jni;
        if (!env) return;
        if (env->ExceptionCheck()) env->ExceptionClear();

        auto local_player = minecraft.get_local_player();
        if (!local_player.object) return;

        auto screen = minecraft.get_current_screen();
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
        if (screen.object != nullptr) {
            set_sprint_key_state(env, minecraft.object, false);
            return;
        }

        bool should_sprint = omni || (GetAsyncKeyState(0x57) & 0x8000) != 0;

        // Intentar via keybind primero (1.8)
        set_sprint_key_state(env, minecraft.object, should_sprint);

        // Fallback 1.7.10: Si la keybind no existe, usar setSprinting directamente
        if (should_sprint && mapper::version == mapper::MINECRAFT_17) {
            local_player.set_sprinting(true);
        }

        if (env->ExceptionCheck()) env->ExceptionClear();
    }
}
