#pragma once

#include <imgui.h>

#include <string>

struct UIConfig;

enum CustomThemeColor : int {
	CustomCol_None = -100,
	CustomCol_CanvasBackground = -1,
	CustomCol_SelectionBoxBorder = -2,
	CustomCol_SelectionBoxFill = -3,
};

struct CuratedColor {
	int col;
	const char* category;
	const char* label;
};

class UITheme {
public:
	UITheme() = delete;

	static void init_style();
	static void apply_theme_style(ImGuiStyle& style, const UIConfig& cfg);
	static float get_spacing(float offset = 0.0f);
	static const ImVec4* get_default_colors();
	static void reset_theme_colors();
	static bool apply_theme_from_file(const std::string& theme_path, const std::string& title = "");
	static void render_theme_editor();

private:
	static ImVec4 default_colors[ImGuiCol_COUNT];
	static bool default_colors_initialized;
};