#include "userconfig.h"

// ============================================================
// IN-MEMORY CONFIG SYSTEM (no folders, no disk writes)
// ============================================================
std::map<std::string, std::string> g_InMemoryConfigs;
// Profile currently selected in the UI. Selection never implies persistence;
// only an explicit SaveConfig call may overwrite its snapshot.
std::string g_ActiveConfig;

// Mapa de keybinds por nombre de mÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â³dulo ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â se puebla en InitMenuData
// Permite a SaveConfig/LoadConfig guardar/cargar binds sin depender del tipo Module
static std::map<std::string, int> g_KeybindMap;

// Nested ImGui children own separate draw lists. Share the animated reveal
// rectangle with them so their contents cannot outlive the closing menu shell.
static ImVec2 g_MenuRevealClipMin(0.0f, 0.0f);
static ImVec2 g_MenuRevealClipMax(0.0f, 0.0f);
static bool g_MenuRevealClipActive = false;

struct Setting {
    enum Type { TOGGLE, SLIDER, COLOR, COLOR4, LABEL, BUTTON, DROPDOWN };
    Type type = TOGGLE;
    std::string name = "";
    bool* boolPtr = nullptr;
    float* floatPtr = nullptr;
    float  min = 0.0f, max = 0.0f;
    const char* format = "";
    float* colorPtr = nullptr;
    int* intPtr = nullptr;
    std::vector<std::string> dropdownItems;
    std::function<bool()> visibleCondition = nullptr;
    std::vector<int> dropdownValues;
};

struct Module {
    std::string name;
    int   category = 0;
    int   keybind = 0;
    bool* enabledPtr = nullptr;
    std::string description = "";
    std::vector<Setting> settings;
    bool hidden = false;
};

std::vector<Module> modules;

Module CreateMod(std::string name, std::string desc, int cat, bool* enabledPtr = nullptr) {
    Module m; m.name = name; m.description = desc; m.category = cat; m.enabledPtr = enabledPtr; return m;
}

static std::string ConfigModuleKey(const std::string& name) {
    std::string key = "module_";
    key.reserve(key.size() + name.size() + 8);
    for (unsigned char ch : name) {
        if (std::isalnum(ch)) key.push_back((char)std::tolower(ch));
        else if (key.back() != '_') key.push_back('_');
    }
    key += "_enabled";
    return key;
}

static std::string ConfigSettingKey(const std::string& moduleName, const std::string& settingName) {
    std::string key = "setting_" + moduleName + "_" + settingName;
    for (char& ch : key) {
        if (!std::isalnum(static_cast<unsigned char>(ch))) ch = '_';
    }
    return key;
}

void RefreshConfigs() {
    g_ConfigList.clear();
    for (auto& kv : g_InMemoryConfigs)
        g_ConfigList.push_back(kv.first);
}

// ============================================================
// CONFIG PERSISTENCE - Registro oculto
// Clave disfrazada como entrada COM de Windows, pasa desapercibida
// ============================================================
#pragma comment(lib, "advapi32.lib")
#define CFG_REG_KEY "Software\\pandoraClient\\Configs"

static std::string ConfigFolderPath() {
    char appData[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableA("APPDATA", appData, MAX_PATH);
    std::string base = (len > 0 && len < MAX_PATH)
        ? std::string(appData) : std::string(".");
    std::string root = base + "\\pandora";
    std::string configs = root + "\\Configs";
    CreateDirectoryA(root.c_str(), nullptr);
    CreateDirectoryA(configs.c_str(), nullptr);
    return configs;
}

static std::string SafeConfigFileName(const std::string& name) {
    std::string safe = name;
    for (char& c : safe) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
            c == '\\' || c == '|' || c == '?' || c == '*') c = '_';
    }
    while (!safe.empty() && (safe.back() == ' ' || safe.back() == '.'))
        safe.pop_back();
    return safe.empty() ? "config" : safe;
}

static std::string ConfigFilePath(const std::string& name) {
    return ConfigFolderPath() + "\\" + SafeConfigFileName(name) + ".pcfg";
}

static void WriteConfigFile(const std::string& name, const std::string& data) {
    std::ofstream file(ConfigFilePath(name), std::ios::binary | std::ios::trunc);
    if (file.is_open()) file.write(data.data(), (std::streamsize)data.size());
}

void OpenConfigFolder() {
    const std::string folder = ConfigFolderPath();
    ShellExecuteA(nullptr, "open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static void Registry_SaveConfig(const std::string& name, const std::string& data) {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, name.c_str(), 0, REG_BINARY,
            (const BYTE*)data.c_str(), (DWORD)data.size());
        RegCloseKey(hKey);
    }
    WriteConfigFile(name, data);
}

static void Registry_DeleteConfig(const std::string& name) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, name.c_str());
        RegCloseKey(hKey);
    }
    DeleteFileA(ConfigFilePath(name).c_str());
}

static void ReloadConfigsFromDisk() {
    // The visible Configs folder is authoritative. The registry remains only as
    // a mirror written by SaveConfig; it must never recreate files deleted by
    // the user.
    const std::string folder = ConfigFolderPath();
    std::map<std::string, std::string> diskConfigs;
    WIN32_FIND_DATAA findData = {};
    HANDLE find = FindFirstFileA((folder + "\\*.pcfg").c_str(), &findData);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
            std::string fileName = findData.cFileName;
            std::string configName = fileName.substr(0, fileName.size() - 5);
            std::ifstream file(folder + "\\" + fileName, std::ios::binary);
            if (!file.is_open()) continue;
            std::string data(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            const std::string typePrefix = "// CLOUD_CONFIG_TYPE=";
            if (data.compare(0, typePrefix.size(), typePrefix) == 0) {
                const size_t newline = data.find('\n');
                if (newline != std::string::npos) data = data.substr(newline + 1);
            }
            if (!data.empty()) diskConfigs[configName] = std::move(data);
        } while (FindNextFileA(find, &findData));
        FindClose(find);
    }
    g_InMemoryConfigs = std::move(diskConfigs);
    RefreshConfigs();
}

