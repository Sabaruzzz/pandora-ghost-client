#include "../../../includes.hh"
#include <d3dcompiler.h>
#include <GL/gl.h>
#include "../../../render/stb/stb_image.hh"
#include "../../../../assets/pandoralogo_png.hpp"

#define ICON_FA_SEARCH "\xEF\x80\x82"
#define ICON_FA_pandoraFLAKE "\xef\x8b\x9c"

namespace framework
{
	static constexpr float k_account_footer_height = 50.f;
	static constexpr float k_content_bottom_padding = 10.f;

	c_window::c_window(std::string title, math::c_vector_2d pos, math::c_vector_2d size) : m_title(title), m_pos(pos), m_size(size)
	{ 
		slog::log::success("[+] c_window:: {} created at pos: {}, {}, with size: {}, {}", m_title, m_pos.x, m_pos.y, m_size.x, m_size.y);
	}

	
	void c_window::paint()
	{
		extern void UpdatePlayerHeadTextureOnRenderThread();
		extern GLuint g_PlayerHeadTexture;
		extern std::string g_CachedPlayerName;
		UpdatePlayerHeadTextureOnRenderThread();
		float t = animations::m_window_opacity.val();
		float eased = t * t * (3.f - 2.f * t);

		float origin_y = this->m_pos.y;
		float origin_x = this->m_pos.x + this->m_size.x * 0.5f;

		// middle layer doesn't pass ignore_clipping so it stays on m_draw_list
		ImDrawList* dl = g_render->draw_list();
		int vtx_start = dl->VtxBuffer.Size;

		g_render->rect_shadow(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_shadow.modulate(this->m_window_opacity.limit(0.5).val()), 15.f, 15.f);
		
		this->m_window_opacity.restore();
		
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_background.modulate(this->m_window_opacity.val()), 15.f);

		g_render->push_clip(this->m_pos.x, this->m_pos.y, this->m_size.x, 45);
		g_render->rect_shadow(this->m_pos.x + 45, this->m_pos.y + 14, 20, 2, g_style->m_accent.modulate(this->m_window_opacity.limit(0.15).val()), 70.f, 0.f);
		g_render->restore_clip();

		g_render->gradient(this->m_pos.x, this->m_pos.y + 45, this->m_size.x, 10, g_style->m_window_shadow.modulate(this->m_window_opacity.limit(0.2).val()), g_style->m_window_shadow.modulate(this->m_window_opacity.limit(0.1).val()).with_alpha(0), engine::fade_direction::horizontally);
		
		this->m_window_opacity.restore();

        extern GLuint g_PandoraLogoTexture;
        extern int g_PandoraLogoWidth;
        extern int g_PandoraLogoHeight;
        
