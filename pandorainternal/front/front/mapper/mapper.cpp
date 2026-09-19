#include "mapper.hpp"
#include <jvmti.h>
#include <iostream>

extern "C" void pandoraLog(const char* format, ...);

namespace mapper
{
    jvmtiEnv* jvmti = nullptr;
}

mapper::__field mapper::__class::get_field(std::string name, std::string signature)
{
    mapper::__field field = {};
    for (auto& temporal_field : this->fields)
    {
        if (temporal_field.name == name && temporal_field.signature == signature)
        {
            field = temporal_field;
            break;
        }
    }
    return field;
}

mapper::__method mapper::__class::get_method(std::string name, std::string signature)
{
    mapper::__method method = {};
    for (auto& temporal_method : this->methods)
    {
        if (temporal_method.name == name && temporal_method.signature == signature)
        {
            method = temporal_method;
            break;
        }
    }
    return method;
}

mapper::__class mapper::get_class(std::string name)
{
    mapper::__class klass;
    klass.name = name;
    klass.signature = "L" + name + ";";
    klass.klass = nullptr;

    // 1. Intentar el buscador estandar
    if (jclass temporal_class = sdk::jni->FindClass(name.c_str()); temporal_class != nullptr)
    {
        klass.klass = (jclass)sdk::jni->NewGlobalRef(temporal_class);
        sdk::jni->DeleteLocalRef(temporal_class);
    }
    else
    {
        sdk::jni->ExceptionClear();
    }

    // 2. Si el buscador estandar falla (tipico en Lunar), usar el escaner profundo JVMTI
    if (klass.klass == nullptr && mapper::jvmti != nullptr)
    {
        jint loaded_classes_count = 0;
        jclass* loaded_classes = nullptr;

        mapper::jvmti->GetLoadedClasses(&loaded_classes_count, &loaded_classes);

        for (jint x = 0; x < loaded_classes_count && klass.klass == nullptr; ++x)
        {
            char* class_signature = nullptr;
            char* class_reserved = nullptr;

            mapper::jvmti->GetClassSignature(loaded_classes[x], &class_signature, &class_reserved);

            if (class_signature != nullptr)
            {
                if (std::string(class_signature) == klass.signature)
                {
                    klass.klass = (jclass)sdk::jni->NewGlobalRef(loaded_classes[x]);
                }
                mapper::jvmti->Deallocate((unsigned char*)class_signature);
            }

            if (class_reserved != nullptr)
                mapper::jvmti->Deallocate((unsigned char*)class_reserved);
        }

        if (loaded_classes != nullptr)
            mapper::jvmti->Deallocate((unsigned char*)loaded_classes);
    }

    if (klass.klass == nullptr)
    {
        pandoraLog("[MAPPER] [!] FATAL ERROR: Class not found -> %s\n", name.c_str());
        sdk::jni->ExceptionClear();
        return klass;
    }
    else
    {
        pandoraLog("[MAPPER] [+] Clase mapeada con exito -> %s\n", name.c_str());
    }

    jclass temporal_class_0 = klass.klass;

    while (temporal_class_0 != nullptr)
    {
        jint fields_count = 0;
        jfieldID* fields = nullptr;

        mapper::jvmti->GetClassFields(temporal_class_0, &fields_count, &fields);

        for (jint x = 0; x < fields_count; ++x)
        {
            mapper::__field field;
            field.identifier = fields[x];

            char* field_name = nullptr;
            char* field_signature = nullptr;
            char* field_reserved = nullptr;

            mapper::jvmti->GetFieldName(temporal_class_0, fields[x], &field_name, &field_signature, &field_reserved);

            if (field_name != nullptr) { field.name = field_name;      mapper::jvmti->Deallocate((unsigned char*)field_name); }
            if (field_signature != nullptr) { field.signature = field_signature; mapper::jvmti->Deallocate((unsigned char*)field_signature); }
            if (field_reserved != nullptr)  mapper::jvmti->Deallocate((unsigned char*)field_reserved);

            klass.fields.push_back(field);
        }

        if (fields != nullptr)
            mapper::jvmti->Deallocate((unsigned char*)fields);

        jint methods_count = 0;
        jmethodID* methods = nullptr;

        mapper::jvmti->GetClassMethods(temporal_class_0, &methods_count, &methods);

        for (jint x = 0; x < methods_count; ++x)
        {
            mapper::__method method;
            method.identifier = methods[x];

            char* method_name = nullptr;
            char* method_signature = nullptr;
            char* method_reserved = nullptr;

            mapper::jvmti->GetMethodName(methods[x], &method_name, &method_signature, &method_reserved);

            if (method_name != nullptr) { method.name = method_name;      mapper::jvmti->Deallocate((unsigned char*)method_name); }
            if (method_signature != nullptr) { method.signature = method_signature; mapper::jvmti->Deallocate((unsigned char*)method_signature); }
            if (method_reserved != nullptr)  mapper::jvmti->Deallocate((unsigned char*)method_reserved);

            klass.methods.push_back(method);
        }

        if (methods != nullptr)
            mapper::jvmti->Deallocate((unsigned char*)methods);

        jclass temporal_class_1 = sdk::jni->GetSuperclass(temporal_class_0);

        if (temporal_class_0 != klass.klass)
            sdk::jni->DeleteLocalRef(temporal_class_0);

        temporal_class_0 = temporal_class_1;
    }

    sdk::jni->ExceptionClear();
    return klass;
}

