#pragma once

#include <vector>

#include "ui.hpp"

class UIShortcuts {
public:
	UIShortcuts() = delete;

	static void render();
	static std::vector<UI::MaterialShortcutItem> get_all_material_shortcuts();
	static std::string get_effective_material_shortcut(uint8_t id);
};