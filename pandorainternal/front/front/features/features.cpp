#include "features.hpp"
#include <windows.h>

namespace features
{
    void run_on_run_tick(mapper::__minecraft& minecraft)
    {
        if (!minecraft.is_valid()) return;

        //combat
        combat::auto_click::run(minecraft);
        combat::aim_assist::run(minecraft);
        combat::reach::run(minecraft);
        combat::velocity::run(minecraft);
        combat::block_hit::run(minecraft);
        combat::no_hit_delay::run(minecraft);
        combat::refill::run(minecraft);
        combat::macros::run(minecraft);
        combat::armor_switcher::run(minecraft);

        //movement
        movement::sprint::run(minecraft);
        movement::no_slow::run(minecraft);
        movement::no_item_release::run(minecraft);
        movement::no_jump_delay::run(minecraft);


        // ============================================================
        // MISC
        // ============================================================
        misc::fastplace::run(minecraft);
        misc::auto_armor::run(minecraft);
        misc::blink::run(minecraft);
        friends::run(minecraft);
    }

    void run_on_render_world(mapper::__minecraft& minecraft, unsigned __int64* last_render_time)
    {
        if (!minecraft.is_valid()) return;

        // Visuales - llamados desde LogicThread (JNI adjuntado)
        visual::player_esp_2d::run(minecraft);
        visual::player_esp_3d::run(minecraft);
        visual::nametags::run(minecraft);
        visual::tracers::run(minecraft);
        misc::blink::render_world(minecraft);
    }
}

