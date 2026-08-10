#pragma once

struct AppConfig {
	int window_x = -1;
	int window_y = -1;
	int window_width = 1600;
	int window_height = 900;
	bool is_maximized = false;
	bool is_fullscreen = false;

	bool vsync = false;
	int target_fps = 500;
	int quality_preset = 1;	 // 0 = Slow, 1 = Fast
	int thread_count = 8;
};

class ConfigManager {
public:
	static void load();
	static void save();

	static AppConfig& get_config() { return config; }

private:
	static AppConfig config;
};
