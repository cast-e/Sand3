#include "ui_theme.hpp"

#include <fmt/format.h>
#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "config_manager.hpp"
#include "icon_manager.hpp"
#include "toast_manager.hpp"
#include "ui_modals.hpp"
#include "ui_widgets.hpp"
#include "window.hpp"

ImVec4 UITheme::default_colors[ImGuiCol_COUNT];
bool UITheme::default_colors_initialized = false;

static const CuratedColor CURATED_THEME_COLORS[] = {
	{ImGuiCol_TitleBg, "Window Title Bar", "Title Bar Normal"},
	{ImGuiCol_TitleBgActive, "Window Title Bar", "Title Bar Active (Focused)"},
	{ImGuiCol_TitleBgCollapsed, "Window Title Bar", "Title Bar Collapsed"},

	{CustomCol_CanvasBackground, "Windows & Backgrounds", "Canvas / Simulation Background"},
	{ImGuiCol_WindowBg, "Windows & Backgrounds", "Window Background"},
	{ImGuiCol_ChildBg, "Windows & Backgrounds", "Panel / Child Window Background"},
	{ImGuiCol_PopupBg, "Windows & Backgrounds", "Popup & Tooltip Background"},
	{ImGuiCol_Border, "Windows & Backgrounds", "Border Color"},
	{ImGuiCol_BorderShadow, "Windows & Backgrounds", "Border Shadow"},
	{ImGuiCol_MenuBarBg, "Windows & Backgrounds", "Menu Bar Background"},

	{ImGuiCol_FrameBg, "Checkboxes, Inputs & Sliders", "Frame Background (Unchecked Checkbox / Input)"},
	{ImGuiCol_FrameBgHovered, "Checkboxes, Inputs & Sliders", "Frame Background Hovered"},
	{ImGuiCol_FrameBgActive, "Checkboxes, Inputs & Sliders", "Frame Background Active"},
	{ImGuiCol_CheckboxSelectedBg, "Checkboxes, Inputs & Sliders", "Checkbox Background (Checked)"},
	{ImGuiCol_CheckMark, "Checkboxes, Inputs & Sliders", "Checkmark Tick Color"},
	{ImGuiCol_SliderGrab, "Checkboxes, Inputs & Sliders", "Slider Grabber"},
	{ImGuiCol_SliderGrabActive, "Checkboxes, Inputs & Sliders", "Slider Grabber Active"},
	{ImGuiCol_InputTextCursor, "Checkboxes, Inputs & Sliders", "Input Text Caret / Cursor"},

	{ImGuiCol_Button, "Buttons & Accent", "Button Normal"},
	{ImGuiCol_ButtonHovered, "Buttons & Accent", "Button Hovered"},
	{ImGuiCol_ButtonActive, "Buttons & Accent", "Button Active / Pressed"},
	{ImGuiCol_Header, "Buttons & Accent", "Header / List Selected"},
	{ImGuiCol_HeaderHovered, "Buttons & Accent", "Header Hovered"},
	{ImGuiCol_HeaderActive, "Buttons & Accent", "Header Active"},

	{ImGuiCol_Text, "Text & Selection", "Main Text"},
	{ImGuiCol_TextDisabled, "Text & Selection", "Disabled Text"},
	{ImGuiCol_TextSelectedBg, "Text & Selection", "Text Selection Background"},
	{ImGuiCol_TextLink, "Text & Selection", "Hyperlink Color"},
	{CustomCol_SelectionBoxBorder, "Text & Selection", "Selection Box Border"},
	{CustomCol_SelectionBoxFill, "Text & Selection", "Selection Box Background / Fill"},

	{ImGuiCol_Tab, "Tabs", "Tab Inactive"},
	{ImGuiCol_TabHovered, "Tabs", "Tab Hovered"},
	{ImGuiCol_TabSelected, "Tabs", "Tab Active / Selected"},
	{ImGuiCol_TabSelectedOverline, "Tabs", "Tab Active Overline"},
	{ImGuiCol_TabDimmed, "Tabs", "Tab Unfocused Inactive"},
	{ImGuiCol_TabDimmedSelected, "Tabs", "Tab Unfocused Selected"},
	{ImGuiCol_TabDimmedSelectedOverline, "Tabs", "Tab Unfocused Overline"},

	{ImGuiCol_Separator, "Dividers & Scrollbars", "Separator Line"},
	{ImGuiCol_SeparatorHovered, "Dividers & Scrollbars", "Separator Line Hovered"},
	{ImGuiCol_SeparatorActive, "Dividers & Scrollbars", "Separator Line Active"},
	{ImGuiCol_ScrollbarBg, "Dividers & Scrollbars", "Scrollbar Track"},
	{ImGuiCol_ScrollbarGrab, "Dividers & Scrollbars", "Scrollbar Thumb"},
	{ImGuiCol_ScrollbarGrabHovered, "Dividers & Scrollbars", "Scrollbar Thumb Hovered"},
	{ImGuiCol_ScrollbarGrabActive, "Dividers & Scrollbars", "Scrollbar Thumb Active"},
	{ImGuiCol_ResizeGrip, "Dividers & Scrollbars", "Window Resize Grip"},
	{ImGuiCol_ResizeGripHovered, "Dividers & Scrollbars", "Window Resize Grip Hovered"},
	{ImGuiCol_ResizeGripActive, "Dividers & Scrollbars", "Window Resize Grip Active"},

	{ImGuiCol_TableHeaderBg, "Tables", "Table Header Background"},
	{ImGuiCol_TableBorderStrong, "Tables", "Table Border Strong"},
	{ImGuiCol_TableBorderLight, "Tables", "Table Border Light"},
	{ImGuiCol_TableRowBg, "Tables", "Table Row Background"},
	{ImGuiCol_TableRowBgAlt, "Tables", "Table Row Alt Background"},

	{ImGuiCol_ModalWindowDimBg, "Modals & Highlights", "Modal Background Dim"},
	{ImGuiCol_NavCursor, "Modals & Highlights", "Navigation Focus Border"},
	{ImGuiCol_NavWindowingHighlight, "Modals & Highlights", "Windowing (Ctrl+Tab) Highlight"},
	{ImGuiCol_NavWindowingDimBg, "Modals & Highlights", "Windowing Dim Background"},
	{ImGuiCol_DragDropTarget, "Modals & Highlights", "Drag & Drop Target Border"},
	{ImGuiCol_DragDropTargetBg, "Modals & Highlights", "Drag & Drop Target Background"},
	{ImGuiCol_UnsavedMarker, "Modals & Highlights", "Unsaved Marker"},
	{ImGuiCol_PlotLines, "Modals & Highlights", "Plot Lines"},
	{ImGuiCol_PlotLinesHovered, "Modals & Highlights", "Plot Lines Hovered"},
	{ImGuiCol_PlotHistogram, "Modals & Highlights", "Plot Histogram"},
	{ImGuiCol_PlotHistogramHovered, "Modals & Highlights", "Plot Histogram Hovered"},
	{ImGuiCol_TreeLines, "Modals & Highlights", "Tree Lines"},
};

