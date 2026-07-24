#include "ui.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "grid.hpp"
#include "material_manager.hpp"
#include "roboto.h"
#include "sanitize.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "window.hpp"

namespace fs = std::filesystem;

static void init_style() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 6.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;

	style.ItemSpacing = ImVec2(8, 6);
	style.ItemInnerSpacing = ImVec2(6, 6);
	style.WindowPadding = ImVec2(12, 12);
	style.FramePadding = ImVec2(8, 5);

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
	colors[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.39f, 0.42f, 1.00f);
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
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.11f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
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
}

char UI::save_file_name_buf[128] = "";
char UI::save_as_buf[64] = "";
char UI::new_set_name_buf[64] = "";

int UI::selected_save_id = -1;
int UI::selected_id = -1;
int UI::mouse_size = 5;

bool UI::open_switch_popup = false;
bool UI::open_create_set_popup = false;
bool UI::open_delete_set_popup = false;
bool UI::open_empty_rule_warning_popup = false;

bool UI::duplicate_set_checkbox = false;
bool UI::exit_save_as_new_set = false;

bool UI::update = false;
bool UI::step_frame = false;

bool UI::show_exit_popup = false;

bool UI::unsaved_changes = false;
std::string UI::pending_set_switch = "";
std::string UI::pending_save_load = "";
bool UI::ui_compact = false;

float UI::zoom = 1.0f;
float UI::pan_x = 0.0f;
float UI::pan_y = 0.0f;
float UI::target_zoom = 1.0f;
float UI::target_pan_x = 0.0f;
float UI::target_pan_y = 0.0f;

void UI::init() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImFontConfig font_cfg;
	font_cfg.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(___assets_Roboto_Regular_ttf),
								   ___assets_Roboto_Regular_ttf_len, 16.0f, &font_cfg);

	init_style();

	ImGui_ImplSDL3_InitForSDLRenderer(Window::get_window(), Window::get_renderer());
	ImGui_ImplSDLRenderer3_Init(Window::get_renderer());
}

void UI::shutdown() {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
}

