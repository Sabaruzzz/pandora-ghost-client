#include "../../../includes.hh"

namespace framework
{
    static constexpr float k_fade_dur = 0.15f;
    static constexpr float k_offset_px = 3.0f;

    static float ease_out_cubic(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return 1.f - std::pow(1.f - t, 3.f);
    }

    static std::string to_upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return toupper(c); });
        return s;
    }

    static float ease_in_cubic(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * t;
    }

    c_text_input::c_text_input(std::string label, std::string* var, bool hide_label)
        : m_var(var)
    {
        m_label = std::move(label);
        m_hide_label = hide_label;
        m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 30 };
        m_type = element_type::text_input;
        m_focus_priority = focus_priority::interactive;
        m_parent_width = m_child_size;
    }

    void c_text_input::draw()
    {
        animations::m_textinput_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
        animations::m_textinput_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_value", m_visible && g_ctx->top_focus() == this && g_ctx->m_open, 0.5f);
        animations::m_textinput_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

        float target_opacity = 0.2f;
        if (animations::m_textinput_value.val() > 0.f)
            target_opacity = 0.2f + (0.6f * animations::m_textinput_value.val());
        else if (animations::m_textinput_hover.val() > 0.f)
            target_opacity = 0.2f + (0.2f * animations::m_textinput_hover.val());

        static std::unordered_map<std::string, float> smooth_opacity_cache;
        float& smooth_opacity = smooth_opacity_cache[m_parent + m_label + "#smooth_opacity"];
        smooth_opacity += (target_opacity - smooth_opacity) * 0.3f;

        const float final_opacity = animations::m_checkbox_opacity.val() * smooth_opacity;
        auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);


        g_render->use_layer(m_layer, [&]()
            {
                if (!m_hide_label)
                    g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

                const auto box_pos = m_pos + position;

                g_render->rect_shadow((m_pos + position).x, (m_pos + position).y, m_child_size, 25.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3).val()), 8.f, 3.f);
                animations::m_window_opacity.restore();
                g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, 25.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 3.f);

                if (g_ctx->is_focused(this))
                {
                    while (m_char_anims.size() < this->m_var->size())
                    {
                        m_char_anims.push_back(0.f);
                        m_char_removing.push_back(false);
                    }

                    float dt = ImGui::GetIO().DeltaTime;
                    float speed = 12.f;

                    float cursor_x = this->m_pos.x + 8.f;
                    float base_y = this->m_pos.y + g_font->f_childs.measure(m_label).y + 8.f;

                    for (int i = 0; i < (int)m_ghost_chars.size(); i++)
                    {
                        auto& ghost = m_ghost_chars[i];
                        ghost.m_anim += (0.f - ghost.m_anim) * dt * speed;

                        if (ghost.m_anim < 0.01f) {
                            m_ghost_chars.erase(m_ghost_chars.begin() + i);
                            i--;
                            continue;
                        }

                        float t = ghost.m_anim;
                        float eased = t * t * (3.f - 2.f * t);

                        char buf[2] = { ghost.m_glyph, '\0' };
                        auto char_size = g_font->f_childs.measure(buf);
                        float char_center_x = ghost.m_x + char_size.x * 0.5f;
                        float char_center_y = base_y + char_size.y * 0.5f;

                        int vtx_start = g_render->draw_list()->VtxBuffer.Size;

                        g_font->f_childs.text(ghost.m_x, base_y, buf,
                            g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                        animations::m_window_opacity.restore();

                        int vtx_end = g_render->draw_list()->VtxBuffer.Size;

                        ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
                        for (int v = vtx_start; v < vtx_end; v++)
                        {
                            verts[v].pos.x = char_center_x + (verts[v].pos.x - char_center_x) * eased;
                            verts[v].pos.y = char_center_y + (verts[v].pos.y - char_center_y) * eased;

                            int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
                            verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                        }
                    }

                    for (int i = 0; i < (int)m_char_anims.size(); i++)
                    {
                        bool is_active = i < (int)this->m_var->size();

                        float target = is_active ? 1.f : 0.f;
                        m_char_anims[i] += (target - m_char_anims[i]) * dt * speed;

                        if (!is_active && m_char_anims[i] < 0.01f)
                        {
                            m_char_anims.erase(m_char_anims.begin() + i);
                            m_char_removing.erase(m_char_removing.begin() + i);
                            i--;
                            continue;
                        }

                        float t = m_char_anims[i];
                        float eased = t * t * (3.f - 2.f * t);

                        char buf[2] = { is_active ? (*this->m_var)[i] : ' ', '\0' };
                        auto char_size = g_font->f_childs.measure(buf);
                        float char_center_x = cursor_x + char_size.x * 0.5f;
                        float char_center_y = base_y + char_size.y * 0.5f;

                        int vtx_start = g_render->draw_list()->VtxBuffer.Size;

                        g_font->f_childs.text(cursor_x, base_y, buf,
                            g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                        animations::m_window_opacity.restore();

                        int vtx_end = g_render->draw_list()->VtxBuffer.Size;

                        ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
                        for (int v = vtx_start; v < vtx_end; v++)
                        {
                            verts[v].pos.x = char_center_x + (verts[v].pos.x - char_center_x) * eased;
                            verts[v].pos.y = char_center_y + (verts[v].pos.y - char_center_y) * eased;

                            int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
                            verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
                        }

                        cursor_x += char_size.x;
                    }

                    std::string to_show{};
                }
                else
                {
                    std::string display_text = this->m_var->empty() && !g_ctx->is_focused(this) ? m_label : *this->m_var;
                    if (g_ctx->is_focused(this) && (int)(ImGui::GetTime() * 2.0f) % 2 == 0) {
                        display_text += "_";
                    }
                    g_font->f_childs.text(this->m_pos.x + 8, this->m_pos.y + g_font->f_childs.measure(m_label).y + 8.f, display_text, g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                }
            });

       
    }

    void c_text_input::input()
    {
        auto position = m_hide_label
            ? math::c_vector_2d(0, 0)
            : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

        math::c_rect bounding = math::c_rect(
            m_pos + math::c_vector_2d(0, position.y),
            math::c_vector_2d(m_child_size, 20.f));

        if (!g_ctx->can_interact(this, m_focus_priority))
            return;

       //if (g_ctx->m_focus_took != nullptr && g_ctx->m_focus_took != this)
       //    return;

        if (g_input->mouse_in_region(bounding.pos(), bounding.size())) {
            g_ctx->m_hovered = this;
        } else if (g_ctx->m_hovered == this) {
            g_ctx->m_hovered = nullptr;
        }

        if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
        {
            g_ctx->push_focus(this, m_focus_priority);
            g_ctx->m_click_consumed = true;
        }

        if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered != this) {
            g_ctx->pop_focus(this);
        }

        if (!g_ctx->is_focused(this))
            return;

        const float now = (float)ImGui::GetTime();

        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            g_ctx->pop_focus(this);
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, true)) {
            if (!this->m_var->empty()) {
                int last = (int)this->m_var->size() - 1;

                // snapshot the dying char
                if (last < (int)m_char_anims.size()) {
                    ghost_char_t ghost{};
                    ghost.m_glyph = (*this->m_var)[last];
                    ghost.m_anim = m_char_anims[last]; // inherit current anim value

                    // calculate its x at time of removal
                    float gx = this->m_pos.x + 8.f;
                    for (int c = 0; c < last; c++) {
                        char tmp[2] = { (*this->m_var)[c], '\0' };
                        gx += g_font->f_childs.measure(tmp).x;
                    }
                    ghost.m_x = gx;

                    m_ghost_chars.push_back(ghost);
                    m_char_anims.erase(m_char_anims.begin() + last);
                    m_char_removing.erase(m_char_removing.begin() + last);
                }

                this->m_var->erase(last, 1);
            }
        }

        for (int n = 0; n < io.InputQueueCharacters.Size; n++) {
            unsigned int c = (unsigned int)io.InputQueueCharacters[n];
            if (c != 0 && c >= 32 && c <= 255) {
                this->m_var->push_back((char)c);
            }
        }
    }
}
