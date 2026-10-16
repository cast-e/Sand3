#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include "gradient.hpp"
#include "icon_manager.hpp"

class UIWidgets {
public:
	UIWidgets() = delete;

	static bool button_with_icon(const char* label, SDL_Texture* icon, const ImVec2& size = ImVec2(0, 0));
	static bool selectable_with_color(const char* label, const ImVec4& color, bool selected,
									  ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0),
									  bool show_checkbox = false);
	static bool custom_checkbox(const char* label, bool* val, IconID icon_id = IconID::Count,
								const char* tooltip = nullptr, const ImVec2& size = ImVec2(0.0f, 0.0f));
	static bool gradient_editor(const char* label, ColorGradient& gradient, float min_val = 0.0f, float max_val = 1.0f,
								float width = 0.0f);
};