static uint64_t ConfigFolderSignature() {
    const std::string pattern = ConfigFolderPath() + "\\*.pcfg";
    WIN32_FIND_DATAA findData = {};
    HANDLE find = FindFirstFileA(pattern.c_str(), &findData);
    if (find == INVALID_HANDLE_VALUE) return 0;

    uint64_t signature = 1469598103934665603ull;
    do {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        for (const unsigned char ch : std::string(findData.cFileName)) {
            signature ^= ch;
            signature *= 1099511628211ull;
        }
        signature ^= (uint64_t(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
        signature *= 1099511628211ull;
        signature ^= (uint64_t(findData.ftLastWriteTime.dwHighDateTime) << 32) |
                     findData.ftLastWriteTime.dwLowDateTime;
        signature *= 1099511628211ull;
    } while (FindNextFileA(find, &findData));
    FindClose(find);
    return signature;
}

void PollConfigFolderChanges() {
    static ULONGLONG nextCheck = 0;
    static uint64_t knownSignature = 0;
    static bool initialized = false;
    const ULONGLONG now = GetTickCount64();
    if (now < nextCheck) return;
    nextCheck = now + 750;

    const uint64_t currentSignature = ConfigFolderSignature();
    if (initialized && currentSignature == knownSignature) return;
    initialized = true;
    knownSignature = currentSignature;
    ReloadConfigsFromDisk();
    knownSignature = ConfigFolderSignature();
}

void SaveConfig(std::string name) {
    if (name.empty()) return;
    std::string out;
    out += "{\n";
    auto wb = [&](const char* k, bool v) { out += std::string("  \"") + k + "\": " + (v ? "1" : "0") + ",\n"; };
    auto wf = [&](const char* k, float v) { char buf[64]; sprintf_s(buf, "%.6f", v); out += std::string("  \"") + k + "\": " + buf + ",\n"; };
    auto wi = [&](const char* k, int v) { out += std::string("  \"") + k + "\": " + std::to_string(v) + ",\n"; };
    auto wh = [&](const char* k, const char* value) {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string encoded;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p) {
            encoded.push_back(digits[*p >> 4]);
            encoded.push_back(digits[*p & 0x0F]);
        }
        out += std::string("  \"") + k + "\": \"" + encoded + "\",\n";
    };
    wb("en_autoclick", features::combat::auto_click::enabled);
    wb("en_aimassist", features::combat::aim_assist::enabled);
    wb("en_reach", gui_reach_enabled);
    wb("en_velocity", gui_velo_enabled);
    wb("en_nohitdelay", gui_nohitdelay_enabled);
    wb("en_refill", features::combat::refill::enabled);
    wb("en_sprint", gui_sprint_enabled);
    wb("en_noslow", gui_noslow_enabled);
    wb("en_nojumpdelay", gui_nojumpdelay_enabled);
    wb("en_noitemrelease", gui_noitemrelease_enabled);
    wb("en_esp", gui_esp_enabled);
    wi("wesp_rmode", gui_whip_esp_render_mode);
    wi("wesp_m2d", gui_whip_esp_mode2d);
    wi("wesp_m3d", gui_whip_esp_mode3d);
    wb("wesp_hbar", gui_whip_esp_show_healthbar);
    wf("wesp_hwidth", gui_whip_esp_healthbar_width);
    wf("wesp_hoffset", gui_whip_esp_healthbar_offset);
    wb("wesp_friends", gui_whip_esp_hide_friends);
    wf("wesp_o2dwidth", gui_whip_esp_outline2d_width);
    wf("wesp_maxdist", gui_whip_esp_max_distance);
    wf("wesp_hurt_r", gui_whip_esp_hurt_color[0]);
    wf("wesp_hurt_g", gui_whip_esp_hurt_color[1]);
    wf("wesp_hurt_b", gui_whip_esp_hurt_color[2]);
    wf("wesp_hurt_a", gui_whip_esp_hurt_color[3]);
    wb("en_nametags", gui_nametags_enabled);
    wb("en_tracers", gui_tracers_enabled);

    wb("wm_en", gui_watermark_enabled);
    wi("wm_color_mode", gui_watermark_color_mode);
    wf("wm_color_b0", gui_watermark_color_b[0]); wf("wm_color_b1", gui_watermark_color_b[1]); wf("wm_color_b2", gui_watermark_color_b[2]);
    wf("wm_pos_x", gui_watermark_pos_x);
    wf("wm_pos_y", gui_watermark_pos_y);
    wb("wm_ply", gui_watermark_show_player);
    wb("wm_srv", gui_watermark_show_server);
    wb("wm_fps", gui_watermark_show_fps);
    wb("wm_name", gui_watermark_show_name);
    wb("wm_time", gui_watermark_show_time);
    wb("wm_bg", gui_watermark_background);
    wb("wm_text_shadow", gui_watermark_text_shadow);

    wb("en_arraylist", gui_arraylist_enabled);
    wf("al_scale", gui_arraylist_scale);
    wf("al_bgc0", gui_arraylist_bg_color_4[0]); wf("al_bgc1", gui_arraylist_bg_color_4[1]); wf("al_bgc2", gui_arraylist_bg_color_4[2]); wf("al_bgc3", gui_arraylist_bg_color_4[3]);
    wi("al_fnt", gui_arraylist_font);
    wb("al_low", gui_arraylist_lowercase);
    wb("al_shd", gui_arraylist_shadows);
    wb("al_bold", gui_arraylist_bold);
    wb("al_cbar", gui_arraylist_colorbar);
    wf("al_c0", gui_arraylist_color[0]); wf("al_c1", gui_arraylist_color[1]); wf("al_c2", gui_arraylist_color[2]);
    wf("al_ic0", gui_arraylist_info_color[0]); wf("al_ic1", gui_arraylist_info_color[1]); wf("al_ic2", gui_arraylist_info_color[2]);
    wf("al_spd", gui_arraylist_speed);
    wf("al_rad", gui_arraylist_radius);
    wf("al_px", gui_arraylist_pad_x); wf("al_py", gui_arraylist_pad_y);
    wf("al_posx", gui_arraylist_pos_x); wf("al_posy", gui_arraylist_pos_y);
    wb("al_inf", gui_arraylist_show_info);
    wb("al_not", features::misc::notifications::enabled);
    wi("not_pos", features::misc::notifications::position);
    wb("al_drg", g_ArrayListDragMode);
    wb("al_bg", gui_arraylist_background);
    wb("al_bgsh", gui_arraylist_background_shadow);
    wi("al_mod", gui_arraylist_color_mode);
    wi("al_anim", gui_arraylist_anim_style);
    wb("al_rain", gui_arraylist_rainbow_bar);
    wb("al_glow", gui_arraylist_glow);
    wb("al_title", gui_arraylist_show_title);
    wb("al_title_custom", gui_arraylist_title_custom_color);
    wf("al_title_c0", gui_arraylist_title_color[0]);
    wf("al_title_c1", gui_arraylist_title_color[1]);
    wf("al_title_c2", gui_arraylist_title_color[2]);
    wb("al_blur", gui_arraylist_blur);
    wf("al_bopac", gui_arraylist_blur_opacity);
    wf("al_fc0", gui_arraylist_flow_color[0]); wf("al_fc1", gui_arraylist_flow_color[1]); wf("al_fc2", gui_arraylist_flow_color[2]);
    wf("al_fade0", gui_arraylist_fade_color[0]); wf("al_fade1", gui_arraylist_fade_color[1]); wf("al_fade2", gui_arraylist_fade_color[2]);
    // Save hidden modules as pipe-separated string
    {
        std::string hidden_str;
        for (auto& h : g_arraylist_hidden_modules) {
            if (!hidden_str.empty()) hidden_str += "|";
            hidden_str += h;
        }
        out += std::string("  \"al_hidden\": \"") + hidden_str + "\",\n";
    }
    wf("ac_r", g_AccentColor[0]); wf("ac_g", g_AccentColor[1]); wf("ac_b", g_AccentColor[2]);
    wf("gui_col_r", gui_guicolor_custom[0]); wf("gui_col_g", gui_guicolor_custom[1]); wf("gui_col_b", gui_guicolor_custom[2]);
    wb("menu_header_animation", g_MenuHeaderAnim);
    wf("menu_header_background_r", g_MenuHeaderBgColor[0]);
    wf("menu_header_background_g", g_MenuHeaderBgColor[1]);
    wf("menu_header_background_b", g_MenuHeaderBgColor[2]);
    wf("menu_header_stars_r", g_MenuHeaderAnimColor[0]);
    wf("menu_header_stars_g", g_MenuHeaderAnimColor[1]);
    wf("menu_header_stars_b", g_MenuHeaderAnimColor[2]);
    wi("menu_key", g_MenuKey);
    wi("gui_scale_index", g_GuiScaleIndex);
    wb("background_dim", g_BackgroundDim);
    wb("en_hitmarkers", features::visual::hit_markers::enabled);
    wb("en_armorswitcher", gui_armorswitcher_enabled);
    wb("aa_food_only", features::combat::aim_assist::food_only);
    wi("blink_mode", features::misc::blink::mode);
    wf("al_bar_width", gui_arraylist_bar_width);
    wb("al_bracket_flags", gui_arraylist_bracket_flags);
    wb("wm_blur", gui_watermark_blur);
    wf("wm_blur_opacity", gui_watermark_blur_opacity);
    wf("wm_c0", gui_watermark_color[0]); wf("wm_c1", gui_watermark_color[1]); wf("wm_c2", gui_watermark_color[2]);
    wf("al_c2_0", gui_arraylist_color_b[0]); wf("al_c2_1", gui_arraylist_color_b[1]); wf("al_c2_2", gui_arraylist_color_b[2]);
    wf("blink_path_c0", gui_blink_path_color[0]); wf("blink_path_c1", gui_blink_path_color[1]); wf("blink_path_c2", gui_blink_path_color[2]);
    for (int i = 0; i < 4; ++i) {
        wf(("esp_hbg_" + std::to_string(i)).c_str(), gui_whip_esp_healthbar_bg[i]);
        wf(("esp_hfull_" + std::to_string(i)).c_str(), gui_whip_esp_healthbar_full[i]);
        wf(("esp_hlow_" + std::to_string(i)).c_str(), gui_whip_esp_healthbar_low[i]);
        wf(("esp_3d_" + std::to_string(i)).c_str(), gui_whip_esp_neutral_color[i]);
        wf(("esp_2d_" + std::to_string(i)).c_str(), gui_whip_esp_outline2d_color[i]);
        wf(("notif_bar_" + std::to_string(i)).c_str(), features::misc::notifications::bar_color[i]);
    }
    wb("en_autoarmor", gui_autoarmor_enabled);
    wf("autoarmor_delay", gui_autoarmor_delay);
    wb("autoarmor_better", gui_autoarmor_only_better);
    wb("en_blink", gui_blink_enabled);

    wb("en_blockhit", gui_blockhit_enabled);
    wi("gui_blockhit_mode", gui_blockhit_mode);
    wb("gui_blockhit_mouse", gui_blockhit_require_mouse_down);
    wf("gui_blockhit_block_ticks", gui_blockhit_block_ticks);
    wf("gui_blockhit_unblock_ticks", gui_blockhit_unblock_ticks);
    wf("gui_blockhit_chance", gui_blockhit_chance);
    wb("gui_blockhit_only_sword", gui_blockhit_only_sword);
    wb("gui_blockhit_visual", gui_blockhit_visual_only);
    wb("en_fastplace", gui_fastplace_enabled);
    wi("gui_fastplace_held", gui_fastplace_held_item);
    wb("en_autotool", gui_autotool_enabled);
    wf("gui_autotool_delay", gui_autotool_swap_delay);
    wb("gui_autotool_swap_weapon", gui_autotool_swap_weapon);
    wb("gui_autotool_instant", gui_autotool_instant_swap);
    wb("gui_autotool_back", gui_autotool_swap_back);
    wb("gui_autotool_mouse", gui_autotool_require_mouse_down);
    wb("gui_autotool_sneak", gui_autotool_only_sneaking);
    wb("gui_ac_break", gui_ac_break_blocks);
    // -- New modules --
    wb("en_macros", gui_macros_enabled);
    wi("gui_macros_mode", gui_macros_mode);
    wi("gui_macros_bind", gui_macros_bind);
    wf("gui_macros_switch_delay", gui_macros_switch_delay);
    wf("gui_macros_use_delay", gui_macros_use_delay);
    wb("gui_macros_auto_back", gui_macros_auto_switch_back);
    wi("gui_armorsw_kit", gui_armorswitcher_kit);
    wi("gui_armorsw_bind", gui_armorswitcher_bind);
    wf("gui_armorsw_delay", gui_armorswitcher_delay);
    
    // (duplicates removed ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â already saved above)


    wf("gui_refill_delay", gui_refill_delay);
    wf("gui_refill_silent_delay", gui_refill_silent_delay);
    wf("gui_refill_silent_ticks", gui_refill_silent_ticks);

    wf("gui_min_cps", gui_min_cps);
    wf("gui_max_cps", gui_max_cps);
    wf("gui_inv_cps", gui_inv_cps);
    wb("ac_break_blocks", gui_ac_break_blocks);
    wb("ac_inv_en", features::combat::auto_click::inventory_enabled);
    wb("ac_weap_only", features::combat::auto_click::weapons_only);
    wb("ac_target_only", features::combat::auto_click::target_only);
    wb("ac_blockhit", features::combat::auto_click::blockhit_sync);
    wb("ac_random", features::combat::auto_click::randomization);
    wf("ac_drop", features::combat::auto_click::drop_chance);
    wf("ac_spike", features::combat::auto_click::spike_chance);
    wi("ac_click_method", gui_ac_click_method);

    wf("gui_aa_speed", gui_aa_speed);
    wb("gui_aa_silent", gui_aa_silent);
    wf("gui_aa_dist", gui_aa_dist);
    wf("gui_aa_fov", gui_aa_fov);
    wf("gui_reach_min_distance", gui_reach_min_distance); wf("gui_reach_max_distance", gui_reach_max_distance);
    wb("gui_reach_hitbox_enabled", gui_reach_hitbox_enabled); wf("gui_reach_hitbox_size", gui_reach_hitbox_size);
    wf("gui_reach_chance", gui_reach_chance);
    wb("gui_reach_ground_only", gui_reach_ground_only);
    wb("gui_reach_weapon_only", gui_reach_weapon_only);
    wb("gui_reach_liquid_check", gui_reach_liquid_check);
    wb("gui_reach_combo_mode", gui_reach_combo_mode);
    wi("gui_velo_mode", gui_velo_mode);
    wf("gui_velo_horizontal", gui_velo_horizontal);
    wf("gui_velo_vertical", gui_velo_vertical);
    wf("gui_velo_chance", gui_velo_chance);
    wf("gui_velo_delay", gui_velo_delay);
    wb("gui_velo_air_only", gui_velo_air_only);
    wb("gui_velo_moving_only", gui_velo_moving_only);
    wb("gui_velo_weapon_only", gui_velo_weapon_only);
    wb("gui_velo_push_back", gui_velo_push_back);
    wb("gui_velo_clicking_only", gui_velo_clicking_only);
    wf("gui_velo_lag_ms", gui_velo_lag_ms);
    
    wf("gui_tracers_thickness", gui_tracers_thickness);
    wb("gui_tracers_draw_distance", gui_tracers_draw_distance);
    wb("gui_tracers_draw_hurt_time", gui_tracers_draw_hurt_time);
    wb("gui_tracers_draw_invisible", gui_tracers_draw_invisible);
    wf("gui_blink_timer_limit", gui_blink_timer_limit);
    wb("gui_blink_show_path", gui_blink_show_path);
    wb("gui_blink_show_timer", gui_blink_show_timer);
    wb("gui_sprint_omni", gui_sprint_omni);
    wb("gui_nametags_draw_health", gui_nametags_draw_health);
    wi("gui_nametags_health_format", gui_nametags_health_format);
    wf("gui_nametags_health_segments", gui_nametags_health_segments);
    wb("gui_nametags_show_own", gui_nametags_show_own);
    wb("gui_nametags_hide_vanilla", gui_nametags_hide_vanilla);
    wb("gui_nametags_show_equipment", gui_nametags_show_equipment);
    wb("gui_nametags_show_enchantments", gui_nametags_show_enchantments);
    wi("hitmarkers_mode", features::visual::hit_markers::mode);
    wf("hitmarkers_size", features::visual::hit_markers::size);
    wf("hitmarkers_width", features::visual::hit_markers::line_width);
    wf("hitmarkers_duration", features::visual::hit_markers::duration);
    wb("hitmarkers_fade", features::visual::hit_markers::fade_out);
    wb("hitmarkers_scale_anim", features::visual::hit_markers::scale_animation);
    wf("hitmarkers_scale_amount", features::visual::hit_markers::scale_amount);
    wb("hitmarkers_outline", features::visual::hit_markers::outline);
    wf("hitmarkers_outline_width", features::visual::hit_markers::outline_width);
    for (int i = 0; i < 4; ++i) { wf(("hitmarkers_color" + std::to_string(i)).c_str(), features::visual::hit_markers::color[i]); wf(("hitmarkers_outline_color" + std::to_string(i)).c_str(), features::visual::hit_markers::outline_color[i]); }
    wb("gui_nametags_draw_distance", gui_nametags_draw_distance);
    wb("gui_nametags_draw_hurt_time", gui_nametags_draw_hurt_time);
    wb("gui_nametags_draw_invisible", gui_nametags_draw_invisible);
    wb("gui_nametags_background", gui_nametags_background);    wb("gui_nametags_use_fake_name", gui_nametags_use_fake_name);
    wf("gui_nametags_scale", gui_nametags_scale);
    wb("gui_nametags_auto_scale", gui_nametags_auto_scale);
    wb("gui_aa_stick", gui_aa_stick);
    wb("gui_aa_silent", gui_aa_silent);
    wi("gui_aa_mode", features::combat::aim_assist::mode);
    wf("gui_aa_lock_strength", features::combat::aim_assist::lock_strength);
    wf("gui_aa_flick_strength", features::combat::aim_assist::flick_strength);    wb("gui_aa_weapons_only", features::combat::aim_assist::weapons_only);
    wb("gui_aa_axe_only", features::combat::aim_assist::axe_only);
    wb("gui_aa_break_blocks", features::combat::aim_assist::break_blocks);
    wb("gui_aa_clicking_only", features::combat::aim_assist::clicking_only);
    wb("gui_aa_lock_target", features::combat::aim_assist::lock_target);
    wb("gui_aa_ignore_walls", features::combat::aim_assist::ignore_walls);
    wb("gui_aa_ignore_invisible", features::combat::aim_assist::ignore_invisible);
    wb("friends_enabled", gui_friends_enabled);
    wi("friends_add_bind", gui_friends_add_bind);
    wi("friends_nearby_bind", gui_friends_nearby_bind);
    wi("friends_clear_bind", gui_friends_clear_bind);
    // Colors (RGBA)
    wf("tracers_c0", gui_tracers_color_4[0]); wf("tracers_c1", gui_tracers_color_4[1]); wf("tracers_c2", gui_tracers_color_4[2]); wf("tracers_c3", gui_tracers_color_4[3]);
    wf("nametags_c0", gui_nametags_color[0]); wf("nametags_c1", gui_nametags_color[1]); wf("nametags_c2", gui_nametags_color[2]); wf("nametags_c3", gui_nametags_color[3]);
    wh("nametags_fake_name", gui_nametags_fake_name);

    for (const auto& module : modules) {
        if (module.enabledPtr) wb(ConfigModuleKey(module.name).c_str(), *module.enabledPtr);
        // Generic persistence keeps every current/future module option covered,
        // even when an explicit legacy key was not added to this function.
        for (const auto& setting : module.settings) {
            const std::string key = ConfigSettingKey(module.name, setting.name);
            switch (setting.type) {
            case Setting::TOGGLE:
                if (setting.boolPtr) wb(key.c_str(), *setting.boolPtr);
                break;
            case Setting::SLIDER:
                if (setting.floatPtr) wf(key.c_str(), *setting.floatPtr);
                break;
            case Setting::DROPDOWN:
                if (setting.intPtr) wi(key.c_str(), *setting.intPtr);
                break;
            case Setting::COLOR:
                if (setting.colorPtr) {
                    for (int i = 0; i < 3; ++i)
                        wf((key + "_" + std::to_string(i)).c_str(), setting.colorPtr[i]);
                }
                break;
            case Setting::COLOR4:
                if (setting.colorPtr) {
                    for (int i = 0; i < 4; ++i)
                        wf((key + "_" + std::to_string(i)).c_str(), setting.colorPtr[i]);
                }
                break;
            default:
                break;
            }
        }
    }

    // keybinds ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â sync g_KeybindMap from modules first, then save
    for (auto& m : modules) g_KeybindMap[m.name] = m.keybind;
    for (auto& kv : g_KeybindMap) {
        std::string key = "bind_";
        key += kv.first;
        for (char& c : key) if (c == ' ') c = '_';
        wi(key.c_str(), kv.second);
    }

    // teammates list ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â  separados por |
    {
        std::string tlist;
        const auto friendsSnapshot = features::friends::snapshot_entries();
        for (size_t i = 0; i < friendsSnapshot.size(); ++i) {
            if (!tlist.empty()) tlist += "|";
            tlist += friendsSnapshot[i].first;
            if (!friendsSnapshot[i].second.empty()) {
                tlist += "@";
                tlist += friendsSnapshot[i].second;
            }
        }
        out += std::string("  \"friends_list\": \"") + tlist + "\",\n";
    }
    // remove trailing comma from last entry for valid JSON
    if (out.size() >= 2 && out[out.size() - 2] == ',')
        out[out.size() - 2] = ' ';
    out += "}\n";
    if (g_ConfigDateMap.find(name) == g_ConfigDateMap.end()) {
        time_t t = time(0);
        struct tm now; localtime_s(&now, &t);
        char buf[80]; strftime(buf, sizeof(buf), "%m/%d/%Y", &now);
        g_ConfigDateMap[name] = buf;
    }
    g_InMemoryConfigs[name] = out;
    g_ActiveConfig = name;
    Registry_SaveConfig(name, out);
    RefreshConfigs();
}

bool gui_config_just_loaded = false;

void LoadConfig(std::string name) {
    auto it = g_InMemoryConfigs.find(name);
    if (it == g_InMemoryConfigs.end()) return;
    g_ActiveConfig = name;
    gui_config_just_loaded = true;
    std::istringstream ss(it->second);
    std::string line;
    auto readFloat = [&](const std::string& key, float& val) {
        std::string prefix = "\"" + key + "\": ";
        size_t pos = line.find(prefix);
        if (pos != std::string::npos) {
            size_t val_start = pos + prefix.length();
            while (val_start < line.length() && (line[val_start] == ' ' || line[val_start] == '\t')) val_start++;
            size_t end = val_start;
            while (end < line.length() && (isdigit(line[end]) || line[end] == '.' || line[end] == ',' || line[end] == '-')) end++;
            if (end > val_start) {
                std::string s_val = line.substr(val_start, end - val_start);
                float result = 0.0f, sign = 1.0f, fraction = 0.1f;
                bool in_frac = false;
                for (char c : s_val) {
                    if (c == '-') sign = -1.0f;
                    else if (c == '.' || c == ',') in_frac = true;
                    else if (isdigit(c)) {
                        if (!in_frac) result = result * 10.0f + (c - '0');
                        else { result += (c - '0') * fraction; fraction *= 0.1f; }
                    }
                }
                val = result * sign;
            }
        }
    };
    auto readBool = [&](const std::string& key, bool& val) {
        std::string prefix = "\"" + key + "\": ";
        size_t pos = line.find(prefix);
        if (pos != std::string::npos) {
            size_t val_start = pos + prefix.length();
            while (val_start < line.length() && (line[val_start] == ' ' || line[val_start] == '\t')) val_start++;
            size_t end = val_start;
            while (end < line.length() && line[end] != ',' && line[end] != '}' && line[end] != ' ' && line[end] != '\t' && line[end] != '\r') end++;
            if (end > val_start) {
                std::string s_val = line.substr(val_start, end - val_start);
                if (s_val == "true" || s_val == "1") val = true;
                else if (s_val == "false" || s_val == "0") val = false;
            }
        }
    };
    auto readInt = [&](const std::string& key, int& val) {
        std::string prefix = "\"" + key + "\": ";
        size_t pos = line.find(prefix);
        if (pos != std::string::npos) {
            size_t val_start = pos + prefix.length();
            while (val_start < line.length() && (line[val_start] == ' ' || line[val_start] == '\t')) val_start++;
            size_t end = val_start;
            while (end < line.length() && (isdigit(line[end]) || line[end] == '-')) end++;
            if (end > val_start) {
                try { val = std::stoi(line.substr(val_start, end - val_start)); }
                catch (...) {}
            }
        }
    };
    auto readHexString = [&](const std::string& key, char* destination, size_t capacity) {
        const std::string prefix = "\"" + key + "\": \"";
        const size_t pos = line.find(prefix);
        if (pos == std::string::npos || capacity == 0) return;
        const size_t begin = pos + prefix.size();
        const size_t end = line.find('"', begin);
        if (end == std::string::npos) return;
        const std::string encoded = line.substr(begin, end - begin);
        const size_t byteCount = (std::min)(encoded.size() / 2, capacity - 1);
        auto nibble = [](char c) -> unsigned char {
            if (c >= '0' && c <= '9') return static_cast<unsigned char>(c - '0');
            if (c >= 'A' && c <= 'F') return static_cast<unsigned char>(c - 'A' + 10);
            if (c >= 'a' && c <= 'f') return static_cast<unsigned char>(c - 'a' + 10);
            return 0;
        };
        for (size_t i = 0; i < byteCount; ++i)
            destination[i] = static_cast<char>((nibble(encoded[i * 2]) << 4) | nibble(encoded[i * 2 + 1]));
        destination[byteCount] = '\0';
    };
    while (std::getline(ss, line)) {
        readInt("menu_key", g_MenuKey);
        readInt("gui_scale_index", g_GuiScaleIndex);
        readBool("background_dim", g_BackgroundDim);
        readBool("en_hitmarkers", features::visual::hit_markers::enabled);
        readBool("en_armorswitcher", gui_armorswitcher_enabled);
        readBool("aa_food_only", features::combat::aim_assist::food_only);
        readInt("blink_mode", features::misc::blink::mode);
        readFloat("al_bar_width", gui_arraylist_bar_width);
        readBool("al_bracket_flags", gui_arraylist_bracket_flags);
        readBool("wm_blur", gui_watermark_blur);
        readFloat("wm_blur_opacity", gui_watermark_blur_opacity);
        readFloat("wm_c0", gui_watermark_color[0]); readFloat("wm_c1", gui_watermark_color[1]); readFloat("wm_c2", gui_watermark_color[2]);
        readFloat("al_c2_0", gui_arraylist_color_b[0]); readFloat("al_c2_1", gui_arraylist_color_b[1]); readFloat("al_c2_2", gui_arraylist_color_b[2]);
        readFloat("blink_path_c0", gui_blink_path_color[0]); readFloat("blink_path_c1", gui_blink_path_color[1]); readFloat("blink_path_c2", gui_blink_path_color[2]);
        for (int i = 0; i < 4; ++i) {
            readFloat("esp_hbg_" + std::to_string(i), gui_whip_esp_healthbar_bg[i]);
            readFloat("esp_hfull_" + std::to_string(i), gui_whip_esp_healthbar_full[i]);
            readFloat("esp_hlow_" + std::to_string(i), gui_whip_esp_healthbar_low[i]);
            readFloat("esp_3d_" + std::to_string(i), gui_whip_esp_neutral_color[i]);
            readFloat("esp_2d_" + std::to_string(i), gui_whip_esp_outline2d_color[i]);
            readFloat("notif_bar_" + std::to_string(i), features::misc::notifications::bar_color[i]);
        }
        readBool("en_autoclick", features::combat::auto_click::enabled);
        readBool("en_aimassist", features::combat::aim_assist::enabled);
        readBool("en_reach", gui_reach_enabled);
        readBool("en_velocity", gui_velo_enabled);
        readBool("en_nohitdelay", gui_nohitdelay_enabled);
        readBool("en_refill", features::combat::refill::enabled);
        readBool("en_sprint", gui_sprint_enabled);
        readBool("en_noslow", gui_noslow_enabled);
        readBool("en_nojumpdelay", gui_nojumpdelay_enabled);
        readBool("en_noitemrelease", gui_noitemrelease_enabled);
        readBool("en_esp", gui_esp_enabled);
        readInt("wesp_rmode", gui_whip_esp_render_mode);
        readInt("wesp_m2d", gui_whip_esp_mode2d);
        readInt("wesp_m3d", gui_whip_esp_mode3d);
        readBool("wesp_hbar", gui_whip_esp_show_healthbar);
        readFloat("wesp_hwidth", gui_whip_esp_healthbar_width);
        readFloat("wesp_hoffset", gui_whip_esp_healthbar_offset);
        readBool("wesp_friends", gui_whip_esp_hide_friends);
        gui_whip_esp_render_mode = ImClamp(gui_whip_esp_render_mode, 0, 1);
        readFloat("wesp_o2dwidth", gui_whip_esp_outline2d_width);
        readFloat("wesp_maxdist", gui_whip_esp_max_distance);
        readFloat("wesp_hurt_r", gui_whip_esp_hurt_color[0]);
        readFloat("wesp_hurt_g", gui_whip_esp_hurt_color[1]);
        readFloat("wesp_hurt_b", gui_whip_esp_hurt_color[2]);
        readFloat("wesp_hurt_a", gui_whip_esp_hurt_color[3]);
        readBool("en_nametags", gui_nametags_enabled);
        readBool("en_tracers", gui_tracers_enabled);
        readBool("wm_en", gui_watermark_enabled);
        readInt("wm_color_mode", gui_watermark_color_mode);
        readFloat("wm_color_b0", gui_watermark_color_b[0]); readFloat("wm_color_b1", gui_watermark_color_b[1]); readFloat("wm_color_b2", gui_watermark_color_b[2]);
        readFloat("wm_pos_x", gui_watermark_pos_x);
        readFloat("wm_pos_y", gui_watermark_pos_y);
        readBool("wm_ply", gui_watermark_show_player);
        readBool("wm_srv", gui_watermark_show_server);
        readBool("wm_fps", gui_watermark_show_fps);
        readBool("wm_name", gui_watermark_show_name);
        readBool("wm_time", gui_watermark_show_time);
        readBool("wm_bg", gui_watermark_background);
        readBool("wm_text_shadow", gui_watermark_text_shadow);
        readBool("en_arraylist", gui_arraylist_enabled);
        readFloat("al_scale", gui_arraylist_scale);
        gui_arraylist_scale = ImClamp(gui_arraylist_scale, 0.5f, 3.0f);
        readFloat("al_bgc0", gui_arraylist_bg_color_4[0]); readFloat("al_bgc1", gui_arraylist_bg_color_4[1]); readFloat("al_bgc2", gui_arraylist_bg_color_4[2]); readFloat("al_bgc3", gui_arraylist_bg_color_4[3]);
        
        // Respetar colores guardados por el usuario
        
        // "al_pos_x": 5.0,
        readInt("al_fnt", gui_arraylist_font);
        readBool("al_low", gui_arraylist_lowercase);
        readBool("al_shd", gui_arraylist_shadows);
        readBool("al_bold", gui_arraylist_bold);
        readBool("al_cbar", gui_arraylist_colorbar);
        // Respetar configuracion guardada del colorbar
        readFloat("al_c0", gui_arraylist_color[0]); readFloat("al_c1", gui_arraylist_color[1]); readFloat("al_c2", gui_arraylist_color[2]);
        // Respetar color guardado por el usuario
        readFloat("al_ic0", gui_arraylist_info_color[0]); readFloat("al_ic1", gui_arraylist_info_color[1]); readFloat("al_ic2", gui_arraylist_info_color[2]);
        readFloat("al_spd", gui_arraylist_speed);
        readFloat("al_rad", gui_arraylist_radius);
        readFloat("al_px", gui_arraylist_pad_x); readFloat("al_py", gui_arraylist_pad_y);
        readFloat("al_posx", gui_arraylist_pos_x); readFloat("al_posy", gui_arraylist_pos_y);
        readBool("al_inf", gui_arraylist_show_info);
        readBool("al_not", features::misc::notifications::enabled);
        readInt("not_pos", features::misc::notifications::position);
        readBool("al_drg", g_ArrayListDragMode);
        readBool("al_title", gui_arraylist_show_title);
        readBool("al_title_custom", gui_arraylist_title_custom_color);
        readFloat("al_title_c0", gui_arraylist_title_color[0]);
        readFloat("al_title_c1", gui_arraylist_title_color[1]);
        readFloat("al_title_c2", gui_arraylist_title_color[2]);
        readBool("al_blur", gui_arraylist_blur);
        readBool("al_bgsh", gui_arraylist_background_shadow);
        readFloat("al_bopac", gui_arraylist_blur_opacity);
        readFloat("al_fc0", gui_arraylist_flow_color[0]); readFloat("al_fc1", gui_arraylist_flow_color[1]); readFloat("al_fc2", gui_arraylist_flow_color[2]);
        readFloat("al_fade0", gui_arraylist_fade_color[0]); readFloat("al_fade1", gui_arraylist_fade_color[1]); readFloat("al_fade2", gui_arraylist_fade_color[2]);
        readBool("al_bg", gui_arraylist_background);
        readInt("al_mod", gui_arraylist_color_mode);
        readInt("al_anim", gui_arraylist_anim_style);
        readBool("al_rain", gui_arraylist_rainbow_bar);
        readBool("al_glow", gui_arraylist_glow);
        // Respetar configuracion de glow guardada
        // Load hidden modules from pipe-separated string
        if (line.find("\"al_hidden\"") != std::string::npos) {
            size_t q1 = line.find(": \"");
            size_t q2 = line.rfind("\"");
            if (q1 != std::string::npos && q2 > q1 + 3) {
                std::string hlist = line.substr(q1 + 3, q2 - q1 - 3);
                g_arraylist_hidden_modules.clear();
                std::string tok;
                for (char ch : hlist) {
                    if (ch == '|') { if (!tok.empty()) g_arraylist_hidden_modules.insert(tok); tok.clear(); }
                    else tok += ch;
                }
                if (!tok.empty()) g_arraylist_hidden_modules.insert(tok);
            }
        }
        readFloat("ac_r", g_AccentColor[0]); readFloat("ac_g", g_AccentColor[1]); readFloat("ac_b", g_AccentColor[2]);
        readFloat("gui_col_r", gui_guicolor_custom[0]); readFloat("gui_col_g", gui_guicolor_custom[1]); readFloat("gui_col_b", gui_guicolor_custom[2]);
        readBool("menu_header_animation", g_MenuHeaderAnim);
        readFloat("menu_header_background_r", g_MenuHeaderBgColor[0]);
        readFloat("menu_header_background_g", g_MenuHeaderBgColor[1]);
        readFloat("menu_header_background_b", g_MenuHeaderBgColor[2]);
        readFloat("menu_header_stars_r", g_MenuHeaderAnimColor[0]);
        readFloat("menu_header_stars_g", g_MenuHeaderAnimColor[1]);
        readFloat("menu_header_stars_b", g_MenuHeaderAnimColor[2]);
        readBool("en_autoarmor", gui_autoarmor_enabled);
        readFloat("autoarmor_delay", gui_autoarmor_delay);
        readBool("autoarmor_better", gui_autoarmor_only_better);
        readBool("en_blink", gui_blink_enabled);

        readBool("en_blockhit", gui_blockhit_enabled);
        readInt("gui_blockhit_mode", gui_blockhit_mode);
        readBool("gui_blockhit_mouse", gui_blockhit_require_mouse_down);
        readFloat("gui_blockhit_block_ticks", gui_blockhit_block_ticks);
        readFloat("gui_blockhit_unblock_ticks", gui_blockhit_unblock_ticks);
        readFloat("gui_blockhit_chance", gui_blockhit_chance);
        readBool("gui_blockhit_only_sword", gui_blockhit_only_sword);
        readBool("gui_blockhit_visual", gui_blockhit_visual_only);
        readBool("en_fastplace", gui_fastplace_enabled);
        readInt("gui_fastplace_held", gui_fastplace_held_item);
        readBool("en_autotool", gui_autotool_enabled);
        readFloat("gui_autotool_delay", gui_autotool_swap_delay);
        readBool("gui_autotool_swap_weapon", gui_autotool_swap_weapon);
        readBool("gui_autotool_instant", gui_autotool_instant_swap);
        readBool("gui_autotool_back", gui_autotool_swap_back);
        readBool("gui_autotool_mouse", gui_autotool_require_mouse_down);
        readBool("gui_autotool_sneak", gui_autotool_only_sneaking);
        readBool("gui_ac_break", gui_ac_break_blocks);
        // -- New modules --
        readBool("en_macros", gui_macros_enabled);
        readInt("gui_macros_mode", gui_macros_mode);
        readInt("gui_macros_bind", gui_macros_bind);
        readFloat("gui_macros_switch_delay", gui_macros_switch_delay);
        readFloat("gui_macros_use_delay", gui_macros_use_delay);
        readBool("gui_macros_auto_back", gui_macros_auto_switch_back);
        readInt("gui_armorsw_kit", gui_armorswitcher_kit);
        readInt("gui_armorsw_bind", gui_armorswitcher_bind);
        readFloat("gui_armorsw_delay", gui_armorswitcher_delay);

        // (duplicates removed ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬Ãƒâ€¦Ã‚Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â  already loaded above)


        readFloat("gui_refill_delay", gui_refill_delay);
        readFloat("gui_refill_silent_delay", gui_refill_silent_delay);
        readFloat("gui_refill_silent_ticks", gui_refill_silent_ticks);

        readFloat("gui_min_cps", gui_min_cps);
        readFloat("gui_max_cps", gui_max_cps);
        readFloat("gui_inv_cps", gui_inv_cps);
        readBool("ac_break_blocks", gui_ac_break_blocks);
        readBool("ac_inv_en", features::combat::auto_click::inventory_enabled);
        readBool("ac_weap_only", features::combat::auto_click::weapons_only);
        readBool("ac_target_only", features::combat::auto_click::target_only);
        readBool("ac_blockhit", features::combat::auto_click::blockhit_sync);
        readBool("ac_random", features::combat::auto_click::randomization);
        readFloat("ac_drop", features::combat::auto_click::drop_chance);
        readFloat("ac_spike", features::combat::auto_click::spike_chance);
        readInt("ac_click_method", gui_ac_click_method);
        readFloat("gui_aa_speed", gui_aa_speed);
        readBool("gui_aa_silent", gui_aa_silent);
        readFloat("gui_aa_dist", gui_aa_dist);
        readFloat("gui_aa_fov", gui_aa_fov);
        readFloat("gui_reach_min_distance", gui_reach_min_distance); readFloat("gui_reach_max_distance", gui_reach_max_distance);
        readBool("gui_reach_hitbox_enabled", gui_reach_hitbox_enabled); readFloat("gui_reach_hitbox_size", gui_reach_hitbox_size);
        readFloat("gui_reach_chance", gui_reach_chance);
        readBool("gui_reach_ground_only", gui_reach_ground_only);
        readBool("gui_reach_weapon_only", gui_reach_weapon_only);
        readBool("gui_reach_liquid_check", gui_reach_liquid_check);
        readBool("gui_reach_combo_mode", gui_reach_combo_mode);
        readInt("gui_velo_mode", gui_velo_mode);
        readFloat("gui_velo_horizontal", gui_velo_horizontal);
        readFloat("gui_velo_vertical", gui_velo_vertical);
        readFloat("gui_velo_chance", gui_velo_chance);
        readFloat("gui_velo_delay", gui_velo_delay);
        readBool("gui_velo_air_only", gui_velo_air_only);
        readBool("gui_velo_moving_only", gui_velo_moving_only);
        readBool("gui_velo_weapon_only", gui_velo_weapon_only);
        readBool("gui_velo_push_back", gui_velo_push_back);
        readBool("gui_velo_clicking_only", gui_velo_clicking_only);
        readFloat("gui_velo_lag_ms", gui_velo_lag_ms);
        
        readFloat("gui_tracers_thickness", gui_tracers_thickness);
        readBool("gui_tracers_draw_distance", gui_tracers_draw_distance);
        readBool("gui_tracers_draw_hurt_time", gui_tracers_draw_hurt_time);
        readBool("gui_tracers_draw_invisible", gui_tracers_draw_invisible);
        readFloat("gui_blink_timer_limit", gui_blink_timer_limit);
        readBool("gui_blink_show_path", gui_blink_show_path);
        readBool("gui_blink_show_timer", gui_blink_show_timer);
        readBool("gui_sprint_omni", gui_sprint_omni);
        readBool("gui_nametags_draw_health", gui_nametags_draw_health);
        readInt("gui_nametags_health_format", gui_nametags_health_format);
        readFloat("gui_nametags_health_segments", gui_nametags_health_segments);
        readBool("gui_nametags_show_own", gui_nametags_show_own);
        readBool("gui_nametags_hide_vanilla", gui_nametags_hide_vanilla);
        readBool("gui_nametags_show_equipment", gui_nametags_show_equipment);
        readBool("gui_nametags_show_enchantments", gui_nametags_show_enchantments);
        readInt("hitmarkers_mode", features::visual::hit_markers::mode);
        readFloat("hitmarkers_size", features::visual::hit_markers::size);
        readFloat("hitmarkers_width", features::visual::hit_markers::line_width);
        readFloat("hitmarkers_duration", features::visual::hit_markers::duration);
        readBool("hitmarkers_fade", features::visual::hit_markers::fade_out);
        readBool("hitmarkers_scale_anim", features::visual::hit_markers::scale_animation);
        readFloat("hitmarkers_scale_amount", features::visual::hit_markers::scale_amount);
        readBool("hitmarkers_outline", features::visual::hit_markers::outline);
        readFloat("hitmarkers_outline_width", features::visual::hit_markers::outline_width);
        for (int i = 0; i < 4; ++i) { readFloat("hitmarkers_color" + std::to_string(i), features::visual::hit_markers::color[i]); readFloat("hitmarkers_outline_color" + std::to_string(i), features::visual::hit_markers::outline_color[i]); }
        readBool("gui_nametags_draw_distance", gui_nametags_draw_distance);
        readBool("gui_nametags_draw_hurt_time", gui_nametags_draw_hurt_time);
        readBool("gui_nametags_draw_invisible", gui_nametags_draw_invisible);
        readBool("gui_nametags_background", gui_nametags_background);        readBool("gui_nametags_use_fake_name", gui_nametags_use_fake_name);
        readFloat("gui_nametags_scale", gui_nametags_scale);
        gui_nametags_scale = ImClamp(gui_nametags_scale, 0.85f, 2.00f);
        readBool("gui_nametags_auto_scale", gui_nametags_auto_scale);

        readBool("gui_aa_stick", gui_aa_stick);
        readBool("gui_aa_silent", gui_aa_silent);
        readInt("gui_aa_mode", features::combat::aim_assist::mode);
        features::combat::aim_assist::mode = ImClamp(features::combat::aim_assist::mode, 0, 3);
        readFloat("gui_aa_lock_strength", features::combat::aim_assist::lock_strength);
        readFloat("gui_aa_flick_strength", features::combat::aim_assist::flick_strength);        readBool("gui_aa_weapons_only", features::combat::aim_assist::weapons_only);
        readBool("gui_aa_axe_only", features::combat::aim_assist::axe_only);
        readBool("gui_aa_break_blocks", features::combat::aim_assist::break_blocks);
        readBool("gui_aa_clicking_only", features::combat::aim_assist::clicking_only);
        readBool("gui_aa_lock_target", features::combat::aim_assist::lock_target);
        readBool("gui_aa_ignore_walls", features::combat::aim_assist::ignore_walls);
        readBool("gui_aa_ignore_invisible", features::combat::aim_assist::ignore_invisible);
        readBool("friends_enabled", gui_friends_enabled);
        readInt("friends_add_bind", gui_friends_add_bind);
        readInt("friends_nearby_bind", gui_friends_nearby_bind);
        readInt("friends_clear_bind", gui_friends_clear_bind);
        // Colors (RGBA)
        readFloat("tracers_c0", gui_tracers_color_4[0]); readFloat("tracers_c1", gui_tracers_color_4[1]); readFloat("tracers_c2", gui_tracers_color_4[2]); readFloat("tracers_c3", gui_tracers_color_4[3]);
        readFloat("nametags_c0", gui_nametags_color[0]); readFloat("nametags_c1", gui_nametags_color[1]); readFloat("nametags_c2", gui_nametags_color[2]); readFloat("nametags_c3", gui_nametags_color[3]);
        readHexString("nametags_fake_name", gui_nametags_fake_name, sizeof(gui_nametags_fake_name));

        for (auto& module : modules) {
            if (module.enabledPtr) readBool(ConfigModuleKey(module.name), *module.enabledPtr);
            for (auto& setting : module.settings) {
                const std::string key = ConfigSettingKey(module.name, setting.name);
                switch (setting.type) {
                case Setting::TOGGLE:
                    if (setting.boolPtr) readBool(key, *setting.boolPtr);
                    break;
                case Setting::SLIDER:
                    if (setting.floatPtr) readFloat(key, *setting.floatPtr);
                    break;
                case Setting::DROPDOWN:
                    if (setting.intPtr) readInt(key, *setting.intPtr);
                    break;
                case Setting::COLOR:
                    if (setting.colorPtr) {
                        for (int i = 0; i < 3; ++i)
                            readFloat(key + "_" + std::to_string(i), setting.colorPtr[i]);
                    }
                    break;
                case Setting::COLOR4:
                    if (setting.colorPtr) {
                        for (int i = 0; i < 4; ++i)
                            readFloat(key + "_" + std::to_string(i), setting.colorPtr[i]);
                    }
                    break;
                default:
                    break;
                }
            }
        }

        // keybinds
        for (auto& m : modules) {
            std::string key = "bind_";
            key += m.name;
            for (char& c : key) if (c == ' ') c = '_';
            readInt(key, m.keybind);
        }
        // friends list
        if (line.find("\"friends_list\"") != std::string::npos) {
            size_t q1 = line.find(": \"");
            size_t q2 = line.rfind("\"");
            if (q1 != std::string::npos && q2 > q1 + 3) {
                std::string tlist = line.substr(q1 + 3, q2 - q1 - 3);
                std::vector<std::string> loadedFriendNames;
                std::vector<std::string> loadedFriendIds;
                std::string tok;
                for (char ch : tlist) {
                    if (ch == '|') {
                        if (!tok.empty()) {
                            const size_t separator = tok.rfind('@');
                            loadedFriendNames.push_back(separator == std::string::npos ? tok : tok.substr(0, separator));
                            loadedFriendIds.push_back(separator == std::string::npos ? "" : tok.substr(separator + 1));
                        }
                        tok.clear();
                    }
                    else tok += ch;
                }
                if (!tok.empty()) {
                    const size_t separator = tok.rfind('@');
                    loadedFriendNames.push_back(separator == std::string::npos ? tok : tok.substr(0, separator));
                    loadedFriendIds.push_back(separator == std::string::npos ? "" : tok.substr(separator + 1));
                }
                features::friends::replace_all(loadedFriendNames, loadedFriendIds);
                for (size_t i = 0; i < loadedFriendNames.size(); ++i) {
                    if (i >= loadedFriendIds.size() || loadedFriendIds[i].empty())
                        features::friends::request_uuid(loadedFriendNames[i]);
                    else
                        features::friends::verify_uuid(loadedFriendIds[i]);
                }
            }
        }
    }
    g_SliderAnim.clear(); g_SliderFill.clear();
    // Sincronizar binds cargados hacia g_KeybindMap y punteros específicos
    for (auto& m : modules) {
        g_KeybindMap[m.name] = m.keybind;
        
        if (m.name == "Blink")         features::misc::blink::bind = m.keybind;
        if (m.name == "Refill")        features::combat::refill::bind = m.keybind;
        if (m.name == "ArmorSwitcher") gui_armorswitcher_bind = m.keybind;
    }
    for (int i = 0; i < 3; ++i) g_AccentColor[i] = gui_guicolor_custom[i];
    g_GuiScaleIndex = ImClamp(g_GuiScaleIndex, 0, 3);
    static const float savedGuiScales[] = { 1.0f, 1.50f, 1.75f, 2.0f };
    g_GuiScaleTarget = savedGuiScales[g_GuiScaleIndex];
    c::accent = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.0f);
    c::accent_gradient = ImVec4(
        ImClamp(g_AccentColor[0] * 0.82f, 0.0f, 1.0f),
        ImClamp(g_AccentColor[1] * 0.82f, 0.0f, 1.0f),
        ImClamp(g_AccentColor[2] * 0.82f, 0.0f, 1.0f), 1.0f);
}

