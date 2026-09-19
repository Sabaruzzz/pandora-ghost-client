#include "../features.hpp"
#include "../sdk.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <imgui.h>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <jvmti.h>
#include <functional>
#include <cstring>
#include <gl/GL.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../../../../../pandorainjector/imgui/stb_image.h"



// --- VARIABLES EXTERNAS DE TEAMS ---
extern std::atomic<bool> g_PlayerInGui;
extern bool g_MenuVisible;
extern ImFont* g_NametagFont;

extern int   gui_nametags_health_format;
extern float gui_nametags_health_segments;
extern bool  gui_nametags_show_own;
extern bool  gui_nametags_hide_vanilla;

static int get_third_person_view(mapper::__minecraft& minecraft) {
    auto settings = minecraft.get_settings();
    if (!settings.object) return 0;
    mapper::__field field = mapper::classes["GameSettings"].get_field("thirdPersonView", "I");
    if (!field.identifier) field = mapper::classes["GameSettings"].get_field("field_74320_O", "I");
    if (!field.identifier) field = mapper::classes["GameSettings"].get_field("aw", "I");
    return field.identifier ? sdk::jni->GetIntField(settings.object, field.identifier) : 0;
}

struct GlobalItemRef {
    jobject object = nullptr;
    explicit GlobalItemRef(jobject obj) : object(obj) {}
    ~GlobalItemRef() {
        if (!object || !sdk::jvm) return;
        JNIEnv* env = nullptr; bool detach = false;
        if (sdk::jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
            if (sdk::jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK) return;
            detach = true;
        }
        env->DeleteGlobalRef(object);
        if (detach) sdk::jvm->DetachCurrentThread();
    }
};

struct Equipment_Entry {
    std::string enchantments;
    std::shared_ptr<GlobalItemRef> item;
};

struct NativeItemCommand {
    std::shared_ptr<GlobalItemRef> item;
    ImVec2 position;
    float size;
};
static std::vector<NativeItemCommand> g_native_item_queue;

struct EquipmentCacheEntry {
    std::vector<Equipment_Entry> items;
    std::chrono::steady_clock::time_point expiry{};
};
static std::unordered_map<int, EquipmentCacheEntry> g_equipment_cache;

namespace mapper { extern jvmtiEnv* jvmti; }

static jmethodID find_method_dynamic(jclass cls, std::initializer_list<const char*> names,
    const std::function<bool(const std::string&)>& signature_ok) {
    if (!cls || !mapper::jvmti) return nullptr;
    jint count = 0; jmethodID* methods = nullptr;
    if (mapper::jvmti->GetClassMethods(cls, &count, &methods) != JVMTI_ERROR_NONE || !methods) return nullptr;
    jmethodID result = nullptr;
    for (jint i = 0; i < count && !result; ++i) {
        char *name = nullptr, *sig = nullptr, *generic = nullptr;
        if (mapper::jvmti->GetMethodName(methods[i], &name, &sig, &generic) == JVMTI_ERROR_NONE) {
            bool name_ok = names.size() == 0;
            for (const char* candidate : names) if (name && std::strcmp(name, candidate) == 0) { name_ok = true; break; }
            if (name_ok && sig && signature_ok(sig)) result = methods[i];
        }
        if (name) mapper::jvmti->Deallocate((unsigned char*)name);
        if (sig) mapper::jvmti->Deallocate((unsigned char*)sig);
        if (generic) mapper::jvmti->Deallocate((unsigned char*)generic);
    }
    mapper::jvmti->Deallocate((unsigned char*)methods);
    return result;
}

struct Nametag_Entry
{
    int            entity_id = -1;
    std::string    name;
    std::string    formatted_name;
    mapper::__vec3 position;
    mapper::__vec3 old_position;
    float          height_offset = 2.15f;
    mapper::__vec4 color;
    float          health = 0.f;
    double         distance = 0.0;
    __int32        hurt_time = 0;
    std::vector<Equipment_Entry> equipment;
    bool own = false;
};

static const char* enchant_short(int id) {
    switch (id) {
    case 0: return "P"; case 1: return "FP"; case 2: return "FF"; case 3: return "BP"; case 4: return "PP";
    case 5: return "R"; case 6: return "AA"; case 7: return "T"; case 16: return "Sh"; case 17: return "Sm";
    case 18: return "BoA"; case 19: return "KB"; case 20: return "FA"; case 21: return "L"; case 32: return "E";
    case 33: return "ST"; case 34: return "U"; case 35: return "F"; case 48: return "Pw"; case 49: return "Pn";
    case 50: return "Fl"; case 51: return "I"; case 61: return "LoS"; case 62: return "L"; default: return "E";
    }
}

static Equipment_Entry read_equipment(mapper::__item_stack& stack) {
    Equipment_Entry out;
    if (!stack.object || !sdk::jni) return out;
    out.item = std::make_shared<GlobalItemRef>(sdk::jni->NewGlobalRef(stack.object));
    JNIEnv* env = sdk::jni;
    jclass cls = env->GetObjectClass(stack.object);
    if (!cls) return out;
    jmethodID ench = find_method_dynamic(cls,
        {"getEnchantmentTagList", "func_77986_q", "q", "u", "v"},
        [](const std::string& sig) { return sig.size() > 4 && sig.rfind("()L", 0) == 0; });
    jobject list = ench ? env->CallObjectMethod(stack.object, ench) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); list = nullptr; }
    if (list) {
        jclass lc = env->GetObjectClass(list);
        jmethodID count = find_method_dynamic(lc, {"tagCount", "func_74745_c", "c"},
            [](const std::string& sig) { return sig == "()I"; });
        jmethodID at = find_method_dynamic(lc, {"getCompoundTagAt", "func_150305_b", "b"},
            [](const std::string& sig) { return sig.size() > 5 && sig.rfind("(I)L", 0) == 0; });
        int n = count ? env->CallIntMethod(list, count) : 0;
        for (int i = 0; i < n && at; ++i) {
            jobject tag = env->CallObjectMethod(list, at, i);
            if (!tag) continue;
            jclass tc = env->GetObjectClass(tag);
            jmethodID gs = find_method_dynamic(tc, {"getShort", "func_74765_d", "d", "e"},
                [](const std::string& sig) { return sig == "(Ljava/lang/String;)S"; });
            if (gs) {
                jstring jid = env->NewStringUTF("id"), jlvl = env->NewStringUTF("lvl");
                int id = env->CallShortMethod(tag, gs, jid), lvl = env->CallShortMethod(tag, gs, jlvl);
                env->DeleteLocalRef(jid); env->DeleteLocalRef(jlvl);
                // One primary enchantment per item keeps the HUD readable.
                // Vanilla stores the main protection/damage enchant first.
                if (out.enchantments.empty()) {
                    out.enchantments += enchant_short(id);
                    out.enchantments += std::to_string(lvl);
                }
            }
            env->DeleteLocalRef(tc); env->DeleteLocalRef(tag);
        }
        env->DeleteLocalRef(lc); env->DeleteLocalRef(list);
    }
    env->DeleteLocalRef(cls);
    if (env->ExceptionCheck()) env->ExceptionClear();
    return out;
}