void UITheme::apply_theme_style(ImGuiStyle& style, const UIConfig& cfg) {
	style.WindowRounding = cfg.window_rounding;
	style.ChildRounding = cfg.child_rounding;
	style.FrameRounding = cfg.frame_rounding;
	style.PopupRounding = cfg.popup_rounding;
	style.ScrollbarRounding = cfg.scrollbar_rounding;
	style.GrabRounding = cfg.grab_rounding;
	style.TabRounding = cfg.tab_rounding;

	style.WindowBorderSize = cfg.window_border_size;
	style.ChildBorderSize = cfg.child_border_size;
	style.PopupBorderSize = cfg.popup_border_size;
	style.FrameBorderSize = cfg.frame_border_size;
	style.TabBorderSize = cfg.tab_border_size;

	style.WindowPadding = ImVec2(cfg.window_padding_x, cfg.window_padding_y);
	style.FramePadding = ImVec2(cfg.frame_padding_x, cfg.frame_padding_y);
	style.ItemSpacing = ImVec2(cfg.item_spacing_x, cfg.item_spacing_y);
	style.ItemInnerSpacing = ImVec2(cfg.item_inner_spacing_x, cfg.item_inner_spacing_y);
	style.CellPadding = ImVec2(cfg.cell_padding_x, cfg.cell_padding_y);
	style.IndentSpacing = cfg.indent_spacing;
	style.ScrollbarSize = cfg.scrollbar_size;
	style.GrabMinSize = cfg.grab_min_size;
}

