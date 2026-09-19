#include "mapper.hpp"

mapper::__gui_screen::__gui_screen(jobject object)
{
    this->object = object;
}

mapper::__gui_screen::__gui_screen(const mapper::__gui_screen& gui_screen)
{
    if (gui_screen.object != nullptr)
        this->object = sdk::jni->NewLocalRef(gui_screen.object);
}

mapper::__gui_screen::~__gui_screen()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

bool mapper::__gui_screen::is_inventory_instance()
{
    if (this->object == nullptr) return false;

    // Verifica si la pantalla actual es un Inventario o un Cofre
    bool result = false;
    if (mapper::classes["GuiInventory"].klass != nullptr)
        result = sdk::jni->IsInstanceOf(this->object, mapper::classes["GuiInventory"].klass);
    if (!result && mapper::classes["GuiChest"].klass != nullptr)
        result = sdk::jni->IsInstanceOf(this->object, mapper::classes["GuiChest"].klass);
    if (!result && mapper::classes["GuiContainer"].klass != nullptr)
        result = sdk::jni->IsInstanceOf(this->object, mapper::classes["GuiContainer"].klass);
    return result;
}
