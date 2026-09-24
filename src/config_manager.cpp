#include "config_manager.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

#include "grid.hpp"
#include "sanitize.hpp"
#include "shortcut_manager.hpp"
#include "vulkan.hpp"
#include "window.hpp"

Config ConfigManager::config;
const Config ConfigManager::default_config{};
std::unordered_map<std::string, std::string> ConfigManager::color_overrides;

std::string ConfigManager::color_to_hex(const ImVec4& c) {
	int r = std::clamp(static_cast<int>(c.x * 255.0f + 0.5f), 0, 255);
	int g = std::clamp(static_cast<int>(c.y * 255.0f + 0.5f), 0, 255);
	int b = std::clamp(static_cast<int>(c.z * 255.0f + 0.5f), 0, 255);
	int a = std::clamp(static_cast<int>(c.w * 255.0f + 0.5f), 0, 255);
	char buf[16];
	std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
	return std::string(buf);
}

bool ConfigManager::parse_color_string(const std::string& str, ImVec4& out_col) {
	std::string s = trim(str);
	if (s.empty())
		return false;

	if (s[0] == '#') {
		s = s.substr(1);
		if (s.length() == 6) {
			try {
				uint32_t val = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
				out_col.x = ((val >> 16) & 0xFF) / 255.0f;
				out_col.y = ((val >> 8) & 0xFF) / 255.0f;
				out_col.z = (val & 0xFF) / 255.0f;
				out_col.w = 1.0f;
				return true;
			} catch (...) {
				return false;
			}
		} else if (s.length() == 8) {
			try {
				uint32_t val = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
				out_col.x = ((val >> 24) & 0xFF) / 255.0f;
				out_col.y = ((val >> 16) & 0xFF) / 255.0f;
				out_col.z = ((val >> 8) & 0xFF) / 255.0f;
				out_col.w = (val & 0xFF) / 255.0f;
				return true;
			} catch (...) {
				return false;
			}
		}
	} else {
		std::string comma_rep = s;
		std::replace(comma_rep.begin(), comma_rep.end(), ',', ' ');
		std::stringstream ss(comma_rep);
		float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
		if (ss >> r >> g >> b) {
			if (!(ss >> a))
				a = 1.0f;
			out_col = ImVec4(r, g, b, a);
			return true;
		}
	}
	return false;
}

bool ConfigManager::color_differs(const ImVec4& a, const ImVec4& b) {
	return std::fabs(a.x - b.x) > 0.003f || std::fabs(a.y - b.y) > 0.003f || std::fabs(a.z - b.z) > 0.003f ||
		   std::fabs(a.w - b.w) > 0.003f;
}

