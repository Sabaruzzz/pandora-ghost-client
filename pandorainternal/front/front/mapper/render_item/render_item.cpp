#include "../mapper.hpp"
mapper::__render_item::__render_item(jobject object)
{
    this->object = object;
}

mapper::__render_item::__render_item(const mapper::__render_item& render_item)
{
    this->object = render_item.object;
}

mapper::__render_item::~__render_item()
{
}

void mapper::__render_item::render_item_into_gui(mapper::__item_stack item_stack, int x, int y)
{
    if (this->object == nullptr || item_stack.object == nullptr) return;

    std::string sig = "(L" + mapper::classes["ItemStack"].signature + ";II)V";
    mapper::__method method = mapper::classes["RenderItem"].get_method("renderItemIntoGUI", sig);
    if (!method.identifier) method = mapper::classes["RenderItem"].get_method("func_180450_b", sig);
    if (!method.identifier) method = mapper::classes["RenderItem"].get_method("b", sig); // 1.8.9 obf

    if (method.identifier != nullptr) {
        sdk::jni->CallVoidMethod(this->object, method.identifier, item_stack.object, x, y);
        if (sdk::jni->ExceptionCheck()) sdk::jni->ExceptionClear();
    }
}

mapper::__render_item mapper::__minecraft::get_render_item()
{
    if (this->object == nullptr) return mapper::__render_item(nullptr);
    std::string sig = "()L" + mapper::classes["RenderItem"].signature + ";";
    mapper::__method method = mapper::classes["Minecraft"].get_method("getRenderItem", sig);
    if (!method.identifier) method = mapper::classes["Minecraft"].get_method("func_175599_af", sig);
    if (!method.identifier) method = mapper::classes["Minecraft"].get_method("ag", sig); // 1.8.9 obf

    if (method.identifier != nullptr) {
        jobject render_item_obj = sdk::jni->CallObjectMethod(this->object, method.identifier);
        if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); return mapper::__render_item(nullptr); }
        return mapper::__render_item(render_item_obj);
    }
    return mapper::__render_item(nullptr);
}