void UI::render() {
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	if (ui_compact) {
		ImGui::SetNextWindowSize(ImVec2(280.0f, 400.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Simulation Controls")) {
			render_sim_content();
		}
		ImGui::End();
	} else {
		ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(570.0f, 600.0f), ImGuiCond_FirstUseEver);

		ImGui::Begin("Simulation Editor", nullptr);

		render_header(io);

		if (ImGui::BeginTabBar("SidebarTabs")) {
			render_material_editor();
			render_manage_sets();
			render_save_load();
			render_shortcuts();
			render_advanced_options();
			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	SDL_FRect dst_rect = Window::get_dst_rect();

	if (!io.WantCaptureMouse) {
		ImVec2 mouse_pos = io.MousePos;
		float mx = mouse_pos.x;
		float my = mouse_pos.y;

		float norm_x = (mx - dst_rect.x) / dst_rect.w;
		float norm_y = (my - dst_rect.y) / dst_rect.h;
		float fx = norm_x * SIM_WIDTH;
		float fy = norm_y * SIM_HEIGHT;

		int x_start = static_cast<int>(std::round(fx - static_cast<float>(mouse_size) / 2.0f));
		int y_start = static_cast<int>(std::round(fy - static_cast<float>(mouse_size) / 2.0f));

		float sx1 = dst_rect.x + (static_cast<float>(x_start) / SIM_WIDTH) * dst_rect.w;
		float sy1 = dst_rect.y + (static_cast<float>(y_start) / SIM_HEIGHT) * dst_rect.h;
		float sx2 = dst_rect.x + (static_cast<float>(x_start + mouse_size) / SIM_WIDTH) * dst_rect.w;
		float sy2 = dst_rect.y + (static_cast<float>(y_start + mouse_size) / SIM_HEIGHT) * dst_rect.h;

		ImGui::GetBackgroundDrawList()->AddRect(ImVec2(sx1, sy1), ImVec2(sx2, sy2), IM_COL32(255, 255, 255, 140), 0.0f,
												0, 1.5f);
	}

	render_modals();
}

void UI::handle_interaction() {
	auto [win_w, win_h] = Window::get_size();
	float rem_w = static_cast<float>(win_w);
	float rem_h = static_cast<float>(win_h);
	float sim_aspect = static_cast<float>(SIM_WIDTH) / SIM_HEIGHT;

	float target_w = rem_w;
	float target_h = rem_w / sim_aspect;
	if (target_h > rem_h) {
		target_h = rem_h;
		target_w = rem_h * sim_aspect;
	}

	ImGuiIO& io = ImGui::GetIO();

	if (!io.WantCaptureMouse && io.MouseWheel != 0.0f && io.KeyShift) {
		target_zoom += io.MouseWheel * 0.2f * target_zoom;
	}

	float dt = std::min(io.DeltaTime, 0.1f);
	float t = 1.0f - std::exp(-10.0f * dt);
	zoom = zoom + (target_zoom - zoom) * t;
	pan_x = pan_x + (target_pan_x - pan_x) * t;
	pan_y = pan_y + (target_pan_y - pan_y) * t;

	float w_new = target_w * zoom;
	float h_new = target_h * zoom;

	if (!io.WantCaptureMouse && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		ImVec2 delta = io.MouseDelta;
		target_pan_x -= delta.x * (SIM_WIDTH / w_new);
		target_pan_y -= delta.y * (SIM_HEIGHT / h_new);
	}

	pan_x = pan_x + (target_pan_x - pan_x) * 0.15f;
	pan_y = pan_y + (target_pan_y - pan_y) * 0.15f;

	float offset_screen_x = pan_x * (w_new / SIM_WIDTH);
	float offset_screen_y = pan_y * (h_new / SIM_HEIGHT);
	float center_x_shifted = rem_w / 2.0f - offset_screen_x;
	float center_y_shifted = rem_h / 2.0f - offset_screen_y;

	SDL_FRect dst_rect;
	dst_rect.x = center_x_shifted - w_new / 2.0f;
	dst_rect.y = center_y_shifted - h_new / 2.0f;
	dst_rect.w = w_new;
	dst_rect.h = h_new;
	Window::set_dst_rect(dst_rect);

	if (!io.WantTextInput) {
		if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
			update = !update;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_F) && !update) {
			step_frame = true;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
			ui_compact = !ui_compact;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_R)) {
			Grid::clear();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_PageUp) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
			target_zoom *= 1.2f;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
			target_zoom /= 1.2f;
		}
		unsigned char material_count = MaterialManager::get_material_count();
		if (ImGui::IsKeyPressed(ImGuiKey_1) && material_count > 1) {
			selected_id = 1;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_2) && material_count > 2) {
			selected_id = 2;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_3) && material_count > 3) {
			selected_id = 3;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_4) && material_count > 4) {
			selected_id = 4;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_5) && material_count > 5) {
			selected_id = 5;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_6) && material_count > 6) {
			selected_id = 6;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_7) && material_count > 7) {
			selected_id = 7;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_8) && material_count > 8) {
			selected_id = 8;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_9) && material_count > 9) {
			selected_id = 9;
		}
	}

	if (!io.WantCaptureMouse && io.MouseWheel != 0.0f && !io.KeyShift) {
		bool fast = io.KeyCtrl;
		mouse_size += static_cast<int>(io.MouseWheel) * (fast ? 5 : 1);
		mouse_size = std::clamp(mouse_size, 1, 512);
	}

	if (!io.WantCaptureMouse) {
		ImVec2 mouse_pos = io.MousePos;
		float mx = mouse_pos.x;
		float my = mouse_pos.y;

		float norm_x = (mx - dst_rect.x) / dst_rect.w;
		float norm_y = (my - dst_rect.y) / dst_rect.h;
		float fx = norm_x * SIM_WIDTH;
		float fy = norm_y * SIM_HEIGHT;
		unsigned int x_cell = static_cast<unsigned int>(fx);
		unsigned int y_cell = static_cast<unsigned int>(fy);

		if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
			ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
			if (drag_delta.x * drag_delta.x + drag_delta.y * drag_delta.y < 25.0f) {
				if (x_cell < SIM_WIDTH && y_cell < SIM_HEIGHT) {
					unsigned char cell = Grid::get_cell(x_cell, y_cell);
					selected_id = MaterialManager::get_material(cell).id;
				}
			}
		}

		int x_start = static_cast<int>(std::round(fx - static_cast<float>(mouse_size) / 2.0f));
		int y_start = static_cast<int>(std::round(fy - static_cast<float>(mouse_size) / 2.0f));

		for (int dx = 0; dx < mouse_size; ++dx) {
			for (int dy = 0; dy < mouse_size; ++dy) {
				int cx = x_start + dx;
				int cy = y_start + dy;
				if (cx >= 0 && cx < static_cast<int>(SIM_WIDTH) && cy >= 0 && cy < static_cast<int>(SIM_HEIGHT)) {
					if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
						Grid::set_cell(static_cast<unsigned int>(cx), static_cast<unsigned int>(cy), selected_id);
					} else if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
						Grid::set_cell(static_cast<unsigned int>(cx), static_cast<unsigned int>(cy), 0);
					}
				}
			}
		}
	}
}

void UI::render_header(ImGuiIO& io) {
	ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "SAND3 SIMULATOR");
	ImGui::Text("FPS: %.1f (%.3f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
	ImGui::Text("Active cells: %u", Grid::get_changed_cells());
	ImGui::Separator();
}

