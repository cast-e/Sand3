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
	float ui_scale = 1.0f;
	bool show_fps = true;
	bool show_active_cells = true;
	float window_rounding = 6.0f;
	float frame_rounding = 4.0f;
	int button_height = 32;
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

	UIConfig ui;
};

class ConfigManager {
public:
	static void load();
	static void save();

	static Config& get_config() { return config; }
	static const Config& get_default_config() { return default_config; }
	static std::unordered_map<std::string, std::string>& get_color_overrides() { return color_overrides; }

	// Color utilities
	static bool parse_color_string(const std::string& str, ImVec4& out_col);
	static std::string color_to_hex(const ImVec4& c);
	static bool color_differs(const ImVec4& a, const ImVec4& b);

private:
	static Config config;
	static const Config default_config;
	static std::unordered_map<std::string, std::string> color_overrides;
};