// ============================================================
// Funcion auxiliar para cargar todas las clases compartidas
// independientemente de la version. Recibe los nombres ofuscados
// como parametro para reutilizar logica entre 1.8 y 1.7.
// ============================================================
static void load_obfuscated_classes(
    const char* mc, const char* gs, const char* kb, const char* tm,
    const char* gui, const char* gui_inv, const char* rm, const char* ri, const char* wc,
    const char* ent, const char* ep, const char* epxp, const char* aabb,
    const char* mop, const char* is, const char* it, const char* v3,
    const char* pcmp, const char* invp, const char* ipot,
    const char* elb, const char* blk, const char* ibs, const char* bpos,
    const char* isw, const char* ibow, const char* iblk,
    const char* pot, const char* pe,
    const char* ari, const char* er, const char* fr, const char* sr,
    const char* nhpc, const char* nm, const char* pkt,
    const char* c16, const char* c16e, const char* c0d, const char* c08,
    const char* gui_chest, const char* gui_container,
    const char* mop_type)
{
    mapper::classes["Minecraft"] = mapper::get_class(mc);
    mapper::classes["GameSettings"] = mapper::get_class(gs);
    mapper::classes["KeyBinding"] = mapper::get_class(kb);
    mapper::classes["Timer"] = mapper::get_class(tm);
    mapper::classes["GuiScreen"] = mapper::get_class(gui);
    mapper::classes["GuiInventory"] = mapper::get_class(gui_inv);
    mapper::classes["RenderManager"] = mapper::get_class(rm);
    // Minecraft 1.7.10 does not expose the 1.8 RenderItem class/API.
    if (ri) mapper::classes["RenderItem"] = mapper::get_class(ri);
    mapper::classes["WorldClient"] = mapper::get_class(wc);
    mapper::classes["Entity"] = mapper::get_class(ent);
    mapper::classes["EntityPlayer"] = mapper::get_class(ep);
    mapper::classes["EntityPlayerXP"] = mapper::get_class(epxp);
    mapper::classes["AxisAlignedBB"] = mapper::get_class(aabb);
    mapper::classes["MovingObjectPosition"] = mapper::get_class(mop);
    mapper::classes["ItemStack"] = mapper::get_class(is);
    mapper::classes["Item"] = mapper::get_class(it);
    mapper::classes["Vec3"] = mapper::get_class(v3);
    mapper::classes["PlayerControllerMP"] = mapper::get_class(pcmp);
    mapper::classes["InventoryPlayer"] = mapper::get_class(invp);
    mapper::classes["ItemPotion"] = mapper::get_class(ipot);
    mapper::classes["EntityLivingBase"] = mapper::get_class(elb);
    mapper::classes["Block"] = mapper::get_class(blk);
    if (ibs) mapper::classes["IBlockState"] = mapper::get_class(ibs);
    if (bpos) mapper::classes["BlockPos"] = mapper::get_class(bpos);
    mapper::classes["ItemSword"] = mapper::get_class(isw);
    mapper::classes["ItemBow"] = mapper::get_class(ibow);
    mapper::classes["ItemBlock"] = mapper::get_class(iblk);
    mapper::classes["Potion"] = mapper::get_class(pot);
    mapper::classes["PotionEffect"] = mapper::get_class(pe);
    mapper::classes["ActiveRenderInfo"] = mapper::get_class(ari);
    mapper::classes["EntityRenderer"] = mapper::get_class(er);
    mapper::classes["FontRenderer"] = mapper::get_class(fr);
    mapper::classes["ScaledResolution"] = mapper::get_class(sr);
    mapper::classes["NetHandlerPlayClient"] = mapper::get_class(nhpc);
    mapper::classes["NetworkManager"] = mapper::get_class(nm);
    mapper::classes["Packet"] = mapper::get_class(pkt);
    mapper::classes["C16PacketClientStatus"] = mapper::get_class(c16);
    mapper::classes["C16PacketClientStatus$EnumState"] = mapper::get_class(c16e);
    mapper::classes["C0DPacketCloseWindow"] = mapper::get_class(c0d);
    mapper::classes["C08PacketPlayerBlockPlacement"] = mapper::get_class(c08);
    mapper::classes["GuiChest"] = mapper::get_class(gui_chest);
    mapper::classes["GuiContainer"] = mapper::get_class(gui_container);
    mapper::classes["MovingObjectPosition_MovingObjectType"] = mapper::get_class(mop_type);
}

