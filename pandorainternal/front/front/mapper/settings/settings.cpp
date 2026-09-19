#include "mapper.hpp"

mapper::__settings::__settings(jobject object)
{
    this->object = object;
}

mapper::__settings::__settings(const mapper::__settings& settings)
{
    if (settings.object != nullptr)
        this->object = sdk::jni->NewLocalRef(settings.object);
}

mapper::__settings::~__settings()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

float mapper::__settings::get_mouse_sensitivity()
{
    mapper::__field field = mapper::classes["GameSettings"].get_field("mouseSensitivity", "F");

    if (field.identifier == nullptr)
        field = mapper::classes["GameSettings"].get_field("field_74341_c", "F");

    if (field.identifier == nullptr)
        field = mapper::classes["GameSettings"].get_field("a", "F"); // 1.8.9 Lunar/Vanilla

    if (field.identifier == nullptr) return 0.5f;
    return sdk::jni->GetFloatField(this->object, field.identifier);
}

void mapper::__settings::set_virtual_sneak(bool state)
{
    if (this->object == nullptr) return;
    std::string sig = "L" + mapper::classes["KeyBinding"].name + ";";

    mapper::__field kb_fid = mapper::classes["GameSettings"].get_field("keyBindSneak", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("field_74311_E", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("ab", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("T", sig); // 1.7.10

    if (kb_fid.identifier) {
        jobject kb_obj = sdk::jni->GetObjectField(this->object, kb_fid.identifier);
        if (kb_obj) {
            mapper::__field pr_fid = mapper::classes["KeyBinding"].get_field("pressed", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("field_74513_e", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("i", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("h", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("g", "Z"); // 1.7.10

            if (pr_fid.identifier) {
                sdk::jni->SetBooleanField(kb_obj, pr_fid.identifier, state);
            }
            sdk::jni->DeleteLocalRef(kb_obj);
        }
    }
}

// Variables globales estáticas para recordar la tecla original
static int s_original_use_item_keycode = 0;

void mapper::__settings::set_virtual_right_click(bool enable)
{
    if (this->object == nullptr) return;
    std::string sig = "L" + mapper::classes["KeyBinding"].name + ";";

    mapper::__field kb_fid = mapper::classes["GameSettings"].get_field("keyBindUseItem", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("field_74313_G", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("ag", sig);
    if (!kb_fid.identifier) kb_fid = mapper::classes["GameSettings"].get_field("Y", sig); // 1.7.10

    if (kb_fid.identifier) {
        jobject kb_obj = sdk::jni->GetObjectField(this->object, kb_fid.identifier);
        if (kb_obj) {
            mapper::__field pr_fid = mapper::classes["KeyBinding"].get_field("pressed", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("field_74513_e", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("i", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("h", "Z");
            if (!pr_fid.identifier) pr_fid = mapper::classes["KeyBinding"].get_field("g", "Z"); // 1.7.10

            mapper::__field code_fid = mapper::classes["KeyBinding"].get_field("keyCode", "I");
            if (!code_fid.identifier) code_fid = mapper::classes["KeyBinding"].get_field("field_151469_d", "I");
            if (!code_fid.identifier) code_fid = mapper::classes["KeyBinding"].get_field("d", "I");
            if (!code_fid.identifier) code_fid = mapper::classes["KeyBinding"].get_field("e", "I"); // 1.7.10

            if (pr_fid.identifier && code_fid.identifier) {
                int current_code = sdk::jni->GetIntField(kb_obj, code_fid.identifier);
                if (enable) {
                    // Secuestrar: Guardamos la tecla y ponemos 0 (Ninguna tecla)
                    if (current_code != 0) {
                        s_original_use_item_keycode = current_code;
                        sdk::jni->SetIntField(kb_obj, code_fid.identifier, 0);
                    }
                    sdk::jni->SetBooleanField(kb_obj, pr_fid.identifier, true);
                }
                else {
                    // Restaurar: Devolvemos tu click derecho a la normalidad
                    if (current_code == 0 && s_original_use_item_keycode != 0) {
                        sdk::jni->SetIntField(kb_obj, code_fid.identifier, s_original_use_item_keycode);
                    }
                    sdk::jni->SetBooleanField(kb_obj, pr_fid.identifier, false);
                }
            }
            sdk::jni->DeleteLocalRef(kb_obj);
        }
    }
}