static std::shared_ptr<const std::vector<Nametag_Entry>> g_nametag_snapshot =
    std::make_shared<const std::vector<Nametag_Entry>>();
static std::mutex                 g_nametag_mutex;

// Posicion de la camara capturada junto con el buffer
// para sincronizar la conversion a screen coords
static double g_cam_x = 0.0, g_cam_y = 0.0, g_cam_z = 0.0;
static float g_buffer_partial_ticks = 0.0f;
static mapper::__vec2 project_nametag(const mapper::__vec3& p,
    const double model[16], const double projection[16], const int viewport[4])
{
    double eye[4] = {
        p.x * model[0] + p.y * model[4] + p.z * model[8]  + model[12],
        p.x * model[1] + p.y * model[5] + p.z * model[9]  + model[13],
        p.x * model[2] + p.y * model[6] + p.z * model[10] + model[14],
        p.x * model[3] + p.y * model[7] + p.z * model[11] + model[15]
    };
    double clip[4] = {
        eye[0] * projection[0] + eye[1] * projection[4] + eye[2] * projection[8]  + eye[3] * projection[12],
        eye[0] * projection[1] + eye[1] * projection[5] + eye[2] * projection[9]  + eye[3] * projection[13],
        eye[0] * projection[2] + eye[1] * projection[6] + eye[2] * projection[10] + eye[3] * projection[14],
        eye[0] * projection[3] + eye[1] * projection[7] + eye[2] * projection[11] + eye[3] * projection[15]
    };
    if (!std::isfinite(clip[3]) || clip[3] <= 0.0001) return {FLT_MAX, FLT_MAX};
    const double nx = clip[0] / clip[3], ny = clip[1] / clip[3], nz = clip[2] / clip[3];
    if (!std::isfinite(nx) || !std::isfinite(ny) || nz < -1.0 || nz > 1.0)
        return {FLT_MAX, FLT_MAX};
    return {
        (float)(viewport[0] + (nx + 1.0) * viewport[2] * 0.5),
        (float)(viewport[1] + (1.0 - ny) * viewport[3] * 0.5)
    };
}

// ============================================================
// SUPRESION DE NAMETAG VANILLA VIA GAMEPROFILE.NAME
//                                     (LUNAR CLIENT 1.8.9)
//
// POR QUE setAlwaysRenderNameTag NO FUNCIONA:
//   Lunar Client resetea el flag alwaysRenderNameTag a true
//   en cada tick via EntityOtherPlayerMP.onUpdate(), anulando
//   cualquier llamada JNI que hagamos.
//
// SOLUCION: Modificar GameProfile.name a " " (espacio en blanco)
//   - El renderer vanilla lee el nombre desde GameProfile.getName()
//   - Con nombre en blanco, no hay texto que dibujar
//   - Lunar NO resetea GameProfile.name en su ciclo de ticks
//   - Los nombres reales se cachean en C++ (por entityId) para
//     que el ESP de ImGui pueda seguir mostrandolos
//   - Al desactivar hide_nickname, se restauran los originales
//
// NOTA: El hook de glScalef en hooks.cpp sigue activo como
//       defensa primaria. Este enfoque es la defensa secundaria
//       para cubrir rutas de renderizado que glScalef no atrapa.
// ============================================================

struct ProfileCacheEntry {
    std::string original_name;
};
static std::unordered_map<int, ProfileCacheEntry> g_profile_cache;

static std::string get_tablist_name(mapper::__minecraft& minecraft, mapper::__player& player) {
    if (!sdk::jni || !minecraft.object || !player.object) return {};
    JNIEnv* env = sdk::jni;
    jclass player_cls = env->GetObjectClass(player.object);
    jmethodID get_uuid = player_cls ? env->GetMethodID(player_cls, "getUniqueID", "()Ljava/util/UUID;") : nullptr;
    if (!get_uuid) { env->ExceptionClear(); get_uuid = player_cls ? env->GetMethodID(player_cls, "func_110124_au", "()Ljava/util/UUID;") : nullptr; }
    if (!get_uuid) { env->ExceptionClear(); get_uuid = player_cls ? find_method_dynamic(player_cls, {}, [](const std::string& s) { return s == "()Ljava/util/UUID;"; }) : nullptr; }
    jobject uuid = get_uuid ? env->CallObjectMethod(player.object, get_uuid) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); uuid = nullptr; }
    if (player_cls) env->DeleteLocalRef(player_cls);
    if (!uuid) return {};

    auto& mc_cls = mapper::classes["Minecraft"];
    std::string handler_sig = "()" + mapper::classes["NetHandlerPlayClient"].signature;
    jmethodID get_handler = env->GetMethodID(mc_cls.klass, "getNetHandler", handler_sig.c_str());
    if (!get_handler) { env->ExceptionClear(); get_handler = env->GetMethodID(mc_cls.klass, "func_147114_u", handler_sig.c_str()); }
    if (!get_handler) { env->ExceptionClear(); get_handler = find_method_dynamic(mc_cls.klass, {}, [&](const std::string& s) { return s == handler_sig; }); }
    jobject handler = get_handler ? env->CallObjectMethod(minecraft.object, get_handler) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); handler = nullptr; }
    if (!handler) { env->DeleteLocalRef(uuid); return {}; }

    jclass handler_cls = env->GetObjectClass(handler);
    jmethodID get_info = handler_cls ? find_method_dynamic(handler_cls,
        {"getPlayerInfo", "func_175102_a"}, [](const std::string& s) {
            return s.rfind("(Ljava/util/UUID;)", 0) == 0 && s.back() == ';';
        }) : nullptr;
    jobject info = get_info ? env->CallObjectMethod(handler, get_info, uuid) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); info = nullptr; }
    env->DeleteLocalRef(uuid);
    if (handler_cls) env->DeleteLocalRef(handler_cls);
    env->DeleteLocalRef(handler);
    if (!info) return {};

    jclass info_cls = env->GetObjectClass(info);
    jmethodID get_profile = info_cls ? find_method_dynamic(info_cls,
        {"getGameProfile", "func_178845_a"}, [](const std::string& s) {
            return s.find("GameProfile;") != std::string::npos && s.rfind("()", 0) == 0;
        }) : nullptr;
    jobject profile = get_profile ? env->CallObjectMethod(info, get_profile) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); profile = nullptr; }
    if (info_cls) env->DeleteLocalRef(info_cls);
    env->DeleteLocalRef(info);
    if (!profile) return {};

    jclass profile_cls = env->GetObjectClass(profile);
    jmethodID get_name = profile_cls ? env->GetMethodID(profile_cls, "getName", "()Ljava/lang/String;") : nullptr;
    jstring value = get_name ? (jstring)env->CallObjectMethod(profile, get_name) : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); value = nullptr; }
    std::string result;
    if (value) {
        const char* chars = env->GetStringUTFChars(value, nullptr);
        if (chars) { result = chars; env->ReleaseStringUTFChars(value, chars); }
        env->DeleteLocalRef(value);
    }
    if (profile_cls) env->DeleteLocalRef(profile_cls);
    env->DeleteLocalRef(profile);
    return result;
}

