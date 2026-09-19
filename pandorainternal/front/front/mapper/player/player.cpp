#include "mapper.hpp"
#include <algorithm>
#include <cctype>
#include <cmath> // Necesario para calcular los angulos de vision
#include <string>

// Helper para mutar strings internamente (salta caches de Lunar Client)
static void mutate_gameprofile_name(jobject game_profile, jfieldID name_field, const std::string& new_name) {
    if (game_profile == nullptr || name_field == nullptr) return;
    jobject current_name_obj = sdk::jni->GetObjectField(game_profile, name_field);
    if (current_name_obj == nullptr) return;
    
    jclass string_class = sdk::jni->GetObjectClass(current_name_obj);
    if (string_class == nullptr) {
        sdk::jni->DeleteLocalRef(current_name_obj);
        return;
    }
    
    jfieldID value_field = sdk::jni->GetFieldID(string_class, "value", "[C");
    if (value_field != nullptr) {
        jobject array_obj = sdk::jni->GetObjectField(current_name_obj, value_field);
        if (array_obj != nullptr) {
            jcharArray char_array = (jcharArray)array_obj;
            jchar* chars = sdk::jni->GetCharArrayElements(char_array, nullptr);
            int len = sdk::jni->GetArrayLength(char_array);
            for (int i = 0; i < len; i++) {
                if (new_name.empty() || new_name == " ") {
                    chars[i] = ' ';
                } else if (i < new_name.length()) {
                    chars[i] = new_name[i];
                } else {
                    chars[i] = ' ';
                }
            }
            sdk::jni->ReleaseCharArrayElements(char_array, chars, 0);
            sdk::jni->DeleteLocalRef(array_obj);
        }
    } else {
        sdk::jni->ExceptionClear();
        value_field = sdk::jni->GetFieldID(string_class, "value", "[B");
        if (value_field != nullptr) {
            jobject array_obj = sdk::jni->GetObjectField(current_name_obj, value_field);
            if (array_obj != nullptr) {
                jbyteArray byte_array = (jbyteArray)array_obj;
                jbyte* bytes = sdk::jni->GetByteArrayElements(byte_array, nullptr);
                int len = sdk::jni->GetArrayLength(byte_array);
                for (int i = 0; i < len; i++) {
                    if (new_name.empty() || new_name == " ") {
                        bytes[i] = ' ';
                    } else if (i < new_name.length()) {
                        bytes[i] = new_name[i];
                    } else {
                        bytes[i] = ' ';
                    }
                }
                sdk::jni->ReleaseByteArrayElements(byte_array, bytes, 0);
                sdk::jni->DeleteLocalRef(array_obj);
            }
        } else {
            sdk::jni->ExceptionClear();
        }
    }
    sdk::jni->DeleteLocalRef(string_class);
    sdk::jni->DeleteLocalRef(current_name_obj);
}


mapper::__player::__player(jobject object)
{
    this->object = object;
}

mapper::__player::__player(const mapper::__player& player)
{
    if (player.object != nullptr)
        this->object = sdk::jni->NewLocalRef(player.object);
}

mapper::__player::~__player()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