void ConfigManager::load() {
	ShortcutManager::init();

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
				try {
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
				} catch (...) {}
			} else if (section == "Advanced") {
				try {
					if (key == "vsync")
						config.vsync = (val == "true" || val == "1");
					else if (key == "target_fps")
						config.target_fps = std::stoi(val);
					else if (key == "processing_mode")
						config.processing_mode = std::stoi(val);
					else if (key == "thread_count")
						config.thread_count = std::stoi(val);
					else if (key == "prevent_downclock")
						config.prevent_downclock = (val == "true" || val == "1");
				} catch (...) {}
			} else if (section == "UI") {
				try {
					if (key == "sidebar_x")
						config.ui.sidebar_x = std::stoi(val);
					else if (key == "sidebar_y")
						config.ui.sidebar_y = std::stoi(val);
					else if (key == "sidebar_width")
						config.ui.sidebar_width = std::stoi(val);
					else if (key == "compact_x")
						config.ui.compact_x = std::stoi(val);
					else if (key == "compact_y")
						config.ui.compact_y = std::stoi(val);
					else if (key == "compact_width")
						config.ui.compact_width = std::stoi(val);
					else if (key == "compact_height")
						config.ui.compact_height = std::stoi(val);
					else if (key == "font_size")
						config.ui.font_size = std::stof(val);
					else if (key == "ui_scale")
						config.ui.ui_scale = std::stof(val);
					else if (key == "show_fps")
						config.ui.show_fps = (val == "true" || val == "1");
					else if (key == "show_active_cells")
						config.ui.show_active_cells = (val == "true" || val == "1");
					else if (key == "show_simulation_status")
						config.ui.show_simulation_status = (val == "true" || val == "1");
					else if (key == "window_rounding")
						config.ui.window_rounding = std::stof(val);
					else if (key == "frame_rounding")
						config.ui.frame_rounding = std::stof(val);
					else if (key == "button_size")
						config.ui.button_size = std::stoi(val);
					else if (key == "icon_size")
						config.ui.icon_size = std::stoi(val);
					else if (key == "material_list_height")
						config.ui.material_list_height = std::stoi(val);
					else if (key == "background_color")
						parse_color_string(val, config.ui.background_color);
				} catch (...) {}
			} else if (section == "Shortcuts") {
				ShortcutManager::load_from_config(key, val);
			} else if (section == "Colors") {
				color_overrides[key] = val;
			}
		}

		in.close();
	}

	Window::set_background_color(config.ui.background_color);

	Window::set_vsync(config.vsync);
	Window::set_target_fps(static_cast<uint32_t>(config.target_fps));
	Grid::set_processing_mode(static_cast<ProcessingMode>(config.processing_mode));
	Grid::configure_threads(static_cast<uint32_t>(config.thread_count));
	Vulkan::set_prevent_downclock(config.prevent_downclock);

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
			SDL_GetWindowSize(window, reinterpret_cast<int*>(&config.window_width),
							  reinterpret_cast<int*>(&config.window_height));
		}
	}

	config.vsync = Window::get_vsync();
	config.target_fps = static_cast<uint32_t>(Window::get_target_fps());
	config.processing_mode = static_cast<uint32_t>(Grid::get_processing_mode());
	config.thread_count = static_cast<uint32_t>(Grid::get_thread_count());
	config.prevent_downclock = Vulkan::is_prevent_downclock_enabled();

	std::ofstream out("config.ini");
	if (!out.is_open())
		return;

	std::vector<std::string> win_lines;
	if (config.window_x >= 0 && config.window_x != default_config.window_x)
		win_lines.push_back("x = " + std::to_string(config.window_x));
	if (config.window_y >= 0 && config.window_y != default_config.window_y)
		win_lines.push_back("y = " + std::to_string(config.window_y));
	if (config.window_width != default_config.window_width)
		win_lines.push_back("width = " + std::to_string(config.window_width));
	if (config.window_height != default_config.window_height)
		win_lines.push_back("height = " + std::to_string(config.window_height));
	if (config.is_maximized != default_config.is_maximized)
		win_lines.push_back(std::string("maximized = ") + (config.is_maximized ? "true" : "false"));
	if (config.is_fullscreen != default_config.is_fullscreen)
		win_lines.push_back(std::string("fullscreen = ") + (config.is_fullscreen ? "true" : "false"));

	if (!win_lines.empty()) {
		out << "[Window]\n";
		for (const auto& l : win_lines)
			out << l << "\n";
		out << "\n";
	}

	std::vector<std::string> adv_lines;
	if (config.vsync != default_config.vsync)
		adv_lines.push_back(std::string("vsync = ") + (config.vsync ? "true" : "false"));
	if (config.target_fps != default_config.target_fps)
		adv_lines.push_back("target_fps = " + std::to_string(config.target_fps));
	if (config.processing_mode != default_config.processing_mode)
		adv_lines.push_back("processing_mode = " + std::to_string(config.processing_mode));
	if (config.thread_count != default_config.thread_count)
		adv_lines.push_back("thread_count = " + std::to_string(config.thread_count));
	if (config.prevent_downclock != default_config.prevent_downclock)
		adv_lines.push_back(std::string("prevent_downclock = ") + (config.prevent_downclock ? "true" : "false"));

	if (!adv_lines.empty()) {
		out << "[Advanced]\n";
		for (const auto& l : adv_lines)
			out << l << "\n";
		out << "\n";
	}

	std::vector<std::string> ui_lines;
	if (config.ui.sidebar_x != default_config.ui.sidebar_x)
		ui_lines.push_back("sidebar_x = " + std::to_string(config.ui.sidebar_x));
	if (config.ui.sidebar_y != default_config.ui.sidebar_y)
		ui_lines.push_back("sidebar_y = " + std::to_string(config.ui.sidebar_y));
	if (config.ui.sidebar_width != default_config.ui.sidebar_width)
		ui_lines.push_back("sidebar_width = " + std::to_string(config.ui.sidebar_width));
	if (config.ui.compact_x != default_config.ui.compact_x)
		ui_lines.push_back("compact_x = " + std::to_string(config.ui.compact_x));
	if (config.ui.compact_y != default_config.ui.compact_y)
		ui_lines.push_back("compact_y = " + std::to_string(config.ui.compact_y));
	if (config.ui.compact_width != default_config.ui.compact_width)
		ui_lines.push_back("compact_width = " + std::to_string(config.ui.compact_width));
	if (config.ui.compact_height != default_config.ui.compact_height)
		ui_lines.push_back("compact_height = " + std::to_string(config.ui.compact_height));
	if (std::fabs(config.ui.font_size - default_config.ui.font_size) > 0.01f)
		ui_lines.push_back("font_size = " + std::to_string(config.ui.font_size));
	if (std::fabs(config.ui.ui_scale - default_config.ui.ui_scale) > 0.01f)
		ui_lines.push_back("ui_scale = " + std::to_string(config.ui.ui_scale));
	if (config.ui.show_fps != default_config.ui.show_fps)
		ui_lines.push_back(std::string("show_fps = ") + (config.ui.show_fps ? "true" : "false"));
	if (config.ui.show_active_cells != default_config.ui.show_active_cells)
		ui_lines.push_back(std::string("show_active_cells = ") + (config.ui.show_active_cells ? "true" : "false"));
	if (config.ui.show_simulation_status != default_config.ui.show_simulation_status)
		ui_lines.push_back(std::string("show_simulation_status = ") +
						   (config.ui.show_simulation_status ? "true" : "false"));
	if (std::fabs(config.ui.window_rounding - default_config.ui.window_rounding) > 0.01f)
		ui_lines.push_back("window_rounding = " + std::to_string(config.ui.window_rounding));
	if (std::fabs(config.ui.frame_rounding - default_config.ui.frame_rounding) > 0.01f)
		ui_lines.push_back("frame_rounding = " + std::to_string(config.ui.frame_rounding));
	if (config.ui.button_size != default_config.ui.button_size)
		ui_lines.push_back("button_size = " + std::to_string(config.ui.button_size));
	if (config.ui.icon_size != default_config.ui.icon_size)
		ui_lines.push_back("icon_size = " + std::to_string(config.ui.icon_size));
	if (config.ui.material_list_height != default_config.ui.material_list_height)
		ui_lines.push_back("material_list_height = " + std::to_string(config.ui.material_list_height));
	if (color_differs(config.ui.background_color, default_config.ui.background_color))
		ui_lines.push_back("background_color = " + color_to_hex(config.ui.background_color));

	if (!ui_lines.empty()) {
		out << "[UI]\n";
		for (const auto& l : ui_lines)
			out << l << "\n";
		out << "\n";
	}

	if (ShortcutManager::any_modified()) {
		out << "[Shortcuts]\n";
		for (const auto& s : ShortcutManager::get_all()) {
			if (s.current_key != s.default_key) {
				out << s.id << " = " << s.current_key << "\n";
			}
		}
		out << "\n";
	}

	if (!color_overrides.empty()) {
		out << "[Colors]\n";
		for (const auto& [name, val] : color_overrides) {
			out << name << " = " << val << "\n";
		}
		out << "\n";
	}

	out.close();
}