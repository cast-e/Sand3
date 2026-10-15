#pragma once

#include <imgui.h>

#include <cstdint>
#include <string>
#include <unordered_map>

#include "const.hpp"

struct UIConfig {
	int sidebar_x = 0;
	int sidebar_y = 0;
	int sidebar_width = 570;
	int compact_x = 2;
	int compact_y = 2;
	int compact_width = 280;
	int compact_height = 400;
	float font_size = 16.0f;
	bool show_fps = true;
	bool show_active_cells = true;
	bool show_simulation_status = true;
	int button_size = 32;
	int icon_size = 16;
	int material_list_height = 150;
	int set_list_height = 200;
	int save_list_height = 180;
	int stamp_list_height = 180;
	ImVec4 background_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	ImVec4 selection_box_color = ImVec4(80.0f / 255.0f, 200.0f / 255.0f, 1.0f, 1.0f);
	ImVec4 selection_box_fill = ImVec4(50.0f / 255.0f, 150.0f / 255.0f, 1.0f, 30.0f / 255.0f);

	// Unified spacing system (base spacing +/- offsets)
	float base_spacing = 6.0f;
	float window_padding_x = 12.0f;
	float window_padding_y = 12.0f;
	float frame_padding_x = 8.0f;
	float frame_padding_y = 5.0f;
	float item_spacing_x = 8.0f;
	float item_spacing_y = 6.0f;
	float item_inner_spacing_x = 6.0f;
	float item_inner_spacing_y = 6.0f;
	float cell_padding_x = 4.0f;
	float cell_padding_y = 2.0f;
	float indent_spacing = 21.0f;
	float scrollbar_size = 14.0f;
	float grab_min_size = 10.0f;

	// Rounding constants
	float window_rounding = 6.0f;
	float child_rounding = 4.0f;
	float frame_rounding = 4.0f;
	float popup_rounding = 4.0f;
	float scrollbar_rounding = 4.0f;
	float grab_rounding = 4.0f;
	float tab_rounding = 4.0f;

	// Border constants
	float window_border_size = 1.0f;
	float child_border_size = 1.0f;
	float popup_border_size = 1.0f;
	float frame_border_size = 0.0f;
	float tab_border_size = 0.0f;
};

struct Config {
	int32_t window_x = -1;
	int32_t window_y = -1;
	uint32_t window_width = 1600;
	uint32_t window_height = 900;
	bool is_maximized = false;
	bool is_fullscreen = false;

	bool vsync = false;
	uint32_t target_fps = 500;
	uint32_t processing_mode = 0;
	uint32_t thread_count = (DEFAULT_SIM_HEIGHT + STRIP_HEIGHT - 1) / STRIP_HEIGHT / 2;
	bool prevent_downclock = true;
	bool allow_large_neighborhoods = false;

	UIConfig ui;

	struct WorkshopConfig {
		std::string url;
		std::string token;
		std::string username;
	} workshop;
};

class ConfigManager {
public:
	static void load();
	static void save();

	static Config& get_config() { return config; }
	static const Config& get_default_config() { return default_config; }
	static std::unordered_map<std::string, std::string>& get_color_overrides() { return color_overrides; }

	static bool parse_color_string(const std::string& str, ImVec4& out_col);
	static std::string color_to_hex(const ImVec4& c);
	static bool color_differs(const ImVec4& a, const ImVec4& b);

private:
	static Config config;
	static const Config default_config;
	static std::unordered_map<std::string, std::string> color_overrides;
};