mapper::__vec3 mapper::__player::get_position()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("posX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("posY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("posZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70165_t", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("s", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70163_u", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("t", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70161_v", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("u", "D");

    mapper::__vec3 result = {0, 0, 0};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return result;
    result.x = sdk::jni->GetDoubleField(this->object, field_0.identifier);
    result.y = sdk::jni->GetDoubleField(this->object, field_1.identifier);
    result.z = sdk::jni->GetDoubleField(this->object, field_2.identifier);
    return result;
}

void mapper::__player::set_position(mapper::__vec3 position)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("posX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("posY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("posZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70165_t", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("s", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70163_u", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("t", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70161_v", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("u", "D");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return;
    sdk::jni->SetDoubleField(this->object, field_0.identifier, position.x);
    sdk::jni->SetDoubleField(this->object, field_1.identifier, position.y);
    sdk::jni->SetDoubleField(this->object, field_2.identifier, position.z);
}

mapper::__vec3 mapper::__player::get_old_position()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("prevPosX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("prevPosY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("prevPosZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70169_q", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("p", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70167_r", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("q", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70166_s", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("r", "D");

    mapper::__vec3 result = {0, 0, 0};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return result;
    result.x = sdk::jni->GetDoubleField(this->object, field_0.identifier);
    result.y = sdk::jni->GetDoubleField(this->object, field_1.identifier);
    result.z = sdk::jni->GetDoubleField(this->object, field_2.identifier);
    return result;
}

void mapper::__player::set_old_position(mapper::__vec3 old_position)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("prevPosX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("prevPosY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("prevPosZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70169_q", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("p", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70167_r", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("q", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70166_s", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("r", "D");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return;
    sdk::jni->SetDoubleField(this->object, field_0.identifier, old_position.x);
    sdk::jni->SetDoubleField(this->object, field_1.identifier, old_position.y);
    sdk::jni->SetDoubleField(this->object, field_2.identifier, old_position.z);
}

mapper::__vec3 mapper::__player::get_motion()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("motionX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("motionY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("motionZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70159_w", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("v", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70181_x", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("w", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70179_y", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("x", "D");

    mapper::__vec3 result = {0, 0, 0};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return result;
    result.x = sdk::jni->GetDoubleField(this->object, field_0.identifier);
    result.y = sdk::jni->GetDoubleField(this->object, field_1.identifier);
    result.z = sdk::jni->GetDoubleField(this->object, field_2.identifier);
    return result;
}

void mapper::__player::set_motion(mapper::__vec3 motion)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("motionX", "D");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("motionY", "D");
    mapper::__field field_2 = mapper::classes["EntityPlayerXP"].get_field("motionZ", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70159_w", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("v", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70181_x", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("w", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("field_70179_y", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["EntityPlayerXP"].get_field("x", "D");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return;
    sdk::jni->SetDoubleField(this->object, field_0.identifier, motion.x);
    sdk::jni->SetDoubleField(this->object, field_1.identifier, motion.y);
    sdk::jni->SetDoubleField(this->object, field_2.identifier, motion.z);
}

bool mapper::__player::get_on_ground()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("onGround", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70122_E", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("C", "Z");

    if (field.identifier == nullptr) return false;
    return sdk::jni->GetBooleanField(this->object, field.identifier);
}

void mapper::__player::jump()
{
    mapper::__method method = mapper::classes["EntityPlayer"].get_method("jump", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("func_70664_aZ", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bF", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bl", "()V"); // 1.7.10

    if (method.identifier == nullptr) return;
    sdk::jni->CallVoidMethod(this->object, method.identifier);
}

bool mapper::__player::is_swing_in_progress()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("isSwingInProgress", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_82175_bq", "Z");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("ar", "Z");

    if (field.identifier == nullptr) return false;
    return sdk::jni->GetBooleanField(this->object, field.identifier);
}

__int32 mapper::__player::get_hurt_time()
{
    if (this->object == nullptr) return 0;

    mapper::__field field = mapper::classes["EntityLivingBase"].get_field("hurtTime", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityLivingBase"].get_field("field_70737_aN", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityLivingBase"].get_field("au", "I"); // 1.8.9 obf
    if (field.identifier == nullptr) field = mapper::classes["EntityLivingBase"].get_field("aw", "I"); // 1.7.10 obf

    if (field.identifier == nullptr) return 0;
    return sdk::jni->GetIntField(this->object, field.identifier);
}

void mapper::__player::swing_item()
{
    if (this->object == nullptr) return;

    mapper::__method method = mapper::classes["EntityLivingBase"].get_method("swingItem", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityLivingBase"].get_method("func_71038_i", "()V");
    if (method.identifier == nullptr) method = mapper::classes["EntityLivingBase"].get_method("bw", "()V"); // 1.8.9 obf
    if (method.identifier == nullptr) method = mapper::classes["EntityLivingBase"].get_method("ba", "()V"); // 1.7.10 obf

    if (method.identifier != nullptr) {
        sdk::jni->CallVoidMethod(this->object, method.identifier);
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    }
}

void mapper::__player::set_hurt_time(__int32 hurt_time)
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("hurtTime", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70737_aN", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("ax", "I");

    if (field.identifier == nullptr) return;
    sdk::jni->SetIntField(this->object, field.identifier, hurt_time);
}

bool mapper::__player::is_vulnerable()
{
    return this->get_health() > 0.f && this->get_hurt_time() < 5;
}

mapper::__vec3 mapper::__player::get_view_position(float partial_ticks)
{
    mapper::__vec3 position = this->get_position();
    mapper::__vec3 old_position = this->get_old_position();

    mapper::__vec3 result;
    result.x = old_position.x + (position.x - old_position.x) * partial_ticks;
    result.y = old_position.y + (position.y - old_position.y) * partial_ticks;
    result.z = old_position.z + (position.z - old_position.z) * partial_ticks;
    return result;
}

mapper::__vec3 mapper::__player::get_look_position(float partial_ticks)
{
    auto get_vec3_for_rotation = [](float yaw, float pitch) -> mapper::__vec3
        {
            float f0 = cos(-yaw * 0.017453292f - 3.141592653589793f);
            float f1 = sin(-yaw * 0.017453292f - 3.141592653589793f);
            float f2 = cos(-pitch * 0.017453292f);
            float f3 = sin(-pitch * 0.017453292f);

            mapper::__vec3 res;
            res.x = f1 * -f2;
            res.y = f3;
            res.z = f0 * -f2;
            return res;
        };

    mapper::__vec2 view_angles = this->get_view_angles();
    mapper::__vec2 old_view_angles = this->get_old_view_angles();

    return get_vec3_for_rotation(
        old_view_angles.x + (view_angles.x - old_view_angles.x) * partial_ticks,
        old_view_angles.y + (view_angles.y - old_view_angles.y) * partial_ticks
    );
}

mapper::__vec2 mapper::__player::get_view_angles()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("rotationYaw", "F");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("rotationPitch", "F");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70177_z", "F");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("y", "F");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70125_A", "F");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("z", "F");

    mapper::__vec2 result = {};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr) return result;
    result.x = sdk::jni->GetFloatField(this->object, field_0.identifier);
    result.y = sdk::jni->GetFloatField(this->object, field_1.identifier);
    return result;
}

void mapper::__player::set_view_angles(mapper::__vec2 view_angles)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("rotationYaw", "F");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("rotationPitch", "F");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70177_z", "F");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("y", "F");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70125_A", "F");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("z", "F");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field_0.identifier, view_angles.x);
    sdk::jni->SetFloatField(this->object, field_1.identifier, view_angles.y);
}

mapper::__vec2 mapper::__player::get_old_view_angles()
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("prevRotationYaw", "F");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("prevRotationPitch", "F");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70126_B", "F");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("A", "F");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70127_C", "F");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("B", "F");

    mapper::__vec2 result = {};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr) return result;
    result.x = sdk::jni->GetFloatField(this->object, field_0.identifier);
    result.y = sdk::jni->GetFloatField(this->object, field_1.identifier);
    return result;
}

void mapper::__player::set_old_view_angles(mapper::__vec2 old_view_angles)
{
    mapper::__field field_0 = mapper::classes["EntityPlayerXP"].get_field("prevRotationYaw", "F");
    mapper::__field field_1 = mapper::classes["EntityPlayerXP"].get_field("prevRotationPitch", "F");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("field_70126_B", "F");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["EntityPlayerXP"].get_field("A", "F");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("field_70127_C", "F");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["EntityPlayerXP"].get_field("B", "F");

    if (field_0.identifier == nullptr || field_1.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field_0.identifier, old_view_angles.x);
    sdk::jni->SetFloatField(this->object, field_1.identifier, old_view_angles.y);
}

__int32 mapper::__player::get_ticks_existed()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("ticksExisted", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70173_aa", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("W", "I");

    if (field.identifier == nullptr) return 0;
    return sdk::jni->GetIntField(this->object, field.identifier);
}

float mapper::__player::get_health()
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getHealth", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_110143_aJ", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bn", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bm", "()F"); // 1.7.10 alt

    if (method.identifier == nullptr) return 0.f;
    float hp = sdk::jni->CallFloatMethod(this->object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return 0.f; }
    return hp;
}

float mapper::__player::get_move_foreward()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveForward", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70701_bs", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("ba", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("be", "F"); // 1.7.10

    if (field.identifier == nullptr) return 0.f;
    return sdk::jni->GetFloatField(this->object, field.identifier);
}

float mapper::__player::get_move_strafing()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveStrafing", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70702_br", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("aZ", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("bd", "F"); // 1.7.10

    if (field.identifier == nullptr) return 0.f;
    return sdk::jni->GetFloatField(this->object, field.identifier);
}

__int32 mapper::__player::get_total_armor_value()
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getTotalArmorValue", "()I");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70658_aO", "()I");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("br", "()I");

    if (method.identifier == nullptr) return 0;
    return sdk::jni->CallIntMethod(this->object, method.identifier);
}

bool mapper::__player::can_entity_be_seen(mapper::__player player)
{
    std::string signature = "(" + mapper::classes["Entity"].signature + ")Z";

    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("canEntityBeSeen", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70685_l", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("t", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("s", signature); // 1.7.10
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("u", signature); // 1.7.10 alt
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("q", signature); // 1.7.10 alt2
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("r", signature); // 1.7.10 alt3

    // Si no encontramos el metodo, retornar true (asumir visible)
    // para que el AimAssist no filtre todos los jugadores
    if (method.identifier == nullptr) return true;
    bool result = sdk::jni->CallBooleanMethod(this->object, method.identifier, player.object);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return true; }
    return result;
}

bool mapper::__player::get_flag(__int32 flag)
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getFlag", "(I)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70083_f", "(I)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("g", "(I)Z");

    if (method.identifier == nullptr) return false;
    return sdk::jni->CallBooleanMethod(this->object, method.identifier, flag);
}