void UI::render_sim_content() {
	ImGui::Spacing();
	if (update) {
		if (ImGui::Button("Pause", ImVec2(-1, 30))) {
			update = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Pauses the simulation.");
		}
	} else {
		if (ImGui::Button("Resume", ImVec2(-1, 30))) {
			update = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Resumes the simulation.");
		}
	}

	if (update) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Step Frame", ImVec2(-1, 30))) {
		step_frame = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Steps the simulation by one frame (F).");
	}
	if (update) {
		ImGui::EndDisabled();
	}

	ImGui::Spacing();

	if (ImGui::Button("Clear Grid", ImVec2(-1, 30))) {
		Grid::clear();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Clears all cells on the grid.");
	}

	ImGui::Separator();
	ImGui::Text("Brush Settings:");
	ImGui::SliderInt("Brush Size", &mouse_size, 1, 512);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Adjust brush width.");
	}

	std::vector<Material>& materials = MaterialManager::get_materials();

	ImGui::Separator();
	ImGui::Text("Select Material:");
	ImGui::BeginChild("SimMaterialsList", ImVec2(0, 150), true);
	for (int i = 0; i < (int)materials.size(); ++i) {
		std::string label = materials[i].name;
		ImVec4 color =
			ImVec4(materials[i].color[0] / 255.f, materials[i].color[1] / 255.f, materials[i].color[2] / 255.f, 1.f);
		ImGui::ColorButton(("##sim_color_" + std::to_string(i)).c_str(), color, ImGuiColorEditFlags_NoTooltip,
						   ImVec2(15, 15));
		ImGui::SameLine();
		if (ImGui::Selectable((label + "##sim_" + std::to_string(i)).c_str(), selected_id == i)) {
			selected_id = i;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Select %s to draw on the grid (Left Click = Draw,\n"
							  "Right Click = Erase).",
							  label.c_str());
		}
	}
	ImGui::EndChild();
}

