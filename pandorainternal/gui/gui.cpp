#include "gui.h"

// ============================================================
// IN-GAME MENU STATE
// ============================================================
bool         g_MenuVisible = false;
std::atomic<bool> g_PlayerInGui{ false };
std::atomic<bool> g_OnMinecraftMainMenu{ false };
std::atomic<bool> g_MinecraftWorldLoaded{ false };
std::atomic<bool> g_OnMultiplayerScreen{ false };
std::atomic<bool> g_ChatOpen{ false };
bool         g_ArrayListDragMode = false;
bool         g_ImGuiReady = false;
WNDPROC      g_OrigWndProc = nullptr;
static ImGuiContext* g_OurImGuiCtx = nullptr; // nuestro contexto ImGui, guardado al init
namespace pandora_w_port { void invalidate_context(); }
void InvalidateInGameImGuiContext(ImGuiContext* dyingContext) {
    if (g_OurImGuiCtx == dyingContext) g_OurImGuiCtx = nullptr;
    pandora_w_port::invalidate_context();
}
namespace hooks { extern ImGuiContext* g_GameImGuiContext; }
float        g_MenuOpenAnim = 0.0f;
float        g_PendingWheelDelta = 0.0f;
int   g_SelectedMod = -1;
int   g_PrevSelectedMod = -1;
float g_PanelAnim = 1.0f;
float g_TargetScrollY = 0.0f;

bool           g_IsBinding = false;
int* g_BindingPtr = nullptr;
bool           g_BindMouseArmed = false;
std::map<int, bool> keyStates;
std::unordered_map<std::string, ULONGLONG> g_KeybindBlockedUntil;
std::map<ImGuiID, float> g_AnimStates;

float ImLerp(float a, float b, float t) { return a + (b - a) * t; }
ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}
float GetAnim(const char* label, bool active, float speed = 0.11f) {
    ImGuiID id = ImGui::GetID(label);
    float target = active ? 1.0f : 0.0f;
    if (g_AnimStates.find(id) == g_AnimStates.end()) g_AnimStates[id] = 0.0f;
    
    // Smooth frame-rate independent interpolation (approximate)
    float dt = ImGui::GetIO().DeltaTime;
    float lerpAmount = 1.0f - expf(-speed * 60.0f * dt);
    
    g_AnimStates[id] = ImLerp(g_AnimStates[id], target, lerpAmount);
    return g_AnimStates[id];
}
int          g_MenuKey = VK_INSERT;
bool         g_IsBindingMenuKey = false;       // tecla para abrir/cerrar el menu (default: INSERT)
bool         g_ConfigInputActive = false; // TRUE mientras el InputText de configs tiene foco ? bloquea movimiento

float g_AccentColor[4] = { 1.0f, 1.0f, 1.0f, 1.00f };
float gui_guicolor_custom[3] = { 1.0f, 1.0f, 1.0f };
bool g_MenuHeaderAnim = true;
float g_MenuHeaderAnimColor[3] = {1.0f, 1.0f, 1.0f};
float g_MenuHeaderBgColor[3] = {0.302f, 0.235f, 0.420f};

float gui_min_cps = 12.0f;
float gui_max_cps = 14.0f;
float gui_inv_cps = 20.0f;
float gui_aa_speed = 10.0f;
float gui_aa_dist = 4.0f;
float gui_aa_fov = 180.0f;
bool  gui_aa_stick = false;
bool  gui_aa_silent = false;
float gui_jitter = 0.5f;

float gui_refill_delay = 80.0f;
float gui_refill_silent_delay = 80.0f;
float gui_refill_silent_ticks = 2.0f;

bool gui_armorswitcher_enabled = false;
int  gui_armorswitcher_kit = 0;          // 0=Diamond, 1=Iron, 2=Gold, 3=Chain, 4=Leather
int  gui_armorswitcher_bind = 0;
float gui_armorswitcher_delay = 80.0f;

// -- Macros ---------------------------------------------------
bool  gui_macros_enabled = false;
int   gui_macros_mode = 0;               // 0=Bow, 1=Fireball, 2=Gap, 3=Pot
int   gui_macros_bind = 0;
float gui_macros_switch_delay = 50.0f;
float gui_macros_use_delay = 50.0f;
bool  gui_macros_auto_switch_back = true;


bool gui_friends_enabled = true;
int  gui_friends_add_bind = VK_MBUTTON;
int  gui_friends_nearby_bind = 0;
int  gui_friends_clear_bind = 0;

std::vector<std::string> g_FriendsList;
std::vector<std::string> g_FriendUUIDs;
namespace features::friends {
    std::vector<std::string>* list = &g_FriendsList;
    std::vector<std::string>* uuids = &g_FriendUUIDs;
}

bool  gui_velo_enabled = false;
int   gui_velo_mode = 0;               // 0=Default, 1=Lag
bool  gui_velo_air_only = false;
bool  gui_velo_moving_only = false;
bool  gui_velo_weapon_only = false;
bool  gui_velo_push_back = false;
bool  gui_velo_clicking_only = false;
bool  gui_velo_universocraft_bypass = false;
float gui_velo_horizontal = 100.0f;
float gui_velo_vertical = 100.0f;
float gui_velo_chance = 100.0f;
float gui_velo_delay = 0.0f;
float gui_velo_lag_ms = 50.0f;          // Lag mode: duraciÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â³n del spike en ms

bool  gui_reach_enabled = false;
bool  gui_reach_ground_only = false;
bool  gui_reach_weapon_only = false;
bool  gui_reach_liquid_check = false;
bool  gui_reach_combo_mode = false;
float gui_reach_min_distance = 3.1f; float gui_reach_max_distance = 3.15f;
bool gui_reach_hitbox_enabled = false; float gui_reach_hitbox_size = 0.20f;
float gui_reach_chance = 100.0f;


// -- NUEVO: NoHitDelay ----------------------------------------
bool  gui_nohitdelay_enabled = false;

std::string g_CachedPlayerName = "";
GLuint g_PlayerHeadTexture = 0;
static std::mutex g_PlayerHeadMutex;
static std::vector<unsigned char> g_PlayerHeadPendingPng;
static std::string g_PlayerHeadRequestedName;
static std::string g_PlayerHeadPendingName;
static ULONGLONG g_PlayerHeadLastAttempt = 0;
static bool g_PlayerHeadLastSucceeded = false;
static std::atomic<bool> g_PlayerHeadDownloading{ false };
static HANDLE g_PlayerHeadThread = nullptr;

static bool IsValidMinecraftName(const std::string& name) {
    if (name.empty() || name.size() > 16) return false;
    for (unsigned char c : name)
        if (!(std::isalnum(c) || c == '_')) return false;
    return true;
}

static DWORD WINAPI DownloadPlayerHeadThread(LPVOID parameter) {
    std::unique_ptr<std::string> name(static_cast<std::string*>(parameter));
    std::vector<unsigned char> bytes;
    HINTERNET session = WinHttpOpen(L"pandora/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session) {
        WinHttpSetTimeouts(session, 2500, 2500, 3500, 3500);
        HINTERNET connection = WinHttpConnect(session, L"mc-heads.net",
            INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (connection) {
            std::wstring wide_name(name->begin(), name->end());
			// Fetch the original atlas only to compose the footer avatar.
            std::wstring path = L"/skin/" + wide_name + L".png";
            HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(),
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);
            if (request && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(request, nullptr)) {
                DWORD status = 0, status_size = sizeof(status);
                WinHttpQueryHeaders(request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                    WINHTTP_NO_HEADER_INDEX);
                if (status == 200) {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available) {
                        if (bytes.size() + available > 1024 * 1024) { bytes.clear(); break; }
                        const size_t old_size = bytes.size();
                        bytes.resize(old_size + available);
                        DWORD received = 0;
                        if (!WinHttpReadData(request, bytes.data() + old_size,
                                available, &received)) { bytes.clear(); break; }
                        bytes.resize(old_size + received);
                    }
                }
            }
            if (request) WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
        }
        WinHttpCloseHandle(session);
    }
    {
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        g_PlayerHeadPendingPng.swap(bytes);
        g_PlayerHeadPendingName = *name;
        g_PlayerHeadLastSucceeded = !g_PlayerHeadPendingPng.empty();
    }
    g_PlayerHeadDownloading.store(false, std::memory_order_release);
    return 0;
}

void UpdatePlayerHeadTextureOnRenderThread() {
    const std::string player_name = g_CachedPlayerName;
    static std::string displayed_name;
    if (player_name != displayed_name) {
        displayed_name = player_name;
        // Never display the previous account's face next to a new name.
        if (g_PlayerHeadTexture) {
            glDeleteTextures(1, &g_PlayerHeadTexture);
            g_PlayerHeadTexture = 0;
        }
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        g_PlayerHeadPendingPng.clear();
        g_PlayerHeadPendingName.clear();
    }

    bool should_download = false;
    const ULONGLONG now = GetTickCount64();
    {
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        should_download = IsValidMinecraftName(player_name) &&
            (player_name != g_PlayerHeadRequestedName ||
             (!g_PlayerHeadLastSucceeded && now - g_PlayerHeadLastAttempt >= 10000));
    }
    if (should_download &&
        !g_PlayerHeadDownloading.exchange(true, std::memory_order_acq_rel)) {
        if (g_PlayerHeadThread && WaitForSingleObject(g_PlayerHeadThread, 0) == WAIT_OBJECT_0) {
            CloseHandle(g_PlayerHeadThread);
            g_PlayerHeadThread = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
            g_PlayerHeadRequestedName = player_name;
            g_PlayerHeadLastAttempt = now;
            g_PlayerHeadLastSucceeded = false;
        }
        auto* argument = new std::string(player_name);
        g_PlayerHeadThread = CreateThread(nullptr, 0, DownloadPlayerHeadThread, argument, 0, nullptr);
        if (!g_PlayerHeadThread) { delete argument; g_PlayerHeadDownloading.store(false); }
    }

    std::vector<unsigned char> png;
    std::string loaded_name;
    {
        std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
        if (!g_PlayerHeadPendingPng.empty()) {
            png.swap(g_PlayerHeadPendingPng);
            loaded_name = g_PlayerHeadPendingName;
            g_PlayerHeadPendingName.clear();
        }
    }
    // A slow response from the previous account may arrive after Alt Manager
    // changed the visible name. Discard it instead of flashing the stale head.
    if (png.empty() || loaded_name != player_name) return;
    int width = 0, height = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(png.data(), (int)png.size(),
        &width, &height, &channels, 4);
    if (!pixels || width < 64 || height < 32 || width > 600 || height > 600) {
        if (pixels) stbi_image_free(pixels);
        return;
    }
    // Compose the base head and its transparent hat layer for the footer.
    std::vector<unsigned char> head_pixels(8 * 8 * 4, 0);
    if (width >= 64 && height >= 32) {
        for (int y = 0; y < 8; ++y) {
            for (int x = 0; x < 8; ++x) {
                const unsigned char* base = pixels + ((8 + y) * width + (8 + x)) * 4;
                const unsigned char* hat = pixels + ((8 + y) * width + (40 + x)) * 4;
                unsigned char* dst = head_pixels.data() + (y * 8 + x) * 4;
                const float a = hat[3] / 255.0f;
				for (int c = 0; c < 3; ++c) {
					const float composed = hat[c] * a + base[c] * (1.0f - a);
					// Lift dark skins without flattening the original colors or hat layer.
                    dst[c] = (unsigned char)std::clamp(composed, 0.0f, 255.0f);
				}
                dst[3] = 255;
            }
        }
    }
    GLuint head_texture = 0;
    glGenTextures(1, &head_texture);
    glBindTexture(GL_TEXTURE_2D, head_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, head_pixels.data());

    stbi_image_free(pixels);
    if (g_PlayerHeadTexture) glDeleteTextures(1, &g_PlayerHeadTexture);
    g_PlayerHeadTexture = head_texture;
}

void InvalidatePlayerSkinTexturesAfterContextLoss() {
    // The old GL context owns these names.  Never delete or reuse them from the
    // replacement context; request a fresh upload instead.
    g_PlayerHeadTexture = 0;
    std::lock_guard<std::mutex> lock(g_PlayerHeadMutex);
    g_PlayerHeadRequestedName.clear();
    g_PlayerHeadLastSucceeded = false;
}

static void ShutdownPlayerHeadLoader() {
    if (g_PlayerHeadThread) {
        WaitForSingleObject(g_PlayerHeadThread, 5000);
        CloseHandle(g_PlayerHeadThread);
        g_PlayerHeadThread = nullptr;
    }
}

std::string g_CachedServerIP = "";
ImFont* g_Kamerik = nullptr;
ImFont* g_BinaryPoppinsFont = nullptr;
ImFont* g_MenuSfProFont = nullptr;
ImFont* g_UIFont = nullptr;            // Poppins Bold (UI)
ImFont* g_WatermarkFont = nullptr;     // Kamerik pequeño (Watermark)
ImFont* g_ArrayListFont = nullptr;     // Kamerik grande (ArrayList)
ImFont* g_BinaryIconFont = nullptr;
ImFont* g_NametagFont = nullptr;

namespace font { extern ImFont* default_icon; }

bool  gui_watermark_enabled = true;
float gui_watermark_pos_x = 0.015f;
float gui_watermark_pos_y = 0.015f;
bool  gui_watermark_blur = true;
float gui_watermark_blur_opacity = 1.00f;
float gui_watermark_color[3] = { 0.4f, 0.9f, 0.5f }; // default green-ish to blend with white
float gui_watermark_color_b[3] = { 0.35f, 0.55f, 1.0f };
int   gui_watermark_color_mode = 1; // Single, Fade, Rainbow
bool  gui_watermark_show_player = false;
bool  gui_watermark_show_server = true;
bool  gui_watermark_show_fps = false;
bool  gui_watermark_show_name = true;
bool  gui_watermark_show_time = false;
bool  gui_watermark_background = true;
bool  gui_watermark_text_shadow = true;

bool  gui_arraylist_enabled = true;
bool  gui_arraylist_watermark = false;
bool  gui_arraylist_background = true;
bool  gui_arraylist_colorbar = false;
// removed gui_arraylist_notifications
float gui_arraylist_color[3] = { 0.51f, 0.60f, 0.92f }; // default soft blue
float gui_arraylist_wave_saturation = 0.85f;
float gui_arraylist_scale = 1.f;
float gui_arraylist_speed = 0.56f;
float gui_arraylist_pos_x = 0.99f;
float gui_arraylist_pos_y = 0.02f;
float gui_arraylist_pad_x = 2.5f;
float gui_arraylist_pad_y = 0.0f;
float gui_arraylist_radius = 0.9f;
float gui_arraylist_info_color[3] = { 0.678431f, 0.678431f, 0.678431f };
float gui_arraylist_color_b[3] = { 1.0f, 1.0f, 1.0f };
float gui_arraylist_bar_width = 2.0f;
bool  gui_arraylist_bracket_flags = true;
int   gui_arraylist_alignment = 0;
int   gui_arraylist_color_mode = 2; // Fade

int  gui_arraylist_anim_style = 0;
bool gui_arraylist_rainbow_bar = false;
bool gui_arraylist_glow = false;
bool gui_arraylist_show_info = true;
float gui_arraylist_bg_color_4[4] = { 0.0f, 0.0f, 0.0f, 0.85f };
int   gui_arraylist_font = 0; // 0 Default, 1 Modern, 2 Pixelated
bool  gui_arraylist_lowercase = true;
bool  gui_arraylist_shadows = true;
bool  gui_arraylist_bold = false;
bool  gui_arraylist_background_shadow = true;

// WhipClient Adapted Variables
bool  gui_arraylist_show_title = false;
bool  gui_arraylist_title_custom_color = false;
float gui_arraylist_title_color[3] = { 1.0f, 1.0f, 1.0f };
bool  gui_arraylist_blur = true;
float gui_arraylist_blur_opacity = 0.90f;
float gui_arraylist_flow_color[3] = { 1.0f, 1.0f, 1.0f };
float gui_arraylist_fade_color[3] = { 1.0f, 1.0f, 1.0f };

// Mapa de modulos ocultos del ArrayList: key=nombre, value=true si oculto
#include <set>
std::set<std::string> g_arraylist_hidden_modules;

// -- pandora Client ESP Settings ----------------------------------------------
bool  gui_esp_enabled = false;
int   gui_whip_esp_render_mode = 0;   // 0=2D, 1=3D
int   gui_whip_esp_mode2d = 0;        // 0=Outline, 1=Fill, 2=Both
int   gui_whip_esp_mode3d = 0;        // 0=Outline, 1=Fill, 2=Both

