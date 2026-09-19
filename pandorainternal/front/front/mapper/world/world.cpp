#include "mapper.hpp"

mapper::__world::__world(jobject object)
{
    this->object = object;
}

mapper::__world::__world(const mapper::__world& world)
{
    if (world.object != nullptr)
        this->object = sdk::jni->NewLocalRef(world.object);
}

mapper::__world::~__world()
{
    if (this->object != nullptr)
        sdk::jni->DeleteLocalRef(this->object);
}

std::vector<mapper::__player> mapper::__world::get_players()
{
    std::vector<mapper::__player> players;

    // Obtener la lista de jugadores (playerEntities)
    mapper::__field field = mapper::classes["WorldClient"].get_field("playerEntities", mapper::classes["List"].signature);
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("field_73010_i", mapper::classes["List"].signature);
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("j", mapper::classes["List"].signature); // 1.8.9 Lunar
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("i", mapper::classes["List"].signature); // 1.7 Lunar
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("h", mapper::classes["List"].signature); // 1.7 alt

    if (field.identifier == nullptr) return players;
    jobject object = sdk::jni->GetObjectField(this->object, field.identifier);
    if (object == nullptr) return players;

    // Convertir a Array
    mapper::__method method = mapper::classes["List"].get_method("toArray", "()[Ljava/lang/Object;");
    if (method.identifier == nullptr) { sdk::jni->DeleteLocalRef(object); return players; }
    jobjectArray array_object = (jobjectArray)sdk::jni->CallObjectMethod(object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); sdk::jni->DeleteLocalRef(object); return players; }

    if (array_object != nullptr)
    {
        for (__int32 x = 0, array_object_length = sdk::jni->GetArrayLength(array_object); x < array_object_length; ++x)
        {
            jobject player_object = sdk::jni->GetObjectArrayElement(array_object, x);
            if (player_object != nullptr)
                players.emplace_back(player_object);
        }
    }

    if (object != nullptr) sdk::jni->DeleteLocalRef(object);
    if (array_object != nullptr) sdk::jni->DeleteLocalRef(array_object);

    return players;
}

std::vector<mapper::__player> mapper::__world::get_loaded_entities()
{
    std::vector<mapper::__player> entities;

    mapper::__field field = mapper::classes["WorldClient"].get_field("loadedEntityList", mapper::classes["List"].signature);
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("field_72996_f", mapper::classes["List"].signature);
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("c", mapper::classes["List"].signature); // 1.8.9 Lunar
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("b", mapper::classes["List"].signature); // 1.7 Lunar
    if (field.identifier == nullptr) field = mapper::classes["WorldClient"].get_field("d", mapper::classes["List"].signature); // 1.7 alt

    if (field.identifier == nullptr) return entities;

    jobject object = sdk::jni->GetObjectField(this->object, field.identifier);
    if (object == nullptr) return entities;

    mapper::__method method = mapper::classes["List"].get_method("toArray", "()[Ljava/lang/Object;");
    if (method.identifier == nullptr) { sdk::jni->DeleteLocalRef(object); return entities; }

    jobjectArray array_object = (jobjectArray)sdk::jni->CallObjectMethod(object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); sdk::jni->DeleteLocalRef(object); return entities; }

    if (array_object != nullptr)
    {
        for (__int32 x = 0, len = sdk::jni->GetArrayLength(array_object); x < len; ++x)
        {
            jobject entity_obj = sdk::jni->GetObjectArrayElement(array_object, x);
            if (entity_obj != nullptr)
                entities.push_back(mapper::__player(entity_obj));
        }
    }

    if (object != nullptr) sdk::jni->DeleteLocalRef(object);
    if (array_object != nullptr) sdk::jni->DeleteLocalRef(array_object);

    return entities;
}

