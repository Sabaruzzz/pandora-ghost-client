#pragma once

namespace framework
{
	class c_menu
	{
	public:
		void watermark();
		void initialize();
		void runtime();

		std::shared_ptr<c_interactive_preview> ip_raw{};
		std::shared_ptr<c_text_object> armor_raw{};
		std::shared_ptr<c_text_object> defuser_raw{};

		std::shared_ptr<c_notify_panel> notify();
	private:
		std::vector<std::shared_ptr<c_window>> m_windows{};

		std::shared_ptr<c_widgets> m_widgets{};
	};
	inline auto g_menu = std::make_unique<c_menu>();
}