// ============================================================
// FIX ANTI-CRASH: Burbuja limpiadora de Memoria JNI
// ============================================================
struct JNIFrame {
    JNIEnv* env;
    bool active = false;
    JNIFrame(JNIEnv* e, int cap) : env(e) {
        active = env && env->PushLocalFrame(cap) == JNI_OK;
        if (env && !active && env->ExceptionCheck()) env->ExceptionClear();
    }
    ~JNIFrame() { if (active) env->PopLocalFrame(nullptr); }
};

// Extrae el nombre formateado (con colores de equipo/rango del servidor)
// FIX: Ya NO usamos static jmethodID. En PCs con diferente build de Lunar
// Client, la clase puede cargarse en otro orden y el primer lookup falla,
// dejando el methodID permanentemente null. Ahora cacheamos por classID
// usando un map thread-local para que se reintente si cambia la clase.
std::string features::visual::get_formatted_name(void* env_ptr, void* player_obj) {
    JNIEnv* env = (JNIEnv*)env_ptr;
    if (!env || !player_obj) return "";

    // Cache por clase, no static global. Asi si Lunar carga otra clase,
    // el lookup se reintenta automaticamente.
    static std::unordered_map<std::string, jmethodID> s_display_cache;
    static std::unordered_map<std::string, jmethodID> s_format_cache;

    jobject p_obj = (jobject)player_obj;
    jclass player_class = env->GetObjectClass(p_obj);
    if (!player_class) return "";

    // Obtener nombre de la clase como key del cache
    jclass cls_cls = env->FindClass("java/lang/Class");
    std::string class_key = "unknown";
    if (cls_cls) {
        jmethodID getName_mid = env->GetMethodID(cls_cls, "getName", "()Ljava/lang/String;");
        if (getName_mid) {
            jstring cls_name = (jstring)env->CallObjectMethod(player_class, getName_mid);
            if (cls_name) {
                const char* cn = env->GetStringUTFChars(cls_name, nullptr);
                if (cn) { class_key = cn; env->ReleaseStringUTFChars(cls_name, cn); }
                env->DeleteLocalRef(cls_name);
            }
        }
        env->DeleteLocalRef(cls_cls);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();

    // Buscar getDisplayName (cacheado por nombre de clase)
    jmethodID mid = nullptr;
    auto it = s_display_cache.find(class_key);
    if (it != s_display_cache.end()) {
        mid = it->second;
    }
    else {
        mid = env->GetMethodID(player_class, "getDisplayName", "()Lnet/minecraft/util/IChatComponent;");
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(player_class, "func_145748_c_", "()Lnet/minecraft/util/IChatComponent;"); }
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(player_class, "f_=", "()Lnet/minecraft/util/IChatComponent;"); }
        if (!mid) { env->ExceptionClear(); mid = env->GetMethodID(player_class, "getFormattedCommandSenderName", "()Lnet/minecraft/util/IChatComponent;"); }
        if (!mid) {
            env->ExceptionClear();
            const char* obf_names[] = { "e_", "d_", "f_", "e", "f" };
            for (auto& n : obf_names) {
                mid = env->GetMethodID(player_class, n, "()Lnet/minecraft/util/IChatComponent;");
                if (mid) break;
                env->ExceptionClear();
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        s_display_cache[class_key] = mid; // puede ser null, se reintentara si la clase cambia
    }

    std::string result;
    if (mid) {
        jobject chat_comp = env->CallObjectMethod(p_obj, mid);
        if (env->ExceptionCheck()) { env->ExceptionClear(); chat_comp = nullptr; }
        if (chat_comp) {
            jclass chat_comp_class = env->GetObjectClass(chat_comp);
            if (chat_comp_class) {
                // Cache getFormattedText por clase del IChatComponent
                std::string fmt_key = "fmt";
                jmethodID mid_fmt = nullptr;
                auto it2 = s_format_cache.find(fmt_key);
                if (it2 != s_format_cache.end()) {
                    mid_fmt = it2->second;
                }
                else {
                    mid_fmt = env->GetMethodID(chat_comp_class, "getFormattedText", "()Ljava/lang/String;");
                    if (!mid_fmt) { env->ExceptionClear(); mid_fmt = env->GetMethodID(chat_comp_class, "func_150254_d", "()Ljava/lang/String;"); }
                    if (!mid_fmt) { env->ExceptionClear(); mid_fmt = env->GetMethodID(chat_comp_class, "d", "()Ljava/lang/String;"); }
                    if (!mid_fmt) { env->ExceptionClear(); mid_fmt = env->GetMethodID(chat_comp_class, "e", "()Ljava/lang/String;"); }
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    s_format_cache[fmt_key] = mid_fmt;
                }
                if (mid_fmt) {
                    jstring jstr = (jstring)env->CallObjectMethod(chat_comp, mid_fmt);
                    if (env->ExceptionCheck()) { env->ExceptionClear(); jstr = nullptr; }
                    if (jstr) {
                        const char* chars = env->GetStringUTFChars(jstr, nullptr);
                        if (chars) { result = chars; env->ReleaseStringUTFChars(jstr, chars); }
                        env->DeleteLocalRef(jstr);
                    }
                }
                env->DeleteLocalRef(chat_comp_class);
            }
            env->DeleteLocalRef(chat_comp);
        }
    }
    env->DeleteLocalRef(player_class);
    if (env->ExceptionCheck()) env->ExceptionClear();

    return result;
}

mapper::__vec4 features::visual::extract_color_from_format(const std::string& text, mapper::__vec4 fallback_col) {
    const int len = (int)text.length();
    for (int i = 0; i < len; ++i) {
        bool is_cc = false;
        char cc_char = 0;

        if ((unsigned char)text[i] == 0xC2 && i + 2 < len && (unsigned char)text[i + 1] == 0xA7) {
            is_cc = true; cc_char = text[i + 2]; i += 2;
        }
        else if (text[i] == '&' && i + 1 < len) {
            is_cc = true; cc_char = text[i + 1]; i += 1;
        }

        if (is_cc) {
            // First valid color wins
            switch (tolower((unsigned char)cc_char)) {
            case '0': return { 0.f, 0.f, 0.f, fallback_col.w };
            case '1': return { 0.f, 0.f, 170.f/255.f, fallback_col.w };
            case '2': return { 0.f, 170.f/255.f, 0.f, fallback_col.w };
            case '3': return { 0.f, 170.f/255.f, 170.f/255.f, fallback_col.w };
            case '4': return { 170.f/255.f, 0.f, 0.f, fallback_col.w };
            case '5': return { 170.f/255.f, 0.f, 170.f/255.f, fallback_col.w };
            case '6': return { 255.f/255.f, 170.f/255.f, 0.f, fallback_col.w };
            case '7': return { 170.f/255.f, 170.f/255.f, 170.f/255.f, fallback_col.w };
            case '8': return { 85.f/255.f, 85.f/255.f, 85.f/255.f, fallback_col.w };
            case '9': return { 85.f/255.f, 85.f/255.f, 255.f/255.f, fallback_col.w };
            case 'a': return { 85.f/255.f, 255.f/255.f, 85.f/255.f, fallback_col.w };
            case 'b': return { 85.f/255.f, 255.f/255.f, 255.f/255.f, fallback_col.w };
            case 'c': return { 255.f/255.f, 85.f/255.f, 85.f/255.f, fallback_col.w };
            case 'd': return { 255.f/255.f, 85.f/255.f, 255.f/255.f, fallback_col.w };
            case 'e': return { 255.f/255.f, 255.f/255.f, 85.f/255.f, fallback_col.w };
            case 'f': return { 255.f/255.f, 255.f/255.f, 255.f/255.f, fallback_col.w };
            case 'r': return fallback_col;
            }
        }
    }
    return fallback_col;
}

auto features::visual::nametags::run(mapper::__minecraft& minecraft) -> void
{
    const bool current_enabled = features::visual::nametags::enabled;
    sync_hide_vanilla_hook(current_enabled && gui_nametags_hide_vanilla);

    if (!current_enabled) {
        g_equipment_cache.clear();
        std::lock_guard<std::mutex> lock(g_nametag_mutex);
        g_nametag_snapshot = std::make_shared<const std::vector<Nametag_Entry>>();
        return;
    }

    // Entity state changes on Minecraft ticks. Running this JNI-heavy scan at
    // 1 kHz only recreated ItemStack refs and enchantment data between ticks.
    static auto next_snapshot = std::chrono::steady_clock::time_point{};
    const auto snapshot_now = std::chrono::steady_clock::now();
    if (snapshot_now < next_snapshot) return;
    next_snapshot = snapshot_now + std::chrono::milliseconds(16);

    // El check de current_screen fue removido para que el ESP se muestre aunque el chat este abierto

    auto timer = minecraft.get_timer();
    if (timer.object == nullptr) return;
    float partial_ticks = timer.get_partial_ticks();

    auto world = minecraft.get_world();
    auto local_player = minecraft.get_local_player();
    if (world.object == nullptr || local_player.object == nullptr) {
        std::lock_guard<std::mutex> lock(g_nametag_mutex);
        g_nametag_snapshot = std::make_shared<const std::vector<Nametag_Entry>>();
        return;
    }

    auto local_pos = local_player.get_view_position(partial_ticks);
    auto world_players = world.get_players();
    if (world_players.empty()) return; // FIX FLICKER: Ignorar actualizaciones vacÃƒÂ­as causadas por excepciones ConcurrentModification de Java

    // Capturar la posicion del RenderManager (camara de OpenGL)
    // para poder calcular rel_pos sincronizado en render()
    auto rm = minecraft.get_render_manager();
    double cam_x = 0.0, cam_y = 0.0, cam_z = 0.0;
    if (rm.object) {
        // FIX JITTER: Sincronizar coordenadas X/Z de la cÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡mara con los mismos partial_ticks que el ESP
        // Esto elimina por completo el "temblor" cuando caminas o entras en PvP.
        cam_x = rm.get_render_pos_x();
        cam_y = rm.get_render_pos_y(); // Y se mantiene original por el eye height y saltos
        cam_z = rm.get_render_pos_z();
    }

    // Borrado: el probe de `find_nametag_setter` ya no se usa.

    std::vector<Nametag_Entry> temp;
    temp.reserve(world_players.size());

    const bool   use_fake = features::visual::nametags::use_fake_name;
    const int third_person_view = get_third_person_view(minecraft);
    static bool previous_show_own = false;
    if (previous_show_own != gui_nametags_show_own) {
        // A mode transition changes which entity owns cached ItemStack refs.
        // Drop the logic-thread cache before rebuilding the next snapshot.
        g_equipment_cache.clear();
        previous_show_own = gui_nametags_show_own;
    }

    for (auto& player : world_players)
    {
        if (player.object == nullptr) continue;
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        const bool is_own = sdk::jni->IsSameObject(local_player.object, player.object);
        if (is_own && !gui_nametags_show_own) continue;
        if (is_own && third_person_view == 0) continue;

        JNIFrame frame(sdk::jni, 128);

        float health = player.get_health();
        if (health <= 0.f) continue;

        int eid = player.get_entity_id();
        if (eid < 0) continue;

        auto now = snapshot_now;
        ProfileCacheEntry& entry_data = g_profile_cache[eid];

        // 1. Si no tenemos el nombre original, lo capturamos
        // Use the same entity name path as the original implementation. This
        // preserves the server/Lunar behavior that was stable before the
        // profile/API experiments.
        std::string current_name = entry_data.original_name;
        // The local player never needs the network tab-list path. Avoiding it
        // removes the only extra NetHandler lookup introduced by Show Own.
        if (current_name.empty() && !is_own) current_name = get_tablist_name(minecraft, player);
        if (current_name.empty()) current_name = player.get_name();
        // Remote player-shaped NPCs absent from the tab list are anticheat decoys.
        if (current_name.empty() && !is_own) continue;
        if (entry_data.original_name.empty()) {
            if (!current_name.empty() && current_name != " ") {
                entry_data.original_name = current_name;
                
            }
        }

        // Para el ESP de ImGui, usamos el nombre original de nuestra cache
        std::string name = entry_data.original_name;
        if (name.empty()) continue;

        if (!features::visual::nametags::draw_invisible_players && player.get_flag(5)) continue;

        auto   player_pos = player.get_view_position(partial_ticks);
        double distance = local_pos.get_distance_to_vec3(player_pos);
        if (distance > 255.0) continue;

        // Astral/Lunar can return a transient or truncated display component
        // (for example kAstralMC-, kAstralMC-CT). Entity#getName is the stable
        // complete value, so use it for the visible nametag.
        std::string fmt_name = name;

        bool is_sneaking = player.get_flag(1);
        // Ajustado para estar un poco mÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡s arriba
        float height_offset = is_sneaking ? 1.85f : 2.15f;

        Nametag_Entry entry;
        entry.entity_id = eid;
        entry.name = name;
        entry.formatted_name = fmt_name;
        // Guardamos posicion ABSOLUTA en el mundo (no relativa)
        // render() calculara la posicion relativa usando RenderManager
        // para sincronizar perfectamente con la camara de OpenGL
        entry.position = player.get_position();
        entry.old_position = player.get_old_position();
        entry.height_offset = height_offset;
        entry.color = features::visual::nametags::color;
        entry.health = health;
        entry.distance = distance;
        entry.hurt_time = features::visual::nametags::draw_hurt_time ? player.get_hurt_time() : 0;
        entry.own = is_own;

        if (features::visual::nametags::show_equipment) {
            EquipmentCacheEntry& cached = g_equipment_cache[eid];
            if (now >= cached.expiry) {
                std::vector<Equipment_Entry> fresh;
                fresh.reserve(5);
                auto held = player.get_held_item_stack();
                auto held_data = read_equipment(held);
                if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                if (held_data.item && held_data.item->object) fresh.push_back(std::move(held_data));
                for (int slot = 39; slot >= 36; --slot) {
                    auto armor = player.get_inventory_slot(slot);
                    auto armor_data = read_equipment(armor);
                    if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
                    if (armor_data.item && armor_data.item->object) fresh.push_back(std::move(armor_data));
                }
                cached.items = std::move(fresh);
                cached.expiry = now + std::chrono::milliseconds(250);
            }
            entry.equipment = cached.items;
        } else if (!g_equipment_cache.empty()) {
            g_equipment_cache.clear();
        }

        temp.push_back(std::move(entry));
    }

    // Lock minimo: solo el swap del vector
    {
        std::lock_guard<std::mutex> lock(g_nametag_mutex);
        g_nametag_snapshot = std::make_shared<const std::vector<Nametag_Entry>>(std::move(temp));
        g_cam_x = cam_x;
        g_cam_y = cam_y;
        g_cam_z = cam_z;
        g_buffer_partial_ticks = partial_ticks;
    }
}

void features::visual::nametags::shutdown(mapper::__minecraft& minecraft)
{
    g_profile_cache.clear();
    g_equipment_cache.clear();
}

// ============================================================
// SOMBRA DE TEXTO ? inline para eliminar overhead de llamada
// ============================================================
static inline void DrawShadowText(ImDrawList* draw, ImFont* font, float font_size,
    ImVec2 pos, ImU32 color, const char* text)
{
    draw->AddText(font, font_size, { pos.x + 1.f, pos.y + 1.f }, IM_COL32(20, 20, 20, 240), text);
    draw->AddText(font, font_size, pos, color, text);
}

void features::visual::nametags::shutdown_render_resources() {
    g_native_item_queue.clear();
}

void features::visual::nametags::abandon_render_resources_after_context_loss() {
    g_native_item_queue.clear();
}
// ============================================================
// CALCULO DE TAMA?O ? buffer en stack, cero heap allocations
// ============================================================
static void DrawPixelHeart(ImDrawList* draw, ImVec2 pos, float scale, ImU32 color) {
    const char* heart_pixels =
        ".XX.XX."
        "XXXXXXX"
        "XXXXXXX"
        ".XXXXX."
        "..XXX.."
        "...X...";
    float p_sz = roundf(scale * 3.0f);
    if (p_sz < 1.0f) p_sz = 1.0f;
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 7; ++x) {
            if (heart_pixels[y * 7 + x] == 'X') {
                ImVec2 p_min = { pos.x + x * p_sz + 1.f, pos.y + y * p_sz + 1.f };
                ImVec2 p_max = { p_min.x + p_sz, p_min.y + p_sz };
                draw->AddRectFilled(p_min, p_max, IM_COL32(20,20,20,240));
            }
        }
    }
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 7; ++x) {
            if (heart_pixels[y * 7 + x] == 'X') {
                ImVec2 p_min = { pos.x + x * p_sz, pos.y + y * p_sz };
                ImVec2 p_max = { p_min.x + p_sz, p_min.y + p_sz };
                draw->AddRectFilled(p_min, p_max, color);
            }
        }
    }
}

