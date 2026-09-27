#include "workshop_item_editor.hpp"

#include <imgui.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "config_manager.hpp"
#include "const.hpp"
#include "grid.hpp"
#include "material_manager.hpp"
#include "set_manager.hpp"
#include "toast_manager.hpp"
#include "ui.hpp"
#include "zip_util.hpp"

namespace {
	// Edit modal state
	bool s_show_edit_modal = false;
	WorkshopItemClient s_edit_item;
	char s_edit_title[128] = "";
	char s_edit_desc[1024] = "";
	std::string s_edit_status = "";
	bool s_edit_saving = false;
	std::function<void()> s_edit_on_success = nullptr;

	// Update modal state
	bool s_show_update_modal = false;
	WorkshopItemClient s_update_item;
	char s_update_title[128] = "";
	char s_update_desc[1024] = "";
	int s_update_version = 2;
	char s_update_changelog[512] = "";
	bool s_update_update_thumb = true;
	int s_update_set_idx = 0;
	std::vector<std::string> s_update_available_sets;
	int s_update_save_mode = 0;	 // 0 = canvas, 1 = local file
	int s_update_save_idx = 0;
	std::vector<std::string> s_update_available_saves;
	int s_update_stamp_idx = 0;
	std::vector<std::string> s_update_available_stamps;
	std::string s_update_status = "";
	bool s_update_saving = false;
	std::function<void()> s_update_on_success = nullptr;

	// Delete modal state
	bool s_show_delete_modal = false;
	WorkshopItemClient s_delete_item;
	std::string s_delete_status = "";
	bool s_delete_saving = false;
	std::function<void()> s_delete_on_success = nullptr;

	void refresh_update_sources(const std::string& item_type, const std::string& item_title) {
		if (item_type == "set") {
			s_update_available_sets.clear();
			if (std::filesystem::exists(SETS_DIRECTORY)) {
				for (const auto& entry : std::filesystem::directory_iterator(SETS_DIRECTORY)) {
					if (entry.is_directory()) {
						s_update_available_sets.push_back(entry.path().filename().string());
					}
				}
			}
			s_update_set_idx = 0;
			for (size_t i = 0; i < s_update_available_sets.size(); ++i) {
				if (s_update_available_sets[i] == item_title) {
					s_update_set_idx = static_cast<int>(i);
					break;
				}
				if (s_update_available_sets[i] == SetManager::get_current_set()) {
					s_update_set_idx = static_cast<int>(i);
				}
			}
		} else if (item_type == "save") {
			s_update_available_saves.clear();
			std::string saves_dir = std::string(SETS_DIRECTORY) + SetManager::get_current_set() + "/saves/";
			if (std::filesystem::exists(saves_dir)) {
				for (const auto& entry : std::filesystem::directory_iterator(saves_dir)) {
					if (entry.is_regular_file() && entry.path().extension() == ".save") {
						s_update_available_saves.push_back(entry.path().filename().string());
					}
				}
			}
			s_update_save_idx = 0;
		} else if (item_type == "stamp") {
			s_update_available_stamps.clear();
			std::string stamps_dir = std::string(SETS_DIRECTORY) + SetManager::get_current_set() + "/stamps/";
			if (std::filesystem::exists(stamps_dir)) {
				for (const auto& entry : std::filesystem::directory_iterator(stamps_dir)) {
					if (entry.is_regular_file() && entry.path().extension() == ".stamp") {
						s_update_available_stamps.push_back(entry.path().filename().string());
					}
				}
			}
			s_update_stamp_idx = 0;
		}
	}
}  // namespace

void WorkshopItemEditor::open_edit_modal(const WorkshopItemClient& item, std::function<void()> on_success) {
	s_edit_item = item;
	std::snprintf(s_edit_title, sizeof(s_edit_title), "%s", item.title.c_str());
	std::snprintf(s_edit_desc, sizeof(s_edit_desc), "%s", item.description.c_str());
	s_edit_status = "";
	s_edit_saving = false;
	s_edit_on_success = on_success;
	s_show_edit_modal = true;
}

