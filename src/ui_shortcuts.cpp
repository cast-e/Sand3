#include "ui_shortcuts.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <unordered_set>

#include "config_manager.hpp"
#include "material_manager.hpp"
#include "set_manager.hpp"
#include "shortcut_manager.hpp"

struct MouseShortcutEntry {
	std::string action;
	std::string control;
	std::string description;
};

static const MouseShortcutEntry MOUSE_SHORTCUTS[] = {
	{"Paint Material", "Left Mouse Button (Click / Drag)", "Draw with active material on the simulation grid"},
	{"Erase Cells", "Right Mouse Button (Click / Drag)", "Erase cells (draw empty material) under brush"},
	{"Pan Camera", "Middle Mouse Button (Drag)", "Pan the camera viewport across the grid"},
	{"Eyedropper", "Middle Mouse Button (Click)", "Sample material from the grid cell under cursor"},
	{"Brush Size", "Mouse Scroll Wheel", "Adjust simulation brush size"},
	{"Fast Brush Size", "Ctrl + Mouse Scroll Wheel", "Rapidly adjust brush size (4x step size)"},
	{"Zoom Viewport", "Shift + Mouse Scroll Wheel", "Smoothly zoom canvas in or out"},
	{"Draw Straight Line", "Shift + Left / Right Drag", "Draw or erase straight lines between points"},
	{"Flood Fill Area", "Shift + Alt + Left / Right Click", "Fill or clear connected region of matching cells"},
	{"Fast Camera Pan", "Shift + W / A / S / D", "Pan camera at 2.5x speed"},
	{"Move Selection", "Left Click Drag inside box", "Reposition floating selection area"},
	{"Resize Selection Box", "Drag Handles on box border", "Resize active selection bounding box"},
	{"Nudge Selection", "Arrow Keys", "Nudge selection box 1 cell (Shift for 10 cells)"},
	{"Alternative Redo", "Ctrl + Shift + Z", "Secondary shortcut to redo undone action"},
	{"Alternative Clear", "Ctrl + Shift + Delete", "Secondary shortcut to clear entire grid"},
	{"Alternative Zoom", "Keypad + / Keypad -", "Secondary shortcuts to zoom camera in or out"},
};

static std::string trim_string(const std::string& str) {
	size_t start = str.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = str.find_last_not_of(" \t\r\n");
	return str.substr(start, end - start + 1);
}

std::vector<UI::MaterialShortcutItem> UIShortcuts::get_all_material_shortcuts() {
	const auto& mats = MaterialManager::get_materials();
	if (mats.empty())
		return {};

	const auto& meta = SetManager::get_current_metadata();

	std::vector<size_t> order;
	order.reserve(mats.size());
	for (size_t i = 1; i < mats.size(); ++i) {
		order.push_back(i);
	}
	order.push_back(0);

	std::vector<UI::MaterialShortcutItem> result;
	result.reserve(order.size());

	std::unordered_set<std::string> used_shortcuts;
	for (size_t idx : order) {
		auto it = meta.shortcuts.find(mats[idx].name);
		if (it != meta.shortcuts.end() && !it->second.empty()) {
			used_shortcuts.insert(it->second);
		}
	}

	int next_order_num = 1;

	auto find_next_available_digit = [&](int start_num) -> int {
		for (int n = start_num; n <= 9; ++n) {
			if (!used_shortcuts.count(std::to_string(n))) {
				return n;
			}
		}
		for (int n = 1; n < start_num && n <= 9; ++n) {
			if (!used_shortcuts.count(std::to_string(n))) {
				return n;
			}
		}
		return 0;
	};

	for (size_t idx : order) {
		UI::MaterialShortcutItem item;
		item.name = mats[idx].name;
		item.id = mats[idx].id;

		auto it = meta.shortcuts.find(mats[idx].name);
		if (it != meta.shortcuts.end() && !it->second.empty()) {
			item.custom_shortcut = it->second;
			item.effective_shortcut = it->second;
			item.is_custom = true;

			if (item.custom_shortcut.length() == 1 && item.custom_shortcut[0] >= '1' &&
				item.custom_shortcut[0] <= '9') {
				next_order_num = (item.custom_shortcut[0] - '0') + 1;
			}
		} else {
			item.is_custom = false;

			int chosen_digit = find_next_available_digit(next_order_num);
			if (chosen_digit >= 1 && chosen_digit <= 9) {
				std::string num_str = std::to_string(chosen_digit);
				item.effective_shortcut = num_str;
				used_shortcuts.insert(num_str);
				next_order_num = chosen_digit + 1;
			} else {
				item.effective_shortcut = "";
			}
		}
		result.push_back(item);
	}

	return result;
}

std::string UIShortcuts::get_effective_material_shortcut(uint8_t id) {
	auto all = get_all_material_shortcuts();
	for (const auto& item : all) {
		if (item.id == id) {
			return item.effective_shortcut;
		}
	}
	return "";
}

