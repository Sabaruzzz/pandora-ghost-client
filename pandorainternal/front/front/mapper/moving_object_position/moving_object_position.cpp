#include "mapper.hpp"

mapper::__moving_object_position::__moving_object_position(jobject object)
{
    this->object = object;
}

mapper::__moving_object_position::__moving_object_position(mapper::__player player, mapper::__vec3__ vec3)
{
    // NO static: la firma cambia entre versiones
    std::string signature = "(" + mapper::classes["Entity"].signature + mapper::classes["Vec3"].signature + ")V";
    mapper::__method method = mapper::classes["MovingObjectPosition"].get_method("<init>", signature);

    if (method.identifier != nullptr && mapper::classes["MovingObjectPosition"].klass != nullptr) {
        this->object = sdk::jni->NewObject(mapper::classes["MovingObjectPosition"].klass, method.identifier, player.object, vec3.object);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); this->object = nullptr; }
    }
}

mapper::__moving_object_position::__moving_object_position(const mapper::__moving_object_position& moving_object_position)
{
    if (moving_object_position.object != nullptr)
        this->object = sdk::jni->NewLocalRef(moving_object_position.object);
}

mapper::__moving_object_position::~__moving_object_position()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

__int32 mapper::__moving_object_position::get_type_of_hit()
{
    if (!this->object) return -1;

    const std::string& mop_type_sig = mapper::classes["MovingObjectPosition_MovingObjectType"].signature;
    mapper::__field toh = mapper::classes["MovingObjectPosition"].get_field("typeOfHit", mop_type_sig);
    if (!toh.identifier) toh = mapper::classes["MovingObjectPosition"].get_field("field_72313_a", mop_type_sig);
    if (!toh.identifier) toh = mapper::classes["MovingObjectPosition"].get_field("a", mop_type_sig);
    if (!toh.identifier) return -1;

    jobject type_enum = sdk::jni->GetObjectField(this->object, toh.identifier);
    if (!type_enum) return -1;

    jclass enum_class = sdk::jni->GetObjectClass(type_enum);
    __int32 result = -1;
    if (enum_class) {
        jmethodID ordinal_mid = sdk::jni->GetMethodID(enum_class, "ordinal", "()I");
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
        if (ordinal_mid) result = sdk::jni->CallIntMethod(type_enum, ordinal_mid);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); result = -1; }
        sdk::jni->DeleteLocalRef(enum_class);
    }
    sdk::jni->DeleteLocalRef(type_enum);
    return result;
}


mapper::__vec3__ mapper::__moving_object_position::get_hit_vector()
{
    if (!this->object) return mapper::__vec3__(nullptr);

    // NO static: el nombre ofuscado cambia entre 1.8 y 1.7
    mapper::__field field = mapper::classes["MovingObjectPosition"].get_field("hitVec", mapper::classes["Vec3"].signature);
    if (field.identifier == nullptr)
        field = mapper::classes["MovingObjectPosition"].get_field("field_72307_f", mapper::classes["Vec3"].signature);
    if (field.identifier == nullptr)
        field = mapper::classes["MovingObjectPosition"].get_field("c", mapper::classes["Vec3"].signature); // 1.8.9 Lunar
    if (field.identifier == nullptr)
        field = mapper::classes["MovingObjectPosition"].get_field("e", mapper::classes["Vec3"].signature); // 1.7.10 Lunar
    if (field.identifier == nullptr)
        field = mapper::classes["MovingObjectPosition"].get_field("d", mapper::classes["Vec3"].signature); // 1.7.10 alt

    if (field.identifier == nullptr) return mapper::__vec3__(nullptr);
    return mapper::__vec3__(sdk::jni->GetObjectField(this->object, field.identifier));
}