static ImVec2 CalcFormattedTextSize(ImFont* font, float font_size, const std::string& text) {
    char clean[128];
    int  ci = 0;
    int  len = (int)text.length();
    for (int i = 0; i < len && ci < 127; ++i) {
        if ((unsigned char)text[i] == 0xC2 && i + 2 < len && (unsigned char)text[i + 1] == 0xA7)
            i += 2;
        else if (text[i] == '&' && i + 1 < len)
            i += 1;
        else
            clean[ci++] = text[i];
    }
    clean[ci] = '\0';
    return font->CalcTextSizeA(font_size, FLT_MAX, 0.f, clean);
}

// ============================================================
// RENDER DE TEXTO CON COLORES DE MINECRAFT
// Chunk en stack ? sin std::string en hot path
// ============================================================
static void DrawFormattedTextShadow(ImDrawList* draw, ImFont* font, float font_size,
    ImVec2 pos, ImU32 default_col, const std::string& text)
{
    ImVec2 cur_pos = pos;
    ImU32  cur_col = default_col;
    char   chunk[128];
    int    chunk_len = 0;

    auto flush = [&]() {
        if (chunk_len == 0) return;
        chunk[chunk_len] = '\0';
        
        // Forzamos a que las coordenadas de dibujo sean enteros perfectos 
        // para evitar difuminado (blur) sub-pixel en ImGui.
        ImVec2 draw_pos = cur_pos;

        // Single crisp shadow offset for pixel fonts
        draw->AddText(font, font_size, { draw_pos.x + 1.f, draw_pos.y + 1.f },
            IM_COL32(20, 20, 20, 240), chunk);
        draw->AddText(font, font_size, draw_pos, cur_col, chunk);
        cur_pos.x += font->CalcTextSizeA(font_size, FLT_MAX, 0.f, chunk, chunk + chunk_len).x;
        chunk_len = 0;
        };

    const int len = (int)text.length();
    for (int i = 0; i < len; ++i) {
        bool is_cc = false;
        char cc_char = 0;

        if ((unsigned char)text[i] == 0xC2 && i + 2 < len && (unsigned char)text[i + 1] == 0xA7)
        {
            is_cc = true; cc_char = text[i + 2]; i += 2;
        }
        else if (text[i] == '&' && i + 1 < len)
        {
            is_cc = true; cc_char = text[i + 1]; i += 1;
        }

        if (is_cc) {
            flush();
            switch (tolower((unsigned char)cc_char)) {
            case '0': cur_col = IM_COL32(0, 0, 0, 255); break;
            case '1': cur_col = IM_COL32(0, 0, 170, 255); break;
            case '2': cur_col = IM_COL32(0, 170, 0, 255); break;
            case '3': cur_col = IM_COL32(0, 170, 170, 255); break;
            case '4': cur_col = IM_COL32(170, 0, 0, 255); break;
            case '5': cur_col = IM_COL32(170, 0, 170, 255); break;
            case '6': cur_col = IM_COL32(255, 170, 0, 255); break;
            case '7': cur_col = IM_COL32(170, 170, 170, 255); break;
            case '8': cur_col = IM_COL32(85, 85, 85, 255); break;
            case '9': cur_col = IM_COL32(85, 85, 255, 255); break;
            case 'a': cur_col = IM_COL32(85, 255, 85, 255); break;
            case 'b': cur_col = IM_COL32(85, 255, 255, 255); break;
            case 'c': cur_col = IM_COL32(255, 85, 85, 255); break;
            case 'd': cur_col = IM_COL32(255, 85, 255, 255); break;
            case 'e': cur_col = IM_COL32(255, 255, 85, 255); break;
            case 'f': cur_col = IM_COL32(255, 255, 255, 255); break;
            case 'r': cur_col = default_col;                  break;
            }
        }
        else if (chunk_len < 127) {
            chunk[chunk_len++] = text[i];
        }
    }
    flush();
}