void UIShortcuts::render() {
	if (ImGui::BeginTabItem("Shortcuts")) {
		ImGui::Spacing();
		if (ImGui::Button("Reset All Shortcuts to Default", ImVec2(240, 26))) {
			ShortcutManager::reset_all_to_defaults();
			ConfigManager::save();
		}
		ImGui::SameLine();
		static char shortcut_filter_buf[64] = "";
		ImGui::SetNextItemWidth(180);
		ImGui::InputTextWithHint("##shortcut_filter", "Search shortcuts...", shortcut_filter_buf,
								 sizeof(shortcut_filter_buf));

		ImGui::Spacing();
		ImGui::BeginChild("ShortcutsEditorScroll", ImVec2(0, -1), true);

		std::string filter = shortcut_filter_buf;
		std::transform(filter.begin(), filter.end(), filter.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		std::string last_cat;
		for (const auto& s : ShortcutManager::get_all()) {
			if (s.action >= ShortcutAction::QuickSelect1 && s.action <= ShortcutAction::QuickSelect9) {
				continue;
			}
			std::string name_lower = s.display_name;
			std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string cat_lower = s.category;
			std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string key_lower = s.current_key;
			std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string def_lower = s.default_key;
			std::transform(def_lower.begin(), def_lower.end(), def_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && name_lower.find(filter) == std::string::npos &&
				cat_lower.find(filter) == std::string::npos && key_lower.find(filter) == std::string::npos &&
				def_lower.find(filter) == std::string::npos) {
				continue;
			}

			if (s.category != last_cat) {
				if (!last_cat.empty())
					ImGui::Spacing();
				ImGui::Text("%s", s.category.c_str());
				ImGui::Separator();
				last_cat = s.category;
			}

			ImGui::PushID(s.id.c_str());
			bool mod = ShortcutManager::is_modified(s.action);
			if (mod) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "*");
				ImGui::SameLine();
			}
			ImGui::Text("%s:", s.display_name.c_str());
			ImGui::SameLine(240);

			char key_buf[32];
			std::snprintf(key_buf, sizeof(key_buf), "%s", s.current_key.c_str());
			ImGui::SetNextItemWidth(120);
			if (ImGui::InputText("##key", key_buf, sizeof(key_buf))) {
				if (ShortcutManager::set_key_string(s.action, key_buf)) {
					ConfigManager::save();
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Key combo (e.g. Space, F, Ctrl+Z, Shift+A, PageUp). Press Enter to apply.\nDefault: %s",
					s.default_key.c_str());
			}

			if (mod) {
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##key")) {
					ShortcutManager::reset_to_default(s.action);
					ConfigManager::save();
				}
			}
			ImGui::PopID();
		}

		bool mouse_header_shown = false;
		for (const auto& entry : MOUSE_SHORTCUTS) {
			std::string a_lower = entry.action;
			std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string c_lower = entry.control;
			std::transform(c_lower.begin(), c_lower.end(), c_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string d_lower = entry.description;
			std::transform(d_lower.begin(), d_lower.end(), d_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && a_lower.find(filter) == std::string::npos &&
				c_lower.find(filter) == std::string::npos && d_lower.find(filter) == std::string::npos &&
				filter != "mouse") {
				continue;
			}

			if (!mouse_header_shown) {
				ImGui::Spacing();
				ImGui::Text("Canvas & Mouse Controls");
				ImGui::Separator();
				mouse_header_shown = true;
			}

			ImGui::Text("%s:", entry.action.c_str());
			ImGui::SameLine(240);
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "%s", entry.control.c_str());
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", entry.description.c_str());
			}
		}

		ImGui::Spacing();
		ImGui::Text("Material Shortcuts (Set: %s)", SetManager::get_current_set().c_str());
		ImGui::Separator();
		ImGui::TextDisabled("Stored in set_config.ini under [Shortcuts]. If a material is assigned a custom shortcut, "
							"next materials follow the order.");
		if (ImGui::SmallButton("Reset Set Material Shortcuts to Default")) {
			SetManager::clear_current_material_shortcuts();
		}
		ImGui::Spacing();

		auto mat_items = get_all_material_shortcuts();
		for (const auto& item : mat_items) {
			std::string name_l = item.name;
			std::transform(name_l.begin(), name_l.end(), name_l.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string eff_l = item.effective_shortcut;
			std::transform(eff_l.begin(), eff_l.end(), eff_l.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && name_l.find(filter) == std::string::npos &&
				eff_l.find(filter) == std::string::npos && filter != "material" && filter != "materials") {
				continue;
			}

			ImGui::PushID(("mat_sc_" + item.name + "_" + std::to_string(item.id)).c_str());

			const auto& mat = MaterialManager::get_material(item.id);
			ImVec4 col(mat.color[0] / 255.f, mat.color[1] / 255.f, mat.color[2] / 255.f, 1.0f);
			ImGui::ColorButton("##swatch", col, ImGuiColorEditFlags_NoTooltip, ImVec2(15, 15));
			ImGui::SameLine();

			if (item.is_custom) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "*");
				ImGui::SameLine();
			}
			ImGui::Text("%s:", item.name.c_str());
			ImGui::SameLine(180);

			if (item.is_custom) {
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[Custom]");
			} else {
				ImGui::TextDisabled("[Auto]");
			}
			ImGui::SameLine(240);

			char mat_buf[32];
			std::snprintf(mat_buf, sizeof(mat_buf), "%s", item.effective_shortcut.c_str());
			ImGui::SetNextItemWidth(120);
			if (ImGui::InputTextWithHint("##mat_input", "-", mat_buf, sizeof(mat_buf))) {
				std::string new_str = trim_string(mat_buf);
				if (new_str.empty() || new_str == "-") {
					SetManager::remove_current_material_shortcut(item.name);
				} else {
					ImGuiKey k;
					bool c, s, a;
					if (ShortcutManager::parse_key_combo(new_str, k, c, s, a)) {
						std::string formatted = ShortcutManager::format_key_combo(k, c, s, a);
						SetManager::set_current_material_shortcut(item.name, formatted);
					}
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Custom shortcut for '%s' (e.g. 1, 2, G, Shift+1). Press Enter to apply.\nStored in "
								  "set_config.ini under [Shortcuts].",
								  item.name.c_str());
			}

			if (item.is_custom) {
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##mat_reset")) {
					SetManager::remove_current_material_shortcut(item.name);
				}
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
		ImGui::EndTabItem();
	}
}