bool  gui_whip_esp_show_healthbar = true;
float gui_whip_esp_healthbar_bg[4] = { 0.2f, 0.2f, 0.2f, 0.8f };
float gui_whip_esp_healthbar_full[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
float gui_whip_esp_healthbar_low[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
float gui_whip_esp_healthbar_width = 3.0f;
float gui_whip_esp_healthbar_offset = 5.0f;

bool  gui_whip_esp_hide_friends = false;
float gui_whip_esp_neutral_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

float gui_whip_esp_outline2d_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float gui_whip_esp_hurt_color[4] = { 1.0f, 0.27f, 0.27f, 1.0f };
float gui_whip_esp_outline2d_width = 1.5f;
float gui_whip_esp_max_distance = 64.0f;

// -- ESP efectos visuales -----------------------------------------------------

bool  gui_nametags_enabled = false;
bool  gui_nametags_draw_health = true;
int   gui_nametags_health_format = 1;
float gui_nametags_health_segments = 10.0f;
bool  gui_nametags_show_own = false;
bool  gui_nametags_hide_vanilla = true;
bool  gui_nametags_show_equipment = true;
bool  gui_nametags_show_enchantments = true;
bool  gui_nametags_draw_distance = false;
bool  gui_nametags_draw_hurt_time = true;
bool  gui_nametags_draw_invisible = true;
bool  gui_nametags_use_fake_name = false;   // reemplaza nombre en ESP por fake_name
bool  gui_nametags_background = true;       // fondo oscuro detrÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡s del nametag ESP
char  gui_nametags_fake_name[64] = "Jugador";
float gui_nametags_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
float gui_nametags_scale = 1.30f;
bool  gui_nametags_auto_scale = true;

bool  gui_tracers_enabled = false;
bool  gui_tracers_draw_distance = true;
bool  gui_tracers_draw_hurt_time = true;
bool  gui_tracers_draw_invisible = false;
float gui_tracers_color_4[4] = { 0.2f, 0.6f, 1.0f, 1.0f };
float gui_tracers_thickness = 1.5f;



// Sprint movido a CAT_COMBAT (variable sin cambios)
bool  gui_sprint_enabled = false;
bool  gui_sprint_omni = false;

bool  gui_noslow_enabled = false;
bool  gui_noitemrelease_enabled = false;
bool  gui_noitemrelease_sword = false;
bool  gui_noitemrelease_food = false;
bool  gui_noitemrelease_bow = false;



bool gui_nojumpdelay_enabled = false;


// -- Fly -----------------------------------------------------

bool  gui_autoarmor_enabled = false;
float gui_autoarmor_delay = 100.0f;
bool  gui_autoarmor_only_better = true;



bool  gui_blink_enabled = false;
bool  gui_blink_show_path = false;
bool  gui_blink_show_timer = false;
float gui_blink_path_color[3] = { 1.0f, 1.0f, 1.0f };
float gui_blink_timer_limit = 10.0f;

// -- ESP unificado -------------------------------------------

// -- BlockHit ------------------------------------------------
bool  gui_blockhit_enabled = false;
int   gui_blockhit_mode = 2;        // 0=Manual, 1=Predict, 2=Auto, 3=Lag
bool  gui_blockhit_require_mouse_down = false;
float gui_blockhit_block_ticks = 2.0f;
float gui_blockhit_unblock_ticks = 3.0f;
float gui_blockhit_chance = 100.0f;
bool  gui_blockhit_only_sword = true;
bool  gui_blockhit_visual_only = false;

// -- FastPlace ------------------------------------------------
bool  gui_fastplace_enabled = false;
int   gui_fastplace_held_item = 0;  // 0=All, 1=Blocks, 2=Projectiles

// -- AutoTool -------------------------------------------------
bool  gui_autotool_enabled = false;
float gui_autotool_swap_delay = 0.0f;
bool  gui_autotool_swap_weapon = true;
bool  gui_autotool_instant_swap = true;
bool  gui_autotool_swap_back = false;
bool  gui_autotool_require_mouse_down = true;
bool  gui_autotool_only_sneaking = false;


// -- AutoClick extra ------------------------------------------
bool  gui_ac_break_blocks = false;
int   gui_ac_click_method = 0; // 0=Normal, 1=Jitter, 2=Butterfly

std::vector<std::string> g_ConfigList;
char  g_NewConfigName[64] = "";
bool  g_InConfigMenu = false;
int   g_SelectedConfig = 0;

// ============================================================
// CONFIG CARD SYSTEM (Drip Lite style)
// ============================================================
enum ConfigType { CFG_LEGIT = 0, CFG_SEMI_LEGIT = 1, CFG_BLATANT = 2 };
static std::map<std::string, int> g_ConfigTypeMap; // name -> ConfigType
static std::map<std::string, std::string> g_ConfigDateMap;
static int g_GuiScaleIndex = 0; // 0=Default, 1=150%, 2=175%, 3=200%
static float g_GuiScale = 1.0f;
static float g_GuiScaleTarget = 1.0f;
static bool g_BackgroundDim = true;
static bool g_ShowNewConfigModal = false;
static bool g_ModalJustOpened = false;
static char g_ModalConfigName[64] = "";
static int  g_ModalConfigType = 0;
static float g_ModalAnim = 0.0f;
static float g_ModalBgAnim = 0.0f;
// Button action animations: key -> { progress, actionType }
// actionType: 0=none, 1=load, 2=export, 3=delete
static std::map<std::string, int>   g_CfgBtnAction;
static std::string g_CfgPendingAction; // name of config being acted on


static int   g_CfgPendingType = 0;     // action type pending
static float g_CfgActionTimer = 0.0f;
static float g_NewConfigBtnAnim = 0.0f;
static std::map<std::string, float> g_CfgCardHoverAnim;
static float g_BtnImportAnim = 0.0f;
static float g_BtnResetAnim = 0.0f;

char g_LoggedUserName[64] = "pandora";
std::string g_LoggedUserExpire = "Lifetime";

// ============================================================
// ============================================================
// NOTIFICATION TOAST SYSTEM v4  Ultra Premium Minimalist
// ============================================================
struct NotificationToast {
    std::string title;
    std::string body;
    std::string tag;
    float       timer    = 0.0f;
    float       duration = 3.5f;
    float       slideY   = 0.0f;   // For vertical stacking animation
    float       alpha    = 0.0f;
    bool        leaving  = false;
};
static std::vector<NotificationToast> g_Toasts;
static constexpr int MAX_TOASTS = 6;
std::mutex g_ToastMutex;

// Shared with arraylist toasts so they can stack above system notifications
float g_system_toast_height = 0.0f;

void TriggerNotification(const char* title,
    const char* body = "",
    const char* tag  = "SYSTEM") {
    std::lock_guard<std::mutex> lock(g_ToastMutex);
    NotificationToast t;
    t.title = title;
    t.body  = body;
    t.tag   = tag;
    if (g_Toasts.size() >= MAX_TOASTS)
        g_Toasts.erase(g_Toasts.begin());
    g_Toasts.push_back(std::move(t));
}

void RenderNotifications() {
    std::lock_guard<std::mutex> lock(g_ToastMutex);
    if (g_Toasts.empty()) { g_system_toast_height = 0.0f; return; }

    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime > 0.05f ? 0.05f : io.DeltaTime;
    const float screenW = io.DisplaySize.x;
    const float screenH = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    ImFont* font = FONT_MANAGER.get_watermark_font();
    if (!font) font = ImGui::GetFont();

    ImFont* iconFont = FONT_MANAGER.get_icon_font();
    if (!iconFont) iconFont = font;

    float fsz_title = 18.f;
    float fsz_body = 14.f;
    float fsz_icon = 16.f;
    float pad_x = 16.f;
    float pad_y = 14.f;
    float icon_area = 30.f;
    float margin_x = 16.f;
    float margin_y = 16.f;
    float spacing = 8.f;
    float rounding = 10.0f;
    float bar_height = 6.0f;
    float bar_gap = 6.0f; // gap between box bottom and bar

    float line_h1 = font->CalcTextSizeA(fsz_title, FLT_MAX, 0.f, "A").y;
    float line_h2 = font->CalcTextSizeA(fsz_body, FLT_MAX, 0.f, "A").y;

    int pos_mode = features::misc::notifications::position;
    bool isTop = (pos_mode == 0 || pos_mode == 1);
    bool isLeft = (pos_mode == 0 || pos_mode == 2);

    float curTargetY = isTop ? margin_y : (screenH - margin_y);

    for (int i = (int)g_Toasts.size() - 1; i >= 0; i--) {
        auto& t = g_Toasts[i];
        t.timer += dt;

        float leaveStart = t.duration - 0.35f;
        if (t.timer >= leaveStart) t.leaving = true;

        if (!t.leaving) {
            float fi = t.timer / 0.2f;
            float want_alpha = fi < 1.f ? fi : 1.f;
            t.alpha = want_alpha + (t.alpha - want_alpha) * std::exp(-20.0f * dt);
        } else {
            float p = (t.duration - t.timer) / 0.35f;
            float want_alpha = p < 0.f ? 0.f : p;
            t.alpha = want_alpha + (t.alpha - want_alpha) * std::exp(-20.0f * dt);
        }
        float a = t.alpha;

        bool hasBody = !t.body.empty();

        // Box height (content only, bar is below with gap)
        float H_box = pad_y * 2.f + line_h1 + (hasBody ? (line_h2 + 3.f) : 0.f);
        float H_total = H_box + bar_gap + bar_height;

        if (isTop) {
            if (t.slideY == 0.0f) t.slideY = curTargetY - 12.f;
            t.slideY = curTargetY + (t.slideY - curTargetY) * std::exp(-18.0f * dt);
            curTargetY += H_total + spacing;
        } else {
            curTargetY -= H_total;
            if (t.slideY == 0.0f) t.slideY = curTargetY + 12.f;
            t.slideY = curTargetY + (t.slideY - curTargetY) * std::exp(-18.0f * dt);
            curTargetY -= spacing;
        }

        ImVec2 titleSz = font->CalcTextSizeA(fsz_title, FLT_MAX, 0.f, t.title.c_str());
        ImVec2 bodySz = hasBody ? font->CalcTextSizeA(fsz_body, FLT_MAX, 0.f, t.body.c_str()) : ImVec2(0, 0);

        float max_text_w = (titleSz.x > bodySz.x) ? titleSz.x : bodySz.x;
        float W = 270.f;
        if (max_text_w + pad_x * 2.f + icon_area > W) W = max_text_w + pad_x * 2.f + icon_area;

        float slideOffset = (1.0f - a) * (W + margin_x + 4.f);
        float x = isLeft ? (margin_x - slideOffset) : (screenW - margin_x - W + slideOffset);
        float y = t.slideY;

        ImVec2 bmin = { x, y };
        ImVec2 bmax = { x + W, y + H_box };

        // Background â€” solid black
        dl->AddRectFilled(bmin, bmax, IM_COL32(15, 15, 18, (int)(255 * a)), rounding);

        // Icon
        if (iconFont) {
            const char* icon = "\xEF\x81\xA8"; // fa-bell default
            bool brightIcon = false;
            if (t.tag == "CLOUD") icon = "\xEF\x83\x82"; // fa-pandora
            else if (t.tag == "CONFIG_CREATE") icon = "\xEF\x81\xA7"; // fa-plus
            else if (t.tag == "CONFIG_LOAD") icon = "\xEF\x80\x99";   // fa-download
            else if (t.tag == "CONFIG_SAVE") icon = "\xEF\x83\x87";   // fa-save
            else if (t.tag == "CONFIG_DELETE") icon = "\xEF\x8B\xAD"; // fa-trash-alt
            else if (t.tag == "CONFIG") icon = "\xEF\x81\xBB";        // fa-folder
            else if (t.tag == "KEYBIND") icon = "\xEF\x84\x9C"; // fa-keyboard
            else if (t.tag == "SUCCESS") icon = "\xEF\x80\x8C";
            else if (t.tag == "RESET") icon = "\xEF\x80\xA1";
            else if (t.tag == "FRIEND_ADD") { icon = "\xEF\x88\xB4"; brightIcon = true; } // fa-user-plus
            else if (t.tag == "FRIEND_REMOVE") { icon = "\xEF\x94\x83"; brightIcon = true; } // fa-user-minus
            else if (t.tag == "DESTRUCT") { icon = "\xEF\x8B\xB5"; brightIcon = true; } // fa-sign-out-alt

            ImVec2 iconSz = iconFont->CalcTextSizeA(fsz_icon, FLT_MAX, 0.0f, icon);
            float iconX = bmin.x + pad_x;
            float iconY = bmin.y + (H_box - iconSz.y) * 0.5f;
            const int iconChannel = brightIcon ? 255 : 200;
            const int iconBlue = brightIcon ? 255 : 210;
            dl->AddText(iconFont, fsz_icon, ImVec2(iconX, iconY),
                IM_COL32(iconChannel, iconChannel, iconBlue, (int)(255 * a)), icon);
        }

        // Title (bold white)
        float tx = x + pad_x + icon_area;
        float ty1 = y + pad_y;
        dl->AddText(font, fsz_title, { tx, ty1 }, IM_COL32(245, 245, 248, (int)(255 * a)), t.title.c_str());

        // Body (gray subtitle)
        if (hasBody) {
            float ty2 = ty1 + line_h1 + 2.f;
            dl->AddText(font, fsz_body, { tx, ty2 }, IM_COL32(150, 150, 158, (int)(255 * a)), t.body.c_str());
        }

        // Progress bar â€” BELOW the box with a small gap, uses bar_color from settings
        float drainProg = 1.0f - (t.timer / t.duration);
        if (drainProg < 0.f) drainProg = 0.f;
        if (drainProg > 1.f) drainProg = 1.f;
        float barY = bmax.y + bar_gap;
        float barMinX = bmin.x + 4.0f;
        float barMaxX = bmin.x + W - 4.0f;
        float barFullW = barMaxX - barMinX;
        float prog_w = barFullW * drainProg;

        // Bar track (dark)
        dl->AddRectFilled(ImVec2(barMinX, barY), ImVec2(barMaxX, barY + bar_height),
            IM_COL32(35, 35, 40, (int)(180 * a)), bar_height * 0.5f);

        // Bar fill
        if (prog_w > 0.01f) {
            float* bc = features::misc::notifications::bar_color;
            ImU32 prog_color = IM_COL32((int)(bc[0]*255), (int)(bc[1]*255), (int)(bc[2]*255), (int)(255 * a));
            dl->AddRectFilled(ImVec2(barMinX, barY), ImVec2(barMinX + prog_w, barY + bar_height),
                prog_color, bar_height * 0.5f);
        }
    }

    g_Toasts.erase(
        std::remove_if(g_Toasts.begin(), g_Toasts.end(),
            [](const NotificationToast& t) { return t.timer >= t.duration; }),
        g_Toasts.end());

    g_system_toast_height = isTop ? (curTargetY - margin_y) : ((screenH - margin_y) - curTargetY);
}

std::map<std::string, float> g_SliderAnim;
std::map<std::string, float> g_SliderFill;

#include "../userconfig/userconfig.cpp"

void ResetAllSettings() {
    gui_guicolor_custom[0] = 1.0f;
    gui_guicolor_custom[1] = 1.0f;
    gui_guicolor_custom[2] = 1.0f;
    for (int i = 0; i < 3; ++i) g_AccentColor[i] = gui_guicolor_custom[i];
    features::combat::auto_click::enabled = false;
    features::combat::aim_assist::enabled = false;
    features::combat::aim_assist::mode = 0;
    features::combat::aim_assist::clicking_only = false;
    features::combat::aim_assist::lock_target = false;
    features::combat::aim_assist::weapons_only = false;
    features::combat::aim_assist::axe_only = false;
    features::combat::aim_assist::food_only = false;
    features::combat::aim_assist::break_blocks = false;
    features::combat::aim_assist::ignore_walls = false;
    features::combat::aim_assist::ignore_invisible = false;
    features::combat::aim_assist::lock_strength = 70.0f;
    features::combat::aim_assist::flick_strength = 40.0f;
    gui_aa_speed = 10.0f; gui_aa_dist = 4.0f; gui_aa_fov = 180.0f;
    gui_aa_stick = false; gui_aa_silent = false;
    features::combat::refill::enabled = false;
    gui_reach_enabled = false; gui_reach_ground_only = false; gui_reach_weapon_only = false;
    gui_reach_liquid_check = false; gui_reach_combo_mode = false;
    gui_reach_min_distance = 3.1f; gui_reach_max_distance = 3.1f; gui_reach_hitbox_enabled = false; gui_reach_hitbox_size = 0.1f; gui_reach_chance = 100.0f;
    gui_velo_enabled = false; gui_velo_air_only = false; gui_velo_moving_only = false;
    gui_velo_weapon_only = false; gui_velo_push_back = false; gui_velo_clicking_only = false;
    gui_velo_horizontal = 100.0f; gui_velo_vertical = 100.0f; gui_velo_chance = 100.0f; gui_velo_delay = 0.0f;
    gui_nohitdelay_enabled = false;
    gui_sprint_enabled = false; gui_sprint_omni = false;
    gui_noslow_enabled = false;
    gui_noitemrelease_enabled = false; gui_noitemrelease_sword = false; gui_noitemrelease_food = false; gui_noitemrelease_bow = false;
    gui_nojumpdelay_enabled = false;
    gui_autoarmor_enabled = false; gui_autoarmor_delay = 100.0f; gui_autoarmor_only_better = true;
    gui_blink_enabled = false; gui_blink_show_path = false; gui_blink_show_timer = false;
    gui_blink_path_color[0] = 1.0f; gui_blink_path_color[1] = 1.0f; gui_blink_path_color[2] = 1.0f;
    gui_blink_timer_limit = 10.0f;
    gui_esp_enabled = false;
    gui_whip_esp_render_mode = 0; gui_whip_esp_mode2d = 0; gui_whip_esp_mode3d = 0;
    gui_whip_esp_show_healthbar = true; gui_whip_esp_hide_friends = false;
    gui_whip_esp_healthbar_width = 3.0f; gui_whip_esp_healthbar_offset = 5.0f;
    gui_whip_esp_outline2d_width = 1.5f; gui_whip_esp_max_distance = 64.0f;
    gui_whip_esp_hurt_color[0] = 1.0f; gui_whip_esp_hurt_color[1] = 0.27f;
    gui_whip_esp_hurt_color[2] = 0.27f; gui_whip_esp_hurt_color[3] = 1.0f;
    gui_whip_esp_outline2d_color[0] = 1.0f; gui_whip_esp_outline2d_color[1] = 1.0f;
    gui_whip_esp_outline2d_color[2] = 1.0f; gui_whip_esp_outline2d_color[3] = 1.0f;
    gui_whip_esp_neutral_color[0] = 1.0f; gui_whip_esp_neutral_color[1] = 1.0f;
    gui_whip_esp_neutral_color[2] = 1.0f; gui_whip_esp_neutral_color[3] = 1.0f;
    gui_nametags_enabled = false; gui_nametags_health_format = 1; gui_nametags_health_segments = 10.0f; gui_nametags_draw_health = true; gui_nametags_show_own = false; gui_nametags_hide_vanilla = true; gui_nametags_show_equipment = true; gui_nametags_show_enchantments = true; gui_nametags_draw_distance = true;
    features::visual::hit_markers::enabled = false; features::visual::hit_markers::mode = 0;
    features::visual::hit_markers::size = 10.f; features::visual::hit_markers::line_width = 2.f;
    features::visual::hit_markers::duration = 0.5f; features::visual::hit_markers::fade_out = true;
    features::visual::hit_markers::scale_animation = true; features::visual::hit_markers::scale_amount = 1.5f;
    features::visual::hit_markers::outline = true; features::visual::hit_markers::outline_width = 1.f;
    gui_nametags_draw_hurt_time = true; gui_nametags_draw_invisible = false;
    gui_nametags_scale = 1.30f; gui_nametags_auto_scale = true;
    gui_tracers_enabled = false; gui_tracers_draw_distance = true; gui_tracers_draw_hurt_time = true;
    gui_tracers_draw_invisible = false; gui_tracers_thickness = 1.5f;
    gui_arraylist_enabled = true; gui_arraylist_watermark = false; gui_arraylist_background = true;
    gui_arraylist_colorbar = false; features::misc::notifications::enabled = false;
    gui_arraylist_scale = 1.94f; gui_arraylist_speed = 0.56f;
    gui_arraylist_pos_x = 1.0f;  gui_arraylist_pos_y = 0.0f;
    gui_arraylist_pad_x = 9.0f;  gui_arraylist_pad_y = 4.0f;   gui_arraylist_radius = 0.9f;
    gui_arraylist_alignment = 0;     gui_arraylist_color_mode = 2;
    gui_arraylist_anim_style = 0;     gui_arraylist_rainbow_bar = false; gui_arraylist_glow = false; gui_arraylist_show_info = true;
    gui_arraylist_font = 0; gui_arraylist_lowercase = true; gui_arraylist_shadows = true;
    gui_arraylist_background_shadow = true; gui_arraylist_blur = true;
    gui_arraylist_bracket_flags = true;
    gui_arraylist_show_title = true;  gui_arraylist_title_custom_color = false;
    gui_arraylist_title_color[0] = 1.0f; gui_arraylist_title_color[1] = 1.0f; gui_arraylist_title_color[2] = 1.0f;
    gui_arraylist_color[0] = 0.51f; gui_arraylist_color[1] = 0.60f; gui_arraylist_color[2] = 0.92f;
    gui_arraylist_color_b[0] = 1.0f; gui_arraylist_color_b[1] = 1.0f; gui_arraylist_color_b[2] = 1.0f;
    gui_arraylist_info_color[0] = 0.678431f; gui_arraylist_info_color[1] = 0.678431f; gui_arraylist_info_color[2] = 0.678431f;
    features::misc::notifications::bar_color[0] = 1.0f;
    features::misc::notifications::bar_color[1] = 1.0f;
    features::misc::notifications::bar_color[2] = 1.0f;
    features::misc::notifications::bar_color[3] = 1.0f;
    gui_arraylist_fade_color[0] = 1.0f; gui_arraylist_fade_color[1] = 1.0f; gui_arraylist_fade_color[2] = 1.0f;
    gui_arraylist_flow_color[0] = 1.0f; gui_arraylist_flow_color[1] = 1.0f; gui_arraylist_flow_color[2] = 1.0f;
    gui_arraylist_bg_color_4[0] = 0.0f; gui_arraylist_bg_color_4[1] = 0.0f;
    gui_arraylist_bg_color_4[2] = 0.0f; gui_arraylist_bg_color_4[3] = 0.85f;
    g_arraylist_hidden_modules.clear();
    

    for (auto& mod : modules) mod.keybind = 0;
    g_KeybindMap.clear();
    g_KeybindBlockedUntil.clear();

    gui_blockhit_enabled = false; gui_blockhit_mode = 2; gui_blockhit_require_mouse_down = false; gui_blockhit_block_ticks = 2.0f; gui_blockhit_unblock_ticks = 3.0f; gui_blockhit_chance = 100.0f; gui_blockhit_only_sword = true; gui_blockhit_visual_only = false;
    gui_fastplace_enabled = false; gui_fastplace_held_item = 0;
    gui_autotool_enabled = false; gui_autotool_swap_delay = 0.0f; gui_autotool_swap_weapon = true;
    gui_autotool_instant_swap = true;  gui_autotool_swap_back = false; gui_autotool_require_mouse_down = true; gui_autotool_only_sneaking = false;
    gui_ac_break_blocks = false;
    gui_min_cps = 12.0f; gui_max_cps = 14.0f; gui_inv_cps = 20.0f; gui_aa_speed = 10.0f; gui_aa_dist = 4.0f; gui_aa_fov = 180.0f;
    gui_aa_stick = false; gui_aa_silent = false; gui_jitter = 0.5f; gui_ac_click_method = 0;
    gui_refill_delay = 80.0f; gui_refill_silent_delay = 80.0f; gui_refill_silent_ticks = 2.0f;
    gui_armorswitcher_enabled = false; gui_friends_enabled = true;
    gui_friends_add_bind = VK_MBUTTON; gui_friends_nearby_bind = 0; gui_friends_clear_bind = 0;
    features::friends::clear();
    gui_armorswitcher_kit = 0; gui_armorswitcher_bind = 0; gui_armorswitcher_delay = 80.0f;
    gui_macros_enabled = false; gui_macros_mode = 0; gui_macros_bind = 0;
    gui_macros_switch_delay = 50.0f; gui_macros_use_delay = 50.0f; gui_macros_auto_switch_back = true;
    g_SliderAnim.clear(); g_SliderFill.clear();
    TriggerNotification("All settings have been reset.", "Every module restored to its default values.", "RESET");
}

enum Category {
    CAT_COMBAT = 0,
    CAT_VISUALS,
    CAT_MOVEMENT,
    CAT_MISC,
    CAT_SETTINGS,
    // Compatibility aliases for the legacy renderer. The W interface no longer
    // exposes separate Player or Configs tabs.
    CAT_PLAYER = CAT_MISC,
    CAT_CONFIGS = CAT_SETTINGS
};
int   g_SelectedCategory = CAT_COMBAT;
// Removed legacy animation and binding state variables

// Removed ImLerp, LerpColor, GetAnim

std::string GetKeyName(int key) {
    if (key == 0) return "NONE";
    if (key >= 'A' && key <= 'Z') return std::string(1, (char)key);
    if (key >= '0' && key <= '9') return std::string(1, (char)key);
    switch (key) {
    case VK_LBUTTON: return "M1"; case VK_RBUTTON: return "M2"; case VK_MBUTTON: return "M3";
    case VK_XBUTTON1: return "M4"; case VK_XBUTTON2: return "M5";
    case VK_SHIFT: return "Shift"; case VK_CONTROL: return "Ctrl"; case VK_MENU: return "Alt";
    case VK_SPACE: return "Space"; case VK_ESCAPE: return "Esc"; case VK_RETURN: return "Enter";
    case VK_INSERT: return "Insert"; case VK_DELETE: return "Del";
    default: return "K" + std::to_string(key);
    }
}

void InitMenuData();

// ============================================================
// HOOKED WNDPROC
// ============================================================
static int s_CursorIncrements = 0;
static std::atomic<bool> s_DeferredCursorRelease{ false };

static void ReleaseMenuCursorForGameplay() {
    s_DeferredCursorRelease.store(false, std::memory_order_release);
    // ImGui may still own the mouse when the menu is closed while a control is
    // hovered/pressed. Release that capture before giving input back to LWJGL.
    if (GetCapture() == g_GameWindow)
        ReleaseCapture();

    // The menu lets the native cursor move freely. If gameplay resumes while
    // it is left away from the client centre, Minecraft interprets that old
    // absolute displacement as a fresh relative mouse delta and the camera
    // jumps by itself. Re-centre first, while the cursor is still visible.
    if (g_GameWindow && IsWindow(g_GameWindow)) {
        RECT clientRect{};
        if (GetClientRect(g_GameWindow, &clientRect)) {
            POINT centre{
                (clientRect.left + clientRect.right) / 2,
                (clientRect.top + clientRect.bottom) / 2
            };
            ClientToScreen(g_GameWindow, &centre);
            SetCursorPos(centre.x, centre.y);
        }
    }

    // Do not leave a held ImGui button active across the menu transition.
    ImGuiContext* previousContext = ImGui::GetCurrentContext();
    if (g_OurImGuiCtx) {
        ImGui::SetCurrentContext(g_OurImGuiCtx);
        ImGuiIO& io = ImGui::GetIO();
        for (bool& buttonDown : io.MouseDown)
            buttonDown = false;
        ImGui::SetCurrentContext(previousContext);
    }

    for (int i = 0; i < s_CursorIncrements; i++) ShowCursor(FALSE);
    s_CursorIncrements = 0;

    // ShowCursor changes the display counter, but Windows may keep painting the
    // last cursor shape until the next mouse message. Clear that cached shape
    // now so closing with Insert never leaves a frozen arrow on screen.
    SetCursor(nullptr);
}

static void MenuOpen(HWND hWnd) {
    ClipCursor(nullptr);
    RECT rc;
    GetClientRect(hWnd, &rc);
    POINT center = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
    ClientToScreen(hWnd, &center);
    SetCursorPos(center.x, center.y);
    // Alt Manager closes back into Minecraft's Multiplayer GUI, where Windows
    // must keep the cursor visible. Reopening it must reuse that ownership,
    // otherwise every open/restore cycle increments ShowCursor again.
    if (s_DeferredCursorRelease.exchange(false, std::memory_order_acq_rel) &&
        s_CursorIncrements > 0) {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        g_SliderAnim.clear(); g_SliderFill.clear();
        return;
    }
    // Minecraft/LWJGL can decrement Windows' cursor display counter while a
    // deferred Alt Manager reference still exists. Always verify the real
    // counter instead of trusting only our bookkeeping; every increment is
    // tracked and restored when gameplay resumes.
    int sc;
    do {
        sc = ShowCursor(TRUE);
        ++s_CursorIncrements;
    } while (sc < 0);
    s_DeferredCursorRelease.store(false, std::memory_order_release);
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    g_SliderAnim.clear(); g_SliderFill.clear();
}

static void MenuClose(bool keepCursorVisible = false) {
    g_ConfigInputActive = false;  // liberar bloqueo de teclado al cerrar el menu
    // Config profiles are explicit snapshots. Closing the menu must never
    // overwrite the last manually saved state.
    // Closing the regular menu returns to gameplay and must release the cursor.
    // Closing Alt Manager returns to Minecraft's multiplayer GUI, where the
    // cursor must remain visible. Keep the same native cursor reference instead
    // of replacing it with a second software cursor.
    if (!keepCursorVisible) {
        ReleaseMenuCursorForGameplay();
    } else {
        s_DeferredCursorRelease.store(true, std::memory_order_release);
        ClipCursor(nullptr);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
}

static LRESULT WINAPI HookedWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_Running) {
        if (msg == WM_NULL) {
            WNDPROC orig = nullptr;
            {
                std::lock_guard<std::recursive_mutex> imguiContextGuard(hooks::render_mutex);
                if (g_OrigWndProc) {
                    orig = g_OrigWndProc;
                    SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)orig);
                    g_OrigWndProc = nullptr;
                }
            }
            if (orig) return CallWindowProc(orig, hWnd, msg, wParam, lParam);
        }
        
        // Ensure ALL input and ImGui processing is completely bypassed during unload.
        return g_OrigWndProc ? CallWindowProc(g_OrigWndProc, hWnd, msg, wParam, lParam) : DefWindowProc(hWnd, msg, wParam, lParam);
    }

    // ImGui uses a process-global current-context pointer. Serialize all window
    // messages with render/reinitialization so F11 cannot dispatch into a freed context.
    std::lock_guard<std::recursive_mutex> imguiContextGuard(hooks::render_mutex);
    extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

    // Losing focus starts the exact same reverse animation as Insert. Keep the
    // native cursor visible for the application that just received focus, but
    // stop ImGui from drawing its own cursor immediately.
    if ((msg == WM_KILLFOCUS ||
         (msg == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) ||
         (msg == WM_ACTIVATEAPP && wParam == FALSE)) && g_MenuVisible) {
        g_MenuVisible = false;
        g_AltManagerMode = false;
        MenuClose(true);
    }

    // If Minecraft regains focus after the focus-triggered close, restore its
    // gameplay cursor state without waiting for camera movement.
    if (msg == WM_SETFOCUS && !g_MenuVisible && s_CursorIncrements > 0) {
        ReleaseMenuCursorForGameplay();
    }

    // Evitar crasheos al presionar ALT (previene que Windows pause el hilo principal)
    if (msg == WM_SYSCOMMAND && wParam == SC_KEYMENU) {
        return 0;
    }

    // Legacy binding mouse capture removed here.

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        const UINT keyScanCode = (UINT)((lParam >> 16) & 0xFF);
        const bool rightShiftPressed =
            (int)wParam == VK_RSHIFT ||
            ((int)wParam == VK_SHIFT && keyScanCode == 0x36);
        // Capture the next real key event instead of polling every frame.
        // Legacy binding code removed here.
        // Right Shift is reserved for the Alt Manager and is accepted only
        // while Minecraft is showing its multiplayer/server-selection screen.
        if (rightShiftPressed && (lParam & (1LL << 30)) == 0 &&
            g_OnMultiplayerScreen.load(std::memory_order_acquire)) {
            if (!g_MenuVisible) {
                g_AltManagerMode = true;
                g_MenuVisible = true;
                MenuOpen(hWnd);
            }
            return 0;
        }
        static ULONGLONG lastMenuToggle = 0;
        const ULONGLONG menuToggleNow = GetTickCount64();
        if ((int)wParam == g_MenuKey && (lParam & (1LL << 30)) == 0 &&
            !g_ChatOpen.load(std::memory_order_acquire) &&
            menuToggleNow - lastMenuToggle >= 220) {
            lastMenuToggle = menuToggleNow;
            if (!g_MenuVisible)
                g_AltManagerMode = false;
            g_MenuVisible = !g_MenuVisible;
            if (g_MenuVisible) MenuOpen(hWnd);
            else               MenuClose();
            return 0;
        }
        if (wParam == VK_ESCAPE && g_MenuVisible && g_AltManagerMode) {
            g_MenuVisible = false;
            g_AltManagerMode = false;
            g_MenuOpenAnim = 0.0f;
            MenuClose(true);
            return 0;
        }
        if (wParam == VK_ESCAPE && g_MenuVisible) {
            bool editingText = g_ConfigInputActive;
            ImGuiContext* previousContext = ImGui::GetCurrentContext();
            if (hooks::g_GameImGuiContext) {
                ImGui::SetCurrentContext(hooks::g_GameImGuiContext);
                editingText = editingText || ImGui::GetIO().WantTextInput;
            }
            ImGui::SetCurrentContext(previousContext);
            if (editingText) {
                if (hooks::g_GameImGuiContext) {
                    ImGui::SetCurrentContext(hooks::g_GameImGuiContext);
                    ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
                    ImGui::SetCurrentContext(previousContext);
                }
                return 0;
            }
            g_MenuVisible = false;
            MenuClose();
            return 0;
        }
    }
    if (g_MenuVisible || g_AltManagerMode) {
        if (msg == WM_SETCURSOR) { SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE; }
        
        std::lock_guard<std::recursive_mutex> lock(hooks::render_mutex);
        
        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
            msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP ||
            msg == WM_MOUSEMOVE || msg == WM_INPUT) {
            ImGuiContext* prev = ImGui::GetCurrentContext();
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(g_OurImGuiCtx);
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(prev);
            return 0;
        }
        if (msg == WM_MOUSEWHEEL) {
            ImGuiContext* prev = ImGui::GetCurrentContext();
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(g_OurImGuiCtx);
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            if (g_OurImGuiCtx) ImGui::SetCurrentContext(prev);
            return 0;
        }
        // Bloquear teclado del juego cuando el menu esta abierto,
        // EXCEPTO las teclas de movimiento para que el jugador
        // pueda seguir moviendose con el menu visible.
        // EXCEPCION: si el InputText de config tiene foco, bloquear TODO
        // para que el usuario pueda escribir sin mover el personaje.
        if (msg == WM_CHAR || msg == WM_KEYDOWN || msg == WM_KEYUP ||
            msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
            if (g_ConfigInputActive || g_AltManagerMode) {
                // InputText activo: enviar WM_CHAR a NUESTRO contexto ImGui (no el del juego)
                ImGuiContext* prev = ImGui::GetCurrentContext();
                if (g_OurImGuiCtx) ImGui::SetCurrentContext(g_OurImGuiCtx);
                ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
                if (g_OurImGuiCtx) ImGui::SetCurrentContext(prev);
                return 0; // bloquear todo al juego
            }
            // Caso normal: usar contexto del juego si estÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ disponible
            ImGuiContext* prev = ImGui::GetCurrentContext();
            if (hooks::g_GameImGuiContext)
                ImGui::SetCurrentContext(hooks::g_GameImGuiContext);
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
            ImGui::SetCurrentContext(prev);
            bool isMovement = (wParam == 'W' || wParam == 'A' || wParam == 'S' || wParam == 'D' ||
                wParam == VK_SPACE || wParam == VK_SHIFT || wParam == VK_CONTROL ||
                wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
                wParam == VK_LCONTROL || wParam == VK_RCONTROL);
            if (isMovement)
                return g_OrigWndProc ? CallWindowProcA(g_OrigWndProc, hWnd, msg, wParam, lParam) : DefWindowProcA(hWnd, msg, wParam, lParam);
            return 0;
        }
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 1;
    }
    return g_OrigWndProc ? CallWindowProcA(g_OrigWndProc, hWnd, msg, wParam, lParam) : DefWindowProcA(hWnd, msg, wParam, lParam);
}