void WorkshopItemEditor::open_update_modal(const WorkshopItemClient& item, std::function<void()> on_success) {
	s_update_item = item;
	std::snprintf(s_update_title, sizeof(s_update_title), "%s", item.title.c_str());
	std::snprintf(s_update_desc, sizeof(s_update_desc), "%s", item.description.c_str());
	s_update_version = item.version + 1;
	s_update_changelog[0] = '\0';
	s_update_update_thumb = true;
	s_update_status = "";
	s_update_saving = false;
	s_update_on_success = on_success;

	refresh_update_sources(item.type, item.title);
	s_show_update_modal = true;
}

void WorkshopItemEditor::open_delete_modal(const WorkshopItemClient& item, std::function<void()> on_success) {
	s_delete_item = item;
	s_delete_status = "";
	s_delete_saving = false;
	s_delete_on_success = on_success;
	s_show_delete_modal = true;
}

void WorkshopItemEditor::render() {
	render_edit_modal();
	render_update_modal();
	render_delete_modal();
}

void WorkshopItemEditor::render_edit_modal() {
	if (!s_show_edit_modal)
		return;

	ImGui::OpenPopup("Edit Item Details##Modal");
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(460, 310), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Edit Item Details##Modal", &s_show_edit_modal,
							   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.0f, 1.0f), "Edit Item: %s", s_edit_item.title.c_str());
		ImGui::TextDisabled("Update online title and description.");
		ImGui::Spacing();

		ImGui::Text("Title:");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##edit_title", s_edit_title, sizeof(s_edit_title));

		ImGui::Spacing();
		ImGui::Text("Description:");
		ImGui::InputTextMultiline("##edit_desc", s_edit_desc, sizeof(s_edit_desc), ImVec2(-1, 90));

		if (!s_edit_status.empty()) {
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "%s", s_edit_status.c_str());
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool can_save = (std::strlen(s_edit_title) > 0) && !s_edit_saving;
		if (!can_save) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(s_edit_saving ? "Saving..." : "Save Changes", ImVec2(130, 26))) {
			s_edit_saving = true;
			s_edit_status = "";
			std::string item_id = s_edit_item.id;
			std::string title_str = s_edit_title;
			std::string desc_str = s_edit_desc;
			std::string meta_str = s_edit_item.meta.dump();

			WorkshopClient::update_item_metadata(item_id, title_str, desc_str, meta_str,
												 [title_str](bool success, const std::string& err) {
													 s_edit_saving = false;
													 if (success) {
														 ToastManager::success("Updated '" + title_str + "' details!");
														 s_show_edit_modal = false;
														 if (s_edit_on_success) {
															 s_edit_on_success();
														 }
													 } else {
														 s_edit_status = "Failed: " + err;
													 }
												 });
		}
		if (!can_save) {
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80, 26))) {
			s_show_edit_modal = false;
		}

		ImGui::EndPopup();
	}
}