std::vector<mapper::__vec3> mapper::__world::get_loaded_tile_entities()
{
    std::vector<mapper::__vec3> positions;

    if (!mapper::classes["WorldClient"].klass) return positions;

    jclass world_class = sdk::jni->GetSuperclass(mapper::classes["WorldClient"].klass);
    if (!world_class) return positions;

    jfieldID field_id = sdk::jni->GetFieldID(world_class, "loadedTileEntityList", "Ljava/util/List;");
    if (!field_id) { sdk::jni->ExceptionClear(); field_id = sdk::jni->GetFieldID(world_class, "field_147482_g", "Ljava/util/List;"); }
    if (!field_id) { sdk::jni->ExceptionClear(); field_id = sdk::jni->GetFieldID(world_class, "h", "Ljava/util/List;"); } // 1.8.9 Lunar
    if (!field_id) { sdk::jni->ExceptionClear(); field_id = sdk::jni->GetFieldID(world_class, "b", "Ljava/util/List;"); } // Fallback 1.7

    sdk::jni->DeleteLocalRef(world_class);

    if (!field_id) return positions;

    jobject object = sdk::jni->GetObjectField(this->object, field_id);
    if (object == nullptr) return positions;

    mapper::__method method = mapper::classes["List"].get_method("toArray", "()[Ljava/lang/Object;");
    if (method.identifier == nullptr) { sdk::jni->DeleteLocalRef(object); return positions; }

    jobjectArray array_object = (jobjectArray)sdk::jni->CallObjectMethod(object, method.identifier);
    if (sdk::jni->ExceptionCheck()) { sdk::jni->ExceptionClear(); sdk::jni->DeleteLocalRef(object); return positions; }

    if (array_object != nullptr)
    {
        for (__int32 x = 0, len = sdk::jni->GetArrayLength(array_object); x < len; ++x)
        {
            jobject tile_entity_obj = sdk::jni->GetObjectArrayElement(array_object, x);
            if (tile_entity_obj != nullptr)
            {
                if (mapper::version == mapper::MINECRAFT_18)
                {
                    static jmethodID getPos_method = nullptr;
                    if (!getPos_method) {
                        jclass te_class = sdk::jni->GetObjectClass(tile_entity_obj);
                        getPos_method = sdk::jni->GetMethodID(te_class, "getPos", "()Lnet/minecraft/util/BlockPos;");
                        if (!getPos_method) { sdk::jni->ExceptionClear(); getPos_method = sdk::jni->GetMethodID(te_class, "v", "()Lcj;"); }
                        sdk::jni->DeleteLocalRef(te_class);
                    }
                    if (getPos_method) {
                        jobject block_pos = sdk::jni->CallObjectMethod(tile_entity_obj, getPos_method);
                        if (block_pos != nullptr) {
                            static jmethodID getX = nullptr, getY = nullptr, getZ = nullptr;
                            if (!getX) {
                                jclass bp_class = sdk::jni->GetObjectClass(block_pos);
                                getX = sdk::jni->GetMethodID(bp_class, "getX", "()I");
                                if (!getX) { sdk::jni->ExceptionClear(); getX = sdk::jni->GetMethodID(bp_class, "n", "()I"); }
                                getY = sdk::jni->GetMethodID(bp_class, "getY", "()I");
                                if (!getY) { sdk::jni->ExceptionClear(); getY = sdk::jni->GetMethodID(bp_class, "o", "()I"); }
                                getZ = sdk::jni->GetMethodID(bp_class, "getZ", "()I");
                                if (!getZ) { sdk::jni->ExceptionClear(); getZ = sdk::jni->GetMethodID(bp_class, "p", "()I"); }
                                sdk::jni->DeleteLocalRef(bp_class);
                            }
                            if (getX && getY && getZ) {
                                int px = sdk::jni->CallIntMethod(block_pos, getX);
                                int py = sdk::jni->CallIntMethod(block_pos, getY);
                                int pz = sdk::jni->CallIntMethod(block_pos, getZ);
                                positions.push_back({ (double)px, (double)py, (double)pz });
                            }
                            sdk::jni->DeleteLocalRef(block_pos);
                        }
                    }
                }
                else
                {
                    static jfieldID xCoord = nullptr, yCoord = nullptr, zCoord = nullptr;
                    if (!xCoord) {
                        jclass te_class = sdk::jni->GetObjectClass(tile_entity_obj);
                        xCoord = sdk::jni->GetFieldID(te_class, "xCoord", "I");
                        if (!xCoord) { sdk::jni->ExceptionClear(); xCoord = sdk::jni->GetFieldID(te_class, "c", "I"); }
                        yCoord = sdk::jni->GetFieldID(te_class, "yCoord", "I");
                        if (!yCoord) { sdk::jni->ExceptionClear(); yCoord = sdk::jni->GetFieldID(te_class, "d", "I"); }
                        zCoord = sdk::jni->GetFieldID(te_class, "zCoord", "I");
                        if (!zCoord) { sdk::jni->ExceptionClear(); zCoord = sdk::jni->GetFieldID(te_class, "e", "I"); }
                        sdk::jni->DeleteLocalRef(te_class);
                    }
                    if (xCoord && yCoord && zCoord) {
                        int px = sdk::jni->GetIntField(tile_entity_obj, xCoord);
                        int py = sdk::jni->GetIntField(tile_entity_obj, yCoord);
                        int pz = sdk::jni->GetIntField(tile_entity_obj, zCoord);
                        positions.push_back({ (double)px, (double)py, (double)pz });
                    }
                }
                sdk::jni->DeleteLocalRef(tile_entity_obj);
            }
        }
    }

    if (object != nullptr) sdk::jni->DeleteLocalRef(object);
    if (array_object != nullptr) sdk::jni->DeleteLocalRef(array_object);

    return positions;
}

