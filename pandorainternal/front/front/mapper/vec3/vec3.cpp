#include "mapper.hpp"
#include <cmath> // Necesario para las funciones matematicas (atan2, sqrt, fmod)

// --- WRAPPER PARA EL VEC3 DE JAVA ---

mapper::__vec3__::__vec3__(mapper::__vec3 vec3)
{
    // PRIMERO intentar constructor directo (1.8.9)
    mapper::__method method = mapper::classes["Vec3"].get_method("<init>", "(DDD)V");

    if (method.identifier != nullptr && mapper::classes["Vec3"].klass != nullptr) {
        this->object = sdk::jni->NewObject(mapper::classes["Vec3"].klass, method.identifier, vec3.x, vec3.y, vec3.z);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); this->object = nullptr; }
        if (this->object != nullptr) return;
    }

    // FALLBACK 1.7.10: Vec3.createVectorHelper(double, double, double) -> Vec3  (metodo estatico)
    if (mapper::classes["Vec3"].klass != nullptr) {
        std::string sig = "(DDD)" + mapper::classes["Vec3"].signature;

        mapper::__method factory = mapper::classes["Vec3"].get_method("createVectorHelper", sig);
        if (!factory.identifier) factory = mapper::classes["Vec3"].get_method("func_72443_a", sig);
        if (!factory.identifier) factory = mapper::classes["Vec3"].get_method("a", sig);

        if (factory.identifier) {
            this->object = sdk::jni->CallStaticObjectMethod(mapper::classes["Vec3"].klass, factory.identifier, vec3.x, vec3.y, vec3.z);
            if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); this->object = nullptr; }
            return;
        }
    }

    this->object = nullptr;
}

mapper::__vec3__::__vec3__(jobject object)
{
    this->object = object;
}

mapper::__vec3__::__vec3__(const mapper::__vec3__& vec3)
{
    if (vec3.object != nullptr)
        this->object = sdk::jni->NewLocalRef(vec3.object);
}

mapper::__vec3__::~__vec3__()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

mapper::__vec3 mapper::__vec3__::get_vec3()
{
    mapper::__field field_0 = mapper::classes["Vec3"].get_field("xCoord", "D");
    mapper::__field field_1 = mapper::classes["Vec3"].get_field("yCoord", "D");
    mapper::__field field_2 = mapper::classes["Vec3"].get_field("zCoord", "D");

    if (field_0.identifier == nullptr) field_0 = mapper::classes["Vec3"].get_field("field_72450_a", "D");
    if (field_0.identifier == nullptr) field_0 = mapper::classes["Vec3"].get_field("a", "D");

    if (field_1.identifier == nullptr) field_1 = mapper::classes["Vec3"].get_field("field_72448_b", "D");
    if (field_1.identifier == nullptr) field_1 = mapper::classes["Vec3"].get_field("b", "D");

    if (field_2.identifier == nullptr) field_2 = mapper::classes["Vec3"].get_field("field_72449_c", "D");
    if (field_2.identifier == nullptr) field_2 = mapper::classes["Vec3"].get_field("c", "D");

    mapper::__vec3 result = {0.0, 0.0, 0.0};
    if (field_0.identifier == nullptr || field_1.identifier == nullptr || field_2.identifier == nullptr) return result;
    result.x = sdk::jni->GetDoubleField(this->object, field_0.identifier);
    result.y = sdk::jni->GetDoubleField(this->object, field_1.identifier);
    result.z = sdk::jni->GetDoubleField(this->object, field_2.identifier);
    return result;
}

// --- LOGICA MATEMATICA NATIVA DE C++ ---

double mapper::__vec3::get_distance_to_vec3(mapper::__vec3 vec3)
{
    double dx = this->x - vec3.x;
    double dy = this->y - vec3.y;
    double dz = this->z - vec3.z;

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float mapper::__vec3::get_angle_x_difference_to_vec3(mapper::__vec3 target, float angle)
{
    double dx = target.x - this->x;
    double dz = target.z - this->z;

    double target_yaw = std::atan2(dz, dx) * 180.0 / 3.14159265358979323846 - 90.0;

    float angle_x_difference = std::fmod((float)target_yaw - angle, 360.0f);

    if (angle_x_difference >= 180.0f)
        angle_x_difference -= 360.0f;

    if (angle_x_difference < -180.0f)
        angle_x_difference += 360.0f;

    return angle_x_difference;
}

float mapper::__vec3::get_angle_y_difference_to_vec3(mapper::__vec3 target, float angle)
{
    double dx = target.x - this->x;
    double dy = this->y - target.y;
    double dz = target.z - this->z;

    double dist = std::sqrt(dx * dx + dz * dz);

    double target_pitch = std::atan2(dy, dist) * 180.0 / 3.14159265358979323846;

    return (float)target_pitch - angle;
}