void UI::render_material_editor() {
	std::vector<Material>& materials = MaterialManager::get_materials();

	if (ImGui::BeginTabItem("Materials")) {
		ImGui::Spacing();
		bool rebuild_needed = false;

		if (ImGui::CollapsingHeader("Materials List", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::BeginChild("MaterialsListScroll", ImVec2(0, 150), true);

			for (size_t i = 1; i < materials.size(); ++i) {
				std::string label = materials[i].name;

				ImVec4 color = ImVec4(materials[i].color[0] / 255.f, materials[i].color[1] / 255.f,
									  materials[i].color[2] / 255.f, 1.f);
				ImGui::ColorButton(("##color_" + std::to_string(i)).c_str(), color, ImGuiColorEditFlags_NoTooltip,
								   ImVec2(15, 15));
				ImGui::SameLine();

				if (ImGui::Selectable((label + "##" + std::to_string(i)).c_str(), selected_id == (int)i)) {
					selected_id = static_cast<int>(i);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Has %zu rule%s", materials[i].user_rules.size(),
									  (materials[i].user_rules.size() == 1 ? "" : "s"));
				}
			}

			// Always end with the 0th material.
			ImVec4 color = ImVec4(materials[0].color[0] / 255.f, materials[0].color[1] / 255.f,
								  materials[0].color[2] / 255.f, 1.f);
			ImGui::ColorButton("##color_0", color, ImGuiColorEditFlags_NoTooltip, ImVec2(15, 15));
			ImGui::SameLine();

			if (ImGui::Selectable((materials[0].name + "##0").c_str(), selected_id == 0)) {
				selected_id = 0;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Has %zu rule%s", materials[0].user_rules.size(),
								  (materials[0].user_rules.size() == 1 ? "" : "s"));
			}

			ImGui::EndChild();

			ImGui::Spacing();
			if (ImGui::Button("New", ImVec2(80, 25))) {
				Material new_mat;
				new_mat.name = "new_material_" + std::to_string(materials.size());
				new_mat.color = {255, 255, 255};
				MaterialManager::add_material(new_mat);
				selected_id = static_cast<int>(materials.size() - 1);
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Create a new material.");
			}

			ImGui::SameLine();
			if (selected_id >= 0 && selected_id < (int)materials.size()) {
				if (ImGui::Button("Copy", ImVec2(80, 25))) {
					Material duplicated_mat = materials[selected_id];
					duplicated_mat.name = duplicated_mat.name + "_copy";
					MaterialManager::add_material(duplicated_mat);
					selected_id = static_cast<int>(materials.size() - 1);
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Duplicate the selected material.");
				}

				ImGui::SameLine();
				bool is_empty_mat = (materials[selected_id].id == 0);
				if (is_empty_mat)
					ImGui::BeginDisabled();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
				if (ImGui::Button("Delete", ImVec2(80, 25))) {
					MaterialManager::remove_material(selected_id);
					selected_id = -1;
					unsaved_changes = true;
				}
				ImGui::PopStyleColor(3);
				if (is_empty_mat) {
					ImGui::EndDisabled();
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
						ImGui::SetTooltip("The default material (ID 0) cannot be deleted.");
					}
				} else if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Remove the selected material.");
				}
			} else {
				ImGui::TextDisabled("Select a material to edit/copy/delete");
			}
		}

		ImGui::Separator();

		if (selected_id >= 0 && selected_id < (int)materials.size()) {
			Material& mat = materials[selected_id];
			bool is_empty_mat = (mat.id == 0);

			ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "Editing: %s", mat.name.c_str());

			char name_buf[128];
			strncpy(name_buf, mat.name.c_str(), sizeof(name_buf));
			name_buf[sizeof(name_buf) - 1] = '\0';
			if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
				std::string new_name = name_buf;
				new_name = sanitize_name(new_name);
				if (MaterialManager::is_valid_name(new_name) && new_name != mat.name) {
					Material temp = mat;
					temp.name = sanitize_name(new_name);
					MaterialManager::edit_material(selected_id, temp);
					unsaved_changes = true;
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Rename the material configuration.");
			}

			float col[3] = {mat.color[0] / 255.f, mat.color[1] / 255.f, mat.color[2] / 255.f};
			if (ImGui::ColorEdit3("Color", col)) {
				mat.color[0] = static_cast<unsigned char>(col[0] * 255.f);
				mat.color[1] = static_cast<unsigned char>(col[1] * 255.f);
				mat.color[2] = static_cast<unsigned char>(col[2] * 255.f);
				mat.packed_color = MaterialManager::pack_color(mat.color);
				unsaved_changes = true;
				Grid::draw_material(mat.id);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Pick color for this material in the simulator.");
			}

			ImGui::Separator();
			ImGui::Text("Rules:");

			if (ImGui::Button("Add Rule")) {
				if (is_empty_mat) {
					open_empty_rule_warning_popup = true;
				} else {
					UserRule new_rule;
					new_rule.when.fill("");
					new_rule.when[12] = mat.name;
					new_rule.then.fill("");
					new_rule.chance = 1;
					mat.user_rules.push_back(new_rule);
					rebuild_needed = true;
					unsaved_changes = true;
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Creates a rule.");
			}

			ImGui::BeginChild("RulesScroll", ImVec2(0, 0), true);
			for (int r_id = 0; r_id < (int)mat.user_rules.size(); ++r_id) {
				UserRule& rule = mat.user_rules[r_id];
				ImGui::PushID(r_id);

				ImGui::BeginChild(("##rule_card_" + std::to_string(r_id)).c_str(), ImVec2(0, 230), true,
								  ImGuiWindowFlags_NoScrollbar);

				ImGui::BeginGroup();
				ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "Rule #%d", r_id + 1);

				float btn_size = ImGui::GetFrameHeight();
				if (r_id > 0) {
					if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
						std::swap(mat.user_rules[r_id], mat.user_rules[r_id - 1]);
						rebuild_needed = true;
						unsaved_changes = true;
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Reorder up. Higher priority rules execute first.");
					}
				} else {
					ImGui::Dummy(ImVec2(btn_size, btn_size));
				}
				ImGui::SameLine();

				if (r_id < (int)mat.user_rules.size() - 1) {
					if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
						std::swap(mat.user_rules[r_id], mat.user_rules[r_id + 1]);
						rebuild_needed = true;
						unsaved_changes = true;
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Reorder down. Lower priority rules execute later.");
					}
				} else {
					ImGui::Dummy(ImVec2(btn_size, btn_size));
				}
				ImGui::SameLine(0.0f, 10.0f);

				if (ImGui::Button("Copy", ImVec2(45, 0))) {
					UserRule duplicated_rule = rule;
					mat.user_rules.push_back(duplicated_rule);
					rebuild_needed = true;
					unsaved_changes = true;
					ImGui::EndGroup();
					ImGui::EndChild();
					ImGui::PopID();
					break;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Duplicate this rule.");
				}
				ImGui::SameLine();

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
				if (ImGui::Button("X", ImVec2(btn_size, btn_size))) {
					mat.user_rules.erase(mat.user_rules.begin() + r_id);
					rebuild_needed = true;
					unsaved_changes = true;
					ImGui::PopStyleColor(3);
					ImGui::EndGroup();
					ImGui::EndChild();
					ImGui::PopID();
					break;
				}
				ImGui::PopStyleColor(3);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Delete this rule.");
				}

				const unsigned char step = 1;
				const unsigned char step_fast = 10;
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::InputScalar("##chance_input", ImGuiDataType_U8, &rule.chance, &step, &step_fast,
									   "1 in %d")) {
					if (rule.chance == 0) {
						rule.chance = 1;
					}
					rebuild_needed = true;
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Controls rule probability. Higher = lower probability.");
				}
				float pct = 100.0f / rule.chance;
				ImGui::Text("Probability: %.3f%%", pct);

				if (ImGui::Checkbox("X Symmetry", &rule.sym_x)) {
					rebuild_needed = true;
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Allows the rule to match its left-right mirror image.");
				}
				if (ImGui::Checkbox("Y Symmetry", &rule.sym_y)) {
					rebuild_needed = true;
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Allows the rule to match its up-down mirror image.");
				}
				if (ImGui::Checkbox("Rotational Symmetry", &rule.sym_rot)) {
					rebuild_needed = true;
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Allows the rule to match with 90 degree rotations of itself.");
				}

				ImGui::EndGroup();

				ImGui::SameLine(0.0f, 5.0f);

				ImGui::BeginGroup();
				ImGui::Text("When");
				for (int y = 0; y < NEIGHBOR_SIZE; ++y) {
					for (int x = 0; x < NEIGHBOR_SIZE; ++x) {
						int c_id = y * NEIGHBOR_SIZE + x;
						ImGui::PushID(c_id);

						std::string label = "?";
						ImVec4 btn_col = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

						if (c_id == 12) {
							label = mat.name.substr(0, 1);
							btn_col = ImVec4(mat.color[0] / 255.f, mat.color[1] / 255.f, mat.color[2] / 255.f, 1.0f);
							ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
							ImGui::Button(label.c_str(), ImVec2(25, 25));
							ImGui::PopStyleColor();
						} else {
							if (!rule.when[c_id].empty()) {
								label = rule.when[c_id].substr(0, 1);
								for (const auto& m : materials) {
									if (m.name == rule.when[c_id]) {
										btn_col =
											ImVec4(m.color[0] / 255.f, m.color[1] / 255.f, m.color[2] / 255.f, 1.0f);
										break;
									}
								}
							}

							ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
							if (ImGui::Button(label.c_str(), ImVec2(25, 25))) {
								ImGui::OpenPopup("select_material_when");
							}
							ImGui::PopStyleColor();

							if (ImGui::BeginPopup("select_material_when")) {
								if (ImGui::Selectable("<wildcard>", rule.when[c_id].empty())) {
									rule.when[c_id] = "";
									rebuild_needed = true;
									unsaved_changes = true;
								}
								for (const auto& m : materials) {
									if (ImGui::Selectable(m.name.c_str(), rule.when[c_id] == m.name)) {
										rule.when[c_id] = m.name;
										rebuild_needed = true;
										unsaved_changes = true;
									}
								}
								ImGui::EndPopup();
							}
						}

						if (x < 4)
							ImGui::SameLine();
						ImGui::PopID();
					}
				}
				ImGui::EndGroup();

				ImGui::SameLine(0.0f, 15.0f);

				ImGui::BeginGroup();
				ImGui::Text("Then");
				for (int y = 0; y < NEIGHBOR_SIZE; ++y) {
					for (int x = 0; x < NEIGHBOR_SIZE; ++x) {
						int c_id = y * NEIGHBOR_SIZE + x;
						ImGui::PushID(c_id + 100);

						std::string label = "?";
						ImVec4 btn_col = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

						if (!rule.then[c_id].empty()) {
							label = rule.then[c_id].substr(0, 1);
							for (const auto& m : materials) {
								if (m.name == rule.then[c_id]) {
									btn_col = ImVec4(m.color[0] / 255.f, m.color[1] / 255.f, m.color[2] / 255.f, 1.0f);
									break;
								}
							}
						} else {
							if (!rule.when[c_id].empty()) {
								label = rule.when[c_id].substr(0, 1);
								for (const auto& m : materials) {
									if (m.name == rule.when[c_id]) {
										btn_col = ImVec4(m.color[0] / 255.f * 0.4f, m.color[1] / 255.f * 0.4f,
														 m.color[2] / 255.f * 0.4f, 1.0f);
										break;
									}
								}
							}
						}

						ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
						if (ImGui::Button(label.c_str(), ImVec2(25, 25))) {
							ImGui::OpenPopup("select_material_then");
						}
						ImGui::PopStyleColor();

						if (ImGui::BeginPopup("select_material_then")) {
							if (ImGui::Selectable("<keep>", rule.then[c_id].empty())) {
								rule.then[c_id] = "";
								rebuild_needed = true;
								unsaved_changes = true;
							}
							for (const auto& m : materials) {
								if (ImGui::Selectable(m.name.c_str(), rule.then[c_id] == m.name)) {
									rule.then[c_id] = m.name;
									rebuild_needed = true;
									unsaved_changes = true;
								}
							}
							ImGui::EndPopup();
						}

						if (x < 4)
							ImGui::SameLine();
						ImGui::PopID();
					}
				}
				ImGui::EndGroup();

				ImGui::EndChild();
				ImGui::PopID();
			}
			ImGui::EndChild();
		}

		if (rebuild_needed) {
			MaterialManager::rebuild_compiled_rules();
		}

		ImGui::EndTabItem();
	}
}