__int32 mapper::initialize()
{
    pandoraLog("[MAPPER] Iniciando motor de busqueda de clases (JVMTI)...\n");

    if (sdk::jvm->GetEnv((void**)&mapper::jvmti, JVMTI_VERSION_1_2) != JNI_OK) {
        pandoraLog("[MAPPER] [!] ERROR: Fallo al conectar con JVMTI.\n");
        return -1;
    }

    pandoraLog("[MAPPER] JVMTI conectado. Mapeando utilidades base...\n");
    mapper::classes["List"] = mapper::get_class("java/util/List");
    mapper::classes["Enum"] = mapper::get_class("java/lang/Enum");

    // ============================================================
    // FASE 1: Intentar nombres largos (Forge 1.7/1.8)
    // ============================================================
    pandoraLog("[MAPPER] Intentando mapeo de nombres largos (Forge)...\n");
    mapper::classes["Minecraft"] = mapper::get_class("net/minecraft/client/Minecraft");

    if (mapper::classes["Minecraft"].klass != nullptr)
    {
        pandoraLog("[MAPPER] Minecraft Forge detectado. Cargando mappings largos...\n");
        mapper::classes["GameSettings"] = mapper::get_class("net/minecraft/client/settings/GameSettings");
        mapper::classes["KeyBinding"] = mapper::get_class("net/minecraft/client/settings/KeyBinding");
        mapper::classes["Timer"] = mapper::get_class("net/minecraft/util/Timer");
        mapper::classes["GuiScreen"] = mapper::get_class("net/minecraft/client/gui/GuiScreen");
        mapper::classes["GuiInventory"] = mapper::get_class("net/minecraft/client/gui/inventory/GuiInventory");
        mapper::classes["RenderManager"] = mapper::get_class("net/minecraft/client/renderer/entity/RenderManager");
        mapper::classes["RenderItem"] = mapper::get_class("net/minecraft/client/renderer/entity/RenderItem");
        mapper::classes["WorldClient"] = mapper::get_class("net/minecraft/client/multiplayer/WorldClient");
        mapper::classes["Entity"] = mapper::get_class("net/minecraft/entity/Entity");
        mapper::classes["EntityPlayer"] = mapper::get_class("net/minecraft/entity/player/EntityPlayer");
        mapper::classes["EntityPlayerXP"] = mapper::get_class("net/minecraft/client/entity/EntityPlayerSP");
        // 1.7.10 usa EntityClientPlayerMP en vez de EntityPlayerSP
        if (mapper::classes["EntityPlayerXP"].klass == nullptr)
            mapper::classes["EntityPlayerXP"] = mapper::get_class("net/minecraft/client/entity/EntityClientPlayerMP");
        mapper::classes["AxisAlignedBB"] = mapper::get_class("net/minecraft/util/AxisAlignedBB");
        mapper::classes["MovingObjectPosition"] = mapper::get_class("net/minecraft/util/MovingObjectPosition");
        mapper::classes["ItemStack"] = mapper::get_class("net/minecraft/item/ItemStack");
        mapper::classes["Item"] = mapper::get_class("net/minecraft/item/Item");
        mapper::classes["Vec3"] = mapper::get_class("net/minecraft/util/Vec3");
        mapper::classes["PlayerControllerMP"] = mapper::get_class("net/minecraft/client/multiplayer/PlayerControllerMP");
        mapper::classes["InventoryPlayer"] = mapper::get_class("net/minecraft/entity/player/InventoryPlayer");
        mapper::classes["ItemPotion"] = mapper::get_class("net/minecraft/item/ItemPotion");

        // Clases de entidad extra (para KillAura, futuros mods)
        mapper::classes["EntityLivingBase"] = mapper::get_class("net/minecraft/entity/EntityLivingBase");

        // Bloques y posiciones
        mapper::classes["Block"] = mapper::get_class("net/minecraft/block/Block");
        mapper::classes["IBlockState"] = mapper::get_class("net/minecraft/block/state/IBlockState");
        mapper::classes["BlockPos"] = mapper::get_class("net/minecraft/util/BlockPos");

        // Items especificos (para modulos que detectan tipo de arma)
        mapper::classes["ItemSword"] = mapper::get_class("net/minecraft/item/ItemSword");
        mapper::classes["ItemBow"] = mapper::get_class("net/minecraft/item/ItemBow");
        mapper::classes["ItemBlock"] = mapper::get_class("net/minecraft/item/ItemBlock");

        // Pociones y efectos (para AutoPotion, AntiPotion)
        mapper::classes["Potion"] = mapper::get_class("net/minecraft/potion/Potion");
        mapper::classes["PotionEffect"] = mapper::get_class("net/minecraft/potion/PotionEffect");

        // Render (para UI, crosshair, fullbright, etc.)
        mapper::classes["ActiveRenderInfo"] = mapper::get_class("net/minecraft/client/renderer/ActiveRenderInfo");
        mapper::classes["EntityRenderer"] = mapper::get_class("net/minecraft/client/renderer/EntityRenderer");
        mapper::classes["RendererLivingEntity"] = mapper::get_class("net/minecraft/client/renderer/entity/RendererLivingEntity");
        mapper::classes["FontRenderer"] = mapper::get_class("net/minecraft/client/gui/FontRenderer");
        mapper::classes["ScaledResolution"] = mapper::get_class("net/minecraft/client/gui/ScaledResolution");

        // Red (para Velocity, AntiKB, Disabler, etc.)
        mapper::classes["NetHandlerPlayClient"] = mapper::get_class("net/minecraft/client/network/NetHandlerPlayClient");
        mapper::classes["NetworkManager"] = mapper::get_class("net/minecraft/network/NetworkManager");
        mapper::classes["Packet"] = mapper::get_class("net/minecraft/network/Packet");
        mapper::classes["C16PacketClientStatus"] = mapper::get_class("net/minecraft/network/play/client/C16PacketClientStatus");
        mapper::classes["C16PacketClientStatus$EnumState"] = mapper::get_class("net/minecraft/network/play/client/C16PacketClientStatus$EnumState");
        mapper::classes["C0DPacketCloseWindow"] = mapper::get_class("net/minecraft/network/play/client/C0DPacketCloseWindow");
        mapper::classes["C08PacketPlayerBlockPlacement"] = mapper::get_class("net/minecraft/network/play/client/C08PacketPlayerBlockPlacement");

        // Clases que faltaban (usadas en gui_screen.cpp y moving_object_position.cpp)
        mapper::classes["GuiChest"] = mapper::get_class("net/minecraft/client/gui/inventory/GuiChest");
        mapper::classes["GuiContainer"] = mapper::get_class("net/minecraft/client/gui/inventory/GuiContainer");
        mapper::classes["MovingObjectPosition_MovingObjectType"] = mapper::get_class("net/minecraft/util/MovingObjectPosition$MovingObjectType");

        // ============================================================
        // DETECCION DE VERSION: 1.8 tiene BlockPos, 1.7 no
        // ============================================================
        if (mapper::classes.count("BlockPos") && mapper::classes["BlockPos"].klass != nullptr) {
            mapper::version = MINECRAFT_18;
            pandoraLog("[MAPPER] Forge/Lunar 1.8 detectado (BlockPos existe).\n");
        } else {
            mapper::version = MINECRAFT_17;
            pandoraLog("[MAPPER] Forge/Lunar 1.7 detectado (BlockPos NO existe).\n");
        }
    }
    else
    {
        // ============================================================
        // FASE 2: Nombres cortos ofuscados (LUNAR/VANILLA 1.8.9)
        // ============================================================
        pandoraLog("[MAPPER] Forge no detectado. Intentando Lunar/Vanilla 1.8.9...\n");
        mapper::classes["Minecraft"] = mapper::get_class("ave");

        if (mapper::classes["Minecraft"].klass != nullptr)
        {
            pandoraLog("[MAPPER] Lunar Client 1.8.9 detectado. Cargando mappings 1.8...\n");
            mapper::version = MINECRAFT_18;

            load_obfuscated_classes(
                "ave",  // Minecraft
                "avh",  // GameSettings
                "avb",  // KeyBinding
                "avl",  // Timer
                "axu",  // GuiScreen
                "azc",  // GuiInventory
                "biu",  // RenderManager
                "bjh",  // RenderItem
                "bdb",  // WorldClient
                "pk",   // Entity
                "wn",   // EntityPlayer
                "bew",  // EntityPlayerSP
                "aug",  // AxisAlignedBB
                "auh",  // MovingObjectPosition
                "zx",   // ItemStack
                "zw",   // Item
                "aui",  // Vec3
                "bda",  // PlayerControllerMP
                "wm",   // InventoryPlayer
                "aam",  // ItemPotion
                "pr",   // EntityLivingBase
                "atr",  // Block
                "atl",  // IBlockState  (existe en 1.8)
                "cj",   // BlockPos     (existe en 1.8)
                "aao",  // ItemSword
                "aah",  // ItemBow
                "aag",  // ItemBlock
                "pe",   // Potion
                "pf",   // PotionEffect
                "bje",  // ActiveRenderInfo
                "bfk",  // EntityRenderer
                "avn",  // FontRenderer
                "avs",  // ScaledResolution
                "bcy",  // NetHandlerPlayClient
                "ej",   // NetworkManager
                "ff",   // Packet
                "ir",   // C16PacketClientStatus
                "ir$a", // C16PacketClientStatus$EnumState
                "ig",   // C0DPacketCloseWindow
                "jo",   // C08PacketPlayerBlockPlacement
                "azb",  // GuiChest
                "ayl",  // GuiContainer
                "auh$a" // MovingObjectPosition$MovingObjectType
            );
            // Post-load for classes not in the big list
            mapper::classes["RenderItem"] = mapper::get_class("bjh");
        }
        else
        {
            // ============================================================
            // FASE 3: Nombres cortos ofuscados (LUNAR/VANILLA 1.7.10)
            // ============================================================
            pandoraLog("[MAPPER] 1.8 no detectado. Intentando Lunar/Vanilla 1.7.10...\n");
            mapper::version = MINECRAFT_17;

            load_obfuscated_classes(
                "bao",  // Minecraft
                "bam",  // GameSettings
                "bag",  // KeyBinding
                "bae",  // Timer
                "axd",  // GuiScreen
                "ayr",  // GuiInventory
                "bib",  // RenderManager
                nullptr,// RenderItem (API de 1.8; no existe como tal en 1.7)
                "bjf",  // WorldClient
                "sa",   // Entity
                "yz",   // EntityPlayer
                "beb",  // EntityPlayerSP
                "aoe",  // AxisAlignedBB
                "aof",  // MovingObjectPosition
                "add",  // ItemStack
                "adb",  // Item
                "aog",  // Vec3
                "bje",  // PlayerControllerMP
                "yy",   // InventoryPlayer
                "acl",  // ItemPotion
                "sv",   // EntityLivingBase
                "aji",  // Block
                nullptr,// IBlockState  (NO EXISTE en 1.7)
                nullptr,// BlockPos     (NO EXISTE en 1.7)
                "acn",  // ItemSword
                "acg",  // ItemBow
                "acf",  // ItemBlock
                "rv",   // Potion
                "rw",   // PotionEffect
                "bhz",  // ActiveRenderInfo
                "bll",  // EntityRenderer
                "bav",  // FontRenderer
                "baz",  // ScaledResolution
                "bjb",  // NetHandlerPlayClient
                "ef",   // NetworkManager
                "ft",   // Packet
                "jb",   // C16PacketClientStatus
                "jb$a", // C16PacketClientStatus$EnumState
                "is",   // C0DPacketCloseWindow
                "jk",   // C08PacketPlayerBlockPlacement
                "ayq",  // GuiChest
                "aya",  // GuiContainer
                "aof$a" // MovingObjectPosition$MovingObjectType
            );

            pandoraLog("[MAPPER] Version 1.7.10 configured successfully.\n");
        }
        
        // Add RendererLivingEntity for Lunar/Vanilla
        if (mapper::version == MINECRAFT_18) {
            mapper::classes["RendererLivingEntity"] = mapper::get_class("bjl");
            mapper::classes["RenderPlayer"] = mapper::get_class("bop");
        } else if (mapper::version == MINECRAFT_17) {
            mapper::classes["RendererLivingEntity"] = mapper::get_class("boh"); // We guess boh or similar, but it will safely fail if not found.
            mapper::classes["RenderPlayer"] = mapper::get_class("bop"); // Need to find correct for 1.7.10 if needed, usually bop
        }
    }

    // Verificar cuantas clases se encontraron.
    {
        int found = 0, total = 0;
        for (auto& kv : mapper::classes) {
            total++;
            if (kv.second.klass != nullptr) found++;
            else pandoraLog("[MAPPER] [!] Clase NO encontrada: %s\n", kv.first.c_str());
        }
        pandoraLog("[MAPPER] Clases encontradas: %d / %d\n", found, total);

    }

    pandoraLog("[MAPPER] Proceso de inicializacion finalizado.\n");
    sdk::jni->ExceptionClear();
    return 0;
}

__int32 mapper::uninitialize()
{
    for (auto& klass : mapper::classes) {
        if (klass.second.klass != nullptr) {
            sdk::jni->DeleteGlobalRef(klass.second.klass);
        }
    }
    sdk::jni->ExceptionClear();
    return 0;
}