void WorkshopItemEditor::render_update_modal() {
	if (!s_show_update_modal)
		return;

	ImGui::OpenPopup("Publish New Version##Modal");
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(530, 520), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Publish New Version##Modal", &s_show_update_modal,
							   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.5f, 1.0f), "Publish New Version: %s", s_update_item.title.c_str());
		ImGui::TextDisabled("Upload an updated payload and bump version with changelog notes.");
		ImGui::Spacing();

		// Version selector
		ImGui::Text("New Version:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90);
		if (ImGui::InputInt("##update_ver", &s_update_version)) {
			if (s_update_version <= s_update_item.version) {
				s_update_version = s_update_item.version + 1;
			}
		}
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(Current: v%d)", s_update_item.version);

		ImGui::Spacing();
		ImGui::Text("Changelog / Release Notes:");
		ImGui::InputTextMultiline("##update_changelog", s_update_changelog, sizeof(s_update_changelog), ImVec2(-1, 55));

		ImGui::Spacing();
		ImGui::Text("Title:");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##update_title", s_update_title, sizeof(s_update_title));

		ImGui::Spacing();
		ImGui::Text("Description:");
		ImGui::InputTextMultiline("##update_desc", s_update_desc, sizeof(s_update_desc), ImVec2(-1, 55));

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Source Payload:");

		// Item type specific source selectors
		if (s_update_item.type == "set") {
			if (s_update_available_sets.empty()) {
				ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "No local sets found.");
			} else {
				std::string preview =
					(s_update_set_idx >= 0 && s_update_set_idx < static_cast<int>(s_update_available_sets.size()))
						? s_update_available_sets[s_update_set_idx]
						: "Select a set";
				ImGui::SetNextItemWidth(-1);
				if (ImGui::BeginCombo("##update_set_combo", preview.c_str())) {
					for (size_t i = 0; i < s_update_available_sets.size(); ++i) {
						bool is_sel = (static_cast<int>(i) == s_update_set_idx);
						if (ImGui::Selectable(s_update_available_sets[i].c_str(), is_sel)) {
							s_update_set_idx = static_cast<int>(i);
						}
						if (is_sel) {
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
			ImGui::Checkbox("Regenerate palette swatch thumbnail", &s_update_update_thumb);
		} else if (s_update_item.type == "save") {
			ImGui::RadioButton("Active Sandbox Canvas", &s_update_save_mode, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Choose from Local Saves", &s_update_save_mode, 1);

			if (s_update_save_mode == 1) {
				if (s_update_available_saves.empty()) {
					ImGui::TextDisabled("No saves found in current set.");
				} else {
					std::string preview = (s_update_save_idx >= 0 &&
										   s_update_save_idx < static_cast<int>(s_update_available_saves.size()))
											  ? s_update_available_saves[s_update_save_idx]
											  : "Select save";
					ImGui::SetNextItemWidth(-1);
					if (ImGui::BeginCombo("##update_save_combo", preview.c_str())) {
						for (size_t i = 0; i < s_update_available_saves.size(); ++i) {
							bool is_sel = (static_cast<int>(i) == s_update_save_idx);
							if (ImGui::Selectable(s_update_available_saves[i].c_str(), is_sel)) {
								s_update_save_idx = static_cast<int>(i);
							}
						}
						ImGui::EndCombo();
					}
				}
			}
			ImGui::Checkbox("Regenerate thumbnail image", &s_update_update_thumb);
		} else if (s_update_item.type == "stamp") {
			if (s_update_available_stamps.empty()) {
				ImGui::TextDisabled("No stamps found in current set.");
			} else {
				std::string preview =
					(s_update_stamp_idx >= 0 && s_update_stamp_idx < static_cast<int>(s_update_available_stamps.size()))
						? s_update_available_stamps[s_update_stamp_idx]
						: "Select stamp";
				ImGui::SetNextItemWidth(-1);
				if (ImGui::BeginCombo("##update_stamp_combo", preview.c_str())) {
					for (size_t i = 0; i < s_update_available_stamps.size(); ++i) {
						bool is_sel = (static_cast<int>(i) == s_update_stamp_idx);
						if (ImGui::Selectable(s_update_available_stamps[i].c_str(), is_sel)) {
							s_update_stamp_idx = static_cast<int>(i);
						}
					}
					ImGui::EndCombo();
				}
			}
			ImGui::Checkbox("Regenerate thumbnail image", &s_update_update_thumb);
		} else if (s_update_item.type == "theme") {
			ImGui::TextDisabled("Will export current active theme configuration.");
			ImGui::Checkbox("Regenerate theme preview thumbnail", &s_update_update_thumb);
		}

		if (!s_update_status.empty()) {
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "%s", s_update_status.c_str());
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool can_update =
			(std::strlen(s_update_title) > 0) && (s_update_version > s_update_item.version) && !s_update_saving;
		if (s_update_item.type == "set" && s_update_available_sets.empty()) {
			can_update = false;
		}
		if (s_update_item.type == "save" && s_update_save_mode == 1 && s_update_available_saves.empty()) {
			can_update = false;
		}
		if (s_update_item.type == "stamp" && s_update_available_stamps.empty()) {
			can_update = false;
		}

		char btn_lbl[64];
		std::snprintf(btn_lbl, sizeof(btn_lbl), "Publish Version v%d", s_update_version);
		if (!can_update) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(s_update_saving ? "Uploading..." : btn_lbl, ImVec2(180, 26))) {
			s_update_saving = true;
			s_update_status = "";

			std::string item_id = s_update_item.id;
			std::string item_type = s_update_item.type;
			std::string title_str = s_update_title;
			std::string desc_str = s_update_desc;
			int target_ver = s_update_version;
			std::string changelog_str = s_update_changelog;
			nlohmann::json meta = s_update_item.meta;

			std::vector<uint8_t> payload_bytes;
			std::string file_ext = "";
			std::string set_hash = "";
			std::string thumb_b64 = "";

			if (item_type == "set") {
				if (s_update_set_idx >= 0 && s_update_set_idx < static_cast<int>(s_update_available_sets.size())) {
					std::string chosen_set = s_update_available_sets[s_update_set_idx];
					if (chosen_set == SetManager::get_current_set()) {
						MaterialManager::save_all_materials(SETS_DIRECTORY + chosen_set);
					}

					std::string set_dir = std::string(SETS_DIRECTORY) + chosen_set;
					std::vector<std::pair<std::string, std::vector<uint8_t>>> set_files;
					for (const auto& entry : std::filesystem::directory_iterator(set_dir)) {
						if (entry.is_regular_file()) {
							std::string fname = entry.path().filename().string();
							std::string ext = entry.path().extension().string();
							if (ext == ".mat" || fname == "set_config.ini") {
								std::ifstream in(entry.path().string(), std::ios::binary);
								if (in.is_open()) {
									std::vector<uint8_t> fbytes((std::istreambuf_iterator<char>(in)),
																std::istreambuf_iterator<char>());
									set_files.emplace_back(fname, fbytes);
								}
							}
						}
					}
					payload_bytes = ZipUtil::create_zip(set_files);
					file_ext = ".zip";
					set_hash = SetManager::compute_set_hash(chosen_set);

					if (s_update_update_thumb) {
						thumb_b64 = UI::generate_thumbnail_base64(1, chosen_set);
					}
					meta["set_name"] = chosen_set;
				}
			} else if (item_type == "save") {
				file_ext = ".save";
				if (s_update_save_mode == 0) {
					// From Canvas
					std::string cur_s = SetManager::get_current_set();
					SaveManager::save_to_file("__ws_temp_update", cur_s);
					std::string fpath = SaveManager::get_saves_directory(cur_s) + "__ws_temp_update.save";
					std::ifstream fin(fpath, std::ios::binary);
					if (fin.is_open()) {
						payload_bytes.assign((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
						fin.close();
					}
					std::filesystem::remove(fpath);
					if (s_update_update_thumb) {
						thumb_b64 = UI::generate_thumbnail_base64(0, cur_s, "");
					}
					meta["width"] = Grid::get_width();
					meta["height"] = Grid::get_height();
				} else {
					// From File
					if (s_update_save_idx >= 0 &&
						s_update_save_idx < static_cast<int>(s_update_available_saves.size())) {
						std::string fname = s_update_available_saves[s_update_save_idx];
						std::string fpath =
							std::string(SETS_DIRECTORY) + SetManager::get_current_set() + "/saves/" + fname;
						std::ifstream in(fpath, std::ios::binary);
						if (in.is_open()) {
							payload_bytes.assign((std::istreambuf_iterator<char>(in)),
												 std::istreambuf_iterator<char>());
						}
						if (s_update_update_thumb) {
							thumb_b64 = UI::generate_thumbnail_base64(0, SetManager::get_current_set(), fname);
						}
					}
				}
			} else if (item_type == "stamp") {
				file_ext = ".stamp";
				if (s_update_stamp_idx >= 0 &&
					s_update_stamp_idx < static_cast<int>(s_update_available_stamps.size())) {
					std::string fname = s_update_available_stamps[s_update_stamp_idx];
					std::string fpath =
						std::string(SETS_DIRECTORY) + SetManager::get_current_set() + "/stamps/" + fname;
					std::ifstream in(fpath, std::ios::binary);
					if (in.is_open()) {
						payload_bytes.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
					}
					if (s_update_update_thumb) {
						thumb_b64 = UI::generate_thumbnail_base64(2, SetManager::get_current_set(), fname);
					}
				}
			} else if (item_type == "theme") {
				file_ext = ".theme";
				const auto& cfg = ConfigManager::get_config();
				nlohmann::json tj;
				tj["name"] = title_str;
				tj["window_rounding"] = cfg.ui.window_rounding;
				tj["frame_rounding"] = cfg.ui.frame_rounding;
				tj["button_size"] = cfg.ui.button_size;
				tj["icon_size"] = cfg.ui.icon_size;
				tj["sidebar_width"] = cfg.ui.sidebar_width;
				tj["material_list_height"] = cfg.ui.material_list_height;
				tj["background_color"] = {cfg.ui.background_color.x, cfg.ui.background_color.y,
										  cfg.ui.background_color.z, cfg.ui.background_color.w};
				tj["selection_box_color"] = {cfg.ui.selection_box_color.x, cfg.ui.selection_box_color.y,
											 cfg.ui.selection_box_color.z, cfg.ui.selection_box_color.w};
				tj["selection_box_fill"] = {cfg.ui.selection_box_fill.x, cfg.ui.selection_box_fill.y,
											cfg.ui.selection_box_fill.z, cfg.ui.selection_box_fill.w};
				tj["colors"] = ConfigManager::get_color_overrides();
				std::string t_str = tj.dump(2);
				payload_bytes.assign(t_str.begin(), t_str.end());

				if (s_update_update_thumb) {
					thumb_b64 = UI::generate_thumbnail_base64(3, "");
				}
			}

			if (payload_bytes.empty()) {
				s_update_saving = false;
				s_update_status = "Failed: Payload data could not be read.";
			} else {
				WorkshopClient::update_item(
					item_id, title_str, desc_str, target_ver, changelog_str, payload_bytes, file_ext, meta.dump(),
					set_hash, thumb_b64,
					[item_id, item_type, title_str, target_ver, set_hash](bool success, const std::string& err) {
						s_update_saving = false;
						if (success) {
							ToastManager::success("Published '" + title_str + "' v" + std::to_string(target_ver) + "!");
							if (item_type == "set" && s_update_set_idx >= 0 &&
								s_update_set_idx < static_cast<int>(s_update_available_sets.size())) {
								std::string chosen_set = s_update_available_sets[s_update_set_idx];
								SetManager::set_workshop_info(chosen_set, item_id, set_hash, target_ver);
							}
							s_show_update_modal = false;
							if (s_update_on_success) {
								s_update_on_success();
							}
						} else {
							s_update_status = "Failed: " + err;
						}
					});
			}
		}
		if (!can_update) {
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80, 26))) {
			s_show_update_modal = false;
		}

		ImGui::EndPopup();
	}
}

void WorkshopItemEditor::render_delete_modal() {
	if (!s_show_delete_modal)
		return;

	ImGui::OpenPopup("Delete Item##Modal");
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(440, 250), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Delete Item##Modal", &s_show_delete_modal,
							   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "WARNING: Permanent Deletion");
		ImGui::Spacing();
		ImGui::TextWrapped("Are you sure you want to permanently delete '%s' (v%d)?", s_delete_item.title.c_str(),
						   s_delete_item.version);
		ImGui::Spacing();
		ImGui::TextDisabled("This will remove the item, its files, and ratings from the community Workshop. This "
							"cannot be undone.");

		if (!s_delete_status.empty()) {
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "%s", s_delete_status.c_str());
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));

		if (s_delete_saving) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Button(s_delete_saving ? "Deleting..." : "Delete Permanently", ImVec2(150, 26))) {
			s_delete_saving = true;
			s_delete_status = "";
			std::string item_id = s_delete_item.id;
			std::string item_title = s_delete_item.title;

			WorkshopClient::delete_item(item_id, [item_title](bool success, const std::string& err) {
				s_delete_saving = false;
				if (success) {
					ToastManager::success("Deleted '" + item_title + "' from Workshop.");
					s_show_delete_modal = false;
					if (s_delete_on_success) {
						s_delete_on_success();
					}
				} else {
					s_delete_status = "Failed: " + err;
				}
			});
		}
		if (s_delete_saving) {
			ImGui::EndDisabled();
		}

		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80, 26))) {
			s_show_delete_modal = false;
		}

		ImGui::EndPopup();
	}
}
