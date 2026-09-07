#pragma once

#include <cstdint>

#include "const.hpp"

struct Config {
	int32_t window_x = -1;
	int32_t window_y = -1;
	uint32_t window_width = 1600;
	uint32_t window_height = 900;
	bool is_maximized = false;
	bool is_fullscreen = false;

	bool vsync = false;
	uint32_t target_fps = 500;
	uint32_t quality_preset = 1;
	uint32_t thread_count = NUM_STRIPS_Y / 2;
	bool prevent_downclock = false;
};

class ConfigManager {
public:
	static void load();
	static void save();

	static Config& get_config() { return config; }

private:
	static Config config;
};