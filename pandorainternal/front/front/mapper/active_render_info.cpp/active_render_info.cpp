#include "mapper.hpp"
#include <vector>

// ============================================================
// Adaptado del Dope Client v2 (ActiveRenderInfo.cpp)
// Lee las matrices ModelView y Projection desde los FloatBuffer
// estaticos que Minecraft actualiza cada frame en ActiveRenderInfo.
// Esto es mas confiable que capturar glGetDoublev en glClear
// porque funciona independientemente del hook de OpenGL.
// ============================================================

static jclass cached_fb_class = nullptr;
static jmethodID cached_get_method = nullptr;

static std::vector<float> read_static_float_buffer(const char* name_forge,
    const char* name_mcp,
    const char* name_lunar)
{
    auto& ari = mapper::classes["ActiveRenderInfo"];
    if (ari.klass == nullptr)
        return {};

    // Intentar los 3 nombres posibles (Forge / SRG / Lunar-Vanilla ofuscado)
    mapper::__field field = ari.get_field(name_forge, "Ljava/nio/FloatBuffer;");
    if (field.identifier == nullptr)
        field = ari.get_field(name_mcp, "Ljava/nio/FloatBuffer;");
    if (field.identifier == nullptr)
        field = ari.get_field(name_lunar, "Ljava/nio/FloatBuffer;");
    if (field.identifier == nullptr)
        return {};

    // El campo es estatico en la clase Java
    jobject buf = sdk::jni->GetStaticObjectField(ari.klass, field.identifier);
    if (buf == nullptr)
        return {};

    if (cached_fb_class == nullptr)
    {
        jclass temp_fb_class = sdk::jni->FindClass("java/nio/FloatBuffer");
        if (temp_fb_class != nullptr)
        {
            cached_fb_class = (jclass)sdk::jni->NewGlobalRef(temp_fb_class);
            cached_get_method = sdk::jni->GetMethodID(cached_fb_class, "get", "(I)F");
            sdk::jni->DeleteLocalRef(temp_fb_class);
        }
    }

    if (cached_get_method == nullptr)
    {
        sdk::jni->DeleteLocalRef(buf);
        return {};
    }

    std::vector<float> ret;
    ret.reserve(16);
    for (int i = 0; i < 16; i++)
        ret.push_back(sdk::jni->CallFloatMethod(buf, cached_get_method, i));

    if (sdk::jni->ExceptionCheck())
        sdk::jni->ExceptionClear();

    sdk::jni->DeleteLocalRef(buf);
    return ret;
}

// MODELVIEW: nombre Forge = "MODELVIEW", SRG = "field_78726_a", Lunar = "a"
std::vector<float> mapper::__active_render_info::get_model_view()
{
    return read_static_float_buffer("MODELVIEW", "field_78726_a", "a");
}

// PROJECTION: nombre Forge = "PROJECTION", SRG = "field_78725_b", Lunar = "b"
std::vector<float> mapper::__active_render_info::get_projection()
{
    return read_static_float_buffer("PROJECTION", "field_78725_b", "b");
}