auto features::visual::nametags::render() -> void
{
    // Never allow commands from an earlier frame to survive after disabling the
    // feature or opening a GUI.
    g_native_item_queue.clear();
    if (!features::visual::nametags::enabled || g_PlayerInGui || g_MenuVisible) return;

    if (features::visual::view_port[2] == 0) {
        features::visual::view_port[0] = 0;
        features::visual::view_port[1] = 0;
        features::visual::view_port[2] = static_cast<int>(ImGui::GetIO().DisplaySize.x);
        features::visual::view_port[3] = static_cast<int>(ImGui::GetIO().DisplaySize.y);
    }
    if (features::visual::view_port[2] == 0) return;

    std::shared_ptr<const std::vector<Nametag_Entry>> local_buf;
    double rm_x, rm_y, rm_z;
    float buffer_partial = 0.0f;
    {
        std::lock_guard<std::mutex> lock(g_nametag_mutex);
        local_buf = g_nametag_snapshot;
        if (!local_buf || local_buf->empty()) return;
        rm_x = g_cam_x;
        rm_y = g_cam_y;
        rm_z = g_cam_z;
        buffer_partial = g_buffer_partial_ticks;
    }
    float frame_partial = buffer_partial;
    if (features::visual::render_frame_snapshot_valid) {
        rm_x = features::visual::render_camera_x;
        rm_y = features::visual::render_camera_y;
        rm_z = features::visual::render_camera_z;
        frame_partial = features::visual::render_partial_ticks;
    }
    frame_partial = (std::max)(0.0f, (std::min)(frame_partial, 1.0f));

    auto* draw = ImGui::GetBackgroundDrawList();
    if (!draw) return;

    ImFont* font = g_NametagFont;
    ImGuiIO& io_ref = ImGui::GetIO();
    if ((!font || !font->IsLoaded()) && io_ref.Fonts && io_ref.Fonts->Fonts.Size > 0) {
        if (io_ref.Fonts->Fonts[0] && io_ref.Fonts->Fonts[0]->IsLoaded())
            font = io_ref.Fonts->Fonts[0];
    }
    if (!font) font = ImGui::GetFont();
    if (!font) return; 
    // Shared Minecraft face for the complete nametag and enchantment labels.
    const float base_font_size = (font == g_NametagFont) ? 20.0f : 16.0f;

    const ImU32  bg_normal = ImGui::ColorConvertFloat4ToU32({ 0.08f, 0.08f, 0.08f, 0.45f });
    const ImU32  dist_color = ImGui::ColorConvertFloat4ToU32({ 1.00f, 1.00f, 1.00f, 1.00f });
    const float  vw = (float)features::visual::view_port[2];
    const float  vh = (float)features::visual::view_port[3];
    const bool   show_health = features::visual::nametags::draw_health;
    constexpr bool show_dist = false;
    const bool   show_hurt = features::visual::nametags::draw_hurt_time;
    const bool   use_fake = features::visual::nametags::use_fake_name;

    for (const auto& e : *local_buf)
    {
        const double interp_x = e.old_position.x + (e.position.x - e.old_position.x) * frame_partial;
        const double interp_y = e.old_position.y + (e.position.y - e.old_position.y) * frame_partial;
        const double interp_z = e.old_position.z + (e.position.z - e.old_position.z) * frame_partial;
        mapper::__vec3 rel_pos;
        rel_pos.x = (float)(interp_x - rm_x);
        rel_pos.y = (float)(interp_y + e.height_offset - rm_y);
        rel_pos.z = (float)(interp_z - rm_z);

        auto screen = project_nametag(rel_pos, features::visual::model_view_matrix,
            features::visual::projection_matrix, features::visual::view_port);
        if (screen.x == FLT_MAX || screen.y == FLT_MAX) continue;

        if (screen.x < -150.f || screen.x > vw + 150.f)  continue;
        if (screen.y < -150.f || screen.y > vh + 150.f)  continue;
        // Fixed HUD pixel size: FOV/zoom changes only the projected anchor and
        // never changes the name, equipment or enchantment sizes.
        constexpr float current_scale = 1.0f;
        const float font_size = base_font_size * current_scale;

        std::string display_name;
        if (use_fake)
            display_name = features::visual::nametags::fake_name.empty() ? "Nick" : features::visual::nametags::fake_name;
        else
            display_name = e.formatted_name;

        // The projection and its freshness marker now come from the same render
        // frame. No temporal smoothing: the tag follows the camera immediately.
        // Pixel fonts shimmer when submitted at fractional framebuffer
        // coordinates. Equipment is rasterized by Minecraft and was already
        // stable; snap the shared anchor once so the text follows it cleanly.
        ImVec2 target_screen = { std::round(screen.x), std::round(screen.y) };

        ImVec2 name_size;
        if (use_fake) name_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, display_name.c_str());
        else name_size = CalcFormattedTextSize(font, font_size, display_name);

        char hp_num_str[32] = {};
        char hp_bracket_str[8] = {};
        ImVec2 hp_num_size = {};
        ImVec2 hp_bracket_size = {};
        // Ancho exacto del corazÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â³n (7 cuadritos * 3px = 21) mÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡s un pequeÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â±o margen, o 0 si estÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ desactivado
        const bool hearts_format = gui_nametags_health_format == 1;
        const bool bar_format = gui_nametags_health_format == 2;
        float heart_width = hearts_format ? (25.0f * current_scale) : 0.0f;
        ImU32 heart_color_u32 = IM_COL32(255, 255, 255, 255);
        
        if (show_health && !use_fake && !bar_format) {
            char hp_color = 'a';
            if (e.health > 16.0f)      { hp_color = 'a'; heart_color_u32 = IM_COL32(85, 255, 85, 255); }
            else if (e.health > 12.0f) { hp_color = 'e'; heart_color_u32 = IM_COL32(255, 255, 85, 255); }
            else if (e.health > 8.0f)  { hp_color = '6'; heart_color_u32 = IM_COL32(255, 170, 0, 255); }
            else if (e.health > 4.0f)  { hp_color = 'c'; heart_color_u32 = IM_COL32(255, 85, 85, 255); }
            else                       { hp_color = '4'; heart_color_u32 = IM_COL32(170, 0, 0, 255); }

            // Los corchetes ahora tienen el MISMO COLOR que la vida. Se quita el espacio interno si no hay corazÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â³n.
            const float shown_health = hearts_format ? e.health * 0.5f : e.health;
            const char* fmt = hearts_format ? " &%c[&%c%.1f " : " &%c[&%c%.0f";
            snprintf(hp_num_str, sizeof(hp_num_str), fmt, hp_color, hp_color, shown_health);
            snprintf(hp_bracket_str, sizeof(hp_bracket_str), "&%c]", hp_color);
            hp_num_size = CalcFormattedTextSize(font, font_size, hp_num_str);
            hp_bracket_size = CalcFormattedTextSize(font, font_size, hp_bracket_str);
        }

        char dist_str[32] = {};
        ImVec2 dist_size = {};
        if (show_dist) {
            snprintf(dist_str, sizeof(dist_str), " &8[&7%dm&8]", (int)e.distance);
            dist_size = CalcFormattedTextSize(font, font_size, dist_str);
        }

        float total_hp_width = show_health && !use_fake && !bar_format ? (hp_num_size.x + heart_width + hp_bracket_size.x) : 0.0f;
        const float total_width = total_hp_width + name_size.x + dist_size.x;
        
        const float start_x = target_screen.x - total_width * 0.5f;
        const float ny = target_screen.y - name_size.y * 0.8f;
        
        const float name_x = start_x;
        const float hp_x = name_x + name_size.x;
        const float dist_x = hp_x + total_hp_width;
        
        const float box_left = start_x;
        const float box_right = start_x + total_width;

        float current_pad_x = 2.0f * current_scale;
        float current_pad_y = 0.5f * current_scale;
        ImU32 current_bg = bg_normal;

        if (features::visual::nametags::background) {
            ImVec2 rect_min = { box_left - current_pad_x, ny - current_pad_y };
            ImVec2 rect_max = { box_right + current_pad_x, ny + name_size.y + current_pad_y };

            draw->AddRectFilled(rect_min, rect_max, current_bg, 0.0f);
        }

        if (show_health && !use_fake && !bar_format) {
            DrawFormattedTextShadow(draw, font, font_size, { hp_x, ny }, dist_color, hp_num_str);
            
            if (hearts_format) {
                // Keep the original pixel-heart artwork, aligned to the text
                // line instead of using the old 32 px-font baseline.
                DrawPixelHeart(draw,
                    { std::round(hp_x + hp_num_size.x + 3.0f), std::round(ny + 1.0f) },
                    current_scale, heart_color_u32);
            }
            
            DrawFormattedTextShadow(draw, font, font_size, { hp_x + hp_num_size.x + heart_width, ny }, dist_color, hp_bracket_str);
        }

        ImU32 name_col = ImGui::ColorConvertFloat4ToU32({ (float)e.color.x, (float)e.color.y, (float)e.color.z, (float)e.color.w });

        if (use_fake)
            DrawShadowText(draw, font, font_size, { name_x, ny }, name_col, display_name.c_str());
        else
            DrawFormattedTextShadow(draw, font, font_size, { name_x, ny }, name_col, display_name);

        if (show_dist && dist_size.x > 0.f)
            DrawFormattedTextShadow(draw, font, font_size, { dist_x, ny }, dist_color, dist_str);

        if (show_health && !use_fake && bar_format) {
            const int segments = (std::max)(1, (std::min)(20, (int)std::round(gui_nametags_health_segments)));
            const float bar_y = ny - 5.0f * current_scale;
            const float bar_h = (std::max)(2.0f, 3.0f * current_scale);
            const float ratio = (std::max)(0.0f, (std::min)(1.0f, e.health / 20.0f));
            draw->AddRectFilled({box_left, bar_y}, {box_right, bar_y + bar_h}, IM_COL32(20, 20, 23, 230), 1.0f);
            draw->AddRectFilled({box_left, bar_y}, {box_left + (box_right - box_left) * ratio, bar_y + bar_h},
                IM_COL32((int)((1.f - ratio) * 255), (int)(ratio * 255), 45, 255), 1.0f);
            for (int seg = 1; seg < segments; ++seg) {
                float sx = box_left + (box_right - box_left) * ((float)seg / segments);
                draw->AddLine({sx, bar_y}, {sx, bar_y + bar_h}, IM_COL32(8, 8, 10, 210), 1.0f);
            }
        }

        if (features::visual::nametags::show_equipment && !e.equipment.empty()) {
            constexpr float equipment_scale = 1.30f;
            const float icon_size = 24.f * equipment_scale;
            const float gap = 4.f * equipment_scale;
            const float total_eq = e.equipment.size() * icon_size + (e.equipment.size() - 1) * gap;
            float ex = target_screen.x - total_eq * 0.5f;
            const float ey = ny - (bar_format ? 39.f : 35.f);
            for (const auto& item : e.equipment) {
                // RenderItem uses Minecraft's baked model and active resource
                // pack, so potions, blocks, modded items and armour all match
                // what the player sees in the inventory.
                g_native_item_queue.push_back({item.item, {ex, ey}, icon_size});
                if (features::visual::nametags::show_enchantments && !item.enchantments.empty()) {
                    const float ef = 18.0f * equipment_scale;
                    ImVec2 es = font->CalcTextSizeA(ef, FLT_MAX, 0.f, item.enchantments.c_str());
                    ImVec2 ep = {ex + (icon_size-es.x)*0.5f, ey-es.y-2.f};
                    draw->AddText(font, ef, {ep.x-1.f,ep.y}, IM_COL32(0,0,0,255), item.enchantments.c_str());
                    draw->AddText(font, ef, {ep.x+1.f,ep.y}, IM_COL32(0,0,0,255), item.enchantments.c_str());
                    draw->AddText(font, ef, {ep.x,ep.y-1.f}, IM_COL32(0,0,0,255), item.enchantments.c_str());
                    draw->AddText(font, ef, {ep.x,ep.y+1.f}, IM_COL32(0,0,0,255), item.enchantments.c_str());
                    draw->AddText(font, ef, ep, IM_COL32(250,250,255,255), item.enchantments.c_str());
                }
                ex += icon_size + gap;
            }
        }
    }
}