        if (g_PandoraLogoTexture == 0) {
            int channels;
            // stbi_load_from_memory comes from the included stbi
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
        
        float titleXOffset = this->m_pos.x + 18.0f;
        
        if (g_PandoraLogoTexture != 0 && g_PandoraLogoTexture != 0xFFFFFFFF) {
            ImVec2 logoSize(36, 36);
            ImDrawList* draw = g_render->draw_list();
            draw->AddImage((ImTextureID)(intptr_t)g_PandoraLogoTexture, 
                ImVec2(this->m_pos.x + 6.0f, this->m_pos.y + (45.0f * 0.5f) - (logoSize.y * 0.5f)), 
                ImVec2(this->m_pos.x + 6.0f + logoSize.x, this->m_pos.y + (45.0f * 0.5f) - (logoSize.y * 0.5f) + logoSize.y),
                ImVec2(0, 0), ImVec2(1, 1),
                g_style->m_accent.modulate(this->m_window_opacity.val()).get_u32());
            titleXOffset = this->m_pos.x + 6.0f + logoSize.x + 4.0f;
        }

		g_font->f_default.text(titleXOffset, this->m_pos.y + 13, this->m_title, g_style->m_text.modulate(this->m_window_opacity.limit(0.6).val()));
		this->m_window_opacity.restore();

		// Tabs belong to the header and must be painted before the content clip.
		this->m_obj_tab->paint();
	
		// search input 
		{
			// schizo
			math::c_rect bounding = math::c_rect(this->m_pos.x - 12 + this->m_size.x - g_font->f_icons_medium.measure(ICON_FA_SEARCH).x, this->m_pos.y + +(45 * 0.5) - (g_font->f_icons_medium.measure(ICON_FA_SEARCH).y * 0.5), g_font->f_icons_medium.measure(ICON_FA_SEARCH).x, g_font->f_icons_medium.measure(ICON_FA_SEARCH).y);
			if (g_input->mouse_in_region(bounding.pos(), bounding.size()) &&
				g_input->clicked(input::mouse_buttons::left) && !g_ctx->m_click_consumed)
			{
				g_search.m_should_draw = !g_search.m_should_draw;
				if (!g_search.m_should_draw) g_search.cleanup();
				g_ctx->m_click_consumed = true;
			}

			g_search.m_search_anim = utils::builder::create_animation_ctx("search_anim_flow", g_search.m_should_draw && g_ctx->m_open, 0.5f);
		}

		// just gheto show it
		g_font->f_icons_medium.text(this->m_pos.x - 12 + this->m_size.x - g_font->f_icons_medium.measure(ICON_FA_SEARCH).x, this->m_pos.y + +(45 * 0.5) - (g_font->f_icons_medium.measure(ICON_FA_SEARCH).y * 0.5), ICON_FA_SEARCH, 
			g_style->m_text.modulate(this->m_window_opacity.limit(0.2).val()).lerp(g_style->m_accent.modulate(this->m_window_opacity.limit(0.8).val()), g_search.m_search_anim.val()));
		this->m_window_opacity.restore();
		// Keep every module panel inside the W window while the category scrolls.
		g_render->push_clip(this->m_pos.x, this->m_pos.y + 45, this->m_size.x, this->m_size.y - 45 - k_account_footer_height);
		this->setup_objects();
		
		g_render->restore_clip();

		// Fixed account footer. It is outside the scrolling content and uses a
		// render-thread-owned texture, so changing tabs or scrolling cannot move it.
		const float footer_y = this->m_pos.y + this->m_size.y - k_account_footer_height;
		g_render->rect_filled(this->m_pos.x, footer_y, this->m_size.x, k_account_footer_height,
			g_style->m_window_background.modulate(this->m_window_opacity.val()), 15.f,
			engine::draw_flags_round_corners_bottom);
		g_render->rect_filled(this->m_pos.x + 1.f, footer_y, this->m_size.x - 2.f, 1.f,
			g_style->m_window_shadow.modulate(this->m_window_opacity.limit(0.35f).val()));
		const ImVec2 head_min(this->m_pos.x + 10.f, footer_y + 7.f);
		const ImVec2 head_max(head_min.x + 36.f, head_min.y + 36.f);
		if (g_PlayerHeadTexture) {
			// Skin avatars are pixel art with a transparent helmet layer. Preserve
			// the complete square instead of clipping its corners.
			dl->AddImage((ImTextureID)(intptr_t)g_PlayerHeadTexture, head_min, head_max,
				ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255,
					(int)(255.f * this->m_window_opacity.val())));
		} else {
			g_render->rect_filled(head_min.x, head_min.y, 36.f, 36.f,
				g_style->m_accent.modulate(this->m_window_opacity.limit(0.22f).val()), 3.f);
		}
		const std::string footer_name = g_CachedPlayerName.empty() ? "Player" : g_CachedPlayerName;
		g_font->f_default.text(head_max.x + 10.f, footer_y + 8.f, footer_name,
			g_style->m_text.modulate(this->m_window_opacity.limit(0.78f).val()));
		g_font->f_default.text(head_max.x + 10.f, footer_y + 27.f, "Expiration: Lifetime",
			g_style->m_text.modulate(this->m_window_opacity.limit(0.38f).val()));

		g_search.set_pos({ this->m_pos.x + this->m_size.x - 220, this->m_pos.y + 37 });

		// this->m_preview->update(this->m_pos + math::c_vector_2d(this->m_size.x + 15, 0), math::c_vector_2d(300, this->m_size.y));
		// this->m_preview->draw();

		int vtx_end = dl->VtxBuffer.Size;