void UI::render_manage_sets() {
	const std::string& current_set = SetManager::get_current_set();
	SetMetadata meta = SetManager::get_current_metadata();

	if (ImGui::BeginTabItem("Sets")) {
		ImGui::Spacing();

		ImGui::Text("Available Sets:");
		ImGui::BeginChild("SetsListScroll", ImVec2(0, 150), true);
		for (const auto& s : SetManager::get_sets()) {
			SetMetadata s_meta = SetManager::load_set_metadata(s);
			if (ImGui::Selectable((s + "##selectable_set_" + s).c_str(), s == current_set)) {
				if (s != current_set) {
					if (unsaved_changes) {
						pending_set_switch = s;
						open_switch_popup = true;
					} else {
						SetManager::set_current_set(s);
						selected_id = -1;
					}
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Author: %s\nDescription: %s", s_meta.author.empty() ? "None" : s_meta.author.c_str(),
								  s_meta.description.empty() ? "No description" : s_meta.description.c_str());
			}
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (ImGui::Button("New", ImVec2(80, 25))) {
			open_create_set_popup = true;
			duplicate_set_checkbox = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create a new empty material set.");
		}

		ImGui::SameLine();
		if (ImGui::Button("Copy", ImVec2(80, 25))) {
			open_create_set_popup = true;
			duplicate_set_checkbox = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create a duplicate copy of the current set.");
		}

		ImGui::SameLine();
		if (SetManager::get_sets().size() > 1) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			if (ImGui::Button("Delete", ImVec2(80, 25))) {
				open_delete_set_popup = true;
			}
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete the current set.");
			}
		} else {
			ImGui::BeginDisabled();
			ImGui::Button("Delete", ImVec2(80, 25));
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("Cannot delete the only set.");
			}
			ImGui::EndDisabled();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "Set Settings:");

		char set_rename_buf[64];
		strncpy(set_rename_buf, current_set.c_str(), sizeof(set_rename_buf));
		set_rename_buf[sizeof(set_rename_buf) - 1] = '\0';
		if (ImGui::InputText("Name##set_rename_input", set_rename_buf, sizeof(set_rename_buf),
							 ImGuiInputTextFlags_EnterReturnsTrue)) {
			std::string new_name = set_rename_buf;
			new_name = sanitize_name(new_name);
			if (!new_name.empty() && new_name != current_set) {
				if (SetManager::rename_set(current_set, new_name)) {
					unsaved_changes = false;
				}
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Name of this set.");
		}

		char s_author[128];
		strncpy(s_author, meta.author.c_str(), sizeof(s_author));
		s_author[sizeof(s_author) - 1] = '\0';
		if (ImGui::InputText("Author", s_author, sizeof(s_author))) {
			meta.author = s_author;
			SetManager::update_current_metadata(meta);
			unsaved_changes = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Name of the author of this set.");
		}

		auto filterNewline = [](ImGuiInputTextCallbackData* data) -> int {
			if (data->EventChar == '\r' || data->EventChar == '\n') {
				return true;
			}
			return false;
		};

		char s_desc[256];
		strncpy(s_desc, meta.description.c_str(), sizeof(s_desc));
		s_desc[sizeof(s_desc) - 1] = '\0';
		ImGui::Text("Description");
		if (ImGui::InputTextMultiline("##set_desc", s_desc, sizeof(s_desc), ImVec2(-1.0f, 150.0f),
									  ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackCharFilter,
									  filterNewline)) {
			meta.description = s_desc;
			SetManager::update_current_metadata(meta);
			unsaved_changes = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Description of this set.");
		}

		ImGui::Spacing();
		if (ImGui::Button("Save Current Set", ImVec2(-1, 30))) {
			SetManager::update_current_metadata(meta);
			MaterialManager::save_all_materials(SETS_DIRECTORY + current_set);
			unsaved_changes = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Saves all materials and set configuration.");
		}

		ImGui::EndTabItem();
	}
}