static void SaveConfigTypes(); // forward declaration

void DeleteConfig(std::string name) {
    if (g_ActiveConfig == name)
        g_ActiveConfig.clear();
    g_InMemoryConfigs.erase(name);
    g_ConfigTypeMap.erase(name);
    g_ConfigDateMap.erase(name);
    Registry_DeleteConfig(name);
    RefreshConfigs();
    // Persist updated type map
    SaveConfigTypes();
}

// ============================================================
// CONFIG TYPE METADATA PERSISTENCE
// ============================================================
static void SaveConfigTypes() {
    std::string data;
    for (auto& kv : g_ConfigTypeMap) {
        if (!data.empty()) data += "|";
        std::string dt = g_ConfigDateMap[kv.first];
        if (dt.empty()) dt = "Unknown";
        data += kv.first + ":" + std::to_string(kv.second) + ":" + dt;
    }
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "__cfg_types__", 0, REG_BINARY,
            (const BYTE*)data.c_str(), (DWORD)data.size());
        
        // Auto-load was removed: also erase metadata written by older builds.
        RegDeleteValueA(hKey, "__autoload__");
            
        RegCloseKey(hKey);
    }
}

static void LoadConfigTypes() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, CFG_REG_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    std::vector<BYTE> buf(8192);
    DWORD dataSize = (DWORD)buf.size(), type = 0;
    if (RegQueryValueExA(hKey, "__cfg_types__", nullptr, &type, buf.data(), &dataSize) == ERROR_SUCCESS && type == REG_BINARY) {
        std::string raw((char*)buf.data(), dataSize);
        std::string token;
        for (size_t i = 0; i <= raw.size(); i++) {
            if (i == raw.size() || raw[i] == '|') {
                size_t sep1 = token.find(':');
                if (sep1 != std::string::npos) {
                    size_t sep2 = token.find(':', sep1 + 1);
                    std::string cfgName = token.substr(0, sep1);
                    int cfgType = atoi(token.substr(sep1 + 1, sep2 != std::string::npos ? sep2 - sep1 - 1 : std::string::npos).c_str());
                    if (cfgType >= 0 && cfgType <= 2) {
                        g_ConfigTypeMap[cfgName] = cfgType;
                        if (sep2 != std::string::npos) {
                            g_ConfigDateMap[cfgName] = token.substr(sep2 + 1);
                        } else {
                            g_ConfigDateMap[cfgName] = "Unknown";
                        }
                    }
                }
                token.clear();
            } else {
                token += raw[i];
            }
        }
    }
    
    RegCloseKey(hKey);
}

