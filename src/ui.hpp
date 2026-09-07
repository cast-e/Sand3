#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

#include "const.hpp"

enum class BrushShape : int { Square, Circle, Size };
enum class ToolMode : int { Brush, Select };
enum class ResizeHandle : int {
	None,
	TopLeft,
	Top,
	TopRight,
	Right,
	BottomRight,
	Bottom,
	BottomLeft,
	Left
};
enum class SelectionState : int { None, Selecting, Selected, Moving, Pasting, Resizing };

struct SelectionBox {
	int start_x = 0;
	int start_y = 0;
	int current_x = 0;
	int current_y = 0;

	int min_x() const { return std::clamp(std::min(start_x, current_x), 0, static_cast<int>(SIM_WIDTH) - 1); }
	int max_x() const { return std::clamp(std::max(start_x, current_x), 0, static_cast<int>(SIM_WIDTH) - 1); }
	int min_y() const { return std::clamp(std::min(start_y, current_y), 0, static_cast<int>(SIM_HEIGHT) - 1); }
	int max_y() const { return std::clamp(std::max(start_y, current_y), 0, static_cast<int>(SIM_HEIGHT) - 1); }
	int width() const { return max_x() - min_x() + 1; }
	int height() const { return max_y() - min_y() + 1; }
	bool contains(int x, int y) const { return x >= min_x() && x <= max_x() && y >= min_y() && y <= max_y(); }
};

struct ClipboardData {
	int width = 0;
	int height = 0;
	std::vector<uint8_t> cells;
	bool empty() const { return cells.empty() || width <= 0 || height <= 0; }
	void clear() {
		width = 0;
		height = 0;
		cells.clear();
	}
};

class UI {
public:
	UI() = delete;

	static void init();
	static void shutdown();

	static void render();
	static void handle_interaction();

	static void trigger_exit() { show_exit_popup = true; }
	static bool should_update() { return update; }
	static void pause_simulation() { update = false; }
	static bool should_step() { return step_frame; }
	static void reset_step() { step_frame = false; }

	static BrushShape get_brush_shape() { return brush_shape; }
	static uint8_t get_selected_id() { return selected_id; }
	static void set_selected_id(uint8_t id) { selected_id = id; }

	static ToolMode get_tool_mode() { return current_tool; }
	static void set_tool_mode(ToolMode mode);
	static SelectionState get_selection_state() { return selection_state; }
	static ResizeHandle get_active_resize_handle() { return active_resize_handle; }
	static const SelectionBox& get_selection_box() { return selection_box; }
	static const ClipboardData& get_clipboard() { return clipboard; }
	static bool get_transparent() { return transparent_mode; }
	static void set_transparent(bool val) { transparent_mode = val; }
	static bool get_transparent_paste() { return transparent_mode; }
	static void set_transparent_paste(bool val) { transparent_mode = val; }

	static void copy_selection();
	static void cut_selection();
	static void fill_selection(uint8_t id);
	static void rotate_selection(bool clockwise = true);
	static void paste_clipboard();
	static void deselect();
	static void restore_selection_state(ToolMode mode, SelectionState state, const SelectionBox& box);

private:
	static void render_header(ImGuiIO& io);
	static void render_sim_content();
	static void render_material_editor();
	static void render_manage_sets();
	static void render_save_load();
	static void render_advanced_options();
	static void render_shortcuts();
	static void render_modals();
	static void render_mouse_overlay();
	static void render_selection_controls();

	static void handle_zoom_and_pan(ImGuiIO& io);
	static void handle_keyboard_shortcuts(ImGuiIO& io);
	static void handle_mouse_wheel_brush_size(ImGuiIO& io);
	static void handle_canvas_interaction();

	static char save_file_name_buf[128];
	static char save_as_buf[64];
	static char new_set_name_buf[64];

	static int selected_save_id;
	static uint8_t selected_id;
	static int mouse_size;
	static BrushShape brush_shape;

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

	static ToolMode current_tool;
	static SelectionState selection_state;
	static ResizeHandle active_resize_handle;
	static SelectionBox selection_box;
	static ClipboardData clipboard;
	static std::vector<uint8_t> floating_cells;
	static int move_origin_x;
	static int move_origin_y;
	static int move_grab_offset_x;
	static int move_grab_offset_y;
	static int current_floating_x;
	static int current_floating_y;
	static bool transparent_mode;
};