void UI::render_save_load() {
	const std::string& current_set = SetManager::get_current_set();

	if (ImGui::BeginTabItem("Saves")) {
		ImGui::Spacing();

		ImGui::Text("Save Current Simulation:");
		ImGui::InputText("Save Name##save_name", save_file_name_buf, sizeof(save_file_name_buf));
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Input a name for the save file.");
		}

		if (ImGui::Button("Save Simulation", ImVec2(-1, 30))) {
			std::string s_name = save_file_name_buf;
			if (!s_name.empty()) {
				if (SaveManager::save_to_file(s_name, current_set)) {
					save_file_name_buf[0] = '\0';
				}
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Saves grid state to a .save file.");
		}

		ImGui::Separator();
		ImGui::Text("Available Saves in current set folder:");

		std::vector<std::string> save_files;
		std::string set_dir = SETS_DIRECTORY + current_set;
		if (fs::exists(set_dir) && fs::is_directory(set_dir)) {
			for (const auto& entry : fs::directory_iterator(set_dir)) {
				if (entry.is_regular_file() && entry.path().extension() == ".save") {
					save_files.push_back(entry.path().filename().string());
				}
			}
		}

		ImGui::BeginChild("SavesListScroll", ImVec2(0, 180), true);
		for (int i = 0; i < (int)save_files.size(); ++i) {
			bool is_selected = (selected_save_id == i);
			if (ImGui::Selectable(save_files[i].c_str(), selected_save_id == i)) {
				selected_save_id = i;
			}
			if (is_selected && ImGui::IsMouseDoubleClicked(0)) {
				if (unsaved_changes) {
					pending_save_load = save_files[i];
					open_switch_popup = true;
				} else {
					std::string loaded_set;
					if (SaveManager::load_from_file(save_files[i], current_set, loaded_set)) {
						selected_id = -1;
						unsaved_changes = false;
					}
				}
			}
		}
		ImGui::EndChild();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Double click to load. Switches current material set.");
		}

		ImGui::Spacing();
		if (selected_save_id >= 0 && selected_save_id < (int)save_files.size()) {
			if (ImGui::Button("Load", ImVec2(80, 25))) {
				if (unsaved_changes) {
					pending_save_load = save_files[selected_save_id];
					open_switch_popup = true;
				} else {
					std::string loaded_set;
					if (SaveManager::load_from_file(save_files[selected_save_id], current_set, loaded_set)) {
						selected_id = -1;
						unsaved_changes = false;
					}
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Load the selected save file.");
			}

			ImGui::SameLine();
			if (ImGui::Button("Copy", ImVec2(80, 25))) {
				std::string old_name = save_files[selected_save_id];
				std::string new_name = old_name;
				size_t dot = new_name.find_last_of('.');
				if (dot != std::string::npos) {
					new_name = new_name.substr(0, dot) + "_copy" + new_name.substr(dot);
				} else {
					new_name += "_copy";
				}
				std::string old_path = SETS_DIRECTORY + current_set + "/" + old_name;
				std::string new_path = SETS_DIRECTORY + current_set + "/" + new_name;
				try {
					fs::copy(old_path, new_path);
				} catch (...) {}
				selected_save_id = -1;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Duplicate the selected save file.");
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			if (ImGui::Button("Delete", ImVec2(80, 25))) {
				std::string filepath = SETS_DIRECTORY + current_set + "/" + save_files[selected_save_id];
				try {
					fs::remove(filepath);
				} catch (...) {}
				selected_save_id = -1;
			}
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete the selected save file.");
			}
		} else {
			ImGui::TextDisabled("Select a save file to load/copy/delete");
		}

		ImGui::EndTabItem();
	}
}

