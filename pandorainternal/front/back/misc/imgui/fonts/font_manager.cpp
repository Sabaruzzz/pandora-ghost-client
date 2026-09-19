#include "../../../../../resource.h"
#include "font_manager.h"
#include "Poppins-Bold.h"    
#include "Kamerik 105 W00 Bold.h"  
#include "../fonts.hpp"   
#include <backends/imgui_impl_win32.h>

extern HMODULE myModule;

font_manager::FontManager* font_manager::FontManager::instance = nullptr;

namespace font_manager {

    bool FontManager::initialize_impl(ImGuiIO& io) {
        io.Fonts->Clear();

        bool success = true;
        success &= load_poppins_bold(io);
        success &= load_kamerik(io);
        success &= load_icon_font(io);

        if (!success) {
            default_font = io.Fonts->AddFontDefault();
            if (!ui_font) ui_font = default_font;
            if (!arraylist_font) arraylist_font = default_font;
            if (!watermark_font) watermark_font = default_font;
            if (!icon_font) icon_font = default_font;
        }

        io.Fonts->Build();
        return success;
    }

    bool FontManager::load_poppins_bold(ImGuiIO& io) {
        ImFontConfig config;
        config.OversampleH = 3;
        config.OversampleV = 3;
        config.PixelSnapH = true;
        config.FontDataOwnedByAtlas = false;

#ifdef IMGUI_ENABLE_FREETYPE
        config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint |
            ImGuiFreeTypeBuilderFlags_LightHinting;
#endif

        const float ui_size = 17.0f;

        ImFont* poppins_font = io.Fonts->AddFontFromMemoryTTF(
            (void*)poppins_bold_data,
            (int)poppins_bold_size,
            ui_size,
            &config
        );

        if (!poppins_font) return false;

        default_font = poppins_font;
        ui_font = poppins_font;

        fonts["Default"] = { poppins_font, ui_size, "Default" };
        fonts["UI"] = { poppins_font, ui_size, "UI" };
        fonts["PoppinsBold"] = { poppins_font, ui_size, "PoppinsBold" };

        return true;
    }

    bool FontManager::load_kamerik(ImGuiIO& io) {
        ImFontConfig config;
        config.OversampleH = 3;
        config.OversampleV = 3;
        config.PixelSnapH = false;
        config.FontDataOwnedByAtlas = false;

#ifdef IMGUI_ENABLE_FREETYPE
        config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint |
            ImGuiFreeTypeBuilderFlags_LightHinting;
#endif

        const float arraylist_size = 30.0f;

        ImFont* kamerik_big = io.Fonts->AddFontFromMemoryTTF(
            (void*)kamerik_bold_data,
            (int)kamerik_bold_size,
            arraylist_size,
            &config
        );

        if (!kamerik_big) return false;

        arraylist_font = kamerik_big;
        fonts["ArrayList"] = { kamerik_big, arraylist_size, "ArrayList" };
        fonts["Kamerik"] = { kamerik_big, arraylist_size, "Kamerik" };
        ImFontConfig wm_config = config;
        wm_config.SizePixels = 13.0f;

#ifdef IMGUI_ENABLE_FREETYPE
        wm_config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint |
            ImGuiFreeTypeBuilderFlags_LightHinting;
#endif

        ImFont* kamerik_small = io.Fonts->AddFontFromMemoryTTF(
            (void*)kamerik_bold_data,
            (int)kamerik_bold_size,
            13.0f,
            &wm_config
        );

        if (kamerik_small) {
            watermark_font = kamerik_small;
            fonts["Watermark"] = { kamerik_small, 13.0f, "Watermark" };
        }
        else {
            watermark_font = kamerik_big;
            fonts["Watermark"] = { kamerik_big, arraylist_size, "Watermark" };
        }

        return true;
    }

    bool FontManager::load_icon_font(ImGuiIO& io) {
        if (!hModule) {
            icon_font = default_font;
            return true;
        }

        HRSRC fontResource = FindResourceA(
            hModule,
            MAKEINTRESOURCEA(IDR_FONT_AWESOME_SOLID),
            MAKEINTRESOURCEA(10)
        );

        if (!fontResource) {
            icon_font = default_font;
            return true;
        }

        HGLOBAL loadedFontResource = LoadResource(hModule, fontResource);
        if (!loadedFontResource) {
            icon_font = default_font;
            return true;
        }

        void* fontData = LockResource(loadedFontResource);
        DWORD fontDataSize = SizeofResource(hModule, fontResource);

        if (!fontData || fontDataSize == 0) {
            icon_font = default_font;
            return true;
        }

        ImFontConfig config;
        config.OversampleH = 3;
        config.OversampleV = 3;
        config.PixelSnapH = true;
        config.FontDataOwnedByAtlas = false;

#ifdef IMGUI_ENABLE_FREETYPE
        config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint |
            ImGuiFreeTypeBuilderFlags_LightHinting;
#endif

        static const ImWchar icon_ranges[] = { 0xf000, 0xf8ff, 0 };

        icon_font = io.Fonts->AddFontFromMemoryTTF(
            fontData,
            (int)fontDataSize,
            18.0f,
            &config,
            icon_ranges
        );

        if (icon_font) {
            fonts["Icons"] = { icon_font, 18.0f, "Icons" };
        }
        else {
            icon_font = default_font;
        }

        return true;
    }

    ImFont* FontManager::get_font(const std::string& name) const {
        auto it = fonts.find(name);
        if (it != fonts.end()) {
            return it->second.font;
        }
        return default_font;
    }

    void FontManager::push_font(const std::string& name) {
        ImFont* font = get_font(name);
        ImGui::PushFont(font ? font : default_font);
    }

    void FontManager::shutdown() {
        fonts.clear();
        default_font = nullptr;
        ui_font = nullptr;
        arraylist_font = nullptr;
        watermark_font = nullptr;
        icon_font = nullptr;
        hModule = nullptr;
    }

} 