float UITheme::get_spacing(float offset) {
	const auto& cfg = ConfigManager::get_config();
	return std::max(0.0f, cfg.ui.base_spacing + offset);
}

void UITheme::init_style() {
	const auto& cfg = ConfigManager::get_config();
	ImGuiStyle& style = ImGui::GetStyle();
	apply_theme_style(style, cfg.ui);

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.13f, 0.95f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.10f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.16f, 0.98f);
	colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.23f, 0.26f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.12f, 0.14f, 0.75f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.39f, 0.42f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.48f, 0.49f, 0.52f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.58f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.54f, 0.94f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.64f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.22f, 0.23f, 0.26f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.50f, 0.85f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.18f, 0.19f, 0.21f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.26f, 0.29f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.33f, 0.37f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.38f, 0.39f, 0.42f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.17f, 0.19f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.54f, 0.94f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
	colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.20f, 0.50f, 0.85f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.11f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.15f, 0.40f, 0.70f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.60f);

	if (!default_colors_initialized) {
		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			default_colors[i] = colors[i];
		}
		default_colors_initialized = true;
	}

	for (const auto& [name, val] : ConfigManager::get_color_overrides()) {
		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			if (name == ImGui::GetStyleColorName(i)) {
				ImVec4 col;
				if (ConfigManager::parse_color_string(val, col)) {
					colors[i] = col;
				}
				break;
			}
		}
	}
}

const ImVec4* UITheme::get_default_colors() { return default_colors; }

void UITheme::reset_theme_colors() {
	ImGuiStyle& style = ImGui::GetStyle();
	for (int i = 0; i < ImGuiCol_COUNT; ++i) {
		style.Colors[i] = default_colors[i];
	}
	auto& cfg = ConfigManager::get_config();
	const auto& def_cfg = ConfigManager::get_default_config();
	cfg.ui.background_color = def_cfg.ui.background_color;
	cfg.ui.selection_box_color = def_cfg.ui.selection_box_color;
	cfg.ui.selection_box_fill = def_cfg.ui.selection_box_fill;
	Window::set_background_color(cfg.ui.background_color);

	ConfigManager::get_color_overrides().clear();
	ConfigManager::save();
}

