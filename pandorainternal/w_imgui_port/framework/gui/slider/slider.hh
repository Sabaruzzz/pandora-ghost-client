#pragma once

namespace framework
{
	class c_slider_float : public c_base_element {
	public:
		c_slider_float(std::string label, float* val, float min, float max, bool hide_label = false, std::wstring prefix = L"");

		void input() override;
		void draw() override;
		void set_range_max(float* value) { m_range_max = value; }
	private:
		float* m_val{};
		float* m_range_max{};
		bool m_dragging_range_max{};
		float m_min{}, m_max{};

		float m_focus_anim = 0.f;
		float m_display_width = 0.f;

		std::wstring m_prefix{};
	};

	class c_slider_int : public c_base_element {
	public:
		c_slider_int(std::string label, int* val, int min, int max, bool hide_label = false, std::wstring prefix = L"");

		void input() override;
		void draw() override;
	private:
		int* m_val{};
		int m_min{}, m_max{};

		float m_focus_anim = 0.f;
		float m_display_width = 0.f;

		std::wstring m_prefix{};
	};
}
