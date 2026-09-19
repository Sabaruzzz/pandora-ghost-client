#pragma once
#include <jni.h>
#include "sdk.hpp"
#include <vector>
#include <map>
#include <string>

namespace mapper
{
    // Punteros del entorno de Java 
    extern JavaVM* jvm;
    extern JNIEnv* jni;

    enum supported_versions
    {
        MINECRAFT_17,
        MINECRAFT_18,
    };

    // Valor inicial únicamente; initialize() lo cambia después de detectar
    // automáticamente si la instancia cargada es 1.7.10 o 1.8.9.
    inline unsigned __int32 version = MINECRAFT_18;
}

namespace mapper
{
    struct __field
    {
        std::string name;
        std::string signature;
        jfieldID identifier = nullptr;
    };

    struct __method
    {
        std::string name;
        std::string signature;
        jmethodID identifier = nullptr;
    };

    struct __class
    {
        std::string name;
        std::string signature;
        jclass klass = nullptr;
        std::vector<mapper::__field> fields;
        std::vector<mapper::__method> methods;

        mapper::__field get_field(std::string name, std::string signature);
        mapper::__method get_method(std::string name, std::string signature);
    };

    inline std::map<std::string, mapper::__class> classes;

    extern mapper::__class get_class(std::string name);
}

namespace mapper
{
    extern __int32 initialize();
    extern __int32 uninitialize();

    struct __player; // Declaracion adelantada

    // --- ESTRUCTURAS MATEMATICAS ---
    struct __vec4
    {
        double x = 0., y = 0., z = 0., w = 0.f;
    };

    struct __vec3
    {
        double x = 0., y = 0., z = 0.;

        double get_distance_to_vec3(mapper::__vec3 vec3);
        float get_angle_x_difference_to_vec3(mapper::__vec3 target, float angle);
        float get_angle_y_difference_to_vec3(mapper::__vec3 target, float angle);
    };

    struct __vec3__
    {
        __vec3__(mapper::__vec3 vec3);
        __vec3__(jobject object);
        __vec3__(const mapper::__vec3__& vec3);
        ~__vec3__();

        jobject object = nullptr;

        mapper::__vec3 get_vec3();
    };

    struct __vec2
    {
        float x = 0.f, y = 0.f;
    };

    // --- CLASES DEL JUEGO (WRAPPERS) ---
    struct __item
    {
        __item(jobject object);
        __item(const mapper::__item& item);
        ~__item();

        jobject object = nullptr;

        __int32 get_id();
        bool is_sword();
        bool is_axe();
        bool is_pickaxe();
        bool is_shovel();
        bool is_block();
        bool is_potion();
        int get_item_damage();
        int get_max_damage();
        bool is_soup();
        bool is_ender_pearl();
        bool is_food();
    };

    struct __item_stack
    {
        __item_stack(jobject object);
        __item_stack(const mapper::__item_stack& item_stack);
        ~__item_stack();

        jobject object = nullptr;

        mapper::__item get_item();
        int get_stack_size();
        bool is_potion();
        int get_item_damage();
        int get_max_damage();
    };

    struct __moving_object_position
    {
        __moving_object_position(jobject object);
        __moving_object_position(mapper::__player player, mapper::__vec3__ vec3);
        __moving_object_position(const mapper::__moving_object_position& moving_object_position);
        ~__moving_object_position();

        jobject object = nullptr;

        __int32 get_type_of_hit();
        mapper::__vec3__ get_hit_vector();
    };

    struct __axis_aligned
    {
        __axis_aligned(jobject object);
        __axis_aligned(const mapper::__axis_aligned& axis_aligned);
        ~__axis_aligned();

        jobject object = nullptr;

        std::pair<mapper::__vec3, mapper::__vec3> get_bounds();
        void set_bounds(std::pair<mapper::__vec3, mapper::__vec3> bounds);
        mapper::__moving_object_position calculate_interception(mapper::__vec3 view_position, mapper::__vec3 view_vector);
    };

    struct __player
    {
        // Nametags / visuals
        bool    is_invisible();
        float   get_max_health();

        bool    is_using_item();

        __player(jobject object);
        __player(const mapper::__player& player);
        ~__player();

        jobject object = nullptr;

