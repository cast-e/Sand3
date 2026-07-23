#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include <string>

class UI {
public:
	UI() = delete;

	static void init();
	static void shutdown();

	static void new_frame();
	static void render();
	static void handle_interaction();

	static void trigger_exit() { show_exit_popup = true; }
	static bool should_update() { return update; }
	static bool should_step() { return step_frame; }
	static void reset_step() { step_frame = false; }

private:
	static void render_header(ImGuiIO& io);
	static void render_sim_content();
	static void render_material_editor();
	static void render_manage_sets();
	static void render_save_load();
	static void render_advanced_options();
	static void render_shortcuts();
	static void render_modals();

	static char save_file_name_buf[128];
	static char save_as_buf[64];
	static char new_set_name_buf[64];
	static char rename_set_buf[64];

	static int selected_save_id;
	static int selected_id;
	static int mouse_size;

	static bool open_switch_popup;
	static bool open_create_set_popup;
	static bool open_delete_set_popup;
	static bool open_empty_rule_warning_popup;

	static bool duplicate_set_checkbox;
	static bool exit_save_as_new_set;

	static bool update;
	static bool step_frame;

	static bool show_exit_popup;

	static bool unsaved_changes;
	static std::string pending_set_switch;
	static std::string pending_save_load;
	static bool ui_compact;

	static float zoom;
	static float pan_x;
	static float pan_y;
	static float target_zoom;
	static float target_pan_x;
	static float target_pan_y;
};