bool UITheme::apply_theme_from_file(const std::string& theme_path, const std::string& title) {
	std::ifstream fin(theme_path);
	if (!fin.is_open())
		return false;
	try {
		nlohmann::json tj = nlohmann::json::parse(fin);
		auto& cfg = ConfigManager::get_config();
		auto& style = ImGui::GetStyle();
		if (tj.contains("window_rounding")) {
			cfg.ui.window_rounding = tj["window_rounding"].get<float>();
			style.WindowRounding = cfg.ui.window_rounding;
		}
		if (tj.contains("frame_rounding")) {
			cfg.ui.frame_rounding = tj["frame_rounding"].get<float>();
			style.FrameRounding = cfg.ui.frame_rounding;
			style.ChildRounding = cfg.ui.frame_rounding;
			style.PopupRounding = cfg.ui.frame_rounding;
			style.GrabRounding = cfg.ui.frame_rounding;
			style.TabRounding = cfg.ui.frame_rounding;
		}
		if (tj.contains("button_size"))
			cfg.ui.button_size = tj["button_size"].get<int>();
		if (tj.contains("icon_size"))
			cfg.ui.icon_size = tj["icon_size"].get<int>();
		if (tj.contains("sidebar_width"))
			cfg.ui.sidebar_width = tj["sidebar_width"].get<int>();
		if (tj.contains("material_list_height"))
			cfg.ui.material_list_height = tj["material_list_height"].get<int>();
		if (tj.contains("set_list_height"))
			cfg.ui.set_list_height = tj["set_list_height"].get<int>();
		if (tj.contains("save_list_height"))
			cfg.ui.save_list_height = tj["save_list_height"].get<int>();
		if (tj.contains("stamp_list_height"))
			cfg.ui.stamp_list_height = tj["stamp_list_height"].get<int>();
		if (tj.contains("background_color") && tj["background_color"].is_array() &&
			tj["background_color"].size() >= 4) {
			cfg.ui.background_color = ImVec4(tj["background_color"][0], tj["background_color"][1],
											 tj["background_color"][2], tj["background_color"][3]);
			Window::set_background_color(cfg.ui.background_color);
		}
		if (tj.contains("selection_box_color") && tj["selection_box_color"].is_array() &&
			tj["selection_box_color"].size() >= 4) {
			cfg.ui.selection_box_color = ImVec4(tj["selection_box_color"][0], tj["selection_box_color"][1],
												tj["selection_box_color"][2], tj["selection_box_color"][3]);
		}
		if (tj.contains("selection_box_fill") && tj["selection_box_fill"].is_array() &&
			tj["selection_box_fill"].size() >= 4) {
			cfg.ui.selection_box_fill = ImVec4(tj["selection_box_fill"][0], tj["selection_box_fill"][1],
											   tj["selection_box_fill"][2], tj["selection_box_fill"][3]);
		}
		if (tj.contains("colors") && tj["colors"].is_object()) {
			for (auto& [cname, val] : tj["colors"].items()) {
				if (val.is_string()) {
					std::string hex = val.get<std::string>();
					ConfigManager::get_color_overrides()[cname] = hex;
					for (int ci = 0; ci < ImGuiCol_COUNT; ++ci) {
						if (cname == ImGui::GetStyleColorName(ci)) {
							ImVec4 parsed;
							if (ConfigManager::parse_color_string(hex, parsed)) {
								style.Colors[ci] = parsed;
							}
							break;
						}
					}
				}
			}
		}
		apply_theme_style(style, cfg.ui);
		ConfigManager::save();
		if (!title.empty()) {
			ToastManager::success("Theme '" + title + "' applied!");
		}
		return true;
	} catch (...) {
		ToastManager::error("Failed to parse theme file.");
		return false;
	}
}