// ============================================================
// INIT IN-GAME IMGUI
// ============================================================
void InitInGameImGui(HWND hwnd) {
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.FontGlobalScale = 1.0f;
    io.Fonts->Clear();

    // ============================================================
    // INICIALIZAR FONT MANAGER
    // ============================================================
    FONT_MANAGER.initialize(io, myModule);

    // Establecer la fuente por defecto (Poppins Bold)
    io.FontDefault = FONT_MANAGER.get_default();

    // ============================================================
    // ASIGNAR FUENTES A VARIABLES GLOBALES
    // ============================================================
    g_UIFont = FONT_MANAGER.get_ui_font();              // Poppins Bold
    g_WatermarkFont = FONT_MANAGER.get_watermark_font(); // Kamerik pequeño
    g_ArrayListFont = FONT_MANAGER.get_arraylist_font(); // Kamerik grande
    g_BinaryIconFont = FONT_MANAGER.get_icon_font();    // Font Awesome
    g_NametagFont = FONT_MANAGER.get_ui_font();         // <--- Poppins Bold para nametags

    // Para compatibilidad con código existente
    g_Kamerik = FONT_MANAGER.get_arraylist_font();
    g_BinaryPoppinsFont = FONT_MANAGER.get_ui_font();
    g_MenuSfProFont = FONT_MANAGER.get_ui_font();

    io.FontDefault = FONT_MANAGER.get_default();

    // ============================================================
    // CONFIGURAR ESTILO (igual que antes)
    // ============================================================
    ImGuiStyle& style = ImGui::GetStyle();
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;
    style.CurveTessellationTol = 0.5f;
    style.CircleTessellationMaxError = 0.3f;
    style.WindowRounding = 10.f; style.ChildRounding = 6.f;  style.FrameRounding = 5.f;
    style.PopupRounding = 6.f;  style.ScrollbarRounding = 6.f; style.GrabRounding = 6.f;
    style.WindowBorderSize = 0.f; style.ChildBorderSize = 0.f; style.FrameBorderSize = 0.f;
    style.ItemSpacing = ImVec2(8, 10); style.ItemInnerSpacing = ImVec2(6, 4);
    style.WindowPadding = ImVec2(0, 0);  style.FramePadding = ImVec2(10, 6);
    style.ScrollbarSize = 4.f;           style.GrabMinSize = 4.f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.032f, 0.035f, 0.043f, 0.98f);
    c[ImGuiCol_ChildBg] = ImVec4(0.045f, 0.048f, 0.058f, 0.98f);
    c[ImGuiCol_PopupBg] = ImVec4(0.055f, 0.058f, 0.068f, 1.f);
    c[ImGuiCol_Border] = ImVec4(0.11f, 0.115f, 0.13f, 1.f);
    c[ImGuiCol_FrameBg] = ImVec4(0.050f, 0.054f, 0.066f, 1.f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.078f, 0.082f, 0.098f, 1.f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.095f, 0.10f, 0.12f, 1.f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.04f, 0.04f, 0.05f, 1.f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.22f, 1.f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 0.6f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
    c[ImGuiCol_CheckMark] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
    c[ImGuiCol_SliderGrab] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.98f, 0.99f, 1.f);
    c[ImGuiCol_Button] = ImVec4(0.050f, 0.052f, 0.062f, 1.f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.080f, 0.083f, 0.098f, 1.f);
    c[ImGuiCol_ButtonActive] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 0.5f);
    c[ImGuiCol_Header] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.075f, 0.080f, 0.095f, 1.f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.095f, 0.10f, 0.12f, 1.f);
    c[ImGuiCol_Separator] = ImVec4(0.27f, 0.27f, 0.30f, 1.f);
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.92f, 1.f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.38f, 0.38f, 0.43f, 1.f);
#if IMGUI_VERSION_NUM >= 19200
    c[ImGuiCol_InputTextCursor] = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.f);
#endif
    // Only hook WndProc if we haven't already (prevents infinite recursion on F11 reinit)
    if (!g_OrigWndProc) {
        g_OrigWndProc = (WNDPROC)SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
    }
    g_render->setup(); // W_AUTHENTIC_SETUP
    g_OurImGuiCtx = ImGui::GetCurrentContext(); // guardar para InputText

    static bool s_MenuDataInited = false;
    if (!s_MenuDataInited) {
        InitMenuData();
        ReloadConfigsFromDisk();
        LoadConfigTypes();
        s_MenuDataInited = true;
    }

    g_ImGuiReady = true;
}

// ============================================================
// PROCESS KEYBINDS
// ============================================================
void ProcessKeybinds() {
    // Module binds are local to Minecraft. GetAsyncKeyState is global, so
    // without this guard a bind pressed in Explorer, Discord, etc. toggles a
    // module in the background. Keep the edge state synchronized while the
    // game is unfocused so holding a key during Alt+Tab cannot trigger it when
    // focus returns either.
    const HWND foregroundWindow = GetForegroundWindow();
    const bool gameHasFocus = g_GameWindow && IsWindow(g_GameWindow) &&
        (foregroundWindow == g_GameWindow ||
         GetAncestor(foregroundWindow, GA_ROOT) == GetAncestor(g_GameWindow, GA_ROOT));
    if (!gameHasFocus) {
        for (const auto& mod : modules) {
            if (mod.keybind != 0)
                keyStates[mod.keybind] = (GetAsyncKeyState(mod.keybind) & 0x8000) != 0;
        }
        return;
    }

    // The click that opens a bind editor must be released before mouse buttons
    // become eligible as the new bind. Keyboard events remain immediately valid.
    if (g_IsBinding && g_BindingPtr && !g_BindMouseArmed) {
        const bool anyMouseDown =
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0 ||
            (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
        if (!anyMouseDown) g_BindMouseArmed = true;
    } else if (!g_IsBinding) {
        g_BindMouseArmed = false;
    }

    // g_PlayerInGui se actualiza desde el hilo principal (get_time) donde JNI es valido
    if (!g_MenuVisible && !g_PlayerInGui) {
        std::vector<int> checkedKeys;
        for (auto& mod : modules) {
            if (mod.keybind != 0 && mod.enabledPtr) {
                bool pressed = (GetAsyncKeyState(mod.keybind) & 0x8000) != 0;
                const auto blocked = g_KeybindBlockedUntil.find(mod.name);
                if (blocked != g_KeybindBlockedUntil.end()) {
                    if (GetTickCount64() < blocked->second) {
                        // Keep the edge state synchronized while the confirmation
                        // toast is visible. Holding the new key cannot trigger the
                        // module as soon as the toast finishes; it must be released
                        // and pressed again.
                        checkedKeys.push_back(mod.keybind);
                        continue;
                    }
                    g_KeybindBlockedUntil.erase(blocked);
                }
                if (pressed && !keyStates[mod.keybind]) {
                    *mod.enabledPtr = !(*mod.enabledPtr);
                    extern void PushModuleToast(const std::string& name, bool state, int keybind);
                    PushModuleToast(mod.name, *mod.enabledPtr, mod.keybind);
                }
                checkedKeys.push_back(mod.keybind);
            }
        }
        for (int k : checkedKeys) {
            keyStates[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
        }
    }
}

// Legacy rendering widgets removed here

// ============================================================
// INIT MENU DATA
// ============================================================
// Custom dropdown/combo widget matching the client's visual style â€” with smooth slide animation
bool CustomDropdown(const char* label, int* current, const std::vector<std::string>& items) {
    // --- Per-dropdown animation state ---
    struct DDState { bool open = false; float anim = 0.f; };
    static std::unordered_map<ImGuiID, DDState> s_states;
    static std::unordered_map<ImGuiID, float> optionMotion;
    // Make buttonId globally unique by appending the pointer address
    const std::string buttonId = std::string("##dd_") + label + "_" + std::to_string((uintptr_t)current);
    const ImGuiID stateId = ImGui::GetID(buttonId.c_str());
    DDState& st = s_states[stateId];

    float dt = ImGui::GetIO().DeltaTime;
    const float target = st.open ? 1.0f : 0.0f;
    const float step = ImClamp(dt * 6.5f, 0.0f, 1.0f);
    st.anim = target > st.anim ? ImMin(target, st.anim + step)
                               : ImMax(target, st.anim - step);

    ImGui::SetCursorPosX(28);
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x - 28.0f;
    float height = 28.0f;
    float alpha = g_PanelAnim;

    bool clicked = ImGui::InvisibleButton(buttonId.c_str(), ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImU32 acR = (int)(g_AccentColor[0] * 255), acG = (int)(g_AccentColor[1] * 255), acB = (int)(g_AccentColor[2] * 255);

    // Main bar background
    draw->AddRectFilled(p, ImVec2(p.x + width, p.y + height),
        IM_COL32(31, 32, 36, (int)(250 * alpha)), 6.0f);
    draw->AddRect(p, ImVec2(p.x + width, p.y + height),
        (hovered || st.open) ? IM_COL32(88, 89, 97, (int)(235 * alpha))
                             : IM_COL32(61, 62, 69, (int)(225 * alpha)),
        6.0f, 0, 1.0f);

    // Label (left)
    ImVec2 lblSz = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(p.x + 10, p.y + (height - lblSz.y) * 0.5f),
        IM_COL32(155, 156, 166, (int)(235 * alpha)), label);

    // Selected value (right)
    const char* sel = (*current >= 0 && *current < (int)items.size()) ? items[*current].c_str() : "---";
    ImVec2 selSz = ImGui::CalcTextSize(sel);
    draw->AddText(ImVec2(p.x + width - selSz.x - 24, p.y + (height - selSz.y) * 0.5f),
        IM_COL32(224, 224, 228, (int)(245 * alpha)), sel);

    // Animated arrow (rotates smoothly)
    float arrowRot = st.anim; // 0 = down, 1 = up
    const char* arrowChar = arrowRot > 0.5f ? "^" : "v";
    draw->AddText(ImVec2(p.x + width - 14, p.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
        IM_COL32(132, 132, 140, (int)(225 * alpha)), arrowChar);

    if (clicked) st.open = !st.open;

    // --- Animated inline dropdown list ---
    bool changed = false;
    if (st.anim > 0.01f) {
        const float easedAnim = st.anim * st.anim * (3.0f - 2.0f * st.anim);
        float itemH = 26.f;
        float fullH = itemH * (float)items.size() + 8.f;
        float curH = fullH * easedAnim;
        float ddY = p.y + height + 2.f;

        // Clip the dropdown area
        draw->PushClipRect(ImVec2(p.x, ddY), ImVec2(p.x + width, ddY + curH), true);

        // Background
        draw->AddRectFilled(ImVec2(p.x, ddY), ImVec2(p.x + width, ddY + fullH),
            IM_COL32(27, 28, 32, (int)(252 * alpha * easedAnim)), 6.0f);
        draw->AddRect(ImVec2(p.x, ddY), ImVec2(p.x + width, ddY + fullH),
            IM_COL32(64, 65, 72, (int)(230 * alpha * easedAnim)), 6.0f, 0, 1.0f);

        ImVec2 mousePos = ImGui::GetIO().MousePos;
        for (int i = 0; i < (int)items.size(); i++) {
            float iy = ddY + 4.f + itemH * (float)i;
            bool isSel = (*current == i);
            // Fix: ensure the item is actually visible within the animating clipped bounds
            bool itemHov = (mousePos.x >= p.x && mousePos.x <= p.x + width &&
                           mousePos.y >= iy && mousePos.y <= iy + itemH &&
                           mousePos.y <= ddY + curH);

            if (itemHov) {
                draw->AddRectFilled(ImVec2(p.x + 4, iy), ImVec2(p.x + width - 4, iy + itemH),
                    IM_COL32(255, 255, 255, (int)(14 * alpha * st.anim)), 4.f);
            }

            ImU32 txtCol = isSel
                ? IM_COL32(239, 239, 242, (int)(255 * alpha * st.anim))
                : IM_COL32(166, 166, 174, (int)(230 * alpha * st.anim));
            
            ImVec2 tSz = ImGui::CalcTextSize(items[i].c_str());
            const ImGuiID motionId = stateId ^ (0x9E3779B9u + (ImGuiID)i * 0x85EBCA6Bu); float& motion = optionMotion[motionId]; motion = ImLerp(motion, (itemHov || isSel) ? 1.0f : 0.0f, ImClamp(dt * 12.0f, 0.0f, 1.0f)); draw->AddText(ImVec2(p.x + 14 + motion * 8.0f, iy + (itemH - tSz.y) * 0.5f), txtCol, items[i].c_str());

            // A thin accent line is enough to show the selected item.
            if (isSel) {
                draw->AddRectFilled(
                    ImVec2(p.x + 5.0f, iy + 5.0f),
                    ImVec2(p.x + 7.0f, iy + itemH - 5.0f),
                    IM_COL32(acR, acG, acB, (int)(210 * alpha * st.anim)), 1.0f);
            }

            if (itemHov && ImGui::IsMouseClicked(0)) {
                *current = i;
                changed = true;
                st.open = false;
            }
        }
        draw->PopClipRect();

        // Reserve space so elements below get pushed down smoothly
        ImGui::SetCursorPosX(28);
        ImGui::Dummy(ImVec2(width, curH));
    } else {
        ImGui::Dummy(ImVec2(0, 2));
    }

    // Close when clicking outside
    if (st.open && ImGui::IsMouseClicked(0) && !hovered) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        float ddY = p.y + height + 2.f;
        float fullH = 26.f * (float)items.size() + 8.f;
        if (!(mousePos.x >= p.x && mousePos.x <= p.x + width &&
              mousePos.y >= ddY && mousePos.y <= ddY + fullH * st.anim)) {
            st.open = false;
        }
    }

    return changed;
}



void SelectFirstModuleInCategory(int category) {
    g_SelectedMod = -1;
    for (int i = 0; i < (int)modules.size(); ++i) {
        if (modules[i].category == category && !modules[i].hidden) {
            g_SelectedMod = i;
            break;
        }
    }
}

static void DrawUnifiedCheckbox(ImDrawList* draw, const ImVec2& pos, float anim, bool hovered, float alpha)
{
    // Desktop\W checkbox: compact 17px square, 3px rounding, soft shadow,
    // accent interpolation and an animated check mark.
    const float size = 17.0f;
    anim = ImClamp(anim, 0.0f, 1.0f);
    const float hover = hovered ? 1.0f : 0.0f;
    const ImVec4 offColor(0.070f + hover * 0.018f, 0.072f + hover * 0.018f,
                          0.090f + hover * 0.020f, alpha);
    const ImVec4 onColor(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], alpha);
    const ImVec4 mixed = LerpColor(offColor, onColor, anim);
    for (int layer = 3; layer >= 1; --layer) {
        const float spread = (float)layer;
        draw->AddRectFilled(ImVec2(pos.x - spread, pos.y - spread),
            ImVec2(pos.x + size + spread, pos.y + size + spread),
            IM_COL32(0, 0, 0, (int)(10.0f * alpha * (4 - layer))), 3.0f + spread);
    }
    for (int layer = 4; layer >= 1; --layer) {
        const float spread = 1.5f + layer * 1.35f;
        const float glowAlpha = alpha * anim * (5 - layer) * 6.0f;
        draw->AddRectFilled(ImVec2(pos.x - spread, pos.y - spread),
            ImVec2(pos.x + size + spread, pos.y + size + spread),
            IM_COL32((int)(g_AccentColor[0] * 255.0f), (int)(g_AccentColor[1] * 255.0f),
                     (int)(g_AccentColor[2] * 255.0f), (int)glowAlpha), 3.0f + spread);
    }
    draw->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size),
        ImGui::GetColorU32(mixed), 3.0f);
    if (anim > 0.001f) {
        draw->AddRectFilledMultiColor(ImVec2(pos.x + 1.0f, pos.y + 1.0f),
            ImVec2(pos.x + size - 1.0f, pos.y + size - 1.0f),
            IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0),
            IM_COL32(0, 0, 0, (int)(50.0f * alpha * anim)),
            IM_COL32(0, 0, 0, (int)(50.0f * alpha * anim)));
        const float reveal = ImClamp(anim * 1.30f, 0.0f, 1.0f);
        const ImU32 checkColor = IM_COL32(0, 0, 0, (int)(255.0f * alpha * anim));
        const ImVec2 a(pos.x + 4.4f, pos.y + 8.6f);
        const ImVec2 b(pos.x + 7.1f, pos.y + 11.2f);
        const ImVec2 c(pos.x + 12.8f, pos.y + 5.5f);
        const float first = ImMin(reveal * 2.0f, 1.0f);
        draw->AddLine(a, ImLerp(a, b, first), checkColor, 1.8f);
        if (reveal > 0.5f)
            draw->AddLine(b, ImLerp(b, c, (reveal - 0.5f) * 2.0f), checkColor, 1.8f);
    }
}

static void DrawUnifiedSlider(ImDrawList* draw, float x, float y, float width,
    float normalized, bool hovered, bool active, float alpha)
{
    // Desktop\W slider: a 6px filled track with shadow and a 30% vertical
    // focus expansion. Values and interaction remain owned by pandora.
    normalized = ImClamp(normalized, 0.0f, 1.0f);
    struct WSliderState { float focus = 0.0f; float hover = 0.0f; };
    static std::unordered_map<ImGuiID, WSliderState> states;
    WSliderState& state = states[ImGui::GetItemID()];
    const float step = ImClamp(ImGui::GetIO().DeltaTime * 10.0f, 0.0f, 1.0f);
    state.focus = ImLerp(state.focus, active ? 1.0f : 0.0f, step);
    state.hover = ImLerp(state.hover, hovered ? 1.0f : 0.0f, step);
    const float eased = state.focus * state.focus * (3.0f - 2.0f * state.focus);
    const float scale = 1.0f + eased * 0.30f;
    const float trackH = 6.0f;
    const float centerY = y + trackH * 0.5f;
    const float visualH = trackH * scale;
    const float top = centerY - visualH * 0.5f;
    const float bottom = centerY + visualH * 0.5f;
    const float fillX = x + width * normalized;

    draw->AddRectFilled(ImVec2(x - 2.0f, top - 2.0f),
        ImVec2(x + width + 2.0f, bottom + 2.0f),
        IM_COL32(0, 0, 0, (int)(48.0f * alpha)), 3.5f);
    ImVec4 base = c::slider::background;
    if (state.hover > 0.001f) base = LerpColor(base, c::slider::background_hov, state.hover);
    base.w *= alpha;
    draw->AddRectFilled(ImVec2(x, top), ImVec2(x + width, bottom),
        ImGui::GetColorU32(base), 2.0f * scale);
    if (fillX > x + 0.25f) {
        ImVec4 accent = c::accent; accent.w *= alpha;
        for (int layer = 4; layer >= 1; --layer) {
            const float spread = 0.8f + layer * 0.9f;
            const int glowAlpha = (int)(alpha * (5 - layer) * 8.0f);
            draw->AddRectFilled(ImVec2(x - spread, top - spread),
                ImVec2(fillX + spread, bottom + spread),
                IM_COL32((int)(g_AccentColor[0] * 255.0f), (int)(g_AccentColor[1] * 255.0f),
                         (int)(g_AccentColor[2] * 255.0f), glowAlpha), 2.0f * scale + spread);
        }
        draw->AddRectFilled(ImVec2(x, top), ImVec2(fillX, bottom),
            ImGui::GetColorU32(accent), 2.0f * scale);
        draw->AddRectFilledMultiColor(ImVec2(x, centerY), ImVec2(fillX, bottom),
            IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
            IM_COL32(0,0,0,(int)(50.0f*alpha)), IM_COL32(0,0,0,(int)(50.0f*alpha)));
    }
}

static void DrawUnifiedCheckbox(ImDrawList* draw, const ImVec2& pos,
    float anim, bool hovered, float alpha);
static void DrawUnifiedSlider(ImDrawList* draw, float x, float y, float width,
    float normalized, bool hovered, bool active, float alpha);
