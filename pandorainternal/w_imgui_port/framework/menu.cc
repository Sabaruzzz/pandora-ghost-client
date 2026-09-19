#include "../includes.hh"

#define half_height 255
#define full_height 525

static bool materials_expand{ false };
static std::vector<std::string> materials{ "Flat", "Shaded", "Glow", "Galaxy", "Metallic" };
// materials = { "Flat", "Shaded", "Glow", "Galaxy", "Metallic" };

namespace framework
{
	void c_menu::initialize()
	{
		slog::log::info("[>] started menu initialization");

		auto window = std::make_shared<c_window>("Electro", math::c_vector_2d((core::g_overlay->width * 0.5) - 350, (core::g_overlay->height * 0.5) - 300), math::c_vector_2d(700, 600));
		{
			if (window == nullptr)
			{
				slog::log::error("[-] failed to create window");
			}
			else // safe conditions
			{
				window->prebuild_tabs([](framework::c_tab* controller) {
					controller->create_tab(ICON_FA_USER, "Players", { });
					controller->create_tab(ICON_FA_SUN, "Visuals", { });
					controller->create_tab(ICON_FA_GEAR, "Overlay", { });
					controller->create_tab(ICON_FA_FOLDER, "Config", { });
					});
				window->finish_tab_prebuild();

				window->build_child("Main", framework::child_width::half, half_height, [](framework::c_child* controller) {
					controller->attach_child("Players", "", 0);

					controller->add_checkbox("Enable", &core::g_config.esp);
					controller->add_keybind("Esp key", &core::g_config.esp_activation)->set_inlined();
					controller->add_checkbox("Customize invisible esp", &core::g_config.customize_invisible_esp);

					controller->add_checkbox("Bounding box", &core::g_config.box);
					controller->add_checkbox("Name", &core::g_config.name);
					controller->add_checkbox("Weapon", &core::g_config.weapon);
					controller->add_checkbox("Health", &core::g_config.health);
					controller->add_checkbox("Ammo", &core::g_config.ammo);
					controller->add_checkbox("OOF", &core::g_config.offscreen_esp);
					controller->add_colorpicker("OOF color", &core::g_config.offscreen_color)->set_inlined();

					controller->add_checkbox("Skeleton", &core::g_config.skeleton);
					controller->add_colorpicker("Skeleton color", &core::g_config.skeleton_color)->set_inlined();
					controller->add_colorpicker("Skeleton color invisible", &core::g_config.skeleton_color_inv)->set_callback_and_inline([] {return core::g_config.customize_invisible_esp; });

					controller->add_popup("Skeleton options", false, [](framework::c_popup* callback)
					{
						callback->add_dropdown("Skeleton type", &core::g_config.skeleton_type, { "Normal", "Curved" });

						callback->add_checkbox("Skeleton fade", &core::g_config.gradient_skeleton)
							->set_callback_visibility([] { return core::g_config.skeleton_type == 0; });
						callback->add_colorpicker("Skeleton s-color", &core::g_config.skeleton_color_2)->set_inlined();
						callback->add_colorpicker("Skeleton s-color invisible", &core::g_config.skeleton_color_2_inv)->set_callback_and_inline([] {return core::g_config.customize_invisible_esp; });
					})->set_inlined();

					controller->add_checkbox("Flags", &core::g_config.esp_informations);
					controller->add_popup("Flags options", false, [](framework::c_popup* callback)
						{
							callback->add_multibox("Informations", false, [](framework::c_multidropdown* call)
								{
									call->add_selection("Money", &core::g_config.flags[0]);
									call->add_selection("Armor", &core::g_config.flags[1]);
									call->add_selection("Defuser", &core::g_config.flags[2]);

								});
						})->set_inlined();
				});

				window->build_child("Chams", framework::child_width::half, half_height, [](framework::c_child* controller) {
					controller->attach_child("Players", "", 0);


					controller->add_checkbox("Expand materials", &materials_expand);

					controller->stack_calls([&]() {
						// obsidian, chrome, plasma, inferno, void

						if (materials_expand)
							materials = { "Flat","Shaded","Glow","Galaxy","Metallic","Holographic",
										  "Thunder","Flows","Gradient","Lava","Aurora",
										  "Obsidian","Chrome","Plasma","Inferno","Void",
										  "Electric","Cyan Fire","Iridescent Shell","Magma Chamber","UV Reactive",
										  "Shadow Realm","Frozen Lightning","Ember Trail","Magma Skin" };
						else
							materials = { "Flat", "Shaded", "Glow", "Galaxy", "Metallic" };
							//materials = { "Flat", "Shaded", "Glow", "Galaxy", "Metallic" };
						});

					controller->add_checkbox("Chams", &core::g_config.chams);
					controller->add_colorpicker("Chams color", &core::g_config.chams_color)->set_inlined();
					controller->add_popup("Chams options", false, [](framework::c_popup* callback)
						{
							callback->add_dropdown("Chams type", &core::g_config.chams_type,
								materials)->execute_stack([] { return materials; });
						
							callback->add_colorpicker("Second color", &core::g_config.chams_color_rim);
								// ->set_callback_visibility([&] {return (core::g_config.chams_type == 2 || core::g_config.chams_type == 3 || core::g_config.chams_type == 6 || core::g_config.chams_type == 8 || core::g_config.chams_type == 9); });
							callback->add_checkbox("Wireframe", &core::g_config.chams_wireframe);
							callback->add_colorpicker("Wire color", &core::g_config.chams_color_wire)->set_inlined();
						})->set_inlined();
					controller->add_checkbox("Chams XQZ", &core::g_config.chams_wallhack);
					controller->add_checkbox("Separate layers", &core::g_config.pixel_depth)->set_callback_visibility([] {
						return core::g_config.chams_wallhack;
						});

					controller->add_colorpicker("Chams color XQZ", &core::g_config.chams_color_wh)->set_inlined();
					controller->add_popup("Chams XQZ options", false, [](framework::c_popup* callback)
						{
							callback->add_checkbox("Override material", &core::g_config.override_invisible_material);
							callback->add_dropdown("Chams type", &core::g_config.chams_type_inv,
								materials)->execute_stack([] { return materials; })->set_callback_and_inline([] {return core::g_config.override_invisible_material; });
							callback->add_colorpicker("Second color", &core::g_config.chams_color_rim_invisible)->set_callback_visibility([&] {return (core::g_config.chams_type_inv == 2 || core::g_config.chams_type_inv == 3 || core::g_config.chams_type_inv == 6 || core::g_config.chams_type_inv == 8
								|| core::g_config.chams_type_inv == 9); });;
					
							callback->add_checkbox("Wireframe", &core::g_config.chams_wireframe_invisible);
							callback->add_colorpicker("Wire color", &core::g_config.chams_color_wire_invisible)->set_inlined();
						})->set_inlined();
					
					controller->add_checkbox("Ragdoll chams", &core::g_config.ragdoll_chams);
					controller->add_colorpicker("Ragdoll visible", &core::g_config.ragdoll_visible)->set_inlined();
					controller->add_colorpicker("Ragdoll invisible", &core::g_config.ragdoll_invisible)->set_inlined();
					controller->add_popup("Ragdoll options", false, [](framework::c_popup* callback)
						{
							callback->add_checkbox("Override material", &core::g_config.override_material_ragdol);
							callback->add_dropdown("Chams type", &core::g_config.ragdol_material,
								materials)->execute_stack([] { return materials; })->set_callback_and_inline([] {return core::g_config.override_material_ragdol; });
							callback->add_colorpicker("Second color", &core::g_config.chams_color_rim_ragdol)->set_callback_visibility([&] {return (core::g_config.ragdol_material == 2 || core::g_config.ragdol_material == 3 || core::g_config.ragdol_material == 6 || core::g_config.ragdol_material == 8
								|| core::g_config.ragdol_material == 9); });;
					
							callback->add_checkbox("Wireframe", &core::g_config.chams_wireframe_ragdoll);
							callback->add_colorpicker("Wire color", &core::g_config.chams_color_wire_ragdol)->set_inlined();
						})->set_inlined();
					controller->add_checkbox("Glow outline", reinterpret_cast<bool*>(&core::g_config.glow_type));
					controller->add_colorpicker("Glow color", &core::g_config.glow_color)->set_inlined();
					controller->add_popup("Glow options", false, [](framework::c_popup* callback)
						{
							callback->add_dropdown("Glow mode", &core::g_config.glow_type,
								{ "Off", "Aura", "Outline" });
							callback->add_slider_float("Thickness", &core::g_config.glow_thickness, 0.5f, 8.f);
						})->set_inlined();

					controller->add_checkbox("Kill effect", &core::g_config.dissolve.enabled);
					controller->add_popup("Effect options", false, [](framework::c_popup* callback) {
						callback->add_slider_int("Max particles", &core::g_config.dissolve.max_particles, 100, 1500);
						callback->add_slider_float("Duration", &core::g_config.dissolve.duration, 0.5f, 5.f);
						callback->add_slider_float("Spread speed", &core::g_config.dissolve.spread_speed, 1.f, 30.f);
						callback->add_slider_float("Size min", &core::g_config.dissolve.size_min, 0.1f, 5.f);
						callback->add_slider_float("Size max", &core::g_config.dissolve.size_max, 0.1f, 5.f);
						callback->add_slider_float("Glow intensity", &core::g_config.dissolve.glow_intensity, 0.1f, 5.f);
						callback->add_colorpicker("Color", &core::g_config.dissolve.color);
						})->set_inlined();
					});


				window->build_child("Interactive", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Players", "", 0);
					auto preview = controller->add_interactive_preview();
					{
						auto box = preview->add_box();
						box->callback(&core::g_config.box);

						box->attach_popup([&](c_popup* p) {		
							p->add_dropdown("Box type", &core::g_config.box_type, { "Full", "Corners" });
							p->add_colorpicker("Box color", &core::g_config.box_color);

							p->add_checkbox("Box shadow", &core::g_config.box_shadow)
								->set_callback_visibility([] { return core::g_config.box_type == 0; });

							p->add_colorpicker("Box color inv", &core::g_config.box_color_inv)->set_callback_visibility([] {return core::g_config.customize_invisible_esp; });

							p->add_checkbox("Box gradient", &core::g_config.box_gradient)
								->set_callback_visibility([] { return core::g_config.box_type == 0; });

							p->add_colorpicker("Box s-color", &core::g_config.box_color2)->set_inlined();
							p->add_colorpicker("Box s-color inv", &core::g_config.box_color2_inv)->set_callback_and_inline([] {return core::g_config.customize_invisible_esp; });
							});

						auto name_esp = preview->add_text_object("Electro", framework::grid_areas::top);
						name_esp->callback(&core::g_config.name);
						name_esp->attach_popup([&](c_popup* p) {
							p->add_dropdown("Name font", &core::g_config.name_font, { "Verdana", "Smallest pixel" });
							p->add_checkbox("Name shadow", &core::g_config.name_shadow);
							p->add_colorpicker("Name color", &core::g_config.name_color);
							p->add_colorpicker("Name color inv", &core::g_config.name_color_inv)->set_callback_visibility([] {return core::g_config.customize_invisible_esp; });
						});

						auto ammo = preview->add_bar_object("Ammobar", hue::c_color(99, 159, 255), framework::grid_areas::bottom);
						ammo->callback(&core::g_config.ammo);
						ammo->attach_popup([&](c_popup* p) {
							p->add_dropdown("Ammo type", &core::g_config.ammo_color_type, { "Static", "Gradient" });

							p->add_checkbox("Ammo shadow", &core::g_config.ammo_shadow);

							p->add_colorpicker("Ammo color", &core::g_config.ammo_main_color);
							p->add_colorpicker("Ammo second color", &core::g_config.ammo_second_color)->set_callback_visibility([] { return core::g_config.ammo_color_type == 1; });
							p->add_colorpicker("Ammo invisible", &core::g_config.ammo_main_color_inv)->set_callback_visibility([] {return core::g_config.customize_invisible_esp; });
							p->add_colorpicker("Ammo second invisible", &core::g_config.ammo_second_color_inv)->set_callback_visibility([] {return core::g_config.ammo_color_type == 1 && core::g_config.customize_invisible_esp; });

							});


						auto weapon = preview->add_text_object("Weapon", framework::grid_areas::bottom);
						weapon->callback(&core::g_config.weapon);
						weapon->attach_popup([&](c_popup* p) {
							p->add_dropdown("Weapon font", &core::g_config.weapon_font, { "Verdana", "Smallest pixel" });
							p->add_checkbox("Weapon shadow", &core::g_config.weapon_shadow);
							p->add_colorpicker("Weapon color", &core::g_config.weapon_color);
							p->add_colorpicker("Weapon color inv", &core::g_config.weapon_color_inv)->set_callback_visibility([] {return core::g_config.customize_invisible_esp; });
						});

						auto health = preview->add_bar_object("Healthbar", hue::c_color(0, 255, 0), framework::grid_areas::left);
						health->callback(&core::g_config.health);
						health->attach_popup([&](c_popup* p) {
							p->add_dropdown("Health type", &core::g_config.health_color_type, { "Dynamic", "Custom", "Gradient" });

							p->add_checkbox("Health shadow", &core::g_config.health_shadow)->set_callback_visibility([] {return core::g_config.health_color_type < 2; });

							p->add_colorpicker("Health color", &core::g_config.health_main_color)->set_callback_visibility([] {return core::g_config.health_color_type > 0; });
							p->add_colorpicker("Health second color", &core::g_config.health_second_color)->set_callback_visibility([] { return core::g_config.health_color_type == 2; });
							p->add_colorpicker("Health invisible", &core::g_config.health_main_color_inv)->set_callback_visibility([] {return core::g_config.health_color_type > 0 && core::g_config.customize_invisible_esp; });
							p->add_colorpicker("Health second inv", &core::g_config.health_second_color_inv)->set_callback_visibility([] {return core::g_config.health_color_type == 2 && core::g_config.customize_invisible_esp; });
							});

						

						auto money = preview->add_text_object("Money", framework::grid_areas::right);
						money->callback(&core::g_config.flags[0]);
						money->default_font(g_font->f_smallest_pixel);

						auto armor = preview->add_text_object("Armor", framework::grid_areas::right);
						armor->callback(&core::g_config.flags[1]);
						armor->default_font(g_font->f_smallest_pixel);

						g_menu->armor_raw = armor;

						auto defuser = preview->add_text_object("KIT", framework::grid_areas::right);
						defuser->callback(&core::g_config.flags[2]); 
						defuser->default_font(g_font->f_smallest_pixel);

						g_menu->defuser_raw = defuser;
					}

					g_menu->ip_raw = preview;

					});

				window->build_child("Other", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Visuals", "", 3);
				
					controller->add_checkbox("Incendiary", &core::g_config.draw_molotov);
					controller->add_colorpicker("Outline color", &core::g_config.drawn_molotov)->set_inlined();

					controller->add_checkbox("Incendiary radius", &core::g_config.molotov_radius)->set_callback_visibility([] {return core::g_config.draw_molotov; });
					controller->add_colorpicker("Radius color", &core::g_config.drawn_radius)->set_inlined();


					controller->add_checkbox("Smoke", &core::g_config.draw_smoke);
					controller->add_colorpicker("Smoke color", &core::g_config.drawn_smoke)->set_inlined();

					controller->add_checkbox("Smoke radius", &core::g_config.smoke_radius)->set_callback_visibility([] {return core::g_config.draw_molotov; });
					controller->add_colorpicker("Smoke-r color", &core::g_config.drawn_radius_smoke)->set_inlined();

					controller->add_checkbox("Hitlog", &core::g_config.hitlog);
					controller->add_popup("Hitlog options", false, [](framework::c_popup* callback) {
						callback->add_checkbox("Hitmarker", &core::g_config.hitmarker);
						callback->add_colorpicker("Hitmarker color", &core::g_config.hitmarker_color)->set_inlined();
				
						callback->add_checkbox("Damage indicator", &core::g_config.damage_markers);
						callback->add_colorpicker("Damage color", &core::g_config.damage_markers_color)->set_inlined();
					})->set_inlined();
				
				
					controller->add_checkbox("Bullet tracers", &core::g_config.bullet_tracer);
					controller->add_colorpicker("Bullet color", &core::g_config.bullet_tracer_color)->set_inlined();
				
					controller->add_checkbox("Hitmarker 3D", &core::g_config.hitmarker_3d);
					controller->add_colorpicker("Hitmarker 3D color", &core::g_config.hitmarker3d_color)->set_inlined();
				
					//controller->add_checkbox("Grenade trajectory (BETA)", &core::g_config.grenade_prediction);
					controller->add_checkbox("Crosshair", &core::g_config.crosshair.enabled);
					controller->add_popup("Crosshair options", false, [](framework::c_popup* callback) {
						callback->add_dropdown("Style", &core::g_config.crosshair.style, { "Classic", "Circle", "Cross + Circle" });
						callback->add_slider_float("Size", &core::g_config.crosshair.size, 1.f, 20.f);
						callback->add_slider_float("Thickness", &core::g_config.crosshair.thickness, 0.5f, 5.f);
						callback->add_slider_float("Gap", &core::g_config.crosshair.gap, 0.f, 15.f);
						callback->add_checkbox("Center dot", &core::g_config.crosshair.dot);
						callback->add_slider_float("Dot size", &core::g_config.crosshair.dot_size, 0.5f, 4.f);
						callback->add_checkbox("Outline", &core::g_config.crosshair.outline);
						callback->add_checkbox("T-shape", &core::g_config.crosshair.t_shape);
						callback->add_colorpicker("Color", &core::g_config.crosshair.color)->set_inlined();
						})->set_inlined();
				
				
				
					controller->add_checkbox("Particle system", &core::g_config.ash_particles.enabled);
					controller->add_popup("Particle options", false, [](framework::c_popup* callback) {
						callback->add_dropdown("Type", &core::g_config.ash_particles.particle_type,
							{ "Ash", "pandora", "Rain", "Stars", "ssss"});
						callback->add_slider_int("Count", &core::g_config.ash_particles.count, 50, 3200);
						callback->add_slider_float("Radius", &core::g_config.ash_particles.radius, 50.f, 3200.f);
						callback->add_slider_float("Speed", &core::g_config.ash_particles.speed, 0.1f, 3.f);
						callback->add_slider_float("Wind X", &core::g_config.ash_particles.wind_x, -1.f, 12.f);
						callback->add_slider_float("Turbulence", &core::g_config.ash_particles.turbulence, 0.f, 3.f);
					
						callback->add_slider_float("Glow", &core::g_config.ash_particles.glow_intensity, 0.5f, 3.f)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 0; });
						callback->add_colorpicker("Ember core", &core::g_config.ash_particles.ember_core)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 0; });
						callback->add_colorpicker("Ember glow", &core::g_config.ash_particles.ember_glow)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 0; });
						callback->add_colorpicker("Debris color", &core::g_config.ash_particles.debris_color)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 0; });
					
						callback->add_colorpicker("pandora color", &core::g_config.ash_particles.pandora_color)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 1; });
						callback->add_colorpicker("Rain color", &core::g_config.ash_particles.rain_color)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 2; });
					
						callback->add_slider_float("Glow", &core::g_config.ash_particles.glow_intensity, 0.5f, 3.f)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 3; });
						callback->add_colorpicker("Star core", &core::g_config.ash_particles.star_color)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 3; });
						callback->add_colorpicker("Star glow", &core::g_config.ash_particles.star_glow)->set_callback_visibility([] {return core::g_config.ash_particles.particle_type == 3; });
					
					
						})->set_inlined();
					controller->add_checkbox("Nightmode", &core::g_config.night_mode);
					controller->add_colorpicker("Nightmode color", &core::g_config.night_modec)->set_inlined();

					controller->add_checkbox("Dropped weapons", &core::g_config.dropped_weapons);
					controller->add_colorpicker("Dropped accent", &core::g_config.dropped_accent)->set_inlined();
				
					controller->add_popup("Dropped options", false, [](framework::c_popup* callback) {
						callback->add_slider_int("Radius", &core::g_config.dropepd_radius, 5, 30);
						})->set_inlined();
				});

				window->build_child("Overlay settings", framework::child_width::full, full_height, [](framework::c_child* controller) {
					controller->attach_child("Overlay", "", 1);
					controller->add_checkbox("Vsync", &core::g_config.overlay_vsync);
					controller->add_checkbox("Flushing", &core::g_config.overlay_flushing);
					});

				window->build_child("Config", framework::child_width::full, full_height, [](framework::c_child* controller) {
					controller->attach_child("Config", "", 2);

					static int a = 0;
					controller->add_listbox("Config list", &a, core::g_config.get_all_configs().empty() ? std::vector<std::string>{"Empty"} : core::g_config.get_all_configs(), 180);

					static std::string config_name = "";
					controller->add_input_box("Config name", &config_name);

					controller->add_button("Save config", []() {
						if (!config_name.empty())
							core::g_config.save(config_name);
						else // fix save
							core::g_config.save(core::g_config.get_all_configs()[a]);

						slog::log::success("saved config {}", config_name);
						});

					controller->add_button("Load config", []() {
						core::g_config.load(core::g_config.get_all_configs()[a]);

						slog::log::success("loaded config {}", core::g_config.get_all_configs()[a].c_str());
						});
				});

				this->m_windows.push_back(window);
			}
		}

		auto widget = std::make_shared<c_widgets>();
		{
			if (widget == nullptr)
			{
				slog::log::error("[-] failed to create widget");
			}
			else
			{
				auto final = g_widget_ctx.m_watermark_pos + math::c_vector_2d(core::g_overlay->width - 320, 0);
				auto final2 = g_widget_ctx.m_keybind_pos + math::c_vector_2d(0, core::g_overlay->height * 0.5);

				widget->create_widget(widget_type::watermark, final);
				final = g_widget_ctx.m_watermark_pos + math::c_vector_2d(core::g_overlay->width - widget->get_watermark_width(), 0);

				widget->create_widget(widget_type::keybind, final2);
				widget->create_widget(widget_type::notification_panel, g_widget_ctx.m_notify_panel_pos);
				this->m_widgets = widget;
			}
		}
	}

	void c_menu::runtime()
	{
		//g_ctx->m_focused = nullptr;
		//g_ctx->m_focus_took = nullptr;

		g_ctx->m_click_consumed = false;

		// runtime window calling
		for (auto& windows : this->m_windows)
		{
			windows->input();
			windows->paint();
		}

		std::vector< framework::keybind_entry_t > entries;
		if (core::g_config.esp_activation.active())
		{
			entries.push_back(framework::keybind_entry_t( "Esp", (widget_mode)core::g_config.esp_activation.mode));
		}

		this->m_widgets->keybind_manager()->update_keybinds(entries);

		// we only have 1 widget controller
		this->m_widgets->draw();

		g_search.paint_database();
	}

	std::shared_ptr<c_notify_panel> c_menu::notify()
	{
		return this->m_widgets->notify();
	}
}