		ImDrawVert* verts = dl->VtxBuffer.Data;
		for (int v = vtx_start; v < vtx_end; v++)
		{
			verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
			verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

			int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
			verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
		}
	}

	void c_window::setup_objects()
	{
		const float layout_width = this->m_size.x;
		const float full_width = layout_width - this->child_padding().x - 10.f;
		const float column_gap = 10.f * ImGui::GetIO().FontGlobalScale;
		const float half_width = (layout_width - this->child_padding().x - 24.f) * 0.5f;
		float column_y[2] = { 0.f, 0.f };
		int half_child_count = 0;

		for (auto& child : this->m_childrens)
		{
			if (child->get_type() == child_width::half) child->m_size.x = half_width;
			else child->m_size.x = full_width;

			child->m_child_opacity = utils::builder::create_animation_ctx(
				child->m_name + "_child_opacity", child->visible() && g_ctx->m_open, 0.5f);

			if (child->m_child_opacity.val() <= 0.f) continue;
			if (!child->visible()) continue;

			if (child->get_type() == child_width::full)
			{
				const float y = std::max(column_y[0], column_y[1]);
				child->m_relative_pos = math::c_vector_2d(0.f, y - m_content_scroll_offset);
				column_y[0] = column_y[1] = y + child->m_size.y + column_gap;
				half_child_count = 0;
			}
			else
			{
				const int column = half_child_count % 2;
				child->m_relative_pos = math::c_vector_2d(
					column * (half_width + column_gap), column_y[column] - m_content_scroll_offset);
				column_y[column] += child->m_size.y + column_gap;
				half_child_count++;
			}

			child->m_pos = child->m_relative_pos + (this->m_pos + subtab_padding());
			child->input();
			child->draw();
		}

		const float content_height = std::max(column_y[0], column_y[1]);
		const float viewport_height = this->m_size.y - subtab_padding().y - k_account_footer_height - k_content_bottom_padding;
		m_content_scroll_max = std::max(0.f, content_height - viewport_height);
		m_content_scroll_target = std::clamp(m_content_scroll_target, 0.f, m_content_scroll_max);
		m_content_scroll_offset += (m_content_scroll_target - m_content_scroll_offset) * 0.18f;
		if (std::abs(m_content_scroll_target - m_content_scroll_offset) < 0.35f)
			m_content_scroll_offset = m_content_scroll_target;
		if (m_content_scroll_max > 0.f) {
			const float track_h = viewport_height;
			const float thumb_h = std::max(32.f, track_h * (viewport_height / content_height));
			const float thumb_y = (m_content_scroll_offset / m_content_scroll_max) * (track_h - thumb_h);
			const float x = this->m_pos.x + layout_width - 6.f;
			g_render->rect_filled(x, this->m_pos.y + subtab_padding().y, 2.f, track_h, hue::c_color(45,45,50).modulate(0.55f), 2.f);
			g_render->rect_shadow(x - 1.f, this->m_pos.y + subtab_padding().y + thumb_y, 4.f, thumb_h, g_style->m_accent.modulate(0.35f), 7.f, 2.f);
			g_render->rect_filled(x - 1.f, this->m_pos.y + subtab_padding().y + thumb_y, 4.f, thumb_h, g_style->m_accent.modulate(0.8f), 2.f);
            
			bool show_shadow = m_content_scroll_offset < m_content_scroll_max - 1.f;
			auto shadow_anim = utils::builder::create_animation_ctx("scroll_shadow_anim", show_shadow, 0.5f);
			if (shadow_anim.val() > 0.01f) {
				g_render->fade_rect_filled(this->m_pos.x + subtab_padding().x, this->m_pos.y + this->m_size.y - k_account_footer_height - 28.f, layout_width - subtab_padding().x * 2.f, 28.f, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, (int)(105 * shadow_anim.val())), engine::fade_direction::vertically, 0.f);
			}
		}
	}
	void c_window::input()
	{
		// pandora owns the configurable menu key; W consumes its open state.

		this->m_window_opacity = utils::builder::create_animation_ctx(this->m_title, g_ctx->m_open, 0.5f);
		this->m_obj_tab->parent_opcity = this->m_window_opacity;

		animations::m_window_opacity = this->m_window_opacity;

		if (!g_ctx->m_open)
		{
			g_ctx->clear_non_modal_focus();
			return;
		}

		static math::c_vector_2d prev_mouse_pos{}, delta{};

		// bounding for input
		// Keep the drag handle limited to the brand/title area. The old region
		// overlapped the first tab icons and swallowed their clicks.
		math::c_rect bounding = math::c_rect(this->m_pos.x, this->m_pos.y, 108, 30);

		// update this shit everyframe
		delta = prev_mouse_pos - g_input->get_mouse_position();

		// check g_ctx->m_dragging state and update it if we are in the corr region
		if (!g_ctx->m_dragging &&
			g_input->mouse_in_region(bounding.pos(), bounding.size()) &&
			g_input->clicked(input::mouse_buttons::left))
		{
			g_ctx->m_dragging = true;
			g_ctx->m_click_consumed = true;
		}
		else if (g_ctx->m_dragging && g_input->click_down(input::mouse_buttons::left))
		{
			this->m_pos -= delta;
		}
		else if (g_ctx->m_dragging && !g_input->click_down(input::mouse_buttons::left))
		{
			g_ctx->m_dragging = false;
		}

		prev_mouse_pos = g_input->get_mouse_position();

		const math::c_vector_2d content_pos = this->m_pos + subtab_padding();
		const math::c_vector_2d content_size = { this->m_size.x - 15.f, this->m_size.y - subtab_padding().y - k_account_footer_height - k_content_bottom_padding };
		bool mouse_over_child = false;
		for (const auto& child : m_childrens) {
			if (child && child->visible() && g_input->mouse_in_region(child->m_pos, child->m_size)) {
				mouse_over_child = true;
				break;
			}
		}
		if (!mouse_over_child && m_content_scroll_max > 0.f && g_input->mouse_in_region(content_pos, content_size)) {
			const float wheel = ImGui::GetIO().MouseWheel;
			if (wheel != 0.f && g_ctx->m_modal_owner == nullptr) {
				g_ctx->clear_non_modal_focus();
				m_content_scroll_target = std::clamp(m_content_scroll_target - wheel * 70.f, 0.f, m_content_scroll_max);
			}
		}

		// update tab position when window moves
		this->m_obj_tab->update_input(this->m_pos, this->m_size);
	}

	std::shared_ptr<c_child> c_window::build_child(std::string name, child_width width, float y, std::function<void(c_child* ptr)> callback)
	{
		auto child = std::make_shared<c_child>(name, width, y);
		{
			if (!child)
			{
				slog::log::error("failed to create child: {}", name);
				return nullptr;
			}

			// attach the pos to the window's, remains to get updated in window's input but we must have atleat framed to it the moment of creation
			child->m_pos = child->m_relative_pos + this->m_pos + subtab_padding();

			// we do not call draw and input here as this only gets called once, well we just do the child caching
			this->m_childrens.push_back(child);

			callback(child.get());

			slog::log::success("[framework::c_window] created child: {}", name);
		}
		return child;
	}

	std::shared_ptr<c_tab> c_window::prebuild_tabs(std::function<void(c_tab* ptr)> callback)
	{
		auto obj = std::make_shared<c_tab>(this->m_pos, this->m_size, this->m_window_opacity);
		{
			slog::log::success("[+] prebuilded tab pointer");

			// pass the pointer
			callback(obj.get());

			// pass the pointer
			this->m_obj_tab = obj;
		}
		return obj;
	}

	void c_window::finish_tab_prebuild()
	{
		// we surely attached it now we have to update it so we wont have problems with childs, assuming the menu just initialized we dont have the problem that user interacted with it yet
		// we can safely asume that the first tab and subtab are active as menu just initialized, this way the data we pass when we attach child to tab will be valid
		g_ctx->m_cur_tab = g_ctx->m_tabs[0].m_name;

		if (g_ctx->m_tabs[0].m_subtab.empty())
		{
			g_ctx->m_tabs[0].m_cur_subtab = "";
		}
	}

	math::c_vector_2d c_window::subtab_padding()
	{
		return math::c_vector_2d(15, 60);
	}

	math::c_vector_2d c_window::child_padding()
	{
		return math::c_vector_2d(20, 65);
	}
}
