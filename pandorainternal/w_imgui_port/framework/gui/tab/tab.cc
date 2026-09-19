#include "../../../includes.hh"

namespace framework
{
	c_tab::c_tab(math::c_vector_2d pos, math::c_vector_2d size, utils::anim_context_t parent) : m_pos(pos), m_size(size), parent_opcity(parent) { }

	void c_tab::paint()
	{
		if (g_ctx->m_tabs.empty())
		{
			slog::log::warn("[!] no tabs have been created, skipping tab rendering");

			// we do not need to run the tabs if there are none
			return;
		}

		int header_height = 45;
		int tab_spacing = 20;

		int total_tab_width = 0;
		for (const auto& tabs : g_ctx->m_tabs)
		{
			int tab_width = g_font->f_icons_medium.measure(tabs.icon).x;
			total_tab_width += tab_width + tab_spacing;
		}

		// remove last spacing
		total_tab_width -= tab_spacing;

		int x = this->m_pos.x + (this->m_size.x / 2.0f) - (total_tab_width / 2.0f);

		int prev_x{};

		for (int i = 0; i < g_ctx->m_tabs.size(); i++) {
			auto& tab = g_ctx->m_tabs[i];

			math::c_vector_2d text_size = g_font->f_icons_medium.measure(tab.icon);
			math::c_rect bounding = math::c_rect(x, this->m_pos.y + 8, text_size.x, text_size.y);

			if (g_input->mouse_in_region(bounding.pos(), bounding.size()) && g_input->clicked(input::mouse_buttons::left))
			{
				g_ctx->clear_non_modal_focus();
				g_ctx->m_active_tab = i;
				g_ctx->m_cur_tab = tab.m_name;

				prev_x = x;
			}

			animations::m_tab_switching = utils::builder::create_animation_ctx(tab.m_name + utils::builder::get_id(i), (g_ctx->m_active_tab == i) && g_ctx->m_open, 0.5);

			g_font->f_icons_medium.text((float)x, this->m_pos.y + (header_height * 0.5) - (g_font->f_icons_medium.measure(tab.icon).y * 0.5), tab.icon,
				g_style->m_text.modulate(this->parent_opcity.limit(0.5).val()).lerp(g_style->m_accent.modulate(this->parent_opcity.limit(1.f).val()), animations::m_tab_switching.val()));

			float anim = animations::m_tab_switching.val();
			float full_w = text_size.x;
			float cur_w = full_w * anim;
			float cur_x = x + (full_w - cur_w) * 0.5f;

			g_render->rect_shadow(cur_x, this->m_pos.y + 43.f, cur_w, 2, g_style->m_accent.modulate(anim), 5, 3);
			g_render->rect_filled(cur_x, this->m_pos.y + 43.f, cur_w, 2, g_style->m_accent.modulate(anim), 3, engine::draw_flags_::draw_flags_round_corners_top);

			// update tab pos
			x += text_size.x + tab_spacing;
		}
	}

	void c_tab::update_input(math::c_vector_2d pos, math::c_vector_2d size)
	{
		this->m_pos = pos;
		this->m_size = size;
	}

	void c_tab::create_tab(std::string icon, std::string name, std::vector<std::string> subtabs)
	{
		g_ctx->m_tabs.push_back({ name, icon, subtabs });
		slog::log::success("[+] a new tab has been created: {} with {} subtabs", name, subtabs.size());
	}
}