void UI::render_advanced_options() {
	if (ImGui::BeginTabItem("Advanced")) {
		ImGui::Spacing();
		ImGui::Text("Advanced Options");
		ImGui::Separator();
		ImGui::Spacing();

		int vsync_val = 0;
		SDL_GetRenderVSync(Window::get_renderer(), &vsync_val);
		bool vsync_enabled = (vsync_val != 0);
		if (ImGui::Checkbox("Enable VSync", &vsync_enabled)) {
			SDL_SetRenderVSync(Window::get_renderer(), vsync_enabled ? 1 : 0);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Synchronizes the frame rate with the monitor's refresh rate to prevent screen tearing.");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Simulation Quality Preset");
		QualityPreset q = Grid::get_quality_preset();

		if (ImGui::RadioButton("Slow (Accuracy)", q == QualityPreset::Slow)) {
			Grid::set_quality_preset(QualityPreset::Slow);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Total accuracy for precise cellular automata (e.g. Sierpinski triangles or other fractal patterns).");
		}

		if (ImGui::RadioButton("Fast (Multithreaded)", q == QualityPreset::Fast)) {
			Grid::set_quality_preset(QualityPreset::Fast);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Fast multithreaded simulation. Ideal for fast physics simulations (e.g. sand, fire, etc.)");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool mt_active = (q != QualityPreset::Slow);
		if (!mt_active) {
			ImGui::BeginDisabled();
		}
		int thread_count = static_cast<int>(Grid::get_thread_count());
		if (ImGui::SliderInt("Active Threads", &thread_count, 1, NUM_STRIPS_Y / 2)) {
			Grid::configure_threads(static_cast<unsigned int>(thread_count));
		}
		if (ImGui::IsItemHovered() && mt_active) {
			ImGui::SetTooltip("Sets the size of the worker thread pool. Automatically set to the maximum number of\n"
							  "parallelizable strips.");
		}
		if (!mt_active) {
			ImGui::EndDisabled();
		}

		ImGui::EndTabItem();
	}
}

void UI::render_shortcuts() {
	if (ImGui::BeginTabItem("Shortcuts")) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "Keyboard & Mouse Shortcuts");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BulletText("Space: Pause / Resume the simulation");
		ImGui::BulletText("F: Step simulation by one frame (when paused)");
		ImGui::BulletText("Q: Toggle compact floating UI");
		ImGui::BulletText("R: Clear grid (set all cells to Empty)");
		ImGui::Spacing();

		ImGui::BulletText("Scroll Up/Down: Adjust brush size");
		ImGui::BulletText("Ctrl + Scroll Up/Down: Adjust brush size faster");
		ImGui::Spacing();

		ImGui::BulletText("Left Mouse Button: Draw selected material");
		ImGui::BulletText("Right Mouse Button: Erase material (draw Empty)");
		ImGui::BulletText("Middle Mouse Button: Pick material");
		ImGui::BulletText("Shift + Middle Mouse Button: Pan");
		ImGui::BulletText("Shift + Scroll Up/Down / PageUp/PageDown / +/-: Zoom");
		ImGui::EndTabItem();
	}
}