void mapper::__player::set_move_foreward(float value)
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveForward", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70701_bs", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("ba", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("be", "F"); // 1.7.10

    if (field.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field.identifier, value);
}

void mapper::__player::set_move_strafing(float value)
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("moveStrafing", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70702_br", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("aZ", "F");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("bd", "F"); // 1.7.10

    if (field.identifier == nullptr) return;
    sdk::jni->SetFloatField(this->object, field.identifier, value);
}

void mapper::__player::set_flag(__int32 flag, bool state)
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("setFlag", "(IZ)V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70052_a", "(IZ)V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("b", "(IZ)V");

    if (method.identifier == nullptr) return;
    sdk::jni->CallVoidMethod(this->object, method.identifier, flag, state);
}

bool mapper::__player::is_offset_position_in_liquid(double x, double y, double z)
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("isOffsetPositionInLiquid", "(DDD)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70038_c", "(DDD)Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("c", "(DDD)Z");

    if (method.identifier == nullptr) return true; // safe default: assume in liquid
    return !sdk::jni->CallBooleanMethod(this->object, method.identifier, x, y, z);
}

mapper::__item_stack mapper::__player::get_held_item_stack()
{
    if (this->object == nullptr) return mapper::__item_stack(nullptr);

    std::string signature = "()" + mapper::classes["ItemStack"].signature;

    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getHeldItem", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70694_bm", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bA", signature);
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bI", signature); // 1.7.10 alt
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bJ", signature); // 1.7.10 alt
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("be", signature); // 1.7.10 obf (EntityLivingBase)
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bf", signature); // 1.7.10 obf alt2

    // 1.7.10 Fallback: get inventory field, then call getCurrentItem
    if (method.identifier == nullptr && mapper::version == mapper::MINECRAFT_17) {
        mapper::__field inv_field = mapper::classes["EntityPlayerXP"].get_field("inventory", "L" + mapper::classes["InventoryPlayer"].name + ";");
        if (inv_field.identifier == nullptr) inv_field = mapper::classes["EntityPlayerXP"].get_field("field_71071_by", "L" + mapper::classes["InventoryPlayer"].name + ";");
        if (inv_field.identifier == nullptr) inv_field = mapper::classes["EntityPlayerXP"].get_field("bm", "L" + mapper::classes["InventoryPlayer"].name + ";");
        if (inv_field.identifier == nullptr) inv_field = mapper::classes["EntityPlayerXP"].get_field("bg", "L" + mapper::classes["InventoryPlayer"].name + ";");
        
        if (inv_field.identifier != nullptr) {
            jobject inv_obj = sdk::jni->GetObjectField(this->object, inv_field.identifier);
            if (inv_obj != nullptr) {
                mapper::__method get_curr = mapper::classes["InventoryPlayer"].get_method("getCurrentItem", signature);
                if (get_curr.identifier == nullptr) get_curr = mapper::classes["InventoryPlayer"].get_method("func_70448_g", signature);
                if (get_curr.identifier == nullptr) get_curr = mapper::classes["InventoryPlayer"].get_method("h", signature);
                
                if (get_curr.identifier != nullptr) {
                    jobject result = sdk::jni->CallObjectMethod(inv_obj, get_curr.identifier);
                    sdk::jni->DeleteLocalRef(inv_obj);
                    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return mapper::__item_stack(nullptr); }
                    return mapper::__item_stack(result);
                }
                sdk::jni->DeleteLocalRef(inv_obj);
            }
        }
    }

    if (method.identifier == nullptr) return mapper::__item_stack(nullptr);
    jobject result = sdk::jni->CallObjectMethod(this->object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return mapper::__item_stack(nullptr); }
    return mapper::__item_stack(result);
}