static bool DrawCompactColorControl(const char* id, const char* label,
    float* color, bool withAlpha, float width, float alpha)
{
    if (!color || width <= 0.0f) return false;

    ImGui::PushID(id);
    ImVec2 rowPos = ImGui::GetCursorScreenPos();
    const float rowHeight = 27.0f;
    ImGui::InvisibleButton("##color_control", ImVec2(width, rowHeight));
    const bool rowClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const bool rowHovered = ImGui::IsItemHovered();
    const ImGuiID pickerAnimId = ImGui::GetID("##color_picker_anim");

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (rowHovered) {
        draw->AddRectFilled(ImVec2(rowPos.x - 5.0f, rowPos.y),
            ImVec2(rowPos.x + width + 5.0f, rowPos.y + rowHeight),
            IM_COL32(255, 255, 255, (int)(6 * alpha)), 5.0f);
    }
    draw->AddText(ImVec2(rowPos.x, rowPos.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f),
        IM_COL32(185, 185, 194, (int)(245 * alpha)), label);

    const ImVec4 preview(color[0], color[1], color[2], withAlpha ? color[3] : 1.0f);
    const ImU32 paletteColor = ImGui::ColorConvertFloat4ToU32(preview);
    if (g_BinaryIconFont) {
        const char* paletteIcon = "\xEF\x94\xBF";
        const float iconSize = 17.0f;
        const ImVec2 iconText = g_BinaryIconFont->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, paletteIcon);
        draw->AddText(g_BinaryIconFont, iconSize,
            ImVec2(rowPos.x + width - iconText.x - 3.0f, rowPos.y + (rowHeight - iconText.y) * 0.5f),
            paletteColor, paletteIcon);
    } else {
        draw->AddRectFilled(ImVec2(rowPos.x + width - 18.0f, rowPos.y + 6.0f),
            ImVec2(rowPos.x + width - 3.0f, rowPos.y + 21.0f), paletteColor, 4.0f);
    }

    struct PickerAnimation { float value = 0.0f; };
    static std::unordered_map<ImGuiID, PickerAnimation> animations;
    PickerAnimation& popupAnim = animations[pickerAnimId];
    if (rowClicked) {
        popupAnim.value = 0.0f;
        ImGui::OpenPopup("##color_picker");
    }

    const ImVec2 pickerSize(248.0f, withAlpha ? 286.0f : 266.0f);
    ImVec2 pickerPos(rowPos.x + width - pickerSize.x, rowPos.y + rowHeight + 5.0f);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    pickerPos.x = ImClamp(pickerPos.x, 8.0f, ImMax(8.0f, display.x - pickerSize.x - 8.0f));
    if (pickerPos.y + pickerSize.y > display.y - 8.0f)
        pickerPos.y = ImMax(8.0f, rowPos.y - pickerSize.y - 5.0f);

    if (ImGui::IsPopupOpen("##color_picker"))
        popupAnim.value = ImLerp(popupAnim.value, 1.0f,
            1.0f - expf(-13.0f * ImMax(ImGui::GetIO().DeltaTime, 0.0f)));
    else
        popupAnim.value = 0.0f;

    ImGui::SetNextWindowPos(pickerPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(pickerSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImClamp(popupAnim.value, 0.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 7.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.055f, 0.055f, 0.062f, 0.995f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.13f, 0.13f, 0.15f, 0.95f));

    bool changed = false;
    if (ImGui::BeginPopup("##color_picker", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        ImDrawList* popupDraw = ImGui::GetWindowDrawList();
        const ImVec2 pp = ImGui::GetWindowPos();
        const ImVec2 ps = ImGui::GetWindowSize();
        popupDraw->AddLine(ImVec2(pp.x + 8.0f, pp.y + 27.0f),
            ImVec2(pp.x + ps.x - 8.0f, pp.y + 27.0f), IM_COL32(31, 31, 35, 230), 1.0f);
        ImGui::TextColored(ImVec4(0.86f, 0.86f, 0.89f, 1.0f), "%s", label);
        ImGui::SetCursorPosY(33.0f);
        ImGui::PushItemWidth(pickerSize.x - 16.0f);
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview |
            ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoOptions |
            ImGuiColorEditFlags_PickerHueBar;
        if (withAlpha) flags |= ImGuiColorEditFlags_AlphaBar;
        float rgbOnly[4] = { color[0], color[1], color[2], 1.0f };
        float* pickerColor = withAlpha ? color : rgbOnly;
        changed = ImGui::ColorPicker4("##picker", pickerColor, flags, nullptr);
        if (changed && !withAlpha) {
            color[0] = rgbOnly[0]; color[1] = rgbOnly[1]; color[2] = rgbOnly[2];
        }
        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    ImGui::PopID();
    return changed;
}
// Legacy rendering widgets removed here





void InitMenuData() {
    modules.clear();

    // -- Left Clicker --------------------------------------------
    Module lc = CreateMod("Left Clicker", "Automatically clicks for you at the desired CPS rate.", CAT_COMBAT, &features::combat::auto_click::enabled);
    // Replaced standard CPS slider with Custom Range Slider inside Render code
    // The CPS range variables will be drawn explicitly.
    Setting s_inv_cps; s_inv_cps.type = Setting::SLIDER; s_inv_cps.name = "Inventory CPS"; s_inv_cps.floatPtr = &gui_inv_cps; s_inv_cps.min = 1.0f; s_inv_cps.max = 25.0f; s_inv_cps.format = "%.1f";
    s_inv_cps.visibleCondition = [](){ return features::combat::auto_click::inventory_enabled; };
    lc.settings.push_back(s_inv_cps);
    lc.settings.push_back({ Setting::TOGGLE, "Break Blocks",     &gui_ac_break_blocks });
    lc.settings.push_back({ Setting::TOGGLE, "Inventory Fill",   &features::combat::auto_click::inventory_enabled });
    lc.settings.push_back({ Setting::TOGGLE, "Weapons Only",     &features::combat::auto_click::weapons_only });
    lc.settings.push_back({ Setting::TOGGLE, "Target Only",      &features::combat::auto_click::target_only });
    lc.settings.push_back({ Setting::TOGGLE, "Blockhit Sync",    &features::combat::auto_click::blockhit_sync });
    lc.settings.push_back({ Setting::TOGGLE, "Randomization",    &features::combat::auto_click::randomization });

    Setting s_drop; s_drop.type = Setting::SLIDER; s_drop.name = "Drop Chance"; s_drop.floatPtr = &features::combat::auto_click::drop_chance; s_drop.min = 0.0f; s_drop.max = 100.0f; s_drop.format = "%.1f%%";
    s_drop.visibleCondition = [](){ return features::combat::auto_click::randomization; };
    lc.settings.push_back(s_drop);

    Setting s_spike; s_spike.type = Setting::SLIDER; s_spike.name = "Spike Chance"; s_spike.floatPtr = &features::combat::auto_click::spike_chance; s_spike.min = 0.0f; s_spike.max = 100.0f; s_spike.format = "%.1f%%";
    s_spike.visibleCondition = [](){ return features::combat::auto_click::randomization; };
    lc.settings.push_back(s_spike);

    {
        Setting s_cm; s_cm.type = Setting::DROPDOWN; s_cm.name = "Click Method";
        s_cm.intPtr = &gui_ac_click_method;
        s_cm.dropdownItems = { "Stable", "Jitter", "Butterfly" };
        s_cm.dropdownValues = { 0, 1, 2 };
        lc.settings.push_back(s_cm);
    }
    
    Setting s_jit_int; s_jit_int.type = Setting::SLIDER; s_jit_int.name = "Jitter Intensity"; s_jit_int.floatPtr = &gui_jitter; s_jit_int.min = 0.1f; s_jit_int.max = 2.0f; s_jit_int.format = "%.2f";
    s_jit_int.visibleCondition = [](){ return gui_ac_click_method == 1; };
    lc.settings.push_back(s_jit_int);

    modules.push_back(lc);

    Module aa = CreateMod("Aim Assist", "Aims at targets using customized assistance modes.", CAT_COMBAT, &features::combat::aim_assist::enabled);
    {
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &features::combat::aim_assist::mode;
        s_mode.dropdownItems = { "Smooth", "Aim-Lock", "Vertical" };
        aa.settings.push_back(s_mode);
    }
    
    Setting aaSpeed{ Setting::SLIDER, "Speed", nullptr, &gui_aa_speed, 1.0f, 100.0f, "%.1f" };
    aaSpeed.visibleCondition = []() { return features::combat::aim_assist::mode == 0; };
    aa.settings.push_back(aaSpeed);

    Setting s_lock_str; s_lock_str.type = Setting::SLIDER; s_lock_str.name = "Lock Strength"; s_lock_str.floatPtr = &features::combat::aim_assist::lock_strength; s_lock_str.min = 10.0f; s_lock_str.max = 100.0f; s_lock_str.format = "%.0f%%";
    s_lock_str.visibleCondition = [](){ return features::combat::aim_assist::mode == 1; }; // Aim-Lock mode
    aa.settings.push_back(s_lock_str);

    Setting s_head_str; s_head_str.type = Setting::SLIDER; s_head_str.name = "Vertical Strength"; s_head_str.floatPtr = &features::combat::aim_assist::flick_strength; s_head_str.min = 10.0f; s_head_str.max = 100.0f; s_head_str.format = "%.0f%%";
    s_head_str.visibleCondition = [](){ return features::combat::aim_assist::mode == 2; }; // Vertical mode
    aa.settings.push_back(s_head_str);

    Setting aaDistance{ Setting::SLIDER, "Distance", nullptr, &gui_aa_dist, 1.0f, 10.0f, "%.1f" };
    aaDistance.visibleCondition = []() { return features::combat::aim_assist::mode == 0 || features::combat::aim_assist::mode == 1 || features::combat::aim_assist::mode == 2; };
    aa.settings.push_back(aaDistance);

    Setting aaFov{ Setting::SLIDER, "Field Of View", nullptr, &gui_aa_fov, 1.0f, 180.0f, "%.1f" };
    aaFov.visibleCondition = []() { return features::combat::aim_assist::mode == 0 || features::combat::aim_assist::mode == 1 || features::combat::aim_assist::mode == 2; };
    aa.settings.push_back(aaFov);

    aa.settings.push_back({ Setting::TOGGLE, "Weapons Only", &features::combat::aim_assist::weapons_only });
    aa.settings.push_back({ Setting::TOGGLE, "Axe Only",     &features::combat::aim_assist::axe_only });
    aa.settings.push_back({ Setting::TOGGLE, "Food / Potions", &features::combat::aim_assist::food_only });
    aa.settings.push_back({ Setting::TOGGLE, "Ignore Walls", &features::combat::aim_assist::ignore_walls });
    aa.settings.push_back({ Setting::TOGGLE, "Ignore Invisible", &features::combat::aim_assist::ignore_invisible });

    modules.push_back(aa);

    Module reach = CreateMod("Reach", "Extends your attack reach distance against players.", CAT_COMBAT, &gui_reach_enabled);
    reach.settings.push_back({ Setting::TOGGLE, "Only on Ground",    &gui_reach_ground_only });
    reach.settings.push_back({ Setting::TOGGLE, "Only with Weapon",  &gui_reach_weapon_only });
    reach.settings.push_back({ Setting::TOGGLE, "Liquid Check",      &gui_reach_liquid_check });
    reach.settings.push_back({ Setting::TOGGLE, "Combo Mode",        &gui_reach_combo_mode });
    reach.settings.push_back({ Setting::SLIDER, "Max Reach", nullptr, &gui_reach_max_distance, 3.0f, 6.0f,   "%.2f" });
    reach.settings.push_back({ Setting::SLIDER, "Min Reach", nullptr, &gui_reach_min_distance, 3.0f, 6.0f,   "%.2f" });
    reach.settings.push_back({ Setting::TOGGLE, "Hitbox",   &gui_reach_hitbox_enabled });
    {
        Setting hbs;
        hbs.type = Setting::SLIDER;
        hbs.name = "Hitbox Size";
        hbs.floatPtr = &gui_reach_hitbox_size;
        hbs.min = 0.1f;
        hbs.max = 1.0f;
        hbs.format = "%.2f";
        hbs.visibleCondition = [](){ return gui_reach_hitbox_enabled; };
        reach.settings.push_back(hbs);
    }
    reach.settings.push_back({ Setting::SLIDER, "Chance",   nullptr, &gui_reach_chance,   0.0f, 100.0f, "%.0f%%" });
    modules.push_back(reach);

    Module velo = CreateMod("Velocity", "Reduces or removes incoming knockback force.", CAT_COMBAT, &gui_velo_enabled);
    {
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &gui_velo_mode;
        s_mode.dropdownItems = { "Default", "Lag" };
        velo.settings.push_back(s_mode);
    }
    Setting s_vhor; s_vhor.type = Setting::SLIDER; s_vhor.name = "Horizontal %"; s_vhor.floatPtr = &gui_velo_horizontal; s_vhor.min = 0.0f; s_vhor.max = 100.0f; s_vhor.format = "%.2f";
    s_vhor.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vhor);

    Setting s_vver; s_vver.type = Setting::SLIDER; s_vver.name = "Vertical %"; s_vver.floatPtr = &gui_velo_vertical; s_vver.min = 0.0f; s_vver.max = 100.0f; s_vver.format = "%.2f";
    s_vver.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vver);

    Setting s_vdel; s_vdel.type = Setting::SLIDER; s_vdel.name = "Delay"; s_vdel.floatPtr = &gui_velo_delay; s_vdel.min = 0.0f; s_vdel.max = 9.0f; s_vdel.format = "%.2f";
    s_vdel.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vdel);

    Setting s_vcha; s_vcha.type = Setting::SLIDER; s_vcha.name = "Chance %"; s_vcha.floatPtr = &gui_velo_chance; s_vcha.min = 0.0f; s_vcha.max = 100.0f; s_vcha.format = "%.2f";
    s_vcha.visibleCondition = [](){ return gui_velo_mode == 0; };
    velo.settings.push_back(s_vcha);

    Setting s_vlag; s_vlag.type = Setting::SLIDER; s_vlag.name = "Lag Duration"; s_vlag.floatPtr = &gui_velo_lag_ms; s_vlag.min = 20.0f; s_vlag.max = 150.0f; s_vlag.format = "%.0f ms";
    s_vlag.visibleCondition = [](){ return gui_velo_mode == 1; };
    velo.settings.push_back(s_vlag);

    velo.settings.push_back({ Setting::TOGGLE, "Clicking Only", &gui_velo_clicking_only });
    velo.settings.push_back({ Setting::TOGGLE, "Weapons Only",  &gui_velo_weapon_only });
    velo.settings.push_back({ Setting::TOGGLE, "Push Back",     &gui_velo_push_back });
    modules.push_back(velo);



    // -- Sprint movido a Combat --------------------------------
    Module sprint = CreateMod("Sprint", "Automatically sprints for you at all times.", CAT_MOVEMENT, &gui_sprint_enabled);
    sprint.settings.push_back({ Setting::TOGGLE, "Omni Sprint", &gui_sprint_omni });
    modules.push_back(sprint);

    
    // -- ArmorSwitcher (Combat/Player) -- FULL MODULE
    {
        Module as = CreateMod("ArmorSwitcher", "Quickly swaps between armor sets (Diamond/Iron...).", CAT_MISC, &gui_armorswitcher_enabled);
        Setting s_kit; s_kit.type = Setting::DROPDOWN; s_kit.name = "Kit";
        s_kit.intPtr = &gui_armorswitcher_kit;
        s_kit.dropdownItems = { "Diamond", "Iron", "Gold", "Chain", "Leather" };
        as.settings.push_back(s_kit);
        as.settings.push_back({ Setting::SLIDER, "Delay (ms)", nullptr, &gui_armorswitcher_delay, 20.0f, 300.0f, "%.0f ms" });

        modules.push_back(as);
    }

    // -- Macros (Player) --------------------------------------
    {
        Module mc = CreateMod("Macros", "Automated hotbar macros for Bow, Fireball, Gaps and Pots.", CAT_MISC, &gui_macros_enabled);
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &gui_macros_mode;
        s_mode.dropdownItems = { "Bow", "Fireball", "Gap", "Pot" };
        mc.settings.push_back(s_mode);
        mc.settings.push_back({ Setting::SLIDER, "Switch Delay", nullptr, &gui_macros_switch_delay, 0.0f, 200.0f, "%.0f ms" });
        mc.settings.push_back({ Setting::SLIDER, "Use Delay",    nullptr, &gui_macros_use_delay,    0.0f, 200.0f, "%.0f ms" });
        mc.settings.push_back({ Setting::TOGGLE, "Auto Switch Back", &gui_macros_auto_switch_back });
        modules.push_back(mc);
    }

    Module refill = CreateMod("Refill", "Automatically refills your hotbar items and soups.", CAT_MISC, &features::combat::refill::enabled);
    refill.settings.push_back({ Setting::SLIDER, "Delay (ms)",    nullptr, &gui_refill_delay,        10.0f, 300.0f, "%.0f ms" });
    modules.push_back(refill);


    // -- BlockHit (Combat) -------------------------------------
    {
        Module bh = CreateMod("BlockHit", "Automatically blocks before hitting for damage reduction and kb.", CAT_COMBAT, &gui_blockhit_enabled);
        Setting s_bmode; s_bmode.type = Setting::DROPDOWN; s_bmode.name = "Mode";
        s_bmode.intPtr = &gui_blockhit_mode;
        s_bmode.dropdownItems = { "Manual", "Predict", "Auto", "Lag" };
        bh.settings.push_back(s_bmode);
        bh.settings.push_back({ Setting::TOGGLE, "Require Mouse Down", &gui_blockhit_require_mouse_down });
        modules.push_back(bh);
    }

    {
    }

    // -- pandora Client ESP ------------------------------------------------
    {
        Module esp = CreateMod("ESP", "Renders customizable 2D and 3D bounding boxes around players.", CAT_VISUALS, &gui_esp_enabled);

        Setting s_rmode; s_rmode.type = Setting::DROPDOWN; s_rmode.name = "Render Mode";
        s_rmode.intPtr = &gui_whip_esp_render_mode;
        s_rmode.dropdownItems = { "2D", "3D" };
        esp.settings.push_back(s_rmode);

        Setting s_m2d; s_m2d.type = Setting::DROPDOWN; s_m2d.name = "Mode 2D";
        s_m2d.intPtr = &gui_whip_esp_mode2d;
        s_m2d.dropdownItems = { "Outline", "Fill", "Both" };
        s_m2d.visibleCondition = []() { return gui_whip_esp_render_mode == 0; };
        esp.settings.push_back(s_m2d);

        Setting s_m3d; s_m3d.type = Setting::DROPDOWN; s_m3d.name = "Mode 3D";
        s_m3d.intPtr = &gui_whip_esp_mode3d;
        s_m3d.dropdownItems = { "Outline", "Fill", "Both" };
        s_m3d.visibleCondition = []() { return gui_whip_esp_render_mode == 1; };
        esp.settings.push_back(s_m3d);

        esp.settings.push_back({ Setting::TOGGLE, "Show Health Bar", &gui_whip_esp_show_healthbar });
        
        auto hbCond = []() -> bool { return gui_whip_esp_show_healthbar; };
        esp.settings.push_back({ Setting::COLOR4, "Health Bar Bg", nullptr, nullptr, 0.f, 0.f, "", gui_whip_esp_healthbar_bg, nullptr, {}, hbCond });
        esp.settings.push_back({ Setting::COLOR4, "Health Bar Full", nullptr, nullptr, 0.f, 0.f, "", gui_whip_esp_healthbar_full, nullptr, {}, hbCond });
        esp.settings.push_back({ Setting::COLOR4, "Health Bar Low", nullptr, nullptr, 0.f, 0.f, "", gui_whip_esp_healthbar_low, nullptr, {}, hbCond });
        esp.settings.push_back({ Setting::SLIDER, "Health Bar Width", nullptr, &gui_whip_esp_healthbar_width, 1.0f, 10.0f, "%.2f", nullptr, nullptr, {}, hbCond });
        esp.settings.push_back({ Setting::SLIDER, "Health Bar Offset", nullptr, &gui_whip_esp_healthbar_offset, 0.0f, 20.0f, "%.2f", nullptr, nullptr, {}, hbCond });

        auto o3dCond = []() -> bool { return gui_whip_esp_render_mode == 1; };
        esp.settings.push_back({ Setting::COLOR4, "3D Color", nullptr, nullptr, 0.f, 0.f, "", gui_whip_esp_neutral_color, nullptr, {}, o3dCond });
        esp.settings.push_back({ Setting::COLOR4, "Hurt Color", nullptr, nullptr, 0.f, 0.f, "", gui_whip_esp_hurt_color, nullptr, {}, o3dCond });

        auto o2dCond = []() -> bool { return gui_whip_esp_render_mode == 0 && (gui_whip_esp_mode2d == 0 || gui_whip_esp_mode2d == 2); };
        esp.settings.push_back({ Setting::COLOR4, "2D Color", nullptr, nullptr, 0.f, 0.f, "", gui_whip_esp_outline2d_color, nullptr, {}, o2dCond });
        esp.settings.push_back({ Setting::SLIDER, "2D Outline Width", nullptr, &gui_whip_esp_outline2d_width, 0.5f, 3.0f, "%.2f", nullptr, nullptr, {}, o2dCond });

        esp.settings.push_back({ Setting::SLIDER, "Max Render Distance", nullptr, &gui_whip_esp_max_distance, 16.0f, 128.0f, "%.2f" });

        modules.push_back(esp);
    }

    Module nametags = CreateMod("Nametags", "Displays enhanced custom nametags and health armor stats above players.", CAT_VISUALS, &gui_nametags_enabled);
    nametags.settings.push_back({ Setting::TOGGLE, "Show Health", &gui_nametags_draw_health });
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Health Format"; s.intPtr = &gui_nametags_health_format; s.dropdownItems = { "HP", "Hearts", "Bar" }; s.visibleCondition = [] { return gui_nametags_draw_health; }; nametags.settings.push_back(s); }
    { Setting s; s.type = Setting::SLIDER; s.name = "Segments"; s.floatPtr = &gui_nametags_health_segments; s.min = 1.f; s.max = 20.f; s.format = "%.0f"; s.visibleCondition = [] { return gui_nametags_draw_health && gui_nametags_health_format == 2; }; nametags.settings.push_back(s); }
    nametags.settings.push_back({ Setting::TOGGLE, "Show Own Nametag", &gui_nametags_show_own });
    nametags.settings.push_back({ Setting::TOGGLE, "Hide Vanilla Nametags", &gui_nametags_hide_vanilla });
    nametags.settings.push_back({ Setting::TOGGLE, "Show Equipment", &gui_nametags_show_equipment });
    { Setting s; s.type = Setting::TOGGLE; s.name = "Show Enchantments"; s.boolPtr = &gui_nametags_show_enchantments; s.visibleCondition = [] { return gui_nametags_show_equipment; }; nametags.settings.push_back(s); }
        // Draw Invisible Players se habilita automatico con el Modulo
    nametags.settings.push_back({ Setting::TOGGLE, "Background",             &gui_nametags_background });    nametags.settings.push_back({ Setting::TOGGLE, "Use Fake Name",          &gui_nametags_use_fake_name });
    nametags.settings.push_back({ Setting::COLOR4, "Color", nullptr, nullptr, 0, 0, "", gui_nametags_color });
    modules.push_back(nametags);

    Module tracers = CreateMod("Tracers", "Draws visual indicator tracer lines to every active player.", CAT_VISUALS, &gui_tracers_enabled);
    tracers.settings.push_back({ Setting::TOGGLE, "Draw Distance",          &gui_tracers_draw_distance });
        // Draw Invisible Players se habilita automatico con el Modulo
    tracers.settings.push_back({ Setting::SLIDER, "Thickness", nullptr, &gui_tracers_thickness, 0.1f, 5.0f, "%.1f" });
    tracers.settings.push_back({ Setting::COLOR4, "Color", nullptr, nullptr, 0, 0, "", gui_tracers_color_4 });
    modules.push_back(tracers);

    Module hitmarkers = CreateMod("Hit Markers", "Shows a configurable marker only when an attack actually damages a player.", CAT_VISUALS, &features::visual::hit_markers::enabled);
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Mode"; s.intPtr = &features::visual::hit_markers::mode; s.dropdownItems = { "2D", "3D" }; hitmarkers.settings.push_back(s); }
    hitmarkers.settings.push_back({ Setting::COLOR4, "Color", nullptr, nullptr, 0.f, 0.f, "", features::visual::hit_markers::color });
    hitmarkers.settings.push_back({ Setting::SLIDER, "Size", nullptr, &features::visual::hit_markers::size, 3.f, 24.f, "%.0f" });
    hitmarkers.settings.push_back({ Setting::SLIDER, "Line Width", nullptr, &features::visual::hit_markers::line_width, 0.5f, 5.f, "%.1f" });
    hitmarkers.settings.push_back({ Setting::SLIDER, "Duration", nullptr, &features::visual::hit_markers::duration, 0.1f, 2.f, "%.1fs" });
    hitmarkers.settings.push_back({ Setting::TOGGLE, "Fade Out", &features::visual::hit_markers::fade_out });
    hitmarkers.settings.push_back({ Setting::TOGGLE, "Scale Animation", &features::visual::hit_markers::scale_animation });
    { Setting s; s.type = Setting::SLIDER; s.name = "Scale Amount"; s.floatPtr = &features::visual::hit_markers::scale_amount; s.min = 0.5f; s.max = 2.5f; s.format = "%.1fx"; s.visibleCondition = [] { return features::visual::hit_markers::scale_animation; }; hitmarkers.settings.push_back(s); }
    hitmarkers.settings.push_back({ Setting::TOGGLE, "Outline", &features::visual::hit_markers::outline });
    { Setting s; s.type = Setting::COLOR4; s.name = "Outline Color"; s.colorPtr = features::visual::hit_markers::outline_color; s.visibleCondition = [] { return features::visual::hit_markers::outline; }; hitmarkers.settings.push_back(s); }
    { Setting s; s.type = Setting::SLIDER; s.name = "Outline Width"; s.floatPtr = &features::visual::hit_markers::outline_width; s.min = 0.5f; s.max = 3.f; s.format = "%.1f"; s.visibleCondition = [] { return features::visual::hit_markers::outline; }; hitmarkers.settings.push_back(s); }
    modules.push_back(hitmarkers);

    Module arr = CreateMod("Array List", "Displays a sleek minimalist HUD list of enabled modules.", CAT_VISUALS, &gui_arraylist_enabled);
    // --- General ---
    {
        Setting s_render; s_render.type = Setting::DROPDOWN; s_render.name = "Mode";
        s_render.intPtr = &gui_arraylist_color_mode;
        s_render.dropdownItems = { "Single", "Rainbow", "Fade", "Flow", "GUI Based" };
        arr.settings.push_back(s_render);
    }
    {
        Setting s; s.type = Setting::COLOR; s.name = "Color 1"; s.colorPtr = gui_arraylist_color;
        s.visibleCondition = [] { return gui_arraylist_color_mode != 4; }; // hide on GUI Based
        arr.settings.push_back(s);
    }
    {
        Setting s; s.type = Setting::COLOR; s.name = "Color 2"; s.colorPtr = gui_arraylist_color_b;
        s.visibleCondition = [] { return gui_arraylist_color_mode == 2 || gui_arraylist_color_mode == 3; };
        arr.settings.push_back(s);
    }
    arr.settings.push_back({ Setting::COLOR, "Flag Color", nullptr, nullptr, 0, 0, "", gui_arraylist_info_color });
    {
        Setting s; s.type = Setting::SLIDER; s.name = "Wave Speed"; s.floatPtr = &gui_arraylist_speed;
        s.min = 0.1f; s.max = 3.0f; s.format = "%.2f";
        s.visibleCondition = [] { return gui_arraylist_color_mode != 0 && gui_arraylist_color_mode != 4; }; // hide on Single & GUI Based
        arr.settings.push_back(s);
    }
    // --- Style ---
    // (Font dropdown removed, using default Outfit exclusively)
    // Keep module visibility reachable without scrolling to the panel bottom.
    {
        static int hideModulesIdx = 0;
        Setting s; s.type = Setting::DROPDOWN; s.name = "Hide Modules";
        s.intPtr = &hideModulesIdx;
        s.dropdownItems = { "placeholder" }; // dynamically replaced at render time
        arr.settings.push_back(s);
    }
    arr.settings.push_back({ Setting::SLIDER, "Scale", nullptr, &gui_arraylist_scale, 0.5f, 3.0f, "%.2fx" });
    arr.settings.push_back({ Setting::TOGGLE, "Background", &gui_arraylist_background });
    {
        Setting s; s.type = Setting::TOGGLE; s.name = "Shadow"; s.boolPtr = &gui_arraylist_background_shadow;
        s.visibleCondition = [] { return gui_arraylist_background; };
        arr.settings.push_back(s);
    }
    {
        Setting s; s.type = Setting::TOGGLE; s.name = "Blur"; s.boolPtr = &gui_arraylist_blur;
        s.visibleCondition = [] { return gui_arraylist_background; };
        arr.settings.push_back(s);
    }
    {
        Setting s; s.type = Setting::COLOR4; s.name = "Background Color"; s.colorPtr = gui_arraylist_bg_color_4;
        s.visibleCondition = [] { return gui_arraylist_background; };
        arr.settings.push_back(s);
    }
    arr.settings.push_back({ Setting::TOGGLE, "Side Bar", &gui_arraylist_colorbar });
    {
        Setting s; s.type = Setting::SLIDER; s.name = "Bar Width"; s.floatPtr = &gui_arraylist_bar_width;
        s.min = 0.5f; s.max = 5.0f; s.format = "%.1f";
        s.visibleCondition = [] { return gui_arraylist_colorbar; };
        arr.settings.push_back(s);
    }
    // --- Layout ---
    arr.settings.push_back({ Setting::SLIDER, "Rounding", nullptr, &gui_arraylist_radius, 0.0f, 10.0f, "%.1f" });
    arr.settings.push_back({ Setting::SLIDER, "Horizontal Spacing", nullptr, &gui_arraylist_pad_x, 0.0f, 10.0f, "%.1f" });
    arr.settings.push_back({ Setting::SLIDER, "Vertical Spacing", nullptr, &gui_arraylist_pad_y, -2.0f, 5.0f, "%.1f" });
    // --- Text ---
    arr.settings.push_back({ Setting::TOGGLE, "Text Shadows", &gui_arraylist_shadows });

    arr.settings.push_back({ Setting::TOGGLE, "Lowercase", &gui_arraylist_lowercase });
    arr.settings.push_back({ Setting::TOGGLE, "Show Flags", &gui_arraylist_show_info });
    {
        Setting s; s.type = Setting::TOGGLE; s.name = "Bracket Flags"; s.boolPtr = &gui_arraylist_bracket_flags;
        s.visibleCondition = [] { return gui_arraylist_show_info; };
        arr.settings.push_back(s);
    }
    modules.push_back(arr);


    Module wm = CreateMod("Watermark", "Displays client version and build information on screen.", CAT_VISUALS, &gui_watermark_enabled);
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Elements"; wm.settings.push_back(s); }
    { Setting s; s.type = Setting::DROPDOWN; s.name = "Color Mode"; s.intPtr = &gui_watermark_color_mode; s.dropdownItems = { "Shadow", "Gradient" }; wm.settings.push_back(s); }
    wm.settings.push_back({ Setting::TOGGLE, "Background",   &gui_watermark_background });
    wm.settings.push_back({ Setting::TOGGLE, "Text Shadow",  &gui_watermark_text_shadow });
    wm.settings.push_back({ Setting::COLOR, "Color 1", nullptr, nullptr, 0.f, 0.f, "", gui_watermark_color });
    { Setting s; s.type = Setting::COLOR; s.name = "Color 2"; s.colorPtr = gui_watermark_color_b; s.visibleCondition = [] { return gui_watermark_color_mode == 1; }; wm.settings.push_back(s); }
    modules.push_back(wm);

    // -- Movement (Sprint ya NO estÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ aqui) --------------------
    Module noSlow = CreateMod("NoSlowdown", "Removes movement deceleration while using or consuming items.", CAT_MOVEMENT, &gui_noslow_enabled);
    modules.push_back(noSlow);

    Module noItemRelease = CreateMod("NoItemRelease", "Keeps items actively used without holding right click.", CAT_MOVEMENT, &gui_noitemrelease_enabled);
    noItemRelease.settings.push_back({ Setting::TOGGLE, "Consumable",  &gui_noitemrelease_food });
    modules.push_back(noItemRelease);


    // -- FastPlace (Movement) ---------------------------------
    {
        Module fp = CreateMod("FastPlace", "Places blocks at supercharged speed with zero tick delay.", CAT_MISC, &gui_fastplace_enabled);
        Setting s_held; s_held.type = Setting::DROPDOWN; s_held.name = "Held Item";
        s_held.intPtr = &gui_fastplace_held_item;
        s_held.dropdownItems = { "All", "Blocks", "Projectiles" };
        fp.settings.push_back(s_held);
        modules.push_back(fp);
    }

    {
        Module aa = CreateMod("AutoArmor", "Automatically equips the highest defense armor in inventory.", CAT_MISC, &gui_autoarmor_enabled);
        aa.settings.push_back({ Setting::SLIDER, "Delay (ms)", nullptr, &gui_autoarmor_delay, 0.0f, 500.0f, "%.0f ms" });
        aa.settings.push_back({ Setting::TOGGLE, "Only Better",  &gui_autoarmor_only_better });
        modules.push_back(aa);
    }

    Module blink = CreateMod("Blink", "Simulates temporary network freeze to instantly teleport.", CAT_MISC, &gui_blink_enabled);
    {
        Setting s_mode; s_mode.type = Setting::DROPDOWN; s_mode.name = "Mode";
        s_mode.intPtr = &features::misc::blink::mode;
        s_mode.dropdownItems = { "Smooth", "Freeze" };
        blink.settings.push_back(s_mode);
    }
    blink.settings.push_back({ Setting::TOGGLE, "Show Path",  &gui_blink_show_path });
    blink.settings.push_back({ Setting::TOGGLE, "Show Timer", &gui_blink_show_timer });
    blink.settings.push_back({ Setting::COLOR,  "Path Color", nullptr, nullptr, 0, 0, "", gui_blink_path_color });
    blink.settings.push_back({ Setting::SLIDER, "Timer Limit (s)", nullptr, &gui_blink_timer_limit, 1.0f, 20.0f, "%.1f" });
    modules.push_back(blink);

    // Teams va después de Misc ? aparece segundo en la lista lateral de Misc
    {
        Module dr = CreateMod("Delay Remover", "Removes various game delays.", CAT_MISC, nullptr);
        dr.settings.push_back({ Setting::TOGGLE, "No Hit Delay", &gui_nohitdelay_enabled });
        dr.settings.push_back({ Setting::TOGGLE, "No Jump Delay", &gui_nojumpdelay_enabled });
        modules.push_back(dr);


        Module friends = CreateMod("Friends", "Manages players excluded by combat and visual modules.", CAT_MISC, &gui_friends_enabled);
        modules.push_back(friends);
        
    }

    // Poblar g_KeybindMap con los nombres de mÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â³dulos (para SaveConfig/LoadConfig)
    for (auto& m : modules) g_KeybindMap[m.name] = m.keybind;
    SelectFirstModuleInCategory(g_SelectedCategory);
}