void UI::render_modals() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	const std::string& current_set = SetManager::get_current_set();

	if (open_empty_rule_warning_popup) {
		ImGui::OpenPopup("Performance Warning##EmptyRule");
		open_empty_rule_warning_popup = false;
	}

	if (open_switch_popup) {
		ImGui::OpenPopup("Unsaved Changes Switch");
		open_switch_popup = false;
	}

	if (open_create_set_popup) {
		ImGui::OpenPopup("Create Set...");
		save_as_buf[0] = '\0';
		open_create_set_popup = false;
	}

	if (open_delete_set_popup) {
		ImGui::OpenPopup("Delete Set Confirmation");
		open_delete_set_popup = false;
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Performance Warning##EmptyRule", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.70f, 0.20f, 1.00f), "PERFORMANCE WARNING");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped(
			"Adding rules to the 'empty' material means every empty cell on the grid will be evaluated every frame.");
		ImGui::TextWrapped(
			"This can significantly lower simulation performance when large areas of the grid are empty.");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Add Rule Anyway", ImVec2(150, 30))) {
			auto& mats = MaterialManager::get_materials();
			for (auto& m : mats) {
				if (m.id == 0) {
					UserRule new_rule;
					new_rule.when.fill("");
					new_rule.when[12] = m.name;
					new_rule.then.fill("");
					new_rule.chance = 1;
					m.user_rules.push_back(new_rule);
					MaterialManager::rebuild_compiled_rules();
					unsaved_changes = true;
					break;
				}
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Create Set...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		if (duplicate_set_checkbox) {
			ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "DUPLICATE CURRENT SET");
		} else {
			ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "CREATE NEW EMPTY SET");
		}
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Enter name for the new set:");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##new_set_name_input", save_as_buf, sizeof(save_as_buf),
							 ImGuiInputTextFlags_EnterReturnsTrue)) {
			std::string name = save_as_buf;
			name = sanitize_name(name);
			if (!name.empty()) {
				if (duplicate_set_checkbox) {
					SetManager::copy_set(current_set, name);
				} else {
					SetManager::create_new_empty_set(name);
				}
				unsaved_changes = false;
				ImGui::CloseCurrentPopup();
			}
		}

		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Confirm", ImVec2(120, 30))) {
			std::string name = save_as_buf;
			if (!name.empty()) {
				if (duplicate_set_checkbox) {
					SetManager::copy_set(current_set, name);
				} else {
					SetManager::create_new_empty_set(name);
				}
				unsaved_changes = false;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Delete Set Confirmation", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.00f), "DELETE MATERIAL SET");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped("Are you sure you want to delete the set '%s'?", current_set.c_str());
		ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.00f), "This will delete all its materials, rules, and saves.");
		ImGui::Text("This action cannot be undone.");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
		if (ImGui::Button("Delete Permanently", ImVec2(180, 30))) {
			SetManager::delete_set(current_set);
			unsaved_changes = false;
			selected_id = -1;
			ImGui::PopStyleColor(3);
			ImGui::CloseCurrentPopup();
		} else {
			ImGui::PopStyleColor(3);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (show_exit_popup) {
		if (unsaved_changes) {
			ImGui::OpenPopup("Exit Confirmation");
			exit_save_as_new_set = false;
			new_set_name_buf[0] = '\0';
		} else {
			UI::shutdown();
			Grid::shutdown();
			Window::shutdown();
			exit(0);
		}
		show_exit_popup = false;
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Exit Confirmation", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.00f), "WARNING: UNSAVED CHANGES");
		ImGui::Separator();
		ImGui::Spacing();

		if (!exit_save_as_new_set) {
			ImGui::TextWrapped("You have unsaved changes in %s. What would you like to do?", current_set.c_str());
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			std::string save_btn_lbl = "Save & Exit (" + current_set + ")";
			if (ImGui::Button(save_btn_lbl.c_str(), ImVec2(190, 30))) {
				MaterialManager::save_all_materials(SETS_DIRECTORY + current_set);
				UI::shutdown();
				Grid::shutdown();
				Window::shutdown();
				exit(0);
			}
			ImGui::SameLine();
			if (ImGui::Button("Save as New Set...", ImVec2(190, 30))) {
				exit_save_as_new_set = true;
			}

			if (ImGui::Button("Exit Without Saving", ImVec2(190, 30))) {
				UI::shutdown();
				Grid::shutdown();
				Window::shutdown();
				exit(0);
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(190, 30))) {
				ImGui::CloseCurrentPopup();
			}
		} else {
			ImGui::Text("Enter name for the new set:");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##exit_new_set_name", new_set_name_buf, sizeof(new_set_name_buf),
								 ImGuiInputTextFlags_EnterReturnsTrue)) {
				std::string new_set_name = new_set_name_buf;
				new_set_name = sanitize_name(new_set_name);
				if (!new_set_name.empty()) {
					SetManager::copy_set(current_set, new_set_name);
					UI::shutdown();
					Grid::shutdown();
					Window::shutdown();
					exit(0);
				}
			}

			if (ImGui::IsWindowAppearing()) {
				ImGui::SetKeyboardFocusHere();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Save & Exit", ImVec2(120, 30))) {
				std::string new_set_name = new_set_name_buf;
				new_set_name = sanitize_name(new_set_name);
				if (!new_set_name.empty()) {
					SetManager::copy_set(current_set, new_set_name);
					UI::shutdown();
					Grid::shutdown();
					Window::shutdown();
					exit(0);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Back", ImVec2(120, 30))) {
				exit_save_as_new_set = false;
			}
		}

		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	bool should_proceed_switch = false;
	if (ImGui::BeginPopupModal("Unsaved Changes Switch", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.70f, 0.20f, 1.00f), "WARNING: UNSAVED CHANGES");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped("You have unsaved changes in the current set. Do you "
						   "want to save them before switching?");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Save & Switch", ImVec2(120, 30))) {
			MaterialManager::save_all_materials(SETS_DIRECTORY + current_set);
			should_proceed_switch = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard & Switch", ImVec2(120, 30))) {
			should_proceed_switch = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			pending_set_switch = "";
			pending_save_load = "";
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (should_proceed_switch) {
		if (!pending_set_switch.empty()) {
			SetManager::set_current_set(pending_set_switch);
			selected_id = -1;
			unsaved_changes = false;
			pending_set_switch = "";
		} else if (!pending_save_load.empty()) {
			std::string loaded_set;
			if (SaveManager::load_from_file(pending_save_load, current_set, loaded_set)) {
				selected_id = -1;
				unsaved_changes = false;
			}
			pending_save_load = "";
		}
	}
}