__int32 mapper::__world::get_block_id(double x, double y, double z)
{
    __int32 block_id = 0;

    // ============================================================
    // RUTA 1.7.10: World.getBlock(int, int, int) -> Block
    // En 1.7.10 no existen BlockPos ni IBlockState.
    // ============================================================
    if (mapper::version == mapper::MINECRAFT_17)
    {
        static jmethodID s_method_getBlock = nullptr;
        if (!s_method_getBlock) {
            std::string sig_getBlock = "(III)" + mapper::classes["Block"].signature;
            mapper::__method method_getBlock = mapper::classes["WorldClient"].get_method("getBlock", sig_getBlock);
            if (method_getBlock.identifier == nullptr) method_getBlock = mapper::classes["WorldClient"].get_method("func_147439_a", sig_getBlock);
            if (method_getBlock.identifier == nullptr) method_getBlock = mapper::classes["WorldClient"].get_method("a", sig_getBlock);
            if (method_getBlock.identifier) s_method_getBlock = method_getBlock.identifier;
        }
        if (!s_method_getBlock) return 0;

        jobject block_obj = sdk::jni->CallObjectMethod(this->object, s_method_getBlock, (jint)x, (jint)y, (jint)z);
        if (block_obj == nullptr) return 0;

        static jmethodID s_method_id = nullptr;
        if (!s_method_id) {
            std::string sig_id = "(" + mapper::classes["Block"].signature + ")I";
            mapper::__method method_id = mapper::classes["Block"].get_method("getIdFromBlock", sig_id);
            if (method_id.identifier == nullptr) method_id = mapper::classes["Block"].get_method("func_149682_b", sig_id);
            if (method_id.identifier == nullptr) method_id = mapper::classes["Block"].get_method("a", sig_id);
            if (method_id.identifier) s_method_id = method_id.identifier;
        }

        if (s_method_id != nullptr)
            block_id = sdk::jni->CallStaticIntMethod(mapper::classes["Block"].klass, s_method_id, block_obj);

        sdk::jni->DeleteLocalRef(block_obj);
        return block_id;
    }

    // --- LOGICA 1.8.9 (BlockPos -> IBlockState -> Block) ---

    // --- LÃ“GICA EXCLUSIVA 1.8.9 (BlockPos) ---

    // 1. Crear nuevo BlockPos(x, y, z)
    if (mapper::classes["BlockPos"].klass == nullptr) return 0;
    static jmethodID s_method_0 = nullptr;
    if (!s_method_0) {
        mapper::__method method_0 = mapper::classes["BlockPos"].get_method("<init>", "(DDD)V");
        if (method_0.identifier) s_method_0 = method_0.identifier;
    }
    if (!s_method_0) return 0;
    jobject object_0 = sdk::jni->NewObject(mapper::classes["BlockPos"].klass, s_method_0, x, y, z);

    // 2. Obtener IBlockState a partir del BlockPos
    static jmethodID s_method_1 = nullptr;
    if (!s_method_1) {
        std::string sig_1 = "(" + mapper::classes["BlockPos"].signature + ")" + mapper::classes["IBlockState"].signature;
        mapper::__method method_1 = mapper::classes["WorldClient"].get_method("getBlockState", sig_1);
        if (method_1.identifier == nullptr) method_1 = mapper::classes["WorldClient"].get_method("func_180495_p", sig_1);
        if (method_1.identifier == nullptr) method_1 = mapper::classes["WorldClient"].get_method("p", sig_1); // Lunar
        if (method_1.identifier) s_method_1 = method_1.identifier;
    }
    if (!s_method_1) { sdk::jni->DeleteLocalRef(object_0); return 0; }

    jobject object_1 = sdk::jni->CallObjectMethod(this->object, s_method_1, object_0);
    if (object_1 == nullptr) { sdk::jni->DeleteLocalRef(object_0); return 0; }

    // 3. Extraer el Block del IBlockState
    static jmethodID s_method_2 = nullptr;
    if (!s_method_2) {
        std::string sig_2 = "()" + mapper::classes["Block"].signature;
        mapper::__method method_2 = mapper::classes["IBlockState"].get_method("getBlock", sig_2);
        if (method_2.identifier == nullptr) method_2 = mapper::classes["IBlockState"].get_method("func_177230_c", sig_2);
        if (method_2.identifier == nullptr) method_2 = mapper::classes["IBlockState"].get_method("c", sig_2); // Lunar
        if (method_2.identifier) s_method_2 = method_2.identifier;
    }
    if (!s_method_2) { sdk::jni->DeleteLocalRef(object_0); sdk::jni->DeleteLocalRef(object_1); return 0; }

    jobject object_2 = sdk::jni->CallObjectMethod(object_1, s_method_2);

    // 4. Obtener el ID del bloque (metodo estatico en Block)
    static jmethodID s_method_3 = nullptr;
    if (!s_method_3) {
        std::string sig_3 = "(" + mapper::classes["Block"].signature + ")I";
        mapper::__method method_3 = mapper::classes["Block"].get_method("getIdFromBlock", sig_3);
        if (method_3.identifier == nullptr) method_3 = mapper::classes["Block"].get_method("func_149682_b", sig_3);
        if (method_3.identifier == nullptr) method_3 = mapper::classes["Block"].get_method("a", sig_3); // Lunar
        if (method_3.identifier) s_method_3 = method_3.identifier;
    }
    if (s_method_3 != nullptr)
        block_id = sdk::jni->CallStaticIntMethod(mapper::classes["Block"].klass, s_method_3, object_2);

    // Limpiar memoria
    if (object_0 != nullptr) sdk::jni->DeleteLocalRef(object_0);
    if (object_1 != nullptr) sdk::jni->DeleteLocalRef(object_1);
    if (object_2 != nullptr) sdk::jni->DeleteLocalRef(object_2);

    return block_id;
}