// ============================================================
// RENDER MENU CONTENTS
// ============================================================
static bool g_BinaryCurrentModuleEnabled = true;

static bool RenderpandoraSlider(const char* label, float* value, float minimum, float maximum, const char* format)
{
    const float width = ImMax(100.0f, ImGui::GetWindowWidth() - 40.0f);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    char valueText[64];
    sprintf_s(valueText, format && format[0] ? format : "%.2f", *value);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddText(origin, ImGui::GetColorU32(g_BinaryCurrentModuleEnabled ? c::text::text_active : c::text::text), label);
    const ImVec2 valueSize = ImGui::CalcTextSize(valueText);
    const ImVec2 valuePos(origin.x + width - valueSize.x, origin.y);
    draw->AddText(valuePos, ImGui::GetColorU32(g_BinaryCurrentModuleEnabled ? c::text::text_active : c::text::text), valueText);

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 24.0f));
    ImGui::InvisibleButton("##pandora_slider", ImVec2(width, 22.0f));
    bool changed = false;
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float ratio = ImClamp((ImGui::GetIO().MousePos.x - origin.x) / width, 0.0f, 1.0f);
        const float next = minimum + (maximum - minimum) * ratio;
        if (*value != next) { *value = next; changed = true; }
    }
    const float ratio = maximum > minimum ? ImClamp((*value - minimum) / (maximum - minimum), 0.0f, 1.0f) : 0.0f;
    const ImGuiID sliderId = ImGui::GetItemID();
    static std::unordered_map<ImGuiID, float> animatedRatios;
    auto inserted = animatedRatios.emplace(sliderId, ratio);
    float& animatedRatio = inserted.first->second;
    const float animationSpeed = ImGui::IsItemActive() ? 24.0f : 14.0f;
    animatedRatio = ImLerp(animatedRatio, ratio,
        ImClamp(ImGui::GetIO().DeltaTime * animationSpeed, 0.0f, 1.0f));
    if (ratio <= 0.0001f || ratio >= 0.9999f)
        animatedRatio = ratio;
    DrawUnifiedSlider(draw, origin.x, origin.y + 31.0f, width, animatedRatio,
                      ImGui::IsItemHovered(), ImGui::IsItemActive(), 1.0f);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 52.0f));
    return changed;
}