std::string mapper::__player::get_name()
{
    if (this->object == nullptr) return "";

    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getName", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("getCommandSenderName", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70005_c_", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("e_", "()Ljava/lang/String;");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("d_", "()Ljava/lang/String;"); // 1.7.10 alt

    if (method.identifier == nullptr) return "";

    jstring jstr = (jstring)sdk::jni->CallObjectMethod(this->object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return ""; }
    if (!jstr) return "";

    const char* name_chars = sdk::jni->GetStringUTFChars(jstr, nullptr);
    std::string fixed_name = "";

    if (name_chars != nullptr)
    {
        for (int i = 0; name_chars[i] != '\0'; ++i)
        {
            unsigned char c = (unsigned char)name_chars[i];

            if (c >= 32 && c <= 126)
            {
                fixed_name += (char)c;
            }
        }
        sdk::jni->ReleaseStringUTFChars(jstr, name_chars);
    }
    sdk::jni->DeleteLocalRef(jstr);

    return fixed_name;
}

std::string mapper::__player::get_uuid()
{
    if (!this->object || !sdk::jni) return "";

    mapper::__field gp_field = mapper::classes["EntityPlayer"].get_field(
        "gameProfile", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "field_146106_i", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "i", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "bH", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) gp_field = mapper::classes["EntityPlayer"].get_field(
        "cc", "Lcom/mojang/authlib/GameProfile;");
    if (!gp_field.identifier) {
        for (const auto& field : mapper::classes["EntityPlayer"].fields) {
            if (field.signature.find("GameProfile") != std::string::npos) {
                gp_field = field;
                break;
            }
        }
    }
    if (!gp_field.identifier) return "";

    jobject profile = sdk::jni->GetObjectField(this->object, gp_field.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return ""; }
    if (!profile) return "";

    jclass profile_class = sdk::jni->GetObjectClass(profile);
    jmethodID get_id = profile_class
        ? sdk::jni->GetMethodID(profile_class, "getId", "()Ljava/util/UUID;")
        : nullptr;
    if (!get_id && sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();

    jobject uuid_object = get_id
        ? sdk::jni->CallObjectMethod(profile, get_id)
        : nullptr;
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); uuid_object = nullptr; }

    std::string result;
    if (uuid_object) {
        jclass uuid_class = sdk::jni->GetObjectClass(uuid_object);
        jmethodID to_string = uuid_class
            ? sdk::jni->GetMethodID(uuid_class, "toString", "()Ljava/lang/String;")
            : nullptr;
        if (to_string) {
            jstring text = static_cast<jstring>(
                sdk::jni->CallObjectMethod(uuid_object, to_string));
            if (!sdk::jni->ExceptionCheck() && text) {
                const char* chars = sdk::jni->GetStringUTFChars(text, nullptr);
                if (chars) {
                    result = chars;
                    sdk::jni->ReleaseStringUTFChars(text, chars);
                }
                sdk::jni->DeleteLocalRef(text);
            } else if (sdk::jni->ExceptionCheck()) {
                sdk::jni->ExceptionClear();
            }
        }
        if (uuid_class) sdk::jni->DeleteLocalRef(uuid_class);
        sdk::jni->DeleteLocalRef(uuid_object);
    }

    if (profile_class) sdk::jni->DeleteLocalRef(profile_class);
    sdk::jni->DeleteLocalRef(profile);
    result.erase(std::remove(result.begin(), result.end(), '-'), result.end());
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return result.size() == 32 ? result : std::string{};
}