// ============================================================
// CONFIG FILE EXPORT / IMPORT (share with friends)
// ============================================================
static void ExportConfigToFile(const std::string& cfgName) {
    auto it = g_InMemoryConfigs.find(cfgName);
    if (it == g_InMemoryConfigs.end()) return;

    // Build export data: prepend type metadata
    int cfgType = 0;
    auto tit = g_ConfigTypeMap.find(cfgName);
    if (tit != g_ConfigTypeMap.end()) cfgType = tit->second;
    std::string exportData = "// CLOUD_CONFIG_TYPE=" + std::to_string(cfgType) + "\n" + it->second;

    char fileName[MAX_PATH] = "";
    strncpy_s(fileName, cfgName.c_str(), _TRUNCATE);
    strcat_s(fileName, ".pcfg");

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_GameWindow;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "pandora Config (*.pcfg)\0*.pcfg\0All Files (*.*)\0*.*\0";
    ofn.lpstrDefExt = "pcfg";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn)) {
        std::ofstream ofs(fileName, std::ios::binary);
        if (ofs.is_open()) {
            ofs.write(exportData.c_str(), exportData.size());
            ofs.close();
            TriggerNotification("Config exported!", ("Saved: " + cfgName).c_str(), "CONFIG_SAVE");
        }
    }
}

