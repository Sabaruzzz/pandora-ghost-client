#include "mapper.hpp"

mapper::__axis_aligned::__axis_aligned(jobject object)
{
    this->object = object;
}

mapper::__axis_aligned::__axis_aligned(const mapper::__axis_aligned& axis_aligned)
{
    if (axis_aligned.object != nullptr)
        this->object = sdk::jni->NewLocalRef(axis_aligned.object);
}

mapper::__axis_aligned::~__axis_aligned()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

// Busca un field double por 3 nombres posibles (sin static para reintentar en 1.7)
static jfieldID find_aabb_field(const char* name, const char* srg, const char* obf)
{
    mapper::__field f = mapper::classes["AxisAlignedBB"].get_field(name, "D");
    if (!f.identifier) f = mapper::classes["AxisAlignedBB"].get_field(srg, "D");
    if (!f.identifier) f = mapper::classes["AxisAlignedBB"].get_field(obf, "D");
    return f.identifier;
}

std::pair<mapper::__vec3, mapper::__vec3> mapper::__axis_aligned::get_bounds()
{
    if (!this->object) return { {0,0,0}, {0,0,0} };

    jfieldID f0 = find_aabb_field("minX", "field_72340_a", "a");
    jfieldID f1 = find_aabb_field("minY", "field_72338_b", "b");
    jfieldID f2 = find_aabb_field("minZ", "field_72339_c", "c");
    jfieldID f3 = find_aabb_field("maxX", "field_72336_d", "d");
    jfieldID f4 = find_aabb_field("maxY", "field_72337_e", "e");
    jfieldID f5 = find_aabb_field("maxZ", "field_72334_f", "f");

    if (!f0 || !f1 || !f2 || !f3 || !f4 || !f5)
        return { {0,0,0}, {0,0,0} };

    return std::pair<mapper::__vec3, mapper::__vec3>(
        {
            sdk::jni->GetDoubleField(this->object, f0),
            sdk::jni->GetDoubleField(this->object, f1),
            sdk::jni->GetDoubleField(this->object, f2)
        },
        {
            sdk::jni->GetDoubleField(this->object, f3),
            sdk::jni->GetDoubleField(this->object, f4),
            sdk::jni->GetDoubleField(this->object, f5)
        });
}

void mapper::__axis_aligned::set_bounds(std::pair<mapper::__vec3, mapper::__vec3> bounds)
{
    if (!this->object) return;

    jfieldID f0 = find_aabb_field("minX", "field_72340_a", "a");
    jfieldID f1 = find_aabb_field("minY", "field_72338_b", "b");
    jfieldID f2 = find_aabb_field("minZ", "field_72339_c", "c");
    jfieldID f3 = find_aabb_field("maxX", "field_72336_d", "d");
    jfieldID f4 = find_aabb_field("maxY", "field_72337_e", "e");
    jfieldID f5 = find_aabb_field("maxZ", "field_72334_f", "f");

    if (!f0 || !f1 || !f2 || !f3 || !f4 || !f5) return;

    sdk::jni->SetDoubleField(this->object, f0, bounds.first.x);
    sdk::jni->SetDoubleField(this->object, f1, bounds.first.y);
    sdk::jni->SetDoubleField(this->object, f2, bounds.first.z);
    sdk::jni->SetDoubleField(this->object, f3, bounds.second.x);
    sdk::jni->SetDoubleField(this->object, f4, bounds.second.y);
    sdk::jni->SetDoubleField(this->object, f5, bounds.second.z);
}

mapper::__moving_object_position mapper::__axis_aligned::calculate_interception(mapper::__vec3 view_position, mapper::__vec3 view_vector)
{
    if (!this->object) return mapper::__moving_object_position(nullptr);

    std::string signature = "(" + mapper::classes["Vec3"].signature
        + mapper::classes["Vec3"].signature + ")" + mapper::classes["MovingObjectPosition"].signature;

    mapper::__method m = mapper::classes["AxisAlignedBB"].get_method("calculateIntercept", signature);
    if (m.identifier == nullptr) m = mapper::classes["AxisAlignedBB"].get_method("func_72327_a", signature);
    if (m.identifier == nullptr) m = mapper::classes["AxisAlignedBB"].get_method("a", signature);

    if (m.identifier == nullptr) return mapper::__moving_object_position(nullptr);

    mapper::__vec3__ pos_wrapper(view_position);
    mapper::__vec3__ vec_wrapper(view_vector);

    if (!pos_wrapper.object || !vec_wrapper.object)
        return mapper::__moving_object_position(nullptr);

    jobject intercept_obj = sdk::jni->CallObjectMethod(this->object, m.identifier,
        pos_wrapper.object, vec_wrapper.object);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return mapper::__moving_object_position(nullptr); }

    return mapper::__moving_object_position(intercept_obj);
}