void mapper::__player::set_name(const std::string& new_name)
{
    if (this->object == nullptr) return;

    mapper::__field gp_field = mapper::classes["EntityPlayer"].get_field("gameProfile", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("field_146106_i", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("i", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("bH", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("cc", "Lcom/mojang/authlib/GameProfile;"); // 1.7.10

    // Si todo falla, busqueda dinamica por signature (Inmune a ofuscacion o repackaging)
    if (gp_field.identifier == nullptr) {
        for (auto& f : mapper::classes["EntityPlayer"].fields) {
            if (f.signature.find("GameProfile") != std::string::npos) {
                gp_field = f;
                break;
            }
        }
    }

    if (gp_field.identifier == nullptr) return;

    jobject game_profile = sdk::jni->GetObjectField(this->object, gp_field.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
    if (game_profile == nullptr) return;

    jclass gp_class = sdk::jni->GetObjectClass(game_profile);
    if (gp_class != nullptr)
    {
        jfieldID name_field = sdk::jni->GetFieldID(gp_class, "name", "Ljava/lang/String;");
        if (name_field != nullptr)
        {
            // Mutamos el string directamente para evadir caches
            mutate_gameprofile_name(game_profile, name_field, new_name);
        }
        sdk::jni->DeleteLocalRef(gp_class);
    }
    sdk::jni->DeleteLocalRef(game_profile);
}

__int32 mapper::__player::get_entity_id()
{
    if (this->object == nullptr) return -1;
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("entityId", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_145783_c", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("d", "I");

    // Try on base class if not found
    if (field.identifier == nullptr) field = mapper::classes["Entity"].get_field("entityId", "I");
    if (field.identifier == nullptr) field = mapper::classes["Entity"].get_field("field_145783_c", "I");
    if (field.identifier == nullptr) field = mapper::classes["Entity"].get_field("d", "I");

    if (field.identifier == nullptr) return -1;
    return sdk::jni->GetIntField(this->object, field.identifier);
}

mapper::__axis_aligned mapper::__player::get_bounding_box()
{
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("boundingBox", mapper::classes["AxisAlignedBB"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("field_70121_D", mapper::classes["AxisAlignedBB"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("f", mapper::classes["AxisAlignedBB"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayerXP"].get_field("bb", mapper::classes["AxisAlignedBB"].signature); // 1.7 alt

    if (field.identifier == nullptr) return mapper::__axis_aligned(nullptr);
    return mapper::__axis_aligned(sdk::jni->GetObjectField(this->object, field.identifier));
}

// --- NUEVA FUNCIÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã¢â‚¬Å“N PARA EL REFILL (LEER INVENTARIO) ---
// --- NUEVA FUNCION PARA EL REFILL (LEER INVENTARIO) ---
mapper::__item_stack mapper::__player::get_inventory_slot(int slot_id)
{
    mapper::__field field = mapper::classes["EntityPlayer"].get_field("inventory", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71071_by", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bg", mapper::classes["InventoryPlayer"].signature); // 1.8.9 / 1.7.10
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bi", mapper::classes["InventoryPlayer"].signature);

    if (field.identifier == nullptr) return mapper::__item_stack(nullptr);

    jobject inventory_obj = sdk::jni->GetObjectField(this->object, field.identifier);
    if (inventory_obj == nullptr) return mapper::__item_stack(nullptr);

    // 2. Igual que el Dream: GetObjectClass directo sobre el objeto real
    // Mas confiable en Lunar/Vanilla donde los nombres estan ofuscados
    jclass inventory_class = sdk::jni->GetObjectClass(inventory_obj);
    if (inventory_class == nullptr) {
        sdk::jni->DeleteLocalRef(inventory_obj);
        return mapper::__item_stack(nullptr);
    }

    // 3. Buscar getStackInSlot con los 3 nombres posibles
    std::string method_sig = "(I)" + mapper::classes["ItemStack"].signature;
    jmethodID get_stack = sdk::jni->GetMethodID(inventory_class, "getStackInSlot", method_sig.c_str());
    if (get_stack == nullptr) { sdk::jni->ExceptionClear(); get_stack = sdk::jni->GetMethodID(inventory_class, "func_70301_a", method_sig.c_str()); }
    if (get_stack == nullptr) { sdk::jni->ExceptionClear(); get_stack = sdk::jni->GetMethodID(inventory_class, "a", method_sig.c_str()); }

    jobject item_stack_obj = nullptr;
    if (get_stack != nullptr) {
        item_stack_obj = sdk::jni->CallObjectMethod(inventory_obj, get_stack, slot_id);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); item_stack_obj = nullptr; }
    }

    sdk::jni->DeleteLocalRef(inventory_class);
    sdk::jni->DeleteLocalRef(inventory_obj);

    return mapper::__item_stack(item_stack_obj);
}

// ==========================================
// [NUEVO] OBTENER Y CAMBIAR SLOT (Para Macros)
// ==========================================
int mapper::__player::get_current_slot()
{
    mapper::__field field = mapper::classes["EntityPlayer"].get_field("inventory", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71071_by", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bg", mapper::classes["InventoryPlayer"].signature); // 1.8.9 / 1.7.10
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bi", mapper::classes["InventoryPlayer"].signature);

    if (field.identifier == nullptr) return 0;
    jobject inventory_obj = sdk::jni->GetObjectField(this->object, field.identifier);
    if (inventory_obj == nullptr) return 0;

    mapper::__field slot_field = mapper::classes["InventoryPlayer"].get_field("currentItem", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("field_70461_c", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("c", "I");

    int slot = sdk::jni->GetIntField(inventory_obj, slot_field.identifier);
    sdk::jni->DeleteLocalRef(inventory_obj);
    return slot;
}

void mapper::__player::set_current_slot(int slot)
{
    mapper::__field field = mapper::classes["EntityPlayer"].get_field("inventory", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71071_by", mapper::classes["InventoryPlayer"].signature);
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bg", mapper::classes["InventoryPlayer"].signature); // 1.8.9 / 1.7.10
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("bi", mapper::classes["InventoryPlayer"].signature);

    if (field.identifier == nullptr) return;
    jobject inventory_obj = sdk::jni->GetObjectField(this->object, field.identifier);
    if (inventory_obj == nullptr) return;

    mapper::__field slot_field = mapper::classes["InventoryPlayer"].get_field("currentItem", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("field_70461_c", "I");
    if (slot_field.identifier == nullptr) slot_field = mapper::classes["InventoryPlayer"].get_field("c", "I");

    sdk::jni->SetIntField(inventory_obj, slot_field.identifier, slot);
    sdk::jni->DeleteLocalRef(inventory_obj);
}
// ==========================================
// METODOS PARA NAMETAGS
// ==========================================

bool mapper::__player::is_invisible()
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("isInvisible", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70070_b", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("P", "()Z");

    if (method.identifier == nullptr) return false;
    return sdk::jni->CallBooleanMethod(this->object, method.identifier);
}

float mapper::__player::get_max_health()
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("getMaxHealth", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_110148_a", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bm", "()F");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("bn", "()F"); // 1.7.10

    if (method.identifier == nullptr) return 20.0f;
    return sdk::jni->CallFloatMethod(this->object, method.identifier);
}

bool mapper::__player::is_using_item()
{
    if (this->object == nullptr) return false;

    // MÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â°TODO 1: Leer el contador interno de uso (100% infalible)
    mapper::__field field = mapper::classes["EntityPlayer"].get_field("itemInUseCount", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71072_f", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("h", "I"); // Obfuscado 1.8.9

    if (field.identifier != nullptr) {
        int count = sdk::jni->GetIntField(this->object, field.identifier);
        if (count > 0) return true; // Si es mayor a 0, estÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡s comiendo o bloqueando
    }

    // MÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€šÃ‚Â°TODO 2: Fallback de seguridad al mÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â©todo tradicional
    mapper::__method method = mapper::classes["EntityPlayer"].get_method("isUsingItem", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("func_71039_bw", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bS", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bQ", "()Z");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayer"].get_method("bR", "()Z");

    if (method.identifier != nullptr) {
        return sdk::jni->CallBooleanMethod(this->object, method.identifier);
    }

    return false;
}

void mapper::__player::set_item_in_use_count(int count)
{
    if (this->object == nullptr) return;

    mapper::__field field = mapper::classes["EntityPlayer"].get_field("itemInUseCount", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("field_71072_f", "I");
    if (field.identifier == nullptr) field = mapper::classes["EntityPlayer"].get_field("h", "I"); // Obfuscado 1.8.9

    if (field.identifier != nullptr) {
        sdk::jni->SetIntField(this->object, field.identifier, count);
    }
}

void mapper::__player::set_sprinting(bool state)
{
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("setSprinting", "(Z)V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_70031_b", "(Z)V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("d", "(Z)V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("c", "(Z)V"); // 1.7.10

    if (method.identifier != nullptr)
        sdk::jni->CallVoidMethod(this->object, method.identifier, state);
}

void mapper::__player::set_always_render_nametag(bool state)
{
    if (this->object == nullptr) return;
    mapper::__method method = mapper::classes["EntityPlayerXP"].get_method("setAlwaysRenderNameTag", "(Z)V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("func_174805_g", "(Z)V");
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("g", "(Z)V"); // Notch 1.8.9
    if (method.identifier == nullptr) method = mapper::classes["EntityPlayerXP"].get_method("aG", "(Z)V"); // Alternate Notch 1.8.9
    
    if (method.identifier != nullptr) {
        sdk::jni->CallVoidMethod(this->object, method.identifier, state);
    }
}

void mapper::__player::send_packet(jobject packet)
{
    if (this->object == nullptr || packet == nullptr) return;

    std::string sig = "L" + mapper::classes["NetHandlerPlayClient"].name + ";";
    mapper::__field field = mapper::classes["EntityPlayerXP"].get_field("sendQueue", sig);
    if (!field.identifier) field = mapper::classes["EntityPlayerXP"].get_field("field_71174_a", sig);
    if (!field.identifier) field = mapper::classes["EntityPlayerXP"].get_field("a", sig);

    if (field.identifier == nullptr) return;

    jobject send_queue = sdk::jni->GetObjectField(this->object, field.identifier);
    if (send_queue == nullptr) return;

    std::string method_sig = "(" + mapper::classes["Packet"].signature + ")V";
    mapper::__method method = mapper::classes["NetHandlerPlayClient"].get_method("addToSendQueue", method_sig);
    if (!method.identifier) method = mapper::classes["NetHandlerPlayClient"].get_method("func_147297_a", method_sig);
    if (!method.identifier) method = mapper::classes["NetHandlerPlayClient"].get_method("a", method_sig);

    if (method.identifier != nullptr) {
        sdk::jni->CallVoidMethod(send_queue, method.identifier, packet);
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    }
    sdk::jni->DeleteLocalRef(send_queue);
}

void mapper::__player::hide_name_completely(bool only_string)
{
    if (this->object == nullptr) return;

    if (!only_string) {
        // 1. Apagar renderizado vanilla nativo
        mapper::__method method = mapper::classes["Entity"].get_method("setAlwaysRenderNameTag", "(Z)V");
        if (method.identifier == nullptr) method = mapper::classes["Entity"].get_method("func_174805_g", "(Z)V");
        if (method.identifier == nullptr) method = mapper::classes["Entity"].get_method("g", "(Z)V");
        if (method.identifier == nullptr) method = mapper::classes["Entity"].get_method("aG", "(Z)V");
        
        if (method.identifier != nullptr) {
            sdk::jni->CallVoidMethod(this->object, method.identifier, false);
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        }

        // 1.5 Forzar CustomNameTag vacio
        mapper::__method custom_method = mapper::classes["Entity"].get_method("setCustomNameTag", "(Ljava/lang/String;)V");
        if (custom_method.identifier == nullptr) custom_method = mapper::classes["Entity"].get_method("func_96094_a", "(Ljava/lang/String;)V");
        if (custom_method.identifier == nullptr) custom_method = mapper::classes["Entity"].get_method("a", "(Ljava/lang/String;)V");
        
        if (custom_method.identifier != nullptr) {
            jstring empty_str = sdk::jni->NewStringUTF(" ");
            sdk::jni->CallVoidMethod(this->object, custom_method.identifier, empty_str);
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
            sdk::jni->DeleteLocalRef(empty_str);
        }
    }

    // 2. Limpiar GameProfile para clientes que fuerzan el renderizado (ej: Lunar Client)
    mapper::__field gp_field = mapper::classes["EntityPlayer"].get_field("gameProfile", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("field_146106_i", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("i", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("bH", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("cc", "Lcom/mojang/authlib/GameProfile;"); // 1.7.10

    if (gp_field.identifier == nullptr) {
        for (auto& f : mapper::classes["EntityPlayer"].fields) {
            if (f.signature.find("GameProfile") != std::string::npos) { 
                gp_field = f; 
                break; 
            }
        }
    }
    if (gp_field.identifier == nullptr) return;

    jobject game_profile = sdk::jni->GetObjectField(this->object, gp_field.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
    if (game_profile == nullptr) return;

    jclass gp_class = sdk::jni->GetObjectClass(game_profile);
    if (gp_class != nullptr)
    {
        jfieldID name_field = sdk::jni->GetFieldID(gp_class, "name", "Ljava/lang/String;");
        if (name_field != nullptr)
        {
            // Mutamos el string con espacios para borrarlo del cache visual
            mutate_gameprofile_name(game_profile, name_field, "");
        }
        sdk::jni->DeleteLocalRef(gp_class);
    }
    sdk::jni->DeleteLocalRef(game_profile);
}
void mapper::__player::restore_name_completely(const std::string& original_name, bool only_string)
{
    if (this->object == nullptr) return;

    if (!only_string) {
        // 1. Restaurar CustomNameTag con el original
        mapper::__method custom_method = mapper::classes["Entity"].get_method("setCustomNameTag", "(Ljava/lang/String;)V");
        if (custom_method.identifier == nullptr) custom_method = mapper::classes["Entity"].get_method("func_96094_a", "(Ljava/lang/String;)V");
        if (custom_method.identifier == nullptr) custom_method = mapper::classes["Entity"].get_method("a", "(Ljava/lang/String;)V");
        
        if (custom_method.identifier != nullptr) {
            jstring orig_str = sdk::jni->NewStringUTF(original_name.c_str());
            sdk::jni->CallVoidMethod(this->object, custom_method.identifier, orig_str);
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
            sdk::jni->DeleteLocalRef(orig_str);
        }

        // 2. Restaurar renderizado nativo
        mapper::__method method = mapper::classes["Entity"].get_method("setAlwaysRenderNameTag", "(Z)V");
        if (method.identifier == nullptr) method = mapper::classes["Entity"].get_method("func_174805_g", "(Z)V");
        if (method.identifier == nullptr) method = mapper::classes["Entity"].get_method("g", "(Z)V");
        if (method.identifier == nullptr) method = mapper::classes["Entity"].get_method("aG", "(Z)V");
        
        if (method.identifier != nullptr) {
            sdk::jni->CallVoidMethod(this->object, method.identifier, true);
            if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        }
    }

    // 3. Restaurar GameProfile name
    mapper::__field gp_field = mapper::classes["EntityPlayer"].get_field("gameProfile", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("field_146106_i", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("i", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("bH", "Lcom/mojang/authlib/GameProfile;");
    if (gp_field.identifier == nullptr) gp_field = mapper::classes["EntityPlayer"].get_field("cc", "Lcom/mojang/authlib/GameProfile;"); // 1.7.10

    if (gp_field.identifier == nullptr) {
        for (auto& f : mapper::classes["EntityPlayer"].fields) {
            if (f.signature.find("GameProfile") != std::string::npos) { 
                gp_field = f; 
                break; 
            }
        }
    }
    if (gp_field.identifier == nullptr) return;

    jobject game_profile = sdk::jni->GetObjectField(this->object, gp_field.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return; }
    if (game_profile == nullptr) return;

    jclass gp_class = sdk::jni->GetObjectClass(game_profile);
    if (gp_class != nullptr)
    {
        jfieldID name_field = sdk::jni->GetFieldID(gp_class, "name", "Ljava/lang/String;");
        if (name_field != nullptr)
        {
            mutate_gameprofile_name(game_profile, name_field, original_name);
        }
        sdk::jni->DeleteLocalRef(gp_class);
    }
    sdk::jni->DeleteLocalRef(game_profile);
}