static void ImportConfigFromFile() {
    char fileName[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_GameWindow;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "pandora Config (*.pcfg)\0*.pcfg\0All Files (*.*)\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        std::ifstream ifs(fileName, std::ios::binary);
        if (ifs.is_open()) {
            std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            ifs.close();

            // Extract config name from filename
            std::string path(fileName);
            size_t lastSlash = path.find_last_of("\\/");
            std::string baseName = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
            size_t dot = baseName.find_last_of('.');
            if (dot != std::string::npos) baseName = baseName.substr(0, dot);

            // Parse type from first line if present
            int importType = CFG_LEGIT;
            std::string configData = content;
            if (content.substr(0, 24) == "// CLOUD_CONFIG_TYPE=") {
                size_t nl = content.find('\n');
                if (nl != std::string::npos) {
                    importType = atoi(content.substr(24, nl - 24).c_str());
                    if (importType < 0 || importType > 2) importType = 0;
                    configData = content.substr(nl + 1);
                }
            }

            g_InMemoryConfigs[baseName] = configData;
            g_ConfigTypeMap[baseName] = importType;
            g_ConfigDateMap[baseName] = "Imported";
            Registry_SaveConfig(baseName, configData);
            SaveConfigTypes();
            RefreshConfigs();
            TriggerNotification("Config imported!", ("Profile: " + baseName).c_str(), "CONFIG_LOAD");
        }
    }
}

