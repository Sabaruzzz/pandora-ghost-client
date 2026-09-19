extern void TriggerNotification(const char* title, const char* body, const char* tag);

namespace pandora_w_port {
static std::string extra_config_name;
static int extra_selected_config = 0;
static framework::key_var_t extra_menu_key{};
static std::shared_ptr<framework::c_listbox> extra_config_list;

static void add_friends_panel(const std::shared_ptr<framework::c_window>& target) {
    target->build_child("Friends", framework::child_width::half, 500.f,
        [](framework::c_child* child) {
            child->attach_child("Misc", "", CAT_MISC);
            child->add_checkbox("Enabled", &gui_friends_enabled);

            raw_keys.push_back({ &gui_friends_add_bind, { gui_friends_add_bind, framework::key_mode_t::toggle } });
            auto add_bind = child->add_keybind("Add friend key", &raw_keys.back().value);
            add_bind->set_simple();

            raw_keys.push_back({ &gui_friends_nearby_bind, { gui_friends_nearby_bind, framework::key_mode_t::toggle } });
            auto nearby_bind = child->add_keybind("Add nearby friends key", &raw_keys.back().value);
            nearby_bind->set_simple();

            raw_keys.push_back({ &gui_friends_clear_bind, { gui_friends_clear_bind, framework::key_mode_t::toggle } });
            auto clear_bind = child->add_keybind("Clear friends key", &raw_keys.back().value);
            clear_bind->set_simple();

            friends_listbox = child->add_listbox("Friends list", &selected_friend,
                g_FriendsList.empty() ? std::vector<std::string>{ "Empty" } : g_FriendsList, 240.f);
            child->add_button("Remove selected friend", [] {
                if (!g_FriendsList.empty())
                    features::friends::remove_at((size_t)std::clamp(selected_friend, 0, (int)g_FriendsList.size() - 1));
            });
            child->add_button("Clear all friends", [] { features::friends::clear(); });
        });
}

static void add_extras(const std::shared_ptr<framework::c_window>& target) {
    target->build_child("Configs", framework::child_width::half, 525.f,
        [](framework::c_child* child) {
            child->attach_child("Settings", "", CAT_SETTINGS);
            extra_config_list = child->add_listbox("Saved configs", &extra_selected_config,
                g_ConfigList.empty() ? std::vector<std::string>{ "Empty" } : g_ConfigList, 170.f);
            extra_config_list->set_bottom_spacing(14.f);
            child->add_input_box("Config name", &extra_config_name);
            auto create_btn = child->add_button("Create config", [] {
                if (!extra_config_name.empty()) {
                    SaveConfig(extra_config_name); 
                    TriggerNotification("Config created!", extra_config_name.c_str(), "CONFIG_CREATE");
                    RefreshConfigs(); 
                    extra_config_name.clear();
                }
            });
            create_btn->set_callback_visibility([]() { return !extra_config_name.empty(); });
            
            child->add_button("Save config", [] {
                if (!g_ConfigList.empty()) {
                    std::string cfg = g_ConfigList[std::clamp(extra_selected_config, 0, (int)g_ConfigList.size() - 1)];
                    SaveConfig(cfg);
                    TriggerNotification("Config saved!", cfg.c_str(), "CONFIG_SAVE");
                }
            });
            child->add_button("Load config", [] {
                if (!g_ConfigList.empty()) {
                    std::string cfg = g_ConfigList[std::clamp(extra_selected_config, 0, (int)g_ConfigList.size() - 1)];
                    LoadConfig(cfg);
                    TriggerNotification("Config loaded!", cfg.c_str(), "CONFIG_LOAD");
                }
            });
            child->add_button("Delete config", [] {
                if (!g_ConfigList.empty()) {
                    std::string cfg = g_ConfigList[std::clamp(extra_selected_config, 0, (int)g_ConfigList.size() - 1)];
                    DeleteConfig(cfg);
                    TriggerNotification("Config deleted!", cfg.c_str(), "CONFIG_DELETE");
                    RefreshConfigs(); extra_selected_config = 0;
                }
            });
            child->add_button("Open configs folder", [] { OpenConfigFolder(); });
        });

    target->build_child("UI", framework::child_width::half, 525.f,
        [](framework::c_child* child) {
            child->attach_child("Settings", "", CAT_SETTINGS);
            extra_menu_key = { g_MenuKey, framework::key_mode_t::toggle };
            auto menu_bind = child->add_keybind("Menu key", &extra_menu_key); menu_bind->set_simple();

            child->add_dropdown("GUI Scale", &g_GuiScaleIndex,
                { "Default", "110%", "120%", "130%" });

            colors.push_back({ gui_guicolor_custom, false,
                hue::c_color((int)(gui_guicolor_custom[0] * 255.f),
                             (int)(gui_guicolor_custom[1] * 255.f),
                             (int)(gui_guicolor_custom[2] * 255.f), 255) });
            child->add_colorpicker("Accent color", &colors.back().value);

            child->add_checkbox("Notifications", &features::misc::notifications::enabled);
            auto notification_position = child->add_dropdown("Notification position", &features::misc::notifications::position,
                { "Top left", "Top right", "Bottom left", "Bottom right" });
            notification_position->set_callback_visibility([] {
                return features::misc::notifications::enabled;
            });

            colors.push_back({ features::misc::notifications::bar_color, true,
                hue::c_color((int)(features::misc::notifications::bar_color[0] * 255.f),
                             (int)(features::misc::notifications::bar_color[1] * 255.f),
                             (int)(features::misc::notifications::bar_color[2] * 255.f),
                             (int)(features::misc::notifications::bar_color[3] * 255.f)) });
            auto bar_color = child->add_colorpicker("Bar Color", &colors.back().value);
            bar_color->set_callback_visibility([] {
                return features::misc::notifications::enabled;
            });

            child->add_checkbox("Background dim", &g_BackgroundDim);
            auto unload = child->add_button("Hold to unload client", [] {
                if (!g_DestructStarted.exchange(true)) g_ShouldDestruct = true;
            });
            unload->set_hold_to_activate(1.5f);
        });

}

static void sync_extras(bool before_input) {
    if (before_input) {
        PollConfigFolderChanges();
        extra_menu_key.key = g_MenuKey;
        if (extra_config_list) extra_config_list->set_items(g_ConfigList.empty() ? std::vector<std::string>{ "Empty" } : g_ConfigList);
    }
    else g_MenuKey = extra_menu_key.key;
    static const float scales[] = { 1.f, 1.1f, 1.2f, 1.3f };
    if (!::g_MenuVisible) {
        g_GuiScaleTarget = scales[std::clamp(g_GuiScaleIndex, 0, 3)];
    }
}
} // namespace pandora_w_port