        mapper::__vec3 get_position();
        void set_position(mapper::__vec3 position);
        mapper::__vec3 get_old_position();
        void set_old_position(mapper::__vec3 position);
        mapper::__vec3 get_motion();
        void set_motion(mapper::__vec3 motion);
        bool get_on_ground();
        void jump();
        bool is_swing_in_progress();
        __int32 get_hurt_time();
        void swing_item();
        void set_hurt_time(__int32 hurt_time);
        bool is_vulnerable();
        mapper::__vec3 get_view_position(float partial_ticks);
        mapper::__vec3 get_look_position(float partial_ticks);
        mapper::__vec2 get_view_angles();
        void set_view_angles(mapper::__vec2 view_angles);
        mapper::__vec2 get_old_view_angles();
        void set_old_view_angles(mapper::__vec2 old_view_angles);
        __int32 get_ticks_existed();
        float get_health();
        float get_move_foreward();
        float get_move_strafing();
        void set_move_foreward(float value);
        void set_move_strafing(float value);
        __int32 get_total_armor_value();
        bool can_entity_be_seen(mapper::__player player);
        bool get_flag(__int32 flag);
        void set_flag(__int32 flag, bool state);
        void set_sprinting(bool state);
        void set_always_render_nametag(bool state);
        void set_item_in_use_count(int count);
        bool is_offset_position_in_liquid(double x, double y, double z);
        mapper::__item_stack get_held_item_stack();
        std::string get_name();
        std::string get_uuid();
        mapper::__axis_aligned get_bounding_box();
        mapper::__item_stack get_inventory_slot(int slot_id);
        int get_current_slot();
        void set_current_slot(int slot);
        void send_packet(jobject packet);
        __int32 get_entity_id();
        void set_name(const std::string& name);
        void hide_name_completely(bool only_string = false); // Nuevo metodo para ocultar tag de 0
        void restore_name_completely(const std::string& original_name, bool only_string = false);
    };

    struct __world
    {
        __world(jobject object);
        __world(const mapper::__world& world);
        ~__world();

        jobject object = nullptr;

        std::vector<mapper::__player> get_players();
        std::vector<mapper::__player> get_loaded_entities();
        std::vector<mapper::__vec3> get_loaded_tile_entities();
        __int32 get_block_id(double x, double y, double z);
    };

    struct __render_item
    {
        __render_item(jobject object);
        __render_item(const mapper::__render_item& render_item);
        ~__render_item();

        jobject object = nullptr;

        void render_item_into_gui(mapper::__item_stack item_stack, int x, int y);
    };

    struct __render_manager
    {
        __render_manager(jobject object);
        __render_manager(const mapper::__render_manager& render_manager);
        ~__render_manager();

        jobject object = nullptr;

        bool render_player(mapper::__player player, float partial_ticks);
        bool render_player(mapper::__player player, double x, double y, double z, float yaw, float partial_ticks);
        double get_render_pos_x();
        double get_render_pos_y();
        double get_render_pos_z();
    };

    struct __gui_screen
    {
        __gui_screen(jobject object);
        __gui_screen(const mapper::__gui_screen& gui_screen);
        ~__gui_screen();

        jobject object = nullptr;

        bool is_inventory_instance();
    };

    struct __timer
    {
        __timer(jobject object);
        __timer(const mapper::__timer& timer);
        ~__timer();

        jobject object = nullptr;

        float get_partial_ticks();
    };

    struct __settings
    {
        __settings(jobject object);
        __settings(const mapper::__settings& settings);
        ~__settings();

        jobject object = nullptr;

        float get_mouse_sensitivity();

        void set_virtual_sneak(bool state);
        void set_virtual_right_click(bool enable);
    };

    struct __inventory_player
    {
        __inventory_player(jobject object);
        __inventory_player(const mapper::__inventory_player& inv);
        ~__inventory_player();

        jobject object = nullptr;

        int get_slot();
        void set_slot(int slot);
        mapper::__item_stack get_stack_in_slot(int slot);
    };

    // ============================================================
    // __active_render_info
    // Adaptado del Dope Client v2: lee las matrices ModelView y
    // Projection desde los FloatBuffers estaticos de Java.
    // Permite que world_to_screen funcione sin depender del hook
    // de glClear, que fue eliminado de hooks.cpp.
    // ============================================================
    struct __active_render_info
    {
        static std::vector<float> get_model_view();
        static std::vector<float> get_projection();
    };

    struct __minecraft
    {
        __minecraft();
        __minecraft(const mapper::__minecraft& minecraft);
        ~__minecraft();

        jobject object = nullptr;

        static bool is_on_run_tick();
        static bool is_on_render_world();
        static bool is_on_orient_camera();

        mapper::__settings get_settings();
        mapper::__timer get_timer();
        mapper::__gui_screen get_current_screen();
        mapper::__render_manager get_render_manager();
        mapper::__render_item get_render_item();
        mapper::__world get_world();
        mapper::__player get_local_player();
        mapper::__moving_object_position get_object_mouse_over();
        void set_object_mouse_over(mapper::__moving_object_position moving_object_position);
        mapper::__player get_pointed_entity();
        void set_pointed_entity(mapper::__player player);
        std::string get_server_ip();
        __int32 get_left_click_delay_timer();
        void set_left_click_delay_timer(__int32 right_click_delay_timer);
        __int32 get_right_click_delay_timer();
        void set_right_click_delay_timer(__int32 right_click_delay_timer);
        void right_click_mouse();
        void click_mouse();
        void window_click(int window_id, int slot_id, int mouse_button, int mode, mapper::__player player);

        bool is_valid();
    };
}