void features::visual::nametags::render_equipment_native() {
    std::vector<NativeItemCommand> commands;
    commands.swap(g_native_item_queue);
    if (commands.empty() || !sdk::jvm) return;
    JNIEnv* env = nullptr; bool detach = false;
    if (sdk::jvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (sdk::jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK) return;
        detach = true;
    }
    auto finish = [&] { if (env->ExceptionCheck()) env->ExceptionClear(); if (detach) sdk::jvm->DetachCurrentThread(); };
    auto& mc_info = mapper::classes["Minecraft"];
    jfieldID mc_field = env->GetStaticFieldID(mc_info.klass, "theMinecraft", mc_info.signature.c_str());
    if (!mc_field) { env->ExceptionClear(); mc_field = env->GetStaticFieldID(mc_info.klass, "field_71432_P", mc_info.signature.c_str()); }
    if (!mc_field) { env->ExceptionClear(); mc_field = env->GetStaticFieldID(mc_info.klass, "S", mc_info.signature.c_str()); }
    if (!mc_field) { finish(); return; }
    jobject mc = env->GetStaticObjectField(mc_info.klass, mc_field);
    if (!mc) { finish(); return; }

    std::string renderer_sig = "()" + mapper::classes["RenderItem"].signature;
    jmethodID get_renderer = env->GetMethodID(mc_info.klass, "getRenderItem", renderer_sig.c_str());
    if (!get_renderer) { env->ExceptionClear(); get_renderer = env->GetMethodID(mc_info.klass, "func_175599_af", renderer_sig.c_str()); }
    if (!get_renderer) { env->ExceptionClear(); get_renderer = env->GetMethodID(mc_info.klass, "ag", renderer_sig.c_str()); }
    if (!get_renderer) {
        env->ExceptionClear();
        get_renderer = find_method_dynamic(mc_info.klass, {}, [&](const std::string& sig) { return sig == renderer_sig; });
    }
    jobject renderer = get_renderer ? env->CallObjectMethod(mc, get_renderer) : nullptr;
    if (!renderer || env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(mc); finish(); return; }

    // Minecraft GUI coordinates are scaled; derive the active GUI factor.
    int scale = 1;
    auto& sr_info = mapper::classes["ScaledResolution"];
    if (sr_info.klass) {
        std::string ctor_sig = "(" + mc_info.signature + ")V";
        jmethodID ctor = env->GetMethodID(sr_info.klass, "<init>", ctor_sig.c_str());
        jobject sr = ctor ? env->NewObject(sr_info.klass, ctor, mc) : nullptr;
        if (sr) {
            jmethodID gsm = env->GetMethodID(sr_info.klass, "getScaleFactor", "()I");
            if (!gsm) { env->ExceptionClear(); gsm = env->GetMethodID(sr_info.klass, "func_78325_e", "()I"); }
            if (!gsm) { env->ExceptionClear(); gsm = env->GetMethodID(sr_info.klass, "e", "()I"); }
            if (gsm) scale = (std::max)(1, (int)env->CallIntMethod(sr, gsm));
            env->DeleteLocalRef(sr);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    jclass renderer_class = env->GetObjectClass(renderer);
    std::string draw_sig = "(" + mapper::classes["ItemStack"].signature + "II)V";
    jmethodID draw = env->GetMethodID(renderer_class, "renderItemIntoGUI", draw_sig.c_str());
    if (!draw) { env->ExceptionClear(); draw = env->GetMethodID(renderer_class, "func_180450_b", draw_sig.c_str()); }
    if (!draw) { env->ExceptionClear(); draw = env->GetMethodID(renderer_class, "b", draw_sig.c_str()); }
    if (!draw) {
        env->ExceptionClear();
        draw = find_method_dynamic(renderer_class, {}, [&](const std::string& sig) { return sig == draw_sig; });
    }
    if (draw) {
        const float scaled_w = ImGui::GetIO().DisplaySize.x / (float)scale;
        const float scaled_h = ImGui::GetIO().DisplaySize.y / (float)scale;

        // RenderItemIntoGUI assumes EntityRenderer.setupOverlayRendering was
        // already called. SwapBuffers may still contain the world projection,
        // so recreate Minecraft's HUD matrices locally and restore everything.
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0.0, scaled_w, scaled_h, 0.0, 1000.0, 3000.0);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -2000.0f);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Exact fixed-function lighting used by Minecraft 1.8.9's
        // RenderHelper.enableGUIStandardItemLighting(). Blocks need both
        // directional lights; white GL color alone leaves their baked faces
        // nearly black outside a vanilla GuiScreen.
        const GLfloat light0_position[] = { 0.16169041f, 0.80845208f, -0.56591646f, 0.0f };
        const GLfloat light1_position[] = { -0.16169041f, 0.80845208f, 0.56591646f, 0.0f };
        const GLfloat diffuse[] = { 0.6f, 0.6f, 0.6f, 1.0f };
        const GLfloat black[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        const GLfloat model_ambient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
        glPushMatrix();
        glRotatef(-30.0f, 0.0f, 1.0f, 0.0f);
        glRotatef(165.0f, 1.0f, 0.0f, 0.0f);
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        glEnable(GL_COLOR_MATERIAL);
        glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
        glLightfv(GL_LIGHT0, GL_POSITION, light0_position);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
        glLightfv(GL_LIGHT0, GL_AMBIENT, black);
        glLightfv(GL_LIGHT0, GL_SPECULAR, black);
        glLightfv(GL_LIGHT1, GL_POSITION, light1_position);
        glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
        glLightfv(GL_LIGHT1, GL_AMBIENT, black);
        glLightfv(GL_LIGHT1, GL_SPECULAR, black);
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, model_ambient);
        glShadeModel(GL_FLAT);
        glPopMatrix();

        size_t rendered_commands = 0;
        for (const auto& command : commands) {
            if (rendered_commands++ >= 256) break;
            if (!command.item || !command.item->object) continue;
            if (!mapper::classes["ItemStack"].klass ||
                !env->IsInstanceOf(command.item->object, mapper::classes["ItemStack"].klass)) {
                if (env->ExceptionCheck()) env->ExceptionClear();
                continue;
            }
            // RenderItem multiplies baked textures by the current GL color.
            // World/Lunar passes may leave a dark tint behind, so match the
            // vanilla GUI path and start every item at neutral white.
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glEnable(GL_TEXTURE_2D);
            glPushMatrix();
            glTranslatef(
                command.position.x / scale,
                command.position.y / scale, 0.0f);
            const float native_scale = command.size / (16.0f * (float)scale);
            glScalef(native_scale, native_scale, 1.0f);
            env->CallVoidMethod(renderer, draw, command.item->object, 0, 0);
            glPopMatrix();
            if (env->ExceptionCheck()) env->ExceptionClear();
        }

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
    }
    env->DeleteLocalRef(renderer_class); env->DeleteLocalRef(renderer); env->DeleteLocalRef(mc);
    finish();
}
