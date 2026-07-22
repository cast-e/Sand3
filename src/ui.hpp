#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include <string>

#include "grid.hpp"
#include "window.hpp"

class UI {
public:
	UI(Window& window);
	~UI() = default;

	void new_frame();
	void render(Window& window, Grid& grid);
	void handle_interaction(Window& window, Grid& grid);

	void trigger_exit() { show_exit_popup = true; }
	bool should_update() const { return update; }
	bool should_step() const { return step_frame; }
	void reset_step() { step_frame = false; }

private:
	void render_header(ImGuiIO& io, Grid& grid);
	void render_sim_content(Grid& grid);
	void render_material_editor(Grid& grid);
	void render_manage_sets(Grid& grid);
	void render_save_load(Grid& grid);
	void render_advanced_options(Window& window, Grid& grid);
	void render_shortcuts();
	void render_modals(Grid& grid);

	char save_file_name_buf[128] = "";
	char new_set_name_tab_buf[64] = "";
	char save_as_buf[64] = "";
	char new_set_name_buf[64] = "";

	int selected_save_idx = -1;
	int selected_idx = -1;
	int mouse_size = 5;

	bool open_switch_popup = false;
	bool open_create_set_popup = false;
	bool open_delete_set_popup = false;

	bool duplicate_set_checkbox = false;
	bool exit_save_as_new_set = false;

	bool update = false;
	bool step_frame = false;

	bool show_exit_popup = false;

	bool unsaved_changes = false;
	std::string pending_set_switch = "";
	std::string pending_save_load = "";
	bool ui_compact = false;

	float zoom = 1.0f;
	float pan_x = 0.0f;
	float pan_y = 0.0f;
	float target_zoom = 1.0f;
	float target_pan_x = 0.0f;
	float target_pan_y = 0.0f;
};