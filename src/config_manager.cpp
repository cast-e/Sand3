#include "config_manager.hpp"

#include <SDL3/SDL.h>

#include <fstream>
#include <iostream>

#include "grid.hpp"
#include "sanitize.hpp"
#include "window.hpp"

AppConfig ConfigManager::config;

void ConfigManager::load() {
	std::ifstream in("config.ini");

	if (in.is_open()) {
		std::string line;
		std::string section;

		while (std::getline(in, line)) {
			line = trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;

			if (line.front() == '[' && line.back() == ']') {
				section = line.substr(1, line.size() - 2);
				continue;
			}

			size_t eq_pos = line.find('=');
			if (eq_pos == std::string::npos)
				continue;

			std::string key = trim(line.substr(0, eq_pos));
			std::string val = trim(line.substr(eq_pos + 1));

			if (section == "Window") {
				if (key == "x")
					config.window_x = std::stoi(val);
				else if (key == "y")
					config.window_y = std::stoi(val);
				else if (key == "width")
					config.window_width = std::stoi(val);
				else if (key == "height")
					config.window_height = std::stoi(val);
				else if (key == "maximized")
					config.is_maximized = (val == "true" || val == "1");
				else if (key == "fullscreen")
					config.is_fullscreen = (val == "true" || val == "1");
			} else if (section == "Advanced") {
				if (key == "vsync")
					config.vsync = (val == "true" || val == "1");
				else if (key == "target_fps")
					config.target_fps = std::stoi(val);
				else if (key == "quality_preset")
					config.quality_preset = std::stoi(val);
				else if (key == "thread_count")
					config.thread_count = std::stoi(val);
			}
		}

		in.close();
	}

	// Apply advanced settings
	Window::set_vsync(config.vsync);
	Window::set_target_fps(static_cast<uint32_t>(config.target_fps));
	Grid::set_quality_preset(static_cast<QualityPreset>(config.quality_preset));
	Grid::configure_threads(static_cast<uint32_t>(config.thread_count));

	// Apply window geometry
	SDL_Window* window = Window::get_window();
	if (window) {
		if (config.window_width > 200 && config.window_height > 200) {
			SDL_SetWindowSize(window, config.window_width, config.window_height);
		}
		if (config.window_x >= 0 && config.window_y >= 0) {
			SDL_SetWindowPosition(window, config.window_x, config.window_y);
		}
		if (config.is_maximized) {
			SDL_MaximizeWindow(window);
		} else if (config.is_fullscreen) {
			SDL_SetWindowFullscreen(window, true);
		}
	}
}

void ConfigManager::save() {
	SDL_Window* window = Window::get_window();
	if (window) {
		Uint32 flags = SDL_GetWindowFlags(window);
		config.is_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
		config.is_fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;

		if (!config.is_maximized && !config.is_fullscreen) {
			SDL_GetWindowPosition(window, &config.window_x, &config.window_y);
			SDL_GetWindowSize(window, &config.window_width, &config.window_height);
		}
	}

	config.vsync = Window::get_vsync();
	config.target_fps = static_cast<int>(Window::get_target_fps());
	config.quality_preset = static_cast<int>(Grid::get_quality_preset());
	config.thread_count = static_cast<int>(Grid::get_thread_count());

	std::ofstream out("config.ini");
	if (out.is_open()) {
		out << "[Window]\n";
		out << "x = " << config.window_x << "\n";
		out << "y = " << config.window_y << "\n";
		out << "width = " << config.window_width << "\n";
		out << "height = " << config.window_height << "\n";
		out << "maximized = " << (config.is_maximized ? "true" : "false") << "\n";
		out << "fullscreen = " << (config.is_fullscreen ? "true" : "false") << "\n\n";

		out << "[Advanced]\n";
		out << "vsync = " << (config.vsync ? "true" : "false") << "\n";
		out << "target_fps = " << config.target_fps << "\n";
		out << "quality_preset = " << config.quality_preset << "\n";
		out << "thread_count = " << config.thread_count << "\n";

		out.close();
	}
}