void UITheme::render_theme_editor() {
	if (ImGui::BeginTabItem("Theme")) {
		ImGui::Spacing();

		auto& cfg = ConfigManager::get_config();
		const auto& def_cfg = ConfigManager::get_default_config();
		ImGuiStyle& style = ImGui::GetStyle();

		ImGui::Spacing();
		if (UIWidgets::button_with_icon("Share Custom Theme to Workshop", IconManager::get(IconID::Transmit),
										ImVec2(-1, 20))) {
			UIModals::open_workshop_share_theme();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Publish your current UI styling and color palette to the Community Workshop.");
		}
		ImGui::Spacing();

		if (ImGui::Button("Reset Style to Defaults", ImVec2(220, 26))) {
			cfg.ui.base_spacing = def_cfg.ui.base_spacing;
			cfg.ui.window_padding_x = def_cfg.ui.window_padding_x;
			cfg.ui.window_padding_y = def_cfg.ui.window_padding_y;
			cfg.ui.frame_padding_x = def_cfg.ui.frame_padding_x;
			cfg.ui.frame_padding_y = def_cfg.ui.frame_padding_y;
			cfg.ui.item_spacing_x = def_cfg.ui.item_spacing_x;
			cfg.ui.item_spacing_y = def_cfg.ui.item_spacing_y;
			cfg.ui.item_inner_spacing_x = def_cfg.ui.item_inner_spacing_x;
			cfg.ui.item_inner_spacing_y = def_cfg.ui.item_inner_spacing_y;
			cfg.ui.cell_padding_x = def_cfg.ui.cell_padding_x;
			cfg.ui.cell_padding_y = def_cfg.ui.cell_padding_y;
			cfg.ui.indent_spacing = def_cfg.ui.indent_spacing;
			cfg.ui.scrollbar_size = def_cfg.ui.scrollbar_size;
			cfg.ui.grab_min_size = def_cfg.ui.grab_min_size;

			cfg.ui.window_rounding = def_cfg.ui.window_rounding;
			cfg.ui.child_rounding = def_cfg.ui.child_rounding;
			cfg.ui.frame_rounding = def_cfg.ui.frame_rounding;
			cfg.ui.popup_rounding = def_cfg.ui.popup_rounding;
			cfg.ui.scrollbar_rounding = def_cfg.ui.scrollbar_rounding;
			cfg.ui.grab_rounding = def_cfg.ui.grab_rounding;
			cfg.ui.tab_rounding = def_cfg.ui.tab_rounding;

			cfg.ui.window_border_size = def_cfg.ui.window_border_size;
			cfg.ui.child_border_size = def_cfg.ui.child_border_size;
			cfg.ui.popup_border_size = def_cfg.ui.popup_border_size;
			cfg.ui.frame_border_size = def_cfg.ui.frame_border_size;
			cfg.ui.tab_border_size = def_cfg.ui.tab_border_size;

			cfg.ui.button_size = def_cfg.ui.button_size;
			cfg.ui.icon_size = def_cfg.ui.icon_size;
			cfg.ui.material_list_height = def_cfg.ui.material_list_height;
			cfg.ui.set_list_height = def_cfg.ui.set_list_height;
			cfg.ui.save_list_height = def_cfg.ui.save_list_height;
			cfg.ui.stamp_list_height = def_cfg.ui.stamp_list_height;
			cfg.ui.sidebar_width = def_cfg.ui.sidebar_width;
			cfg.ui.font_size = def_cfg.ui.font_size;
			cfg.ui.show_fps = def_cfg.ui.show_fps;
			cfg.ui.show_active_cells = def_cfg.ui.show_active_cells;
			cfg.ui.show_simulation_status = def_cfg.ui.show_simulation_status;
			cfg.ui.background_color = def_cfg.ui.background_color;

			apply_theme_style(style, cfg.ui);

			ConfigManager::save();
			Window::set_background_color(cfg.ui.background_color);
		}
		ImGui::Spacing();

		if (ImGui::CollapsingHeader("Layout & Dimensions", ImGuiTreeNodeFlags_DefaultOpen)) {
			int btn_size = cfg.ui.button_size;
			if (ImGui::InputInt("Button Size", &btn_size, 1, 5)) {
				btn_size = std::clamp(btn_size, 20, 50);
				cfg.ui.button_size = btn_size;
				ConfigManager::save();
			}

			int icon_sz = cfg.ui.icon_size;
			if (ImGui::InputInt("Icon Size", &icon_sz, 1, 5)) {
				icon_sz = std::clamp(icon_sz, 10, 36);
				cfg.ui.icon_size = icon_sz;
				ConfigManager::save();
			}

			int sb_w = cfg.ui.sidebar_width;
			if (ImGui::InputInt("Sidebar Width", &sb_w, 10, 50)) {
				sb_w = std::clamp(sb_w, 260, 800);
				cfg.ui.sidebar_width = sb_w;
				ConfigManager::save();
			}

			float f_sz = cfg.ui.font_size;
			if (ImGui::InputFloat("Font Size*", &f_sz, 0.5f, 2.0f, "%.1f px")) {
				f_sz = std::clamp(f_sz, 10.0f, 32.0f);
				cfg.ui.font_size = f_sz;
				ConfigManager::save();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("*Requires application restart to reload font atlas.");
			}

			bool show_fps = cfg.ui.show_fps;
			if (UIWidgets::custom_checkbox("FPS##show_fps", &show_fps, IconID::Clock, "Display FPS in header")) {
				cfg.ui.show_fps = show_fps;
				ConfigManager::save();
			}
			ImGui::SameLine(0.0f, get_spacing(0.0f));
			bool show_cells = cfg.ui.show_active_cells;
			if (UIWidgets::custom_checkbox("Cells##show_cells", &show_cells, IconID::Brick,
										   "Display count of active cells in header")) {
				cfg.ui.show_active_cells = show_cells;
				ConfigManager::save();
			}
			ImGui::SameLine(0.0f, get_spacing(0.0f));
			bool show_status = cfg.ui.show_simulation_status;
			if (UIWidgets::custom_checkbox("Status##show_status", &show_status, IconID::Play,
										   "Display PAUSED or RUNNING status indicator in header")) {
				cfg.ui.show_simulation_status = show_status;
				ConfigManager::save();
			}
		}

		if (ImGui::CollapsingHeader("Spacing & Padding System", ImGuiTreeNodeFlags_DefaultOpen)) {
			float base_sp = cfg.ui.base_spacing;
			if (ImGui::InputFloat("Base Spacing", &base_sp, 0.5f, 1.0f, "%.1f px")) {
				base_sp = std::clamp(base_sp, 0.0f, 32.0f);
				float delta = base_sp - cfg.ui.base_spacing;
				cfg.ui.base_spacing = base_sp;
				cfg.ui.window_padding_x = std::max(0.0f, cfg.ui.window_padding_x + delta);
				cfg.ui.window_padding_y = std::max(0.0f, cfg.ui.window_padding_y + delta);
				cfg.ui.frame_padding_x = std::max(0.0f, cfg.ui.frame_padding_x + delta);
				cfg.ui.frame_padding_y = std::max(0.0f, cfg.ui.frame_padding_y + delta);
				cfg.ui.item_spacing_x = std::max(0.0f, cfg.ui.item_spacing_x + delta);
				cfg.ui.item_spacing_y = std::max(0.0f, cfg.ui.item_spacing_y + delta);
				cfg.ui.item_inner_spacing_x = std::max(0.0f, cfg.ui.item_inner_spacing_x + delta);
				cfg.ui.item_inner_spacing_y = std::max(0.0f, cfg.ui.item_inner_spacing_y + delta);
				cfg.ui.cell_padding_x = std::max(0.0f, cfg.ui.cell_padding_x + delta);
				cfg.ui.cell_padding_y = std::max(0.0f, cfg.ui.cell_padding_y + delta);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Master base spacing constant. Modifying shifts all derived spacings proportionally.");
			}

			if (ImGui::Button("Reset Offsets to Default")) {
				cfg.ui.window_padding_x = cfg.ui.base_spacing + 6.0f;
				cfg.ui.window_padding_y = cfg.ui.base_spacing + 6.0f;
				cfg.ui.frame_padding_x = cfg.ui.base_spacing + 2.0f;
				cfg.ui.frame_padding_y = std::max(0.0f, cfg.ui.base_spacing - 1.0f);
				cfg.ui.item_spacing_x = cfg.ui.base_spacing + 2.0f;
				cfg.ui.item_spacing_y = cfg.ui.base_spacing;
				cfg.ui.item_inner_spacing_x = cfg.ui.base_spacing;
				cfg.ui.item_inner_spacing_y = cfg.ui.base_spacing;
				cfg.ui.cell_padding_x = std::max(0.0f, cfg.ui.base_spacing - 2.0f);
				cfg.ui.cell_padding_y = std::max(0.0f, cfg.ui.base_spacing - 4.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Recalculate all padding and spacing values using default offsets from Base Spacing.");
			}

			auto render_spacing_item = [&](const char* name, float& val, const char* id) {
				float offset = val - cfg.ui.base_spacing;
				std::string label = fmt::format("{} (Base {:+.1f})##{}", name, offset, id);
				float v = val;
				if (ImGui::InputFloat(label.c_str(), &v, 0.5f, 1.0f, "%.1f px")) {
					val = std::max(0.0f, v);
					apply_theme_style(style, cfg.ui);
					ConfigManager::save();
				}
			};

			render_spacing_item("Window Padding X", cfg.ui.window_padding_x, "wp_x");
			render_spacing_item("Window Padding Y", cfg.ui.window_padding_y, "wp_y");
			render_spacing_item("Frame Padding X", cfg.ui.frame_padding_x, "fp_x");
			render_spacing_item("Frame Padding Y", cfg.ui.frame_padding_y, "fp_y");
			render_spacing_item("Item Spacing X", cfg.ui.item_spacing_x, "is_x");
			render_spacing_item("Item Spacing Y", cfg.ui.item_spacing_y, "is_y");
			render_spacing_item("Item Inner Spacing X", cfg.ui.item_inner_spacing_x, "iis_x");
			render_spacing_item("Item Inner Spacing Y", cfg.ui.item_inner_spacing_y, "iis_y");
			render_spacing_item("Cell Padding X", cfg.ui.cell_padding_x, "cp_x");
			render_spacing_item("Cell Padding Y", cfg.ui.cell_padding_y, "cp_y");

			float ind = cfg.ui.indent_spacing;
			if (ImGui::InputFloat("Indent Spacing", &ind, 1.0f, 5.0f, "%.1f px")) {
				cfg.ui.indent_spacing = std::max(0.0f, ind);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float scr_sz = cfg.ui.scrollbar_size;
			if (ImGui::InputFloat("Scrollbar Size", &scr_sz, 1.0f, 2.0f, "%.1f px")) {
				cfg.ui.scrollbar_size = std::clamp(scr_sz, 6.0f, 32.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float g_min = cfg.ui.grab_min_size;
			if (ImGui::InputFloat("Grab Min Size", &g_min, 1.0f, 2.0f, "%.1f px")) {
				cfg.ui.grab_min_size = std::clamp(g_min, 4.0f, 32.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}
		}

		if (ImGui::CollapsingHeader("Rounding", ImGuiTreeNodeFlags_DefaultOpen)) {
			float win_r = cfg.ui.window_rounding;
			if (ImGui::InputFloat("Window Rounding", &win_r, 0.5f, 2.0f, "%.1f px")) {
				cfg.ui.window_rounding = std::clamp(win_r, 0.0f, 24.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float child_r = cfg.ui.child_rounding;
			if (ImGui::InputFloat("Child Rounding", &child_r, 0.5f, 2.0f, "%.1f px")) {
				cfg.ui.child_rounding = std::clamp(child_r, 0.0f, 20.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float frame_r = cfg.ui.frame_rounding;
			if (ImGui::InputFloat("Frame Rounding", &frame_r, 0.5f, 2.0f, "%.1f px")) {
				cfg.ui.frame_rounding = std::clamp(frame_r, 0.0f, 20.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float popup_r = cfg.ui.popup_rounding;
			if (ImGui::InputFloat("Popup Rounding", &popup_r, 0.5f, 2.0f, "%.1f px")) {
				cfg.ui.popup_rounding = std::clamp(popup_r, 0.0f, 20.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float scrl_r = cfg.ui.scrollbar_rounding;
			if (ImGui::InputFloat("Scrollbar Rounding", &scrl_r, 0.5f, 2.0f, "%.1f px")) {
				cfg.ui.scrollbar_rounding = std::clamp(scrl_r, 0.0f, 20.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float grab_r = cfg.ui.grab_rounding;
			if (ImGui::InputFloat("Grab Rounding", &grab_r, 0.5f, 2.0f, "%.1f px")) {
				cfg.ui.grab_rounding = std::clamp(grab_r, 0.0f, 20.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float tab_r = cfg.ui.tab_rounding;
			if (ImGui::InputFloat("Tab Rounding", &tab_r, 0.5f, 2.0f, "%.1f px")) {
				cfg.ui.tab_rounding = std::clamp(tab_r, 0.0f, 20.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}
		}

		if (ImGui::CollapsingHeader("Borders", ImGuiTreeNodeFlags_DefaultOpen)) {
			float win_b = cfg.ui.window_border_size;
			if (ImGui::InputFloat("Window Border Size", &win_b, 0.5f, 1.0f, "%.1f px")) {
				cfg.ui.window_border_size = std::clamp(win_b, 0.0f, 10.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float child_b = cfg.ui.child_border_size;
			if (ImGui::InputFloat("Child Border Size", &child_b, 0.5f, 1.0f, "%.1f px")) {
				cfg.ui.child_border_size = std::clamp(child_b, 0.0f, 10.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float popup_b = cfg.ui.popup_border_size;
			if (ImGui::InputFloat("Popup Border Size", &popup_b, 0.5f, 1.0f, "%.1f px")) {
				cfg.ui.popup_border_size = std::clamp(popup_b, 0.0f, 10.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float frame_b = cfg.ui.frame_border_size;
			if (ImGui::InputFloat("Frame Border Size", &frame_b, 0.5f, 1.0f, "%.1f px")) {
				cfg.ui.frame_border_size = std::clamp(frame_b, 0.0f, 10.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}

			float tab_b = cfg.ui.tab_border_size;
			if (ImGui::InputFloat("Tab Border Size", &tab_b, 0.5f, 1.0f, "%.1f px")) {
				cfg.ui.tab_border_size = std::clamp(tab_b, 0.0f, 10.0f);
				apply_theme_style(style, cfg.ui);
				ConfigManager::save();
			}
		}

		ImGui::Spacing();

		if (ImGui::Button("Reset All Colors to Default", ImVec2(220, 26))) {
			reset_theme_colors();
		}
		ImGui::SameLine();
		static char color_filter_buf[64] = "";
		ImGui::SetNextItemWidth(180);
		ImGui::InputTextWithHint("##col_filter", "Filter colors...", color_filter_buf, sizeof(color_filter_buf));

		ImGui::Spacing();
		ImGui::BeginChild("ColorEditorScroll", ImVec2(0, 720), true);
		const ImVec4* defs = get_default_colors();
		std::string filter = color_filter_buf;
		std::transform(filter.begin(), filter.end(), filter.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		std::string last_col_cat;
		for (const auto& item : CURATED_THEME_COLORS) {
			const char* col_name = "";
			ImVec4* col_ptr = nullptr;
			const ImVec4* def_ptr = nullptr;

			if (item.col >= 0) {
				col_name = ImGui::GetStyleColorName(item.col);
				col_ptr = &style.Colors[item.col];
				if (defs)
					def_ptr = &defs[item.col];
			} else if (item.col == CustomCol_CanvasBackground) {
				col_name = "CanvasBackground";
				col_ptr = &cfg.ui.background_color;
				def_ptr = &ConfigManager::get_default_config().ui.background_color;
			} else if (item.col == CustomCol_SelectionBoxBorder) {
				col_name = "SelectionBoxBorder";
				col_ptr = &cfg.ui.selection_box_color;
				def_ptr = &ConfigManager::get_default_config().ui.selection_box_color;
			} else if (item.col == CustomCol_SelectionBoxFill) {
				col_name = "SelectionBoxFill";
				col_ptr = &cfg.ui.selection_box_fill;
				def_ptr = &ConfigManager::get_default_config().ui.selection_box_fill;
			}

			if (!col_ptr)
				continue;

			std::string lname = col_name;
			std::transform(lname.begin(), lname.end(), lname.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string llabel = item.label;
			std::transform(llabel.begin(), llabel.end(), llabel.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string lcat = item.category;
			std::transform(lcat.begin(), lcat.end(), lcat.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && lname.find(filter) == std::string::npos &&
				llabel.find(filter) == std::string::npos && lcat.find(filter) == std::string::npos) {
				continue;
			}

			if (item.category != last_col_cat) {
				if (!last_col_cat.empty())
					ImGui::Spacing();
				ImGui::Text("%s", item.category);
				ImGui::Separator();
				last_col_cat = item.category;
			}

			ImGui::PushID(item.col);
			bool modified = def_ptr && ConfigManager::color_differs(*col_ptr, *def_ptr);
			if (modified) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "*");
				ImGui::SameLine();
			}
			ImGuiColorEditFlags flags = (item.col == CustomCol_CanvasBackground)
											? ImGuiColorEditFlags_NoAlpha
											: (ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
			if (ImGui::ColorEdit4(item.label, (float*)col_ptr, flags)) {
				if (item.col >= 0) {
					ConfigManager::get_color_overrides()[col_name] = ConfigManager::color_to_hex(*col_ptr);
				} else if (item.col == CustomCol_CanvasBackground) {
					Window::set_background_color(cfg.ui.background_color);
				}
				ConfigManager::save();
			}
			if (modified) {
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##col")) {
					*col_ptr = *def_ptr;
					if (item.col >= 0) {
						ConfigManager::get_color_overrides().erase(col_name);
					} else if (item.col == CustomCol_CanvasBackground) {
						Window::set_background_color(cfg.ui.background_color);
					}
					ConfigManager::save();
				}
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::EndTabItem();
	}
}