static void RenderAnimatedTooltip(const char* text)
{
    struct TooltipState { float alpha = 0.0f; double lastSeen = 0.0; };
    static std::unordered_map<ImGuiID, TooltipState> states;
    TooltipState& state = states[ImGui::GetItemID()];
    const double now = ImGui::GetTime();
    if (now - state.lastSeen > 0.18) state.alpha = 0.0f;
    state.lastSeen = now;
    state.alpha = ImLerp(state.alpha, 1.0f, ImClamp(ImGui::GetIO().DeltaTime * 14.0f, 0.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * state.alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(13.0f, 9.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(text);
    ImGui::EndTooltip();
    ImGui::PopStyleVar(3);
}

static std::string BuildMultiDropdownPreview(const std::vector<std::string>& labels,
                                             const std::vector<bool>& selected,
                                             const char* emptyText,
                                             size_t maxCharacters = 30)
{
    std::string result;
    size_t selectedCount = 0;
    for (size_t i = 0; i < labels.size() && i < selected.size(); ++i) {
        if (!selected[i]) continue;
        ++selectedCount;
        const std::string candidate = result.empty() ? labels[i] : result + ", " + labels[i];
        if (candidate.size() > maxCharacters) {
            if (result.empty()) {
                result = labels[i].substr(0, maxCharacters > 3 ? maxCharacters - 3 : 0);
            }
            result += ", ...";
            return result;
        }
        result = candidate;
    }
    if (selectedCount == 0) return emptyText;
    return result;
}

static void RenderBinarySetting(Setting& setting)
{
    if (setting.visibleCondition && !setting.visibleCondition()) return;
    ImGui::PushID(&setting);
    ImGui::SetCursorPosX(20.0f);
    switch (setting.type) {
    case Setting::TOGGLE:
        if (setting.boolPtr) {
            const ImVec2 togglePos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##toggle", ImVec2(19.0f, 22.0f));
            if (ImGui::IsItemClicked()) *setting.boolPtr = !*setting.boolPtr;
            DrawUnifiedCheckbox(ImGui::GetWindowDrawList(), togglePos,
                GetAnim(setting.name.c_str(), *setting.boolPtr), ImGui::IsItemHovered(), 1.0f);
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
            ImGui::TextUnformatted(setting.name.c_str());
        }
        break;
    case Setting::SLIDER:
        if (setting.floatPtr)
            RenderpandoraSlider(setting.name.c_str(), setting.floatPtr, setting.min, setting.max,
                                setting.format && setting.format[0] ? setting.format : "%.2f");
        break;
    case Setting::COLOR:
        if (setting.colorPtr) {
            const float rowStartY = ImGui::GetCursorPosY();
            DrawCompactColorControl(setting.name.c_str(), setting.name.c_str(), setting.colorPtr,
                                    false, ImMax(60.0f, ImGui::GetContentRegionAvail().x - 16.0f), 1.0f);
            ImGui::SetCursorPosY(rowStartY + 35.0f);
        }
        break;
    case Setting::COLOR4:
        if (setting.colorPtr) {
            const float rowStartY = ImGui::GetCursorPosY();
            DrawCompactColorControl(setting.name.c_str(), setting.name.c_str(), setting.colorPtr,
                                    true, ImMax(60.0f, ImGui::GetContentRegionAvail().x - 16.0f), 1.0f);
            ImGui::SetCursorPosY(rowStartY + 35.0f);
        }
        break;
    case Setting::DROPDOWN:
        if (setting.intPtr && !setting.dropdownItems.empty()) {
            std::vector<std::string> choices = setting.dropdownItems;
            if (setting.name == "Hide Modules") {
                g_arraylist_hidden_modules.erase("Notifications");
                g_arraylist_hidden_modules.erase("Array List");
                choices.clear();
                for (const Module& candidate : modules) {
                    if (!candidate.enabledPtr || !*candidate.enabledPtr) continue;
                    if (candidate.name == "Notifications" || candidate.name == "Array List") continue;
                    choices.push_back(candidate.name == "Left Clicker" ? "Auto Clicker" : candidate.name);
                }
            }
            const bool multi = setting.name == "Hide Modules" || setting.name == "Elements";
            std::string preview;
            if (setting.name == "Hide Modules") {
                std::vector<bool> selected;
                for (const auto& choice : choices) selected.push_back(g_arraylist_hidden_modules.count(choice) > 0);
                preview = BuildMultiDropdownPreview(choices, selected, choices.empty() ? "No active modules" : "No hidden modules");
            } else if (setting.name == "Elements") {
                preview = BuildMultiDropdownPreview(choices, {
                    gui_watermark_show_name, gui_watermark_show_fps, gui_watermark_show_server,
                    gui_watermark_show_player, gui_watermark_show_time }, "No elements");
            }
            else {
                int selectedIndex = 0;
                if (!setting.dropdownValues.empty()) {
                    auto found = std::find(setting.dropdownValues.begin(), setting.dropdownValues.end(), *setting.intPtr);
                    if (found == setting.dropdownValues.end()) *setting.intPtr = setting.dropdownValues.front();
                    else selectedIndex = (int)std::distance(setting.dropdownValues.begin(), found);
                } else {
                    *setting.intPtr = ImClamp(*setting.intPtr, 0, (int)choices.size() - 1);
                    selectedIndex = *setting.intPtr;
                }
                preview = choices[selectedIndex];
            }
            ImGui::PushStyleColor(ImGuiCol_Text, g_BinaryCurrentModuleEnabled ? c::text::text_active : c::text::text);
            ImGui::TextUnformatted(setting.name.c_str());
            ImGui::PopStyleColor();
            struct DropdownAnimation {
                float hover = 0.0f;
                float open = 0.0f;
                float textAnim = 1.0f;
                int lastValue = -1;
                bool closing = false;
            };
            static std::unordered_map<ImGuiID, DropdownAnimation> dropdownAnimations;
            const float dropdownMargin = 20.0f;
            const float dropdownWidth = ImMax(60.0f, ImGui::GetWindowWidth() - dropdownMargin * 2.0f);
            ImGui::SetCursorPosX(dropdownMargin);
            const ImVec2 dropdownSize(dropdownWidth, 25.0f);
            const ImVec2 dropdownMin = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##pandora_dropdown_button", dropdownSize);
            const ImGuiID dropdownId = ImGui::GetItemID();
            const bool dropdownHovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                dropdownAnimations[dropdownId].closing = false;
                ImGui::OpenPopup("##pandora_dropdown_popup");
            }
            const bool dropdownOpen = ImGui::IsPopupOpen("##pandora_dropdown_popup");

            DropdownAnimation& dropdownAnim = dropdownAnimations[dropdownId];
            const float dropdownStep = ImClamp(ImGui::GetIO().DeltaTime * 10.0f, 0.0f, 1.0f);
            dropdownAnim.hover = ImLerp(dropdownAnim.hover, dropdownHovered ? 1.0f : 0.0f, dropdownStep);
            const bool expanding = dropdownOpen && !dropdownAnim.closing;
            const float popupTarget = expanding ? 1.0f : 0.0f;
            const float popupStep = ImClamp(ImGui::GetIO().DeltaTime * 6.5f, 0.0f, 1.0f);
            dropdownAnim.open = popupTarget > dropdownAnim.open
                ? ImMin(popupTarget, dropdownAnim.open + popupStep)
                : ImMax(popupTarget, dropdownAnim.open - popupStep);
            if (dropdownAnim.lastValue != *setting.intPtr) {
                dropdownAnim.lastValue = *setting.intPtr;
                dropdownAnim.textAnim = 0.0f;
            }
            dropdownAnim.textAnim = ImLerp(dropdownAnim.textAnim, 1.0f, ImClamp(ImGui::GetIO().DeltaTime * 10.0f, 0.0f, 1.0f));

            ImDrawList* dropdownDraw = ImGui::GetWindowDrawList();
            const ImVec2 dropdownMax(dropdownMin.x + dropdownSize.x, dropdownMin.y + dropdownSize.y);
            const ImVec4 dropdownBg = ImLerp(c::combo::background, c::combo::background_hov, dropdownAnim.hover);
            for (int shadowLayer = 3; shadowLayer >= 1; --shadowLayer) {
                const float spread = 1.0f + shadowLayer * 1.25f;
                dropdownDraw->AddRectFilled(ImVec2(dropdownMin.x - spread, dropdownMin.y - spread),
                    ImVec2(dropdownMax.x + spread, dropdownMax.y + spread),
                    IM_COL32(0, 0, 0, (4 - shadowLayer) * 10), 3.0f + spread);
            }
            dropdownDraw->AddRectFilled(dropdownMin, dropdownMax, ImGui::GetColorU32(dropdownBg), 3.0f);
            dropdownDraw->AddRect(dropdownMin, dropdownMax, ImGui::GetColorU32(ImVec4(c::combo::border.x, c::combo::border.y, c::combo::border.z, 0.25f)), 3.0f);
            dropdownDraw->PushClipRect(ImVec2(dropdownMin.x + 11.0f, dropdownMin.y),
                                       ImVec2(dropdownMax.x - 36.0f, dropdownMax.y), true);
            const ImVec4 previewTextCol(c::text::text.x, c::text::text.y, c::text::text.z, c::text::text.w * dropdownAnim.textAnim);
            dropdownDraw->AddText(ImVec2(dropdownMin.x + 11.0f,
                dropdownMin.y + (dropdownSize.y - ImGui::GetFontSize()) * 0.5f + (1.0f - dropdownAnim.textAnim) * 4.0f),
                ImGui::GetColorU32(previewTextCol), preview.c_str());
            dropdownDraw->PopClipRect();

            // W-style chevron: points right while closed and rotates left while open.
            const float arrowDirection = ImLerp(1.0f, -1.0f, dropdownAnim.open);
            const ImVec2 arrowCenter(dropdownMax.x - 15.0f, dropdownMin.y + dropdownSize.y * 0.5f);
            const ImU32 arrowColor = ImGui::GetColorU32(
                dropdownHovered || dropdownOpen ? c::accent : c::text::text);
            dropdownDraw->AddTriangleFilled(
                ImVec2(arrowCenter.x - 2.5f * arrowDirection, arrowCenter.y - 4.0f),
                ImVec2(arrowCenter.x - 2.5f * arrowDirection, arrowCenter.y + 4.0f),
                ImVec2(arrowCenter.x + 3.5f * arrowDirection, arrowCenter.y), arrowColor);

            // Size the popup from the real Selectable row height.  The old
            // hard-coded 35 px estimate left a large empty strip at the bottom.
            const float visibleChoiceCount = ImMin((float)choices.size(), 5.0f);
            const float popupRowHeight = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
            const float popupHeight = ImMax(popupRowHeight + 8.0f,
                visibleChoiceCount * popupRowHeight + 14.0f);
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            float popupY = dropdownMax.y + 4.0f;
            if (popupY + popupHeight > viewport->WorkPos.y + viewport->WorkSize.y &&
                dropdownMin.y - popupHeight - 4.0f >= viewport->WorkPos.y)
                popupY = dropdownMin.y - popupHeight - 4.0f;
            // Grow the options window from its top edge. Closing keeps the
            // popup alive while its bottom edge travels upward, so it never
            // disappears abruptly after choosing an item.
            const float slideEase = dropdownAnim.open * dropdownAnim.open *
                (3.0f - 2.0f * dropdownAnim.open);
            const float animatedPopupHeight = popupHeight;
            ImGui::SetNextWindowPos(ImVec2(dropdownMin.x, popupY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(dropdownWidth, animatedPopupHeight), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImClamp(slideEase * 1.35f, 0.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, c::combo::rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, c::combo::background);
            ImGui::PushStyleColor(ImGuiCol_Border, c::combo::border);
            if (ImGui::BeginPopup("##pandora_dropdown_popup",
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse)) {
                for (int i = 0; i < (int)choices.size(); ++i) {
                    bool selected = false;
                    if (setting.name == "Hide Modules") selected = g_arraylist_hidden_modules.count(choices[i]) > 0;
                    else if (setting.name == "Elements" && i < 5) {
                        bool* values[] = { &gui_watermark_show_name, &gui_watermark_show_fps, &gui_watermark_show_server, &gui_watermark_show_player, &gui_watermark_show_time };
                        selected = *values[i];
                    } else selected = !setting.dropdownValues.empty()
                        ? (i < (int)setting.dropdownValues.size() && *setting.intPtr == setting.dropdownValues[i])
                        : *setting.intPtr == i;

                    std::string item = choices[i];
                    if (multi) item += selected ? "  [On]" : "  [Off]";
                    if (setting.name == "Hide Modules")
                        item = choices[i] + (selected ? "  [Hidden]" : "  [Visible]");

                    ImGui::PushID(i);
                    const float optionW = ImMax(20.0f, ImGui::GetContentRegionAvail().x);
                    ImGui::InvisibleButton("##dropdown_option", ImVec2(optionW, popupRowHeight));
                    const bool optionHovered = ImGui::IsItemHovered();
                    const bool optionClicked = ImGui::IsItemClicked();
                    ImDrawList* popupDraw = ImGui::GetWindowDrawList();
                    const ImVec2 optionMin = ImGui::GetItemRectMin();
                    const ImVec2 optionMax = ImGui::GetItemRectMax();
                    if (optionHovered)
                        popupDraw->AddRectFilled(optionMin, optionMax, IM_COL32(255, 255, 255, 10), 3.0f);
                    if (selected)
                        popupDraw->AddRectFilled(ImVec2(optionMin.x + 2.0f, optionMin.y + 4.0f),
                            ImVec2(optionMin.x + 5.0f, optionMax.y - 4.0f),
                            ImGui::GetColorU32(c::accent), 2.0f);
                    popupDraw->PushClipRect(ImVec2(optionMin.x + 8.0f, optionMin.y),
                        ImVec2(optionMax.x - 5.0f, optionMax.y), true);
                    popupDraw->AddText(ImVec2(optionMin.x + 11.0f,
                        optionMin.y + (popupRowHeight - ImGui::GetFontSize()) * 0.5f),
                        ImGui::GetColorU32(selected ? c::text::text_active : c::text::text), item.c_str());
                    popupDraw->PopClipRect();

                    if (optionClicked) {
                        if (setting.name == "Hide Modules") {
                            if (selected) g_arraylist_hidden_modules.erase(choices[i]);
                            else g_arraylist_hidden_modules.insert(choices[i]);
                        } else if (setting.name == "Elements" && i < 5) {
                            bool* values[] = { &gui_watermark_show_name, &gui_watermark_show_fps, &gui_watermark_show_server, &gui_watermark_show_player, &gui_watermark_show_time };
                            *values[i] = !*values[i];
                        } else {
                            *setting.intPtr = !setting.dropdownValues.empty() && i < (int)setting.dropdownValues.size()
                                ? setting.dropdownValues[i] : i;
                            dropdownAnim.closing = true;
                        }
                    }
                    ImGui::PopID();
                }
                if (dropdownAnim.closing && dropdownAnim.open < 0.035f) {
                    dropdownAnim.closing = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // A click outside destroys an ImGui popup immediately. Keep a
            // visual, non-interactive copy for the remaining reverse animation
            // so closing uses the same height curve as opening.
            if (false && !dropdownOpen && dropdownAnim.open > 0.01f) {
                ImDrawList* closingDraw = ImGui::GetForegroundDrawList();
                const ImVec2 closingMin(dropdownMin.x, popupY);
                const ImVec2 closingMax(dropdownMin.x + dropdownWidth,
                    popupY + animatedPopupHeight);
                closingDraw->PushClipRect(closingMin, closingMax, true);
                ImVec4 closingBg = c::combo::background;
                ImVec4 closingBorder = c::combo::border;
                closingBg.w *= slideEase;
                closingBorder.w *= slideEase;
                closingDraw->AddRectFilled(closingMin,
                    ImVec2(closingMin.x + dropdownWidth, closingMin.y + popupHeight),
                    ImGui::GetColorU32(closingBg), c::combo::rounding);
                closingDraw->AddRect(closingMin,
                    ImVec2(closingMin.x + dropdownWidth, closingMin.y + popupHeight),
                    ImGui::GetColorU32(closingBorder), c::combo::rounding);
                for (int i = 0; i < (int)choices.size(); ++i) {
                    ImVec4 closingText = c::text::text;
                    closingText.w *= slideEase;
                    closingDraw->AddText(
                        ImVec2(closingMin.x + 12.0f,
                            closingMin.y + 6.0f + popupRowHeight * (float)i),
                        ImGui::GetColorU32(closingText), choices[i].c_str());
                }
                closingDraw->PopClipRect();
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
            if (dropdownAnim.open > 0.001f)
                ImGui::Dummy(ImVec2(1.0f, popupHeight * slideEase + 4.0f));
        }
        break;
    case Setting::LABEL:
        ImGui::TextUnformatted(setting.name.c_str());
        break;
    case Setting::BUTTON:
        ImGui::Button(setting.name.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 35.0f));
        break;
    }
    ImGui::PopID();
}

static bool BeginpandoraBinaryPanel(const char* title, const ImVec2& size, bool collapsible = false)
{
    (void)collapsible;
    ImGui::PushID(title);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 16.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    const bool open = ImGui::BeginChild("##panel", size, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    const ImVec2 p = ImGui::GetWindowPos();
    const ImVec2 s = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (g_MenuRevealClipActive)
        dl->PushClipRect(g_MenuRevealClipMin, g_MenuRevealClipMax, true);
    const ImVec4 panelColor(22.0f/255.0f, 21.0f/255.0f, 27.0f/255.0f, 1.0f);
    const ImVec4 headerColor(25.0f/255.0f, 24.0f/255.0f, 30.0f/255.0f, 1.0f);
    for (int layer = 4; layer >= 1; --layer) {
        const float spread = 1.5f * layer;
        dl->AddRectFilled(ImVec2(p.x - spread, p.y - spread + 2.0f), ImVec2(p.x + s.x + spread, p.y + s.y + spread + 2.0f), IM_COL32(0, 0, 0, 9 * (5 - layer)), 7.0f + spread);
    }
    dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + s.y), ImGui::GetColorU32(panelColor), 7.0f);
    dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), IM_COL32(30, 30, 30, 190), 7.0f, 0, 1.0f);
    dl->AddRectFilled(p, ImVec2(p.x + s.x, p.y + 35.0f), ImGui::GetColorU32(headerColor), 7.0f, ImDrawFlags_RoundCornersTop);
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y + 35.0f), ImVec2(p.x + s.x, p.y + 45.0f), IM_COL32(0,0,0,42), IM_COL32(0,0,0,42), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));    dl->AddText(ImVec2(p.x + 11.0f, p.y + 8.0f), IM_COL32(194, 194, 202, 255), title);
    // The header belongs to the fixed outer child. Only the nested body scrolls,
    // so controls can never move over the title or its separator.
    // Start body below the header+divider with extra breathing room.
    const float bodyTopY = 43.0f;
    ImGui::SetCursorPos(ImVec2(0.0f, bodyTopY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    const bool bodyOpen = ImGui::BeginChild("##panel_body", ImVec2(s.x, ImMax(1.0f, s.y - bodyTopY)),
        false, ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleColor();
    if (g_MenuRevealClipActive)
        ImGui::GetWindowDrawList()->PushClipRect(g_MenuRevealClipMin, g_MenuRevealClipMax, true);
    // Uniform top spacing so controls never touch the divider line.
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    return open && bodyOpen;
}

static void EndpandoraBinaryPanel()
{
    if (ImGui::GetScrollMaxY() > ImGui::GetScrollY() + 1.0f) {
        ImDrawList* bodyDraw = ImGui::GetWindowDrawList();
        const ImVec2 bp = ImGui::GetWindowPos(), bs = ImGui::GetWindowSize();
        bodyDraw->AddRectFilledMultiColor(ImVec2(bp.x, bp.y + bs.y - 28.0f), ImVec2(bp.x + bs.x, bp.y + bs.y),
            IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,105), IM_COL32(0,0,0,105));
    }
    if (g_MenuRevealClipActive)
        ImGui::GetWindowDrawList()->PopClipRect();
    ImGui::EndChild();
    if (g_MenuRevealClipActive)
        ImGui::GetWindowDrawList()->PopClipRect();
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

static void RenderpandoraBind(Module& module)
{
    std::string text = (g_IsBinding && g_BindingPtr == &module.keybind) ? "Press key" : GetKeyName(module.keybind);
    if (text == "NONE") text = "None";
    const float width = ImClamp(ImGui::CalcTextSize(text.c_str()).x + 20.0f, 58.0f, 110.0f);
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - width - 20.0f);
    if (ImGui::Button(text.c_str(), ImVec2(width, 26.0f))) {
        g_IsBinding = true;
        g_BindingPtr = &module.keybind;
        g_BindMouseArmed = false;
    }
}

static void RenderpandoraRawBind(const char* label, int* bind)
{
    ImGui::SetCursorPosX(20.0f);
    const ImVec2 rowPos = ImGui::GetCursorScreenPos();
    ImGui::TextUnformatted(label);
    std::string text = (g_IsBinding && g_BindingPtr == bind) ? "Press key" : GetKeyName(*bind);
    if (text == "NONE") text = "None";
    const float buttonWidth = 90.0f;
    const float buttonX = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 20.0f - buttonWidth;
    ImGui::SetCursorScreenPos(ImVec2(buttonX, rowPos.y - 4.0f));
    ImGui::PushID(bind);
    if (ImGui::Button(text.c_str(), ImVec2(buttonWidth, 28.0f))) {
        g_IsBinding = true;
        g_BindingPtr = bind;
        g_BindMouseArmed = false;
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(ImVec2(rowPos.x, rowPos.y + 34.0f));
}

static bool IspandoraBinarySettingVisible(const Module& module, const Setting& setting)
{
    if (setting.visibleCondition && !setting.visibleCondition()) return false;

    return true;
}

static void RenderBinaryCpsRange(float width)
{
    const float rightPad = 8.0f;
    ImGui::SetCursorPosX(20.0f);
    const ImVec2 labelPos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleColor(ImGuiCol_Text, g_BinaryCurrentModuleEnabled ? c::text::text_active : c::text::text);
    ImGui::TextUnformatted("CPS Range");
    ImGui::PopStyleColor();

    // Values live on the label row; the range track gets the complete row
    // below, matching the full-width Inventory CPS slider.
    const ImVec2 p(labelPos.x, labelPos.y + 28.0f);
    const float panelRight = ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 20.0f;
    const float availableW = ImMax(250.0f, panelRight - p.x - rightPad);
    const float barW = ImMax(110.0f, ImMin(width, availableW));
    const float lo = 1.0f, hi = 25.0f;
    float minT = ImClamp((gui_min_cps - lo) / (hi - lo), 0.0f, 1.0f);
    float maxT = ImClamp((gui_max_cps - lo) / (hi - lo), 0.0f, 1.0f);

    gui_min_cps = ImClamp(gui_min_cps, lo, hi);
    gui_max_cps = ImClamp(gui_max_cps, gui_min_cps, hi);

    char rangeText[40]{};
    snprintf(rangeText, sizeof(rangeText), "%.1f - %.1f", gui_min_cps, gui_max_cps);
    const ImVec2 rangeTextSize = ImGui::CalcTextSize(rangeText);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(panelRight - rightPad - rangeTextSize.x, labelPos.y),
        ImGui::GetColorU32(g_BinaryCurrentModuleEnabled ? c::text::text_active : c::text::text),
        rangeText);

    ImGui::SetCursorScreenPos(p);
    ImGui::InvisibleButton("##binary_cps_range", ImVec2(barW, 38.0f));
    static int activeHandle = 0;
    if (ImGui::IsItemClicked()) {
        const float mouseT = ImClamp((ImGui::GetIO().MousePos.x - p.x) / barW, 0.0f, 1.0f);
        activeHandle = fabsf(minT - maxT) < 0.001f
            ? 3
            : (fabsf(mouseT - minT) <= fabsf(mouseT - maxT) ? 1 : 2);
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) activeHandle = 0;
    if (ImGui::IsItemActive() && activeHandle) {
        const float mouseT = ImClamp((ImGui::GetIO().MousePos.x - p.x) / barW, 0.0f, 1.0f);
        if (activeHandle == 3) activeHandle = mouseT < minT ? 1 : 2;
        const float value = lo + mouseT * (hi - lo);
        if (activeHandle == 1) gui_min_cps = ImMin(value, gui_max_cps);
        else gui_max_cps = ImMax(value, gui_min_cps);
        minT = ImClamp((gui_min_cps - lo) / (hi - lo), 0.0f, 1.0f);
        maxT = ImClamp((gui_max_cps - lo) / (hi - lo), 0.0f, 1.0f);
    }
    static float animatedMinT = -1.0f;
    static float animatedMaxT = -1.0f;
    if (animatedMinT < 0.0f) animatedMinT = minT;
    if (animatedMaxT < 0.0f) animatedMaxT = maxT;
    const float animationSpeed = activeHandle ? 22.0f : 13.0f;
    const float animationStep = ImClamp(ImGui::GetIO().DeltaTime * animationSpeed, 0.0f, 1.0f);
    animatedMinT = ImLerp(animatedMinT, minT, animationStep);
    animatedMaxT = ImLerp(animatedMaxT, maxT, animationStep);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    static float rangeFocus = 0.0f;
    rangeFocus = ImLerp(rangeFocus, activeHandle ? 1.0f : 0.0f,
        ImClamp(ImGui::GetIO().DeltaTime * 10.0f, 0.0f, 1.0f));
    const float easedFocus = rangeFocus * rangeFocus * (3.0f - 2.0f * rangeFocus);
    const float trackH = 6.0f * (1.0f + easedFocus * 0.30f);
    const float centerY = p.y + 15.0f;
    const float topY = centerY - trackH * 0.5f;
    const float bottomY = centerY + trackH * 0.5f;
    const float minX = p.x + barW * animatedMinT;
    const float maxX = p.x + barW * animatedMaxT;
    draw->AddRectFilled(ImVec2(p.x - 2.0f, topY - 2.0f),
        ImVec2(p.x + barW + 2.0f, bottomY + 2.0f),
        IM_COL32(0, 0, 0, 48), 3.5f);
    draw->AddRectFilled(ImVec2(p.x, topY), ImVec2(p.x + barW, bottomY),
        ImGui::GetColorU32(c::slider::background), 2.0f);
    if (maxX > minX + 0.25f) {
        for (int layer = 4; layer >= 1; --layer) {
            const float spread = 0.8f + layer * 0.9f;
            const int glowAlpha = (5 - layer) * 6;
            draw->AddRectFilled(ImVec2(minX - spread, topY - spread),
                ImVec2(maxX + spread, bottomY + spread),
                IM_COL32((int)(g_AccentColor[0] * 255.0f), (int)(g_AccentColor[1] * 255.0f),
                         (int)(g_AccentColor[2] * 255.0f), glowAlpha), 2.0f + spread);
        }
        draw->AddRectFilled(ImVec2(minX, topY), ImVec2(maxX, bottomY),
            ImGui::GetColorU32(c::accent), 2.0f);
        draw->AddRectFilledMultiColor(ImVec2(minX, centerY), ImVec2(maxX, bottomY),
            IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
            IM_COL32(0,0,0,50), IM_COL32(0,0,0,50));
    }
    // The two thin end caps identify min/max without introducing a different
    // circular-knob style. The active cap grows with the same W focus motion.
    const float capBase = 3.0f;
    const float minCap = capBase + (activeHandle == 1 ? easedFocus * 2.0f : 0.0f);
    const float maxCap = capBase + (activeHandle == 2 ? easedFocus * 2.0f : 0.0f);
    draw->AddRectFilled(ImVec2(minX - minCap * 0.5f, topY - 2.0f),
        ImVec2(minX + minCap * 0.5f, bottomY + 2.0f), ImGui::GetColorU32(c::accent), 2.0f);
    draw->AddRectFilled(ImVec2(maxX - maxCap * 0.5f, topY - 2.0f),
        ImVec2(maxX + maxCap * 0.5f, bottomY + 2.0f), ImGui::GetColorU32(c::accent), 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(labelPos.x, p.y + 41.0f));
    ImGui::Dummy(ImVec2(1.0f, 1.0f));
}

static float RenderSettingsDropdown(const char* id, int* value, const char* const* items,
                                    int count, float width, float height)
{
    struct State { float open = 0.0f; bool closing = false; };
    static std::unordered_map<ImGuiID, State> states;
    static std::unordered_map<ImGuiID, float> optionMotion;
    ImGui::PushID(id);
    const ImVec2 min = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##settings_dropdown", ImVec2(width, height));
    const ImGuiID widgetId = ImGui::GetItemID();
    State& state = states[widgetId];
    const bool hovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        state.closing = false;
        ImGui::OpenPopup("##settings_dropdown_popup");
    }
    const bool popupOpen = ImGui::IsPopupOpen("##settings_dropdown_popup");
    const float step = ImClamp(ImGui::GetIO().DeltaTime * 9.0f, 0.0f, 1.0f);
    state.open = ImLerp(state.open, popupOpen && !state.closing ? 1.0f : 0.0f, step);
    const float ease = state.open * state.open * (3.0f - 2.0f * state.open);
    *value = ImClamp(*value, 0, count - 1);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(min.x + width, min.y + height);
    draw->AddRectFilled(min, max, ImGui::GetColorU32(hovered || popupOpen
        ? c::combo::background_hov : c::combo::background), c::combo::rounding);
    draw->AddRect(min, max, ImGui::GetColorU32(c::combo::border), c::combo::rounding);
    draw->AddText(ImVec2(min.x + 11.0f, min.y + (height - ImGui::GetFontSize()) * 0.5f),
        ImGui::GetColorU32(c::text::text), items[*value]);
    const ImVec2 arrow(max.x - 14.0f, min.y + height * 0.5f);
    draw->AddTriangleFilled(ImVec2(arrow.x - 3.0f, arrow.y - 3.0f),
        ImVec2(arrow.x - 3.0f, arrow.y + 3.0f), ImVec2(arrow.x + 3.0f, arrow.y),
        ImGui::GetColorU32(popupOpen ? c::accent : c::text::text));

    const float rowH = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
    const float popupH = count * rowH + 12.0f;
    ImGui::SetNextWindowPos(ImVec2(min.x, max.y + 4.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, popupH), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImClamp(ease * 1.35f, 0.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, c::combo::rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, c::combo::background);
    ImGui::PushStyleColor(ImGuiCol_Border, c::combo::border);
    if (ImGui::BeginPopup("##settings_dropdown_popup",
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        for (int i = 0; i < count; ++i) {
            ImGui::PushID(i);
            ImGui::InvisibleButton("##option", ImVec2(ImGui::GetContentRegionAvail().x, rowH));
            const bool optionHovered = ImGui::IsItemHovered();
            const bool optionClicked = ImGui::IsItemClicked();
            const ImVec2 optionMin = ImGui::GetItemRectMin();
            const ImVec2 optionMax = ImGui::GetItemRectMax();
            ImDrawList* popupDraw = ImGui::GetWindowDrawList();
            if (optionHovered) popupDraw->AddRectFilled(optionMin, optionMax, IM_COL32(255,255,255,10), 3.0f);
            if (*value == i) popupDraw->AddRectFilled(ImVec2(optionMin.x + 2.0f, optionMin.y + 4.0f),
                ImVec2(optionMin.x + 5.0f, optionMax.y - 4.0f), ImGui::GetColorU32(c::accent), 2.0f);
            const ImGuiID motionId = widgetId ^ (0x7F4A7C15u + (ImGuiID)i * 0x9E3779B9u); float& motion = optionMotion[motionId]; motion = ImLerp(motion, (optionHovered || *value == i) ? 1.0f : 0.0f, step); popupDraw->AddText(ImVec2(optionMin.x + 11.0f + motion * 8.0f, optionMin.y + (rowH - ImGui::GetFontSize()) * 0.5f), ImGui::GetColorU32(*value == i ? c::text::text_active : c::text::text), items[i]);
            if (optionClicked) { *value = i; state.closing = true; }
            ImGui::PopID();
        }
        if (state.closing && state.open < 0.035f) {
            state.closing = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    ImGui::PopID();
    return popupH * ease;
}
static void RenderBinarypandoraMenu()
{
    ImGuiIO& io = ImGui::GetIO();
    g_ConfigInputActive = false;
    // Once destruction starts, do not leave the closing menu visible while
    // the cleanup thread finishes its work.
    if (g_ShouldDestruct || g_DestructStarted.load()) {
        g_MenuVisible = false;
        g_MenuOpenAnim = 0.0f;
        return;
    }
    // W-style fixed-duration travel followed by the existing smoothstep easing.
    const float animationVelocity = 7.5f;
    g_MenuOpenAnim += (g_MenuVisible ? 1.0f : -1.0f) * animationVelocity * io.DeltaTime;
    g_MenuOpenAnim = ImClamp(g_MenuOpenAnim, 0.0f, 1.0f);
    if (!g_MenuVisible && g_MenuOpenAnim < 0.01f) {
        g_MenuOpenAnim = 0.0f;
        return;
    }
    const float ease = g_MenuOpenAnim * g_MenuOpenAnim * (3.0f - 2.0f * g_MenuOpenAnim);
    const float frameStep = ImClamp(io.DeltaTime * 8.0f, 0.0f, 1.0f);

    // Animate GUI scale smoothly
    g_GuiScale += (g_GuiScaleTarget - g_GuiScale) * ImClamp(io.DeltaTime * 8.0f, 0.0f, 1.0f);

    for (int i = 0; i < 3; ++i) g_AccentColor[i] = gui_guicolor_custom[i];
    c::accent = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.0f);
    c::accent_gradient = ImVec4(ImClamp(g_AccentColor[0] * .82f, 0.f, 1.f),
                                ImClamp(g_AccentColor[1] * .82f, 0.f, 1.f),
                                ImClamp(g_AccentColor[2] * .82f, 0.f, 1.f), 1.0f);

    const float menuW = 740.0f * g_GuiScale, menuH = 570.0f * g_GuiScale;
    const float topH = 58.0f * g_GuiScale;

    // Evander-style background dim. Its state has an independent animation so
    // the Settings toggle fades the effect in/out while the menu stays open.
    // Array List and Watermark are submitted later in swap_buffers and remain
    // above every one of these background layers.
    static float backgroundDimAnim = 0.0f;
    const float backgroundDimTarget = g_BackgroundDim ? ease : 0.0f;
    const float dimBlend = 1.0f - expf(-7.0f * ImMax(io.DeltaTime, 0.0f));
    backgroundDimAnim = ImLerp(backgroundDimAnim, backgroundDimTarget, dimBlend);
    if (backgroundDimAnim > 0.002f) {
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        const int uniformAlpha = (int)(175.0f * backgroundDimAnim);
        background->AddRectFilled(ImVec2(0.0f, 0.0f), io.DisplaySize,
            IM_COL32(0, 0, 0, uniformAlpha));

    }

    // Velocity-style reveal: retain the real layout and expose it smoothly
    // from the center. This avoids resizing or deforming individual controls.
    ImGui::SetNextWindowSize(ImVec2(menuW, menuH), ImGuiCond_Always);
    static ImVec2 menuPosition(-FLT_MAX, -FLT_MAX);
    if (menuPosition.x <= -FLT_MAX * 0.5f) menuPosition = ImVec2((io.DisplaySize.x-menuW)*.5f,(io.DisplaySize.y-menuH)*.5f);
    menuPosition.x=ImClamp(menuPosition.x,0.0f,ImMax(0.0f,io.DisplaySize.x-menuW));
    menuPosition.y=ImClamp(menuPosition.y,0.0f,ImMax(0.0f,io.DisplaySize.y-menuH));
    ImGui::SetNextWindowPos(menuPosition, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, 14));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4(0.12f, 0.42f, 0.95f, 0.78f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, c::bg::background);
    ImGui::Begin("pandoraBinaryMenu", nullptr, ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
        (!g_MenuVisible ? ImGuiWindowFlags_NoInputs : 0));
    ImGui::PopStyleColor();
    ImGuiWindow* menuAnimationRoot = ImGui::GetCurrentWindow();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    static bool draggingMenu=false;
    const bool overDragStrip=io.MousePos.x>=wp.x&&io.MousePos.x<=wp.x+menuW&&io.MousePos.y>=wp.y&&io.MousePos.y<=wp.y+13.0f;
    if(overDragStrip&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)) draggingMenu=true;
    if(!ImGui::IsMouseDown(ImGuiMouseButton_Left)) draggingMenu=false;
    if(draggingMenu){menuPosition.x=ImClamp(menuPosition.x+io.MouseDelta.x,0.0f,ImMax(0.0f,io.DisplaySize.x-menuW));menuPosition.y=ImClamp(menuPosition.y+io.MouseDelta.y,0.0f,ImMax(0.0f,io.DisplaySize.y-menuH));}
    const ImVec2 revealSize(menuW, menuH);
    const ImVec2 revealMin = wp;
    const ImVec2 revealMax(wp.x + menuW, wp.y + menuH);
    g_MenuRevealClipMin = revealMin;
    g_MenuRevealClipMax = revealMax;
    g_MenuRevealClipActive = true;
    draw->PushClipRect(revealMin, revealMax, true);
    draw->AddRectFilled(wp, ImVec2(wp.x + menuW, wp.y + menuH), IM_COL32(19, 19, 21, 255), c::bg::rounding);
    // W-style top navigation: icon tabs, animated accent indicator, module row and search.
    struct BinaryCategory { const char* name; const char* icon; int value; };
    const BinaryCategory cats[] = {
        {"Combat", "\xEF\x85\x80", CAT_COMBAT}, {"Visuals", "\xEF\x81\xAE", CAT_VISUALS},
        {"Movement", "\xEF\x9C\x8C", CAT_MOVEMENT}, {"Player", "\xEF\x88\x9D", CAT_PLAYER},
        {"Misc", "\xEF\x85\x81", CAT_MISC}, {"Configs", "\xEF\x81\xBB", CAT_CONFIGS},
        {"Settings", "\xEF\x80\x93", CAT_SETTINGS}
    };
    static int categoryOrder[7] = {0,1,2,3,4,5,6};
    static float categorySelect[7] = {};
    static std::unordered_map<int, float> topModuleAnim;
    static bool searchOpen = false;
    static float searchAnim = 0.0f;
    static char moduleSearch[64] = {};
    static int pendingCategory = -1;
    static float categoryTransition = 1.0f;
    if (pendingCategory >= 0) {
        categoryTransition = ImMin(1.0f, categoryTransition + io.DeltaTime * 8.0f);
        if (categoryTransition >= 0.48f && g_SelectedCategory != pendingCategory) {
            g_SelectedCategory = pendingCategory;
            SelectFirstModuleInCategory(g_SelectedCategory);
        }
        if (categoryTransition >= 1.0f) { pendingCategory = -1; categoryTransition = 1.0f; }
    }
    const int visualCategory = pendingCategory >= 0 ? pendingCategory : g_SelectedCategory;
    const float categoryPageAlpha = pendingCategory < 0 ? 1.0f : (categoryTransition < 0.48f ? 1.0f - categoryTransition / 0.48f : (categoryTransition - 0.48f) / 0.52f);
    const float categoryPageSlide = pendingCategory < 0 ? 0.0f : (categoryTransition < 0.48f ? -18.0f * (categoryTransition / 0.48f) : 18.0f * (1.0f - (categoryTransition - 0.48f) / 0.52f));
    if (!g_MenuVisible) { searchOpen = false; moduleSearch[0] = 0; }

    const float headerH = 48.0f * g_GuiScale;
    draw->AddRectFilled(wp, ImVec2(wp.x + menuW, wp.y + headerH), IM_COL32(24, 24, 27, 255), c::bg::rounding, ImDrawFlags_RoundCornersTop);
    
    if (g_MenuHeaderAnim) {
        draw->PushClipRect(wp, ImVec2(wp.x + menuW, wp.y + headerH), true);
        float t = (float)ImGui::GetTime();
        
        // Setup colors from global setting
        int baseR = (int)(g_MenuHeaderAnimColor[0] * 255.0f);
        int baseG = (int)(g_MenuHeaderAnimColor[1] * 255.0f);
        int baseB = (int)(g_MenuHeaderAnimColor[2] * 255.0f);
        
        // Wavy Lines removed as per user request

        // Floating Stars
        struct BackgroundStar { float x, y, size, phase, speedY; };
        static std::vector<BackgroundStar> headerStars;
        if (headerStars.empty()) {
            for (int i = 0; i < 25; i++) {
                headerStars.push_back({
                    (float)(rand() % 1000) / 1000.0f,
                    (float)(rand() % 1000) / 1000.0f,
                    1.5f + (rand() % 25) / 10.0f,
                    (float)(rand() % 100) / 10.0f,
                    -(0.05f + (rand() % 10) / 100.0f)
                });
            }
        }
        
        for (auto& star : headerStars) {
            star.y += star.speedY * ImGui::GetIO().DeltaTime;
            if (star.y < -0.1f) {
                star.y = 1.1f;
                star.x = (float)(rand() % 1000) / 1000.0f;
            }
            
            float alpha = 0.3f + 0.6f * (0.5f + 0.5f * sinf(t * 2.5f + star.phase)); // Increased opacity
            ImU32 starCol = IM_COL32(baseR, baseG, baseB, (int)(alpha * 255.0f));
            
            ImVec2 center(wp.x + star.x * menuW, wp.y + star.y * headerH);
            
            ImVec2 pts[10];
            float a = -3.1415926535f / 2.0f;
            for (int k = 0; k < 10; ++k) {
                float r = (k % 2 == 0) ? star.size : star.size * 0.45f;
                pts[k] = ImVec2(center.x + cosf(a) * r, center.y + sinf(a) * r);
                a += 3.1415926535f / 5.0f;
            }
            draw->AddConvexPolyFilled(pts, 10, starCol);
        }
        draw->PopClipRect();
    }

    draw->AddLine(ImVec2(wp.x + 12.0f, wp.y + headerH), ImVec2(wp.x + menuW - 12.0f, wp.y + headerH), IM_COL32(0, 0, 0, 105));

    const float navStartX = 16.0f * g_GuiScale;
    const float navRight = menuW - 62.0f * g_GuiScale;
    float naturalWidths[7] = {};
    float totalNavW = 0.0f;
    for (int i = 0; i < 7; ++i) {
        const float labelW = ImGui::CalcTextSize(cats[categoryOrder[i]].name).x;
        naturalWidths[i] = (labelW + 49.0f) * g_GuiScale;
        totalNavW += naturalWidths[i];
    }
    const float navGap = 4.0f * g_GuiScale;
    totalNavW += navGap * 6.0f;
    const float availableNavW = navRight - navStartX;
    const float navFit = totalNavW > availableNavW ? availableNavW / totalNavW : 1.0f;
    float tabX = navStartX + ImMax(0.0f, (availableNavW - totalNavW * navFit) * 0.5f);

    // DIBUJAR LOGO DE PANDORA
    extern GLuint g_PandoraLogoTexture;
    extern int g_PandoraLogoWidth;
    extern int g_PandoraLogoHeight;
    
    if (g_PandoraLogoTexture == 0) {
        int channels;
        unsigned char* pixels = stbi_load_from_memory((stbi_uc*)pandoralogo_png, pandoralogo_png_len, &g_PandoraLogoWidth, &g_PandoraLogoHeight, &channels, 4);
        if (pixels) {
            for (int i = 0; i < g_PandoraLogoWidth * g_PandoraLogoHeight * 4; i += 4) {
                pixels[i] = 255;
                pixels[i+1] = 255;
                pixels[i+2] = 255;
            }
            glGenTextures(1, &g_PandoraLogoTexture);
            glBindTexture(GL_TEXTURE_2D, g_PandoraLogoTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_PandoraLogoWidth, g_PandoraLogoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            stbi_image_free(pixels);
        } else {
            g_PandoraLogoTexture = 0xFFFFFFFF; // Error flag
        }
    }

    if (g_PandoraLogoTexture != 0 && g_PandoraLogoTexture != 0xFFFFFFFF) {
        ImVec2 logoSize(36, 36); // Tamaño del logo en el menu
        draw->AddImage((ImTextureID)(intptr_t)g_PandoraLogoTexture, 
            ImVec2(wp.x + 6.0f, wp.y + (headerH - logoSize.y) * 0.5f), 
            ImVec2(wp.x + 6.0f + logoSize.x, wp.y + (headerH - logoSize.y) * 0.5f + logoSize.y),
            ImVec2(0, 0), ImVec2(1, 1),
            framework::g_style->m_accent.get_u32());
        tabX = ImMax(tabX, wp.x + 6.0f + logoSize.x + 10.0f - wp.x); // Asegurarse de que los tabs no se superpongan con el logo
    }

    for (int catIndex = 0; catIndex < 7; ++catIndex) {
        const int catDataIndex = categoryOrder[catIndex];
        const BinaryCategory& cat = cats[catDataIndex];
        const bool selected = g_SelectedCategory == cat.value;
        categorySelect[catIndex] = ImLerp(categorySelect[catIndex], selected ? 1.0f : 0.0f,
            ImClamp(io.DeltaTime * 12.0f, 0.0f, 1.0f));
        const float tabW = naturalWidths[catIndex] * navFit;
        const float tabH = 34.0f * g_GuiScale;
        ImGui::PushID(catIndex + 7100);
        ImGui::SetCursorPos(ImVec2(tabX, 7.0f * g_GuiScale));
        ImGui::InvisibleButton("##top_category", ImVec2(tabW, tabH));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && cat.value != g_SelectedCategory && pendingCategory < 0) {
            pendingCategory = cat.value;
            categoryTransition = 0.0f;
        }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID | ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
            ImGui::SetDragDropPayload("pandora_CATEGORY_SLOT", &catIndex, sizeof(catIndex));
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("pandora_CATEGORY_SLOT")) {
                const int from = *(const int*)payload->Data;
                if (from >= 0 && from < 7 && from != catIndex) std::swap(categoryOrder[from], categoryOrder[catIndex]);
            }
            ImGui::EndDragDropTarget();
        }
        const ImVec2 r0 = ImGui::GetItemRectMin();
        const ImVec2 r1 = ImGui::GetItemRectMax();
        const float selectT = categorySelect[catIndex];
        if (selectT > 0.01f || hovered) {
            const float hoverT = hovered ? 1.0f : 0.0f;
            const float bgT = ImMax(selectT, hoverT * 0.42f);
            draw->AddRectFilled(ImVec2(r0.x + 1.0f, r0.y + 3.0f),
                ImVec2(r1.x + 2.0f, r1.y + 5.0f), IM_COL32(0, 0, 0, (int)(70.0f * bgT)), 6.0f);
            draw->AddRectFilled(r0, r1, IM_COL32(45, 44, 50, (int)(225.0f * bgT)), 6.0f);
            
        }
        const float iconSize = 16.5f * g_GuiScale;
        const ImVec2 labelSz = ImGui::CalcTextSize(cat.name);
        ImVec2 iconSz(0.0f, 0.0f);
        if (g_BinaryIconFont)
            iconSz = g_BinaryIconFont->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, cat.icon);
        const float groupW = iconSz.x + 7.0f * g_GuiScale + labelSz.x;
        const float groupX = r0.x + (tabW - groupW) * 0.5f;
        const ImU32 textCol = selected ? IM_COL32(238, 237, 242, 255)
            : IM_COL32(158, 157, 167, hovered ? 245 : 205);
        if (g_BinaryIconFont)
            draw->AddText(g_BinaryIconFont, iconSize,
                ImVec2(groupX, r0.y + (tabH - iconSz.y) * 0.5f), textCol, cat.icon);
        draw->AddText(ImVec2(groupX + iconSz.x + 7.0f * g_GuiScale,
            r0.y + (tabH - labelSz.y) * 0.5f), textCol, cat.name);
        ImGui::PopID();
        tabX += tabW + navGap;
    }
    const float searchBtnX = menuW - 51.0f * g_GuiScale;
    ImGui::SetCursorPos(ImVec2(searchBtnX, 7.0f * g_GuiScale));
    ImGui::InvisibleButton("##module_search_toggle", ImVec2(34.0f*g_GuiScale, 34.0f*g_GuiScale));
    const bool searchHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) {
        searchOpen = !searchOpen;
        moduleSearch[0] = 0;
    }
    if (g_BinaryIconFont) {
        const char* searchIcon = "\xEF\x80\x82";
        draw->AddText(g_BinaryIconFont, 18.0f*g_GuiScale, ImVec2(wp.x + searchBtnX + 8.0f, wp.y + 14.0f*g_GuiScale),
            searchOpen ? IM_COL32((int)(g_AccentColor[0]*255),(int)(g_AccentColor[1]*255),(int)(g_AccentColor[2]*255),255)
                       : IM_COL32(175,174,182,searchHovered?255:205), searchIcon);
    }
    searchAnim = ImLerp(searchAnim, searchOpen ? 1.0f : 0.0f, ImClamp(io.DeltaTime*12.0f,0.0f,1.0f));

    const bool configsCategory = g_SelectedCategory == CAT_CONFIGS || g_SelectedCategory == CAT_SETTINGS;

    // Centered secondary module row directly below the active category.
    const float moduleRowH = configsCategory ? 0.0f : 38.0f * g_GuiScale;
    if (!configsCategory) {
        std::vector<int> moduleTabs;
        float naturalTotal = 0.0f;
        for (int i = 0; i < (int)modules.size(); ++i) {
            if (!modules[i].hidden && modules[i].category == g_SelectedCategory) {
                moduleTabs.push_back(i);
                naturalTotal += ImGui::CalcTextSize(modules[i].name.c_str()).x + 24.0f * g_GuiScale;
            }
        }
        const float available = menuW - 40.0f * g_GuiScale;
        const float fit = naturalTotal > available ? available / naturalTotal : 1.0f;
        float moduleX = (menuW - naturalTotal * fit) * 0.5f;
        for (int index : moduleTabs) {
            Module& tabModule = modules[index];
            const float tabWidth = (ImGui::CalcTextSize(tabModule.name.c_str()).x + 24.0f * g_GuiScale) * fit;
            const bool selectedModule = index == g_SelectedMod;
            float& tabAnim = topModuleAnim[index];
            tabAnim = ImLerp(tabAnim, selectedModule ? 1.0f : 0.0f, ImClamp(io.DeltaTime * 14.0f, 0.0f, 1.0f));
            ImGui::PushID(8400 + index);
            ImGui::SetCursorPos(ImVec2(moduleX, headerH + 3.0f * g_GuiScale));
            ImGui::InvisibleButton("##module_tab", ImVec2(tabWidth, 29.0f * g_GuiScale));
            const bool hoveredModule = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked() && !selectedModule) g_SelectedMod = index;
            const ImVec2 tabMin = ImGui::GetItemRectMin(), tabMax = ImGui::GetItemRectMax();
            if (tabAnim > 0.01f || hoveredModule) {
                const float amount = ImMax(tabAnim, hoveredModule ? 0.35f : 0.0f);
                draw->AddRectFilled(ImVec2(tabMin.x + 1.0f, tabMin.y + 2.0f), ImVec2(tabMax.x + 1.0f, tabMax.y + 3.0f), IM_COL32(0,0,0,(int)(42.0f*amount)), 4.0f);
                draw->AddRectFilled(tabMin, tabMax, IM_COL32(43,42,48,(int)(210.0f*amount)), 4.0f);
            }
            const ImVec2 nameSize = ImGui::CalcTextSize(tabModule.name.c_str());
            draw->AddText(ImVec2(tabMin.x + (tabWidth-nameSize.x)*0.5f, tabMin.y + (tabMax.y-tabMin.y-nameSize.y)*0.5f),
                selectedModule ? IM_COL32(235,234,239,255) : IM_COL32(145,143,153,hoveredModule?245:210), tabModule.name.c_str());
            ImGui::PopID();
            moduleX += tabWidth;
        }
        draw->AddLine(ImVec2(wp.x + 16.0f, wp.y + headerH + moduleRowH - 2.0f),
            ImVec2(wp.x + menuW - 16.0f, wp.y + headerH + moduleRowH - 2.0f), IM_COL32(0,0,0,75));
    }

    const float activeSideW = 0.0f;
    const float panelW = menuW - activeSideW - 40.0f;    
    if (g_SelectedMod < 0 || g_SelectedMod >= (int)modules.size() ||
        modules[g_SelectedMod].category != g_SelectedCategory)
        SelectFirstModuleInCategory(g_SelectedCategory);

    const float contentTop = topH + moduleRowH;
    ImGui::SetCursorPos(ImVec2(activeSideW, contentTop));
    ImGui::BeginChild("##binary_content", ImVec2(menuW - activeSideW, menuH - contentTop), false, 0);
    ImDrawList* contentDraw = ImGui::GetWindowDrawList();
    contentDraw->PushClipRect(revealMin, revealMax, true);
    if (configsCategory) {
        const ImVec2 sectionPos = ImGui::GetWindowPos();
        const ImVec2 sectionSize = ImGui::GetWindowSize();
        ImDrawList* sectionDraw = ImGui::GetWindowDrawList();
        const ImVec2 sectionMin(sectionPos.x + 10.0f, sectionPos.y + 8.0f);
        const ImVec2 sectionMax(sectionPos.x + sectionSize.x - 10.0f, sectionPos.y + sectionSize.y - 8.0f);
        sectionDraw->AddRectFilled(sectionMin, sectionMax,
            ImGui::GetColorU32(c::bg::background_pad), 12.0f);
        sectionDraw->AddRect(sectionMin, sectionMax,
            ImGui::GetColorU32(c::bg::border), 12.0f);
    }
    static int previousAnimatedCategory = -1;
    static int previousAnimatedModule = -2;
    static float contentTransition = 1.0f;
    if (previousAnimatedCategory != g_SelectedCategory || previousAnimatedModule != g_SelectedMod) {
        previousAnimatedCategory = g_SelectedCategory;
        previousAnimatedModule = g_SelectedMod;
        contentTransition = 0.0f;
    }
    contentTransition = ImMin(1.0f, contentTransition + io.DeltaTime * 8.0f);    // Short damped slide with category fade-out/fade-in.
    float transitionEase = 1.0f - expf(-6.5f * contentTransition) * cosf(5.2f * contentTransition);
    transitionEase = ImClamp(transitionEase, 0.0f, 1.04f);
    const float contentSlideX = categoryPageSlide + (1.0f - transitionEase) * 26.0f;
    const float transitionAlpha = categoryPageAlpha * ImClamp(0.22f + contentTransition * 1.15f, 0.0f, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * transitionAlpha);
    if (g_SelectedCategory == CAT_SETTINGS) {
        ImDrawList* sdl = ImGui::GetWindowDrawList();
        const float settingsX = 28.0f + contentSlideX;
        const float rightW = ImGui::GetWindowWidth();
        const float totalW = rightW - 56.0f;
        const float halfW = (totalW - 24.0f) * 0.5f;

        // Title
        ImGui::SetCursorPos(ImVec2(settingsX, 22.0f));
        ImGui::SetWindowFontScale(1.18f);
        ImGui::TextColored(ImVec4(1, 1, 1, 1), "Settings");
        ImGui::SetWindowFontScale(1.0f);
        
        float leftY = 60.0f;
        float rightY = 60.0f;
        const float leftX = settingsX;
        const float rightX = settingsX + halfW + 24.0f;

        // --- LEFT PANEL ---
        // Menu Key
        ImGui::SetCursorPos(ImVec2(leftX, leftY));
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.80f, 1.0f), "Menu key");
        std::string mkName = g_IsBindingMenuKey ? "Press a key..." : GetKeyName(g_MenuKey);
        ImVec2 mkSz = ImGui::CalcTextSize(mkName.c_str());
        float mkBtnW = mkSz.x + 24.0f;
        float mkBtnH = 28.0f;
        float mkBtnX = leftX + halfW - mkBtnW;
        ImGui::SetCursorPos(ImVec2(mkBtnX, leftY - 4.0f));
        if (ImGui::InvisibleButton("##settings_menukey", ImVec2(mkBtnW, mkBtnH))) {
            g_IsBindingMenuKey = !g_IsBindingMenuKey;
        }
        ImVec2 mkMin = ImGui::GetItemRectMin(), mkMax = ImGui::GetItemRectMax();
        bool mkHov = ImGui::IsItemHovered();
        sdl->AddRectFilled(mkMin, mkMax, mkHov ? IM_COL32(23, 24, 29, 255) : IM_COL32(14, 15, 18, 255), 6.0f);
        sdl->AddRect(mkMin, mkMax, IM_COL32(70, 70, 78, 255), 6.0f);
        sdl->AddText(ImVec2(mkMin.x + (mkBtnW - mkSz.x) * 0.5f, mkMin.y + (mkBtnH - mkSz.y) * 0.5f),
            g_IsBindingMenuKey ? IM_COL32((int)(g_AccentColor[0]*255), (int)(g_AccentColor[1]*255), (int)(g_AccentColor[2]*255), 255) : IM_COL32(200, 200, 210, 255),
            mkName.c_str());
        leftY += 40.0f;

        // GUI Scale dropdown
        ImGui::SetCursorPos(ImVec2(leftX, leftY));
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.80f, 1.0f), "GUI Scale");
        static const char* scaleOptions[] = { "Default", "110%", "120%", "130%" };
        static const float scaleValues[] = { 1.0f, 1.10f, 1.20f, 1.30f };
        const float scaleDropdownW = 130.0f;
        ImGui::SetCursorPos(ImVec2(leftX + halfW - scaleDropdownW, leftY - 3.0f));
        const float scaleExpanded = RenderSettingsDropdown("gui_scale", &g_GuiScaleIndex,
            scaleOptions, 4, scaleDropdownW, 30.0f);
        g_GuiScaleTarget = scaleValues[ImClamp(g_GuiScaleIndex, 0, 3)];
        leftY += 40.0f + scaleExpanded;

        // Notifications toggle
        ImGui::SetCursorPos(ImVec2(leftX, leftY));
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.80f, 1.0f), "Notifications");
        {
            float toggleX = leftX + halfW - 38.0f;
            ImGui::SetCursorPos(ImVec2(toggleX, leftY));
            ImGui::InvisibleButton("##stoggle_notif", ImVec2(38.0f, 20.0f));
            if (ImGui::IsItemClicked()) features::misc::notifications::enabled = !features::misc::notifications::enabled;
            bool notifHov = ImGui::IsItemHovered();
            float notifAnim = GetAnim("settings_notif_anim", features::misc::notifications::enabled);
            DrawUnifiedCheckbox(sdl, ImGui::GetItemRectMin(), notifAnim, notifHov, 1.0f);
        }
        leftY += 36.0f;

        // Notification Position dropdown
        if (features::misc::notifications::enabled) {
            ImGui::SetCursorPos(ImVec2(leftX + 16.0f, leftY));
            ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.60f, 1.0f), "Position");
            static const char* posOptions[] = { "Top left", "Top right", "Bottom left", "Bottom right" };
            const float positionDropdownW = 130.0f;
            ImGui::SetCursorPos(ImVec2(leftX + halfW - positionDropdownW, leftY - 3.0f));
            const float positionExpanded = RenderSettingsDropdown("notification_position",
                &features::misc::notifications::position, posOptions, 4, positionDropdownW, 30.0f);
            leftY += 34.0f + positionExpanded;

            // Notification Bar Color
            ImGui::SetCursorPos(ImVec2(leftX + 16.0f, leftY));
            DrawCompactColorControl("settings_notif_bar", "Bar color", features::misc::notifications::bar_color, true,
                halfW - 16.0f, 1.0f);
            leftY += 36.0f;
        }

        // Background Dim toggle
        ImGui::SetCursorPos(ImVec2(leftX, leftY));
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.80f, 1.0f), "Background dim");
        {
            float toggleX = leftX + halfW - 38.0f;
            ImGui::SetCursorPos(ImVec2(toggleX, leftY));
            ImGui::InvisibleButton("##stoggle_bgdim", ImVec2(38.0f, 20.0f));
            if (ImGui::IsItemClicked()) g_BackgroundDim = !g_BackgroundDim;
            bool dimHov = ImGui::IsItemHovered();
            float dimAnim = GetAnim("settings_bgdim_anim", g_BackgroundDim);
            DrawUnifiedCheckbox(sdl, ImGui::GetItemRectMin(), dimAnim, dimHov, 1.0f);
        }
        leftY += 50.0f;

        // --- RIGHT PANEL ---
        // Accent Color
        ImGui::SetCursorPos(ImVec2(rightX, rightY));
        DrawCompactColorControl("settings_accent", "Accent color", gui_guicolor_custom, false,
            halfW, 1.0f);
        rightY += 40.0f;
        
        // Header Animation Toggle
        ImGui::SetCursorPos(ImVec2(rightX, rightY));
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.80f, 1.0f), "Header animation");
        {
            float toggleX = rightX + halfW - 38.0f;
            ImGui::SetCursorPos(ImVec2(toggleX, rightY));
            ImGui::InvisibleButton("##stoggle_headeranim", ImVec2(38.0f, 20.0f));
            if (ImGui::IsItemClicked()) g_MenuHeaderAnim = !g_MenuHeaderAnim;
            bool animHov = ImGui::IsItemHovered();
            float animT = GetAnim("settings_headeranim_t", g_MenuHeaderAnim);
            DrawUnifiedCheckbox(sdl, ImGui::GetItemRectMin(), animT, animHov, 1.0f);
        }
        rightY += 36.0f;
        
        // Header Animation Color
        if (g_MenuHeaderAnim) {
            ImGui::SetCursorPos(ImVec2(rightX + 16.0f, rightY));
            DrawCompactColorControl("settings_headeranim_color", "Animation color", g_MenuHeaderAnimColor, false,
                halfW - 16.0f, 1.0f);
            rightY += 36.0f;
        }
        
        float finalY = ImMax(leftY, rightY) + 20.0f;

        // --- Unload Client button (Centered at bottom) ---
        const float unloadW = ImMin(420.0f, totalW);
        const float unloadX = settingsX + (totalW - unloadW) * 0.5f;
        ImGui::SetCursorPos(ImVec2(unloadX, finalY));
        const ImVec2 unloadPos = ImGui::GetCursorScreenPos();
        const ImVec2 unloadSize(unloadW, 44.0f);
        ImGui::InvisibleButton("##settings_unload", unloadSize);
        const bool uHolding = ImGui::IsItemActive() && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        constexpr float uSeconds = 0.75f;
        if (uHolding && !g_DestructStarted.load()) g_DestructHoldProgress += ImGui::GetIO().DeltaTime / uSeconds;
        else if (!g_DestructStarted.load()) g_DestructHoldProgress = ImMax(0.0f, g_DestructHoldProgress - ImGui::GetIO().DeltaTime * 2.0f);
        g_DestructHoldProgress = ImClamp(g_DestructHoldProgress, 0.0f, 1.0f);
        sdl->AddRectFilled(unloadPos, ImVec2(unloadPos.x + unloadSize.x, unloadPos.y + unloadSize.y), IM_COL32(31, 22, 26, 255), 8.0f);
        if (g_DestructHoldProgress > 0.001f) {
            sdl->PushClipRect(unloadPos, ImVec2(unloadPos.x + unloadSize.x, unloadPos.y + unloadSize.y), true);
            sdl->AddRectFilled(unloadPos, ImVec2(unloadPos.x + unloadSize.x * g_DestructHoldProgress, unloadPos.y + unloadSize.y), IM_COL32(62, 31, 41, 250), 8.0f);
            sdl->PopClipRect();
        }
        sdl->AddRect(unloadPos, ImVec2(unloadPos.x + unloadSize.x, unloadPos.y + unloadSize.y), IM_COL32(166, 55, 82, 225), 8.0f);
        const char* uIcon = "\xef\x8b\xb5";
        const char* uLabel = (g_DestructHoldProgress > 0.01f && g_DestructHoldProgress < 1.0f) ? "Hold to unload..." : "Unload client";
        ImU32 uCol = (g_DestructHoldProgress > 0.01f) ? IM_COL32(255, 255, 255, 255) : IM_COL32(244, 92, 127, 255);
        float uIconSize = 16.0f;
        ImVec2 uIconSz(0, 0);
        if (g_BinaryIconFont) uIconSz = g_BinaryIconFont->CalcTextSizeA(uIconSize, FLT_MAX, 0.0f, uIcon);
        ImVec2 uLabelSz = ImGui::CalcTextSize(uLabel);
        float uTotalW = uIconSz.x + 8.0f + uLabelSz.x;
        float uStartX = unloadPos.x + (unloadSize.x - uTotalW) * 0.5f;
        float uTextY = unloadPos.y + (unloadSize.y - uLabelSz.y) * 0.5f;
        if (g_BinaryIconFont) {
            float uIconY = unloadPos.y + (unloadSize.y - uIconSz.y) * 0.5f;
            sdl->AddText(g_BinaryIconFont, uIconSize, ImVec2(uStartX, uIconY), uCol, uIcon);
        }
        sdl->AddText(ImVec2(uStartX + uIconSz.x + 8.0f, uTextY), uCol, uLabel);
        if (g_DestructHoldProgress >= 1.0f && !g_DestructStarted.exchange(true)) g_ShouldDestruct = true;
    }
    else if (!configsCategory && false) {
        static std::unordered_map<int, float> panelReveal;
        std::vector<int> visibleModules;
        for (int i=0;i<(int)modules.size();++i)
            if(!modules[i].hidden&&modules[i].category==g_SelectedCategory) visibleModules.push_back(i);
        const float gap=14.0f;
        const float colW=(panelW-gap)*0.5f;
        float colY[2] = { ImGui::GetCursorPosY(), ImGui::GetCursorPosY() };
        for(size_t cardIndex=0;cardIndex<visibleModules.size();++cardIndex){
            Module& cardModule=modules[visibleModules[cardIndex]];
            ImGui::PushID(visibleModules[cardIndex]+9000);
            float& reveal=panelReveal[visibleModules[cardIndex]];
            const float delay=ImMin(0.34f,(float)cardIndex*0.045f);
            const float target=ImSaturate((categoryPageAlpha-delay)/(1.0f-delay));
            reveal=ImLerp(reveal,target,ImClamp(io.DeltaTime*10.0f,0.0f,1.0f));
            float exactHeight = 112.0f + (cardModule.name == "Left Clicker" ? 64.0f : 0.0f);
            if (cardModule.name == "Nametags" && gui_nametags_use_fake_name) exactHeight += 44.0f;
            if (cardModule.name == "Friends") exactHeight += 3 * 30.0f;
            if (cardModule.name == "GUI Color") exactHeight += 42.0f;
            for (const Setting& st : cardModule.settings) {
                if (!IspandoraBinarySettingVisible(cardModule, st)) continue;
                if (st.type == Setting::TOGGLE) exactHeight += 30.0f;
                else if (st.type == Setting::SLIDER) exactHeight += 54.0f;
                else if (st.type == Setting::DROPDOWN) exactHeight += 48.0f;
                else exactHeight += 42.0f;
            }
            float cardH = ImMax(150.0f, exactHeight);
            int col = (colY[0] <= colY[1]) ? 0 : 1;
            float posX = 20.0f + contentSlideX + (1.0f - reveal) * 22.0f + col * (colW + gap);
            ImGui::SetCursorPos(ImVec2(posX, colY[col]));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha,ImGui::GetStyle().Alpha*reveal);
            const bool cardOpen=BeginpandoraBinaryPanel(cardModule.name.c_str(),ImVec2(colW,cardH),true);
            if(cardOpen){
                if(cardModule.enabledPtr){
                    const ImVec2 enabledPos=ImGui::GetCursorScreenPos(); ImGui::InvisibleButton("##card_enabled",ImVec2(19,22));
                    if(ImGui::IsItemClicked())*cardModule.enabledPtr=!*cardModule.enabledPtr;
                    DrawUnifiedCheckbox(ImGui::GetWindowDrawList(),enabledPos,GetAnim((std::string("panel_enabled_")+cardModule.name).c_str(),*cardModule.enabledPtr),ImGui::IsItemHovered(),reveal);
                    ImGui::SameLine(0,8);ImGui::SetCursorPosY(ImGui::GetCursorPosY()-2);ImGui::TextUnformatted("Enabled");ImGui::SameLine();RenderpandoraBind(cardModule);g_KeybindMap[cardModule.name]=cardModule.keybind;
                }
                if(cardModule.name=="Left Clicker") RenderBinaryCpsRange(ImMax(80.0f,ImGui::GetContentRegionAvail().x));
                for(Setting& setting:cardModule.settings){if(!IspandoraBinarySettingVisible(cardModule,setting))continue;ImGui::SetCursorPosX(20.0f);RenderBinarySetting(setting);}
                if(cardModule.name=="Nametags"&&gui_nametags_use_fake_name){ImGui::SetCursorPosX(20.0f);ImGui::TextUnformatted("Fake Name");ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);ImGui::InputTextWithHint("##panel_fake_name","Player name...",gui_nametags_fake_name,sizeof(gui_nametags_fake_name));if(ImGui::IsItemActive())g_ConfigInputActive=true;}
                if(cardModule.name=="Friends"){RenderpandoraRawBind("Add Friends Key",&gui_friends_add_bind);RenderpandoraRawBind("Add Nearby Friends Key",&gui_friends_nearby_bind);RenderpandoraRawBind("Clear Friends Key",&gui_friends_clear_bind);}
                if(cardModule.name=="GUI Color"){ImGui::SetCursorPosX(20.0f);DrawCompactColorControl("panel_accent","Accent Color",gui_guicolor_custom,false,ImMax(60.0f,ImGui::GetContentRegionAvail().x-20.0f),reveal);}
            }
            EndpandoraBinaryPanel();ImGui::PopStyleVar();ImGui::PopID();
            
            colY[col] += cardH + gap;
        }
        ImGui::SetCursorPosY(ImMax(colY[0], colY[1]) + gap);
    }    else if (g_SelectedMod >= 0 && g_SelectedMod < (int)modules.size()) {
        Module& module = modules[g_SelectedMod];
        ImGui::SetCursorPos(ImVec2(20.0f + contentSlideX, 14.0f));
        if (module.enabledPtr) {
            const ImVec2 enabledPos = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##module_enabled", ImVec2(19.0f, 22.0f));
            if (ImGui::IsItemClicked()) *module.enabledPtr = !*module.enabledPtr;
            const bool enabledHovered = ImGui::IsItemHovered();
            DrawUnifiedCheckbox(ImGui::GetWindowDrawList(), enabledPos,
                GetAnim("module_enabled", *module.enabledPtr), enabledHovered, 1.0f);
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
            ImGui::TextUnformatted("Enabled");
            ImGui::SameLine();
            RenderpandoraBind(module);
            g_KeybindMap[module.name] = module.keybind;
        }
        ImGui::SetCursorPos(ImVec2(20.0f + contentSlideX,
            (!module.enabledPtr || module.name == "Configs") ? 20.0f : 54.0f));

        if (module.name == "Configs") {
            static bool g_ShowNewConfigModal = false;
            static bool g_ModalJustOpened = false;
            static float g_ModalAnim = 0.0f;
            static float g_ModalBgAnim = 0.0f;
            static double nextAutomaticConfigRefresh = 0.0;
            static std::string selectedConfig;
            static std::unordered_map<std::string, float> rowMotion;
            const double now = ImGui::GetTime();
            const float dt = ImGui::GetIO().DeltaTime;
            if (now >= nextAutomaticConfigRefresh) {
                ReloadConfigsFromDisk();
                nextAutomaticConfigRefresh = now + 2.0;
            }
            if (!selectedConfig.empty() &&
                std::find(g_ConfigList.begin(), g_ConfigList.end(), selectedConfig) == g_ConfigList.end())
                selectedConfig.clear();
            if (selectedConfig.empty() && !g_ConfigList.empty()) selectedConfig = g_ConfigList.front();

            BeginpandoraBinaryPanel("Configs", ImVec2(panelW, ImMax(360.0f, ImGui::GetContentRegionAvail().y - 16.0f)));
            const float formW = ImMin(520.0f, ImGui::GetContentRegionAvail().x);
            const float contentX = (ImGui::GetWindowWidth() - formW) * 0.5f;
            const float controlsW = ImMax(260.0f, ImMin(420.0f, formW - 100.0f));
            const float controlsX = (ImGui::GetWindowWidth() - controlsW) * 0.5f;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float listY = ImGui::GetCursorPosY() + 4.0f;
            const float listH = 125.0f;

            ImGui::SetCursorPos(ImVec2(contentX, listY));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.050f, 0.070f, 0.70f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::BeginChild("##config_rows", ImVec2(formW, listH), false,
                0);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImDrawList* configRowsDraw = ImGui::GetWindowDrawList();
            if (g_ConfigList.empty()) {
                const char* emptyText = "No configs saved";
                ImVec2 sz = ImGui::CalcTextSize(emptyText);
                ImVec2 childPos = ImGui::GetWindowPos();
                configRowsDraw->AddText(ImVec2(childPos.x + (formW - sz.x) * 0.5f,
                    childPos.y + (listH - sz.y) * 0.5f), IM_COL32(105, 103, 118, 255), emptyText);
            } else {
                const float rowH = 27.0f;
                for (const std::string& cfgName : g_ConfigList) {
                    ImGui::PushID(cfgName.c_str());
                    ImVec2 rowMin = ImGui::GetCursorScreenPos();
                    const float configRowW = ImGui::GetContentRegionAvail().x;
                    if (ImGui::InvisibleButton("##config_row", ImVec2(configRowW, rowH))) selectedConfig = cfgName;
                    const bool hovered = ImGui::IsItemHovered();
                    const bool selected = selectedConfig == cfgName;
                    float& motion = rowMotion[cfgName];
                    motion = ImLerp(motion, (hovered || selected) ? 1.0f : 0.0f,
                        ImClamp(dt * 12.0f, 0.0f, 1.0f));
                    if (hovered || selected)
                        configRowsDraw->AddRectFilled(rowMin, ImVec2(rowMin.x + configRowW, rowMin.y + rowH),
                            selected ? IM_COL32((int)(g_AccentColor[0] * 255), (int)(g_AccentColor[1] * 255),
                                (int)(g_AccentColor[2] * 255), 20) : IM_COL32(255, 255, 255, 8), 4.0f);
                    ImU32 textCol = selected
                        ? IM_COL32((int)(g_AccentColor[0] * 255), (int)(g_AccentColor[1] * 255),
                            (int)(g_AccentColor[2] * 255), 255)
                        : (hovered ? IM_COL32(218, 216, 226, 255) : IM_COL32(145, 142, 157, 255));
                    configRowsDraw->AddText(ImVec2(rowMin.x + 12.0f + motion * 8.0f, rowMin.y + 5.0f), textCol, cfgName.c_str());
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
            const ImVec2 configListMin(ImGui::GetWindowPos().x + contentX, ImGui::GetWindowPos().y + listY);
            dl->AddRect(configListMin, ImVec2(configListMin.x + formW, configListMin.y + listH),
                IM_COL32(58, 55, 68, 220), 6.0f, 0, 1.0f);

            ImGui::SetCursorPos(ImVec2(controlsX + 40.0f, listY + listH + 14.0f));
            const ImVec2 inputMin = ImGui::GetCursorScreenPos();
            const ImVec2 folderMin(inputMin.x - 40.0f, inputMin.y);
            ImGui::SetCursorScreenPos(folderMin);
            ImGui::InvisibleButton("##open_config_folder_icon", ImVec2(32.0f, 27.0f));
            const bool folderHovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) OpenConfigFolder();
            dl->AddRectFilled(folderMin, ImVec2(folderMin.x + 32.0f, folderMin.y + 27.0f),
                ImGui::GetColorU32(folderHovered ? ImVec4(0.085f, 0.085f, 0.095f, 1.0f)
                                                   : ImVec4(0.060f, 0.060f, 0.068f, 1.0f)), 4.0f);
            if (g_BinaryIconFont)
                dl->AddText(g_BinaryIconFont, 16.0f, ImVec2(folderMin.x + 8.0f, folderMin.y + 6.0f),
                    IM_COL32(232, 230, 238, 255), "\xEF\x81\xBB");
            ImGui::SetCursorScreenPos(inputMin);
            const float inputH = 27.0f;
            dl->AddRectFilled(ImVec2(inputMin.x - 3.0f, inputMin.y - 3.0f),
                ImVec2(inputMin.x + controlsW - 40.0f + 3.0f, inputMin.y + inputH + 3.0f),
                IM_COL32(0, 0, 0, 45), 6.0f);
            dl->AddRectFilled(inputMin, ImVec2(inputMin.x + controlsW - 40.0f, inputMin.y + inputH),
                IM_COL32(18, 18, 22, 255), 3.0f);
            ImGui::SetNextItemWidth(controlsW - 40.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg,
                ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 0.35f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
            ImGui::PushClipRect(inputMin, ImVec2(inputMin.x + controlsW - 40.0f, inputMin.y + inputH), true);
            ImGui::InputTextWithHint("##config_name_input", "New config name...", g_NewConfigName,
                IM_ARRAYSIZE(g_NewConfigName));
            ImGui::PopClipRect();
            g_ConfigInputActive = ImGui::IsItemActive();
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(5);

            const float btnH = 28.0f;
            const float gap = 6.0f;
            auto DrawConfigButton = [&](const char* id, const char* label, bool enabled, bool danger, float buttonH) {
                ImGui::SetCursorPosX(controlsX);
                ImVec2 pMin = ImGui::GetCursorScreenPos();
                if (enabled) ImGui::InvisibleButton(id, ImVec2(controlsW, buttonH));
                else ImGui::Dummy(ImVec2(controlsW, buttonH));
                const bool hovered = enabled && ImGui::IsItemHovered();
                const bool rawClicked = enabled && ImGui::IsItemClicked();
                static std::unordered_map<std::string, double> lastConfigAction;
                const double actionNow = ImGui::GetTime();
                double& lastAction = lastConfigAction[id];
                const bool clicked = rawClicked && (lastAction <= 0.0 || actionNow - lastAction >= 0.35);
                if (clicked) lastAction = actionNow;
                ImVec4 base = enabled ? ImVec4(0.060f, 0.060f, 0.068f, 1.0f)
                                      : ImVec4(0.045f, 0.045f, 0.050f, 0.65f);
                if (hovered) base = ImVec4(0.085f, 0.085f, 0.095f, 1.0f);
                dl->AddRectFilled(pMin, ImVec2(pMin.x + controlsW, pMin.y + buttonH),
                    ImGui::GetColorU32(base), 5.0f);
                ImVec2 ts = ImGui::CalcTextSize(label);
                dl->AddText(ImVec2(pMin.x + (controlsW - ts.x) * 0.5f, pMin.y + (buttonH - ts.y) * 0.5f),
                    enabled ? IM_COL32(232, 230, 238, 255) : IM_COL32(100, 98, 110, 210), label);
                if (strcmp(id, "##open_config_folder") == 0 && g_BinaryIconFont) {
                    dl->AddText(g_BinaryIconFont, 16.0f, ImVec2(pMin.x + 14.0f, pMin.y + 9.0f),
                        enabled ? IM_COL32(232, 230, 238, 255) : IM_COL32(100, 98, 110, 210), "\xEF\x81\xBB");
                }
                return clicked;
            };

            std::string newName = g_NewConfigName;
            const bool hasNewName = !newName.empty();
            const bool newNameExists = std::find(g_ConfigList.begin(), g_ConfigList.end(), newName) != g_ConfigList.end();
            static float createConfigAnim = 0.0f;
            createConfigAnim = ImLerp(createConfigAnim, hasNewName ? 1.0f : 0.0f,
                ImClamp(dt * 11.0f, 0.0f, 1.0f));
            const float createEase = createConfigAnim * createConfigAnim * (3.0f - 2.0f * createConfigAnim);
            if (createEase > 0.01f) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gap + (1.0f - createEase) * 7.0f);
                const bool createClicked = DrawConfigButton("##create_config",
                    newNameExists ? "Config already exists" : "Create config", hasNewName && !newNameExists, false, btnH);
                if (createClicked) {
                    SaveConfig(newName);
                    RefreshConfigs();
                    selectedConfig = newName;
                    strcpy_s(g_NewConfigName, "");
                    TriggerNotification("Config created!", newName.c_str(), "CONFIG_CREATE");
                }
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gap);
            const bool hasSelection = !selectedConfig.empty();
            if (DrawConfigButton("##save_config", "Save config", hasSelection, false, btnH)) {
                SaveConfig(selectedConfig); RefreshConfigs();
                TriggerNotification("Config saved!", selectedConfig.c_str(), "CONFIG_SAVE");
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gap);
            if (DrawConfigButton("##load_config", "Load config", hasSelection, false, btnH)) {
                LoadConfig(selectedConfig); g_ActiveConfig = selectedConfig;
                TriggerNotification("Config loaded!", selectedConfig.c_str(), "CONFIG_LOAD");
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gap);
            if (DrawConfigButton("##reset_config", "Reset config", hasSelection, false, btnH)) {
                g_ShowNewConfigModal = true; g_ModalJustOpened = true;
                g_ModalAnim = 0.0f; g_ModalBgAnim = 0.0f;
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + gap);
            if (DrawConfigButton("##delete_config", "Delete config", hasSelection, false, btnH)) {
                const std::string deletedName = selectedConfig;
                DeleteConfig(deletedName);
                selectedConfig = g_ConfigList.empty() ? std::string() : g_ConfigList.front();
                TriggerNotification("Config deleted!", deletedName.c_str(), "CONFIG_DELETE");
            }
            const std::string currentInput = selectedConfig;
            EndpandoraBinaryPanel();
            // RESET MODAL (Drawn outside the panel to overlay it properly)
            if (g_ShowNewConfigModal) {
                g_ModalAnim = ImLerp(g_ModalAnim, 1.0f, dt * 10.0f);
                g_ModalBgAnim = ImLerp(g_ModalBgAnim, 1.0f, dt * 12.0f);

                ImVec2 wMin = ImGui::GetWindowPos(); 
                ImVec2 wMax = ImVec2(wMin.x + ImGui::GetWindowWidth(), wMin.y + ImGui::GetWindowHeight());
                dl->AddRectFilled(wMin, wMax, IM_COL32(0, 0, 0, (int)(170 * g_ModalBgAnim)), 10.f);
                
                float mW = 320, mH = 130;
                float mX = wMin.x + (ImGui::GetWindowWidth() - mW) * 0.5f;
                float mY = wMin.y + (ImGui::GetWindowHeight() - mH) * 0.5f + 10 * (1.0f - g_ModalAnim);
                ImVec2 mMin = {mX, mY}, mMax = {mX + mW, mY + mH};

                if (ImGui::IsMouseClicked(0) && !ImGui::IsMouseHoveringRect(mMin, mMax) && !g_ModalJustOpened) {
                    g_ShowNewConfigModal = false; g_ModalAnim = 0.0f; g_ModalBgAnim = 0.0f;
                }
                g_ModalJustOpened = false;

                ImGui::SetCursorScreenPos(mMin);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(18.f/255.f, 18.f/255.f, 20.f/255.f, g_ModalAnim));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.f);
                ImGui::BeginChild("##ResetModal", ImVec2(mW, mH), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                
                ImDrawList* mdl = ImGui::GetWindowDrawList();
                ImVec2 cmMin = ImGui::GetWindowPos();
                ImVec2 cmMax = {cmMin.x + mW, cmMin.y + mH};
                
                mdl->AddRect(cmMin, cmMax, IM_COL32(50, 50, 55, (int)(200 * g_ModalAnim)), 8.f, 0, 1.f);
                
                const char* mTxt = "Are you sure you want to reset this config?";
                ImVec2 ms = ImGui::CalcTextSize(mTxt);
                mdl->AddText(ImVec2(cmMin.x + (mW - ms.x) * 0.5f, cmMin.y + 35.0f), IM_COL32(230, 230, 235, (int)(255 * g_ModalAnim)), mTxt);

                static float m_confAnim = 0.0f, m_cancAnim = 0.0f;
                
                auto DrawModalBtn = [&](const char* id, const char* text, float x, float& animState, ImU32 bgCol) {
                    ImGui::SetCursorScreenPos(ImVec2(cmMin.x + x, cmMin.y + 75.0f));
                    bool clk = ImGui::InvisibleButton(id, ImVec2(125, 34));
                    bool hov = ImGui::IsItemHovered();
                    animState = ImLerp(animState, hov ? 1.0f : 0.0f, dt * 10.0f);
                    
                    ImVec4 bgF = ImGui::ColorConvertU32ToFloat4(bgCol);
                    ImU32 fill = ImGui::ColorConvertFloat4ToU32(ImVec4(
                        ImLerp(bgF.x, bgF.x * 1.3f, animState),
                        ImLerp(bgF.y, bgF.y * 1.3f, animState),
                        ImLerp(bgF.z, bgF.z * 1.3f, animState),
                        bgF.w * g_ModalAnim
                    ));
                    
                    mdl->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), fill, 6.0f);
                    mdl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(65, 65, 70, (int)(200 * g_ModalAnim)), 6.0f);
                    
                    ImVec2 ts = ImGui::CalcTextSize(text);
                    mdl->AddText(ImVec2(ImGui::GetItemRectMin().x + (125 - ts.x) * 0.5f, ImGui::GetItemRectMin().y + (34 - ts.y) * 0.5f), 
                        IM_COL32(230, 230, 240, (int)(255 * g_ModalAnim)), text);
                    return clk;
                };
                
                if (DrawModalBtn("##conf", "Confirm", 25, m_confAnim, IM_COL32(32, 32, 36, 255))) {
                    ResetAllSettings();
                    SaveConfig(currentInput);
                    RefreshConfigs();
                    g_ShowNewConfigModal = false;
                    TriggerNotification("Config reset!", currentInput.c_str(), "CONFIG");
                }
                if (DrawModalBtn("##canc", "Cancel", 170, m_cancAnim, IM_COL32(32, 32, 36, 255))) {
                    g_ShowNewConfigModal = false;
                }
                
                ImGui::EndChild();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            } else {
                g_ModalAnim = ImLerp(g_ModalAnim, 0.0f, dt * 15.0f);
                g_ModalBgAnim = ImLerp(g_ModalBgAnim, 0.0f, dt * 15.0f);
            }
        } else if (module.name == "Friends") {
            const float colW = (panelW - 18.0f) * .5f;
            BeginpandoraBinaryPanel("Configuration", ImVec2(colW, 425.0f));
            RenderpandoraRawBind("Add Friends Key", &gui_friends_add_bind);
            RenderpandoraRawBind("Add Nearby Friends Key", &gui_friends_nearby_bind);
            RenderpandoraRawBind("Clear Friends Key", &gui_friends_clear_bind);
            const float friendActionWidth = ImMin(220.0f, ImGui::GetContentRegionAvail().x - 28.0f);
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - friendActionWidth) * 0.5f);
            if (ImGui::Button("Clear All Friends", ImVec2(friendActionWidth, 34.0f))) features::friends::clear();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - friendActionWidth) * 0.5f);
            if (ImGui::Button("Remove Last Friend", ImVec2(friendActionWidth, 34.0f))) features::friends::remove_last();
            EndpandoraBinaryPanel();
            ImGui::SameLine();
            BeginpandoraBinaryPanel("Friends", ImVec2(colW, 425.0f));
            {
                const std::vector<std::string> friendNames = features::friends::snapshot_names();
                for (size_t i = 0; i < friendNames.size(); ++i) {
                    ImGui::PushID((int)i);
                    const float rowY = ImGui::GetCursorPosY();
                    ImGui::SetCursorPos(ImVec2(20.0f, rowY));
                    const float rowW = ImGui::GetWindowWidth() - 40.0f;
                    const ImVec2 rowPos = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(rowPos,
                        ImVec2(rowPos.x + rowW, rowPos.y + 34.0f), ImGui::GetColorU32(c::bg::child::background_cap), 5.0f);
                    const std::string& friendName = friendNames[i];
                    const ImVec2 textPos(rowPos.x + 10.0f, rowPos.y + 8.0f);
                    const ImVec4 clipRect(rowPos.x + 8.0f, rowPos.y,
                                          rowPos.x + rowW - 104.0f, rowPos.y + 34.0f);
                    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos,
                        ImGui::GetColorU32(c::text::text_active), friendName.c_str(), nullptr, 0.0f, &clipRect);
                    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 100.0f, rowY + 3.0f));
                    if (ImGui::Button("Remove", ImVec2(80, 28))) { features::friends::remove_at(i); ImGui::PopID(); break; }
                    ImGui::SetCursorPosY(rowY + 42.0f);
                    ImGui::PopID();
                }
            }
            EndpandoraBinaryPanel();
        } else if (module.name == "GUI Color") {
            BeginpandoraBinaryPanel("Interface", ImVec2(panelW, 120.0f));
            ImGui::SetCursorPosX(20.0f);
            DrawCompactColorControl("binary_accent", "Accent Color", gui_guicolor_custom, false,
                                    ImMax(60.0f, ImGui::GetContentRegionAvail().x - 36.0f), 1.0f);
            EndpandoraBinaryPanel();
        } else if (module.name == "Destruct") {
            BeginpandoraBinaryPanel("Unload Client", ImVec2(panelW, 200.0f));
            ImGui::TextWrapped("Hold the button to safely stop every feature and unload pandora.");
            ImGui::Dummy(ImVec2(0, 8.0f));
            const float unloadButtonWidth = ImMin(420.0f, ImGui::GetContentRegionAvail().x);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - unloadButtonWidth) * 0.5f);
            const ImVec2 buttonPos = ImGui::GetCursorScreenPos();
            const ImVec2 buttonSize(unloadButtonWidth, 44.0f);
            ImGui::InvisibleButton("##hold_unload", buttonSize);
            const bool holding = ImGui::IsItemActive() && ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
            constexpr float unloadSeconds = 2.0f;
            if (holding && !g_DestructStarted.load()) g_DestructHoldProgress += ImGui::GetIO().DeltaTime / unloadSeconds;
            else if (!g_DestructStarted.load()) g_DestructHoldProgress = ImMax(0.0f, g_DestructHoldProgress - ImGui::GetIO().DeltaTime * 2.0f);
            g_DestructHoldProgress = ImClamp(g_DestructHoldProgress, 0.0f, 1.0f);
            ImDrawList* dd = ImGui::GetWindowDrawList();
            // Keep the hold progress in the same dark burgundy family as the
            // idle button, only slightly lighter so it remains visible.
            dd->AddRectFilled(buttonPos, ImVec2(buttonPos.x + buttonSize.x, buttonPos.y + buttonSize.y), IM_COL32(31, 22, 26, 255), 8.0f);
            // Fill progress while holding
            if (g_DestructHoldProgress > 0.001f) {
                float fillW = buttonSize.x * g_DestructHoldProgress;
                dd->AddRectFilled(buttonPos, ImVec2(buttonPos.x + fillW, buttonPos.y + buttonSize.y), IM_COL32(62, 31, 41, 250), 8.0f);
            }
            // Border
            dd->AddRect(buttonPos, ImVec2(buttonPos.x + buttonSize.x, buttonPos.y + buttonSize.y), IM_COL32(166, 55, 82, 225), 8.0f);
            // Icon + text
            const char* iconStr = "\xef\x8b\xb5"; // FA f2f5 = right-from-bracket (sign-out-alt)
            const char* labelText = (g_DestructHoldProgress > 0.01f && g_DestructHoldProgress < 1.0f) ? "Hold to unload..." : "Unload client";
            // Color: when holding turn white, otherwise pinkish-red
            ImU32 textCol = (g_DestructHoldProgress > 0.01f) ? IM_COL32(255, 255, 255, 255) : IM_COL32(244, 92, 127, 255);
            // Render icon with icon font
            float iconSize = 16.0f;
            ImVec2 iconSz(0, 0);
            if (g_BinaryIconFont) {
                iconSz = g_BinaryIconFont->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, iconStr);
            }
            const ImVec2 labelSz = ImGui::CalcTextSize(labelText);
            float totalW = iconSz.x + 8.0f + labelSz.x;
            float startX = buttonPos.x + (buttonSize.x - totalW) * 0.5f;
            float textY = buttonPos.y + (buttonSize.y - labelSz.y) * 0.5f;
            if (g_BinaryIconFont) {
                float iconY = buttonPos.y + (buttonSize.y - iconSz.y) * 0.5f;
                dd->AddText(g_BinaryIconFont, iconSize, ImVec2(startX, iconY), textCol, iconStr);
            }
            dd->AddText(ImVec2(startX + iconSz.x + 8.0f, textY), textCol, labelText);
            if (g_DestructHoldProgress >= 1.0f && !g_DestructStarted.exchange(true)) g_ShouldDestruct = true;
            EndpandoraBinaryPanel();
        } else {
            const float gap = 14.0f;
            const float colW = (panelW - gap) * .5f;
            std::vector<Setting*> generalSettings;
            std::vector<Setting*> conditionSettings;
            for (Setting& setting : module.settings) {
                if (!IspandoraBinarySettingVisible(module, setting)) continue;
                if (setting.type == Setting::TOGGLE ||
                    (module.name == "ESP" &&
                     (setting.type == Setting::COLOR || setting.type == Setting::COLOR4)))
                    conditionSettings.push_back(&setting);
                else generalSettings.push_back(&setting);
            }
            // For ESP: sort conditions so toggles appear above color pickers
            if (module.name == "ESP" && !conditionSettings.empty()) {
                std::stable_partition(conditionSettings.begin(), conditionSettings.end(),
                    [](const Setting* s) { return s->type == Setting::TOGGLE; });
            }
            const bool hasCpsRange = module.name == "Left Clicker";
            const bool hasFakeName = module.name == "Nametags" && gui_nametags_use_fake_name;
            const bool splitGeneral = generalSettings.size() > 6;
            const size_t leftGeneralCount = splitGeneral ? (generalSettings.size() + 1) / 2 : generalSettings.size();
            const size_t rightGeneralCount = generalSettings.size() - leftGeneralCount;
            const int leftRows = (int)leftGeneralCount + (hasCpsRange ? 1 : 0) + (hasFakeName ? 1 : 0);
            // Panels are full-height layout sections, not content-sized cards.
            // Keeping them extended to the bottom creates the intended physical
            // gutters between General/Conditions and the surrounding sections.
            const float panelH = ImMax(180.0f, ImGui::GetContentRegionAvail().y - 20.0f);
            const bool hasRightPanel = !conditionSettings.empty() || rightGeneralCount > 0;
            const float singleW = hasRightPanel ? colW : panelW;
            if (leftRows > 0) {
                const bool generalOpen = BeginpandoraBinaryPanel("General", ImVec2(singleW, panelH), true);
                if (generalOpen) {
                    if (module.name == "Left Clicker") RenderBinaryCpsRange(ImMax(80.0f, ImGui::GetContentRegionAvail().x));
                    for (size_t i = 0; i < leftGeneralCount; ++i) {
                        ImGui::SetCursorPosX(20.0f);
                        RenderBinarySetting(*generalSettings[i]);
                    }
                    if (hasFakeName) {
                        ImGui::SetCursorPosX(20.0f);
                        ImGui::TextUnformatted("Fake Name");
                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                        ImGui::InputTextWithHint("##binary_fake_name", "Player name...", gui_nametags_fake_name, sizeof(gui_nametags_fake_name));
                        if (ImGui::IsItemActive()) g_ConfigInputActive = true;
                    }
                }
                EndpandoraBinaryPanel();
            }
            if (hasRightPanel) {
                if (leftRows > 0) ImGui::SameLine(0.0f, gap);
                const bool rightOpen = BeginpandoraBinaryPanel(splitGeneral ? "Misc" : "Conditions",
                    ImVec2(leftRows > 0 ? colW : panelW, panelH), true);
                if (rightOpen) {
                    for (Setting* setting : conditionSettings) {
                        ImGui::SetCursorPosX(20.0f);
                        RenderBinarySetting(*setting);
                    }
                    for (size_t i = leftGeneralCount; i < generalSettings.size(); ++i) {
                        ImGui::SetCursorPosX(20.0f);
                        RenderBinarySetting(*generalSettings[i]);
                    }
                }
                EndpandoraBinaryPanel();
            }
        }
    }
    ImGui::PopStyleVar();
    contentDraw->PopClipRect();
    ImGui::EndChild();

    if (searchAnim > 0.02f) {
        const float searchEase = searchAnim * searchAnim * (3.0f - 2.0f * searchAnim);
        const float cardW = 280.0f * g_GuiScale;
        std::vector<std::pair<int, std::string>> searchResults;
        if (moduleSearch[0]) {
            std::string needle(moduleSearch);
            std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            for (int i=0; i<(int)modules.size() && searchResults.size()<8; ++i) {
                if (modules[i].hidden) continue;
                std::vector<std::string> labels{modules[i].name};
                for (const Setting& setting : modules[i].settings)
                    if (!setting.name.empty() && setting.type != Setting::LABEL)
                        labels.push_back(modules[i].name + "  >  " + setting.name);
                for (const std::string& label : labels) {
                    std::string hay=label;
                    std::transform(hay.begin(),hay.end(),hay.begin(),[](unsigned char c){return (char)std::tolower(c);});
                    if (hay.find(needle)!=std::string::npos) searchResults.emplace_back(i,label);
                    if (searchResults.size()>=8) break;
                }
            }
        }
        const float cardH = (80.0f + 31.0f * searchResults.size()) * g_GuiScale;
        const float cardX = wp.x + menuW - cardW - 12.0f * g_GuiScale;
        const ImVec2 cardPos(cardX,
            wp.y + headerH + 8.0f + (1.0f-searchEase)*8.0f);
        ImGui::SetNextWindowPos(cardPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, searchEase);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
        ImGui::Begin("##pandoraSearchOverlay", nullptr, ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
        ImDrawList* searchDraw=ImGui::GetWindowDrawList();
        const ImVec2 cp=ImGui::GetWindowPos();
        searchDraw->AddRectFilled(ImVec2(cp.x+5.0f,cp.y+6.0f),ImVec2(cp.x+cardW+5.0f,cp.y+cardH+6.0f),IM_COL32(0,0,0,105),8.0f);
        searchDraw->AddRectFilled(cp,ImVec2(cp.x+cardW,cp.y+cardH),IM_COL32(19,19,21,255),8.0f);
        searchDraw->AddRectFilled(cp,ImVec2(cp.x+cardW,cp.y+35.0f*g_GuiScale),IM_COL32(24,24,27,255),8.0f,ImDrawFlags_RoundCornersTop);
        searchDraw->AddRect(cp,ImVec2(cp.x+cardW,cp.y+cardH),IM_COL32(43,43,47,175),8.0f,0,1.0f);
        searchDraw->AddText(ImVec2(cp.x+10.0f*g_GuiScale,cp.y+9.0f*g_GuiScale),IM_COL32(220,220,225,255),"Search results");
        ImGui::SetCursorPos(ImVec2(10.0f*g_GuiScale,43.0f*g_GuiScale));
        ImGui::SetNextItemWidth(cardW-20.0f*g_GuiScale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(8.0f,5.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,IM_COL32(14,14,17,255));
        ImGui::PushStyleColor(ImGuiCol_Border,IM_COL32(38,38,42,180));
        ImGui::InputTextWithHint("##module_search_input","Search...",moduleSearch,IM_ARRAYSIZE(moduleSearch));
        if (ImGui::IsItemActivated()) {
            moduleSearch[0] = '\0';
        }
        g_ConfigInputActive=ImGui::IsItemActive();
        ImGui::PopStyleColor(2); ImGui::PopStyleVar(2);
        float resultY=78.0f*g_GuiScale;
        for (size_t r=0;r<searchResults.size();++r) {
            ImGui::SetCursorPos(ImVec2(8.0f*g_GuiScale,resultY)); ImGui::PushID((int)r+7600);
            ImGui::InvisibleButton("##search_result",ImVec2(cardW-16.0f*g_GuiScale,27.0f*g_GuiScale));
            const ImVec2 a=ImGui::GetItemRectMin(), b=ImGui::GetItemRectMax();
            if (ImGui::IsItemHovered()) searchDraw->AddRectFilled(a,b,IM_COL32(40,40,44,205),5.0f);
            if (ImGui::IsItemClicked()) {
                const int index=searchResults[r].first;
                g_SelectedCategory=modules[index].category; g_SelectedMod=index;
                searchOpen=false; moduleSearch[0]=0;
            }
            searchDraw->PushClipRect(ImVec2(a.x+7.0f,a.y),ImVec2(b.x-7.0f,b.y),true);
            searchDraw->AddText(ImVec2(a.x+7.0f,a.y+5.0f),IM_COL32(205,205,212,255),searchResults[r].second.c_str());
            searchDraw->PopClipRect();
            ImGui::PopID(); resultY+=31.0f*g_GuiScale;
        }
        const bool searchWindowHovered=ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        ImGui::End(); ImGui::PopStyleVar(2);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)||(ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&!searchWindowHovered&&!searchHovered)) {
            searchOpen=false; moduleSearch[0]=0;
        }
    }

    draw->PopClipRect();
    const float animationOriginX = wp.x + menuW * 0.5f;
    const float animationOriginY = wp.y;
    ImGuiContext& animationContext = *ImGui::GetCurrentContext();
    for (ImGuiWindow* animatedWindow : animationContext.Windows) {
        if (!animatedWindow || animatedWindow->RootWindow != menuAnimationRoot) continue;
        ImDrawList* animatedList = animatedWindow->DrawList;
        for (int vertexIndex = 0; vertexIndex < animatedList->VtxBuffer.Size; ++vertexIndex) {
            ImDrawVert& vertex = animatedList->VtxBuffer[vertexIndex];
            vertex.pos.x = animationOriginX + (vertex.pos.x - animationOriginX) * ease;
            vertex.pos.y = animationOriginY + (vertex.pos.y - animationOriginY) * ease;
            const int alpha = (int)(((vertex.col >> IM_COL32_A_SHIFT) & 0xFF) * ease);
            vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
        }
    }
    ImGui::End();
    g_MenuRevealClipActive = false;
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

#include "../altmanager/altmanager.cpp"

#include "w_imgui_port/pandora_adapter.inc"
#include "w_imgui_port/pandora_adapter_extras.inc"

void RenderMenuContents()
{
    const float step = ImClamp(ImGui::GetIO().DeltaTime * 5.0f, 0.0f, 1.0f);
    g_MenuOpenAnim = ImClamp(g_MenuOpenAnim + (g_MenuVisible ? step : -step), 0.0f, 1.0f);
    if (g_AltManagerMode) RenderpandoraAltManager();
    else pandora_w_port::render();
    return;
}

