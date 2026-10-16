#include "ui_modals.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>
#include <imgui.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <utility>
#include <vector>

#include "config_manager.hpp"
#include "const.hpp"
#include "grid.hpp"
#include "icon_manager.hpp"
#include "material_manager.hpp"
#include "sanitize.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "toast_manager.hpp"
#include "ui.hpp"
#include "ui_theme.hpp"
#include "ui_thumbnails.hpp"
#include "ui_widgets.hpp"
#include "ui_workshop.hpp"
#include "undo_manager.hpp"
#include "window.hpp"
#include "workshop_cache.hpp"
#include "workshop_client.hpp"
#include "workshop_item_editor.hpp"
#include "zip_util.hpp"

ModalType s_active_modal = ModalType::None;

bool s_exit_save_as_new_set = false;
char s_exit_new_set_name_buf[64] = "";

std::string s_pending_set_switch;
std::string s_pending_save_load;

bool s_create_set_duplicate = false;
char s_create_set_name_buf[64] = "";

int s_resize_target_w = 0;
int s_resize_target_h = 0;
bool s_resize_preserve_content = true;

SaveFileInfo s_pending_diff_save{};

uint8_t s_pending_empty_inherit_id = 255;
int s_pending_expand_rule_idx = -1;
uint8_t s_pending_expand_mat_id = 0;
int s_pending_expand_side = 0;
WorkshopItemClient s_pending_override_item;
uint32_t s_pending_local_version = 1;
uint32_t s_pending_target_version = 1;
bool s_pending_is_update = false;
bool s_pending_is_version_load = false;
bool s_pending_is_switch = false;
std::function<void()> s_pending_override_confirm = nullptr;
std::function<void()> s_pending_override_keep_local = nullptr;

bool s_auth_is_register = false;
char s_auth_username[64] = "";
char s_auth_password[64] = "";
std::string s_auth_status;

std::string s_report_item_id;
std::string s_report_item_title;
int s_report_reason_idx = 0;
char s_report_details[256] = "";
std::string s_report_status;

int s_publish_type_idx = 0;
char s_publish_title[128] = "";
char s_publish_author[64] = "";
char s_publish_desc[512] = "";
bool s_publishing = false;
std::string s_publish_status;
std::string s_publish_target_set;
std::string s_publish_target_save;
std::string s_publish_target_stamp;
int s_publish_target_w = 0;
int s_publish_target_h = 0;
std::string s_publish_forked_from_id;
int s_publish_forked_from_version = 0;
bool s_publish_is_private = false;

WorkshopItemClient s_versions_item;
std::vector<WorkshopItemVersion> s_current_item_versions;
bool s_loading_versions = false;
std::string s_versions_error;

bool s_migrate_is_save = true;
std::string s_migrate_filename;
std::string s_migrate_item_name;
std::string s_migrate_source_set;
std::vector<std::string> s_migrate_forked_sets;
int s_selected_migrate_set_idx = 0;

WorkshopItemClient s_fork_item;
int s_fork_version = 1;
std::vector<std::string> s_fork_available_sets;
int s_selected_fork_set_idx = 0;

void UIModals::open_exit() {
	if (!UI::has_unsaved_changes()) {
		ConfigManager::save();
		UI::shutdown();
		Grid::shutdown();
		Window::shutdown();
		exit(0);
	}
	s_active_modal = ModalType::ExitConfirmation;
	s_exit_save_as_new_set = false;
	s_exit_new_set_name_buf[0] = '\0';
}

void UIModals::open_unsaved_switch(const std::string& target_set, const std::string& target_save) {
	s_active_modal = ModalType::UnsavedChangesSwitch;
	s_pending_set_switch = target_set;
	s_pending_save_load = target_save;
}

void UIModals::open_create_set(bool duplicate) {
	s_active_modal = ModalType::CreateSet;
	s_create_set_duplicate = duplicate;
	s_create_set_name_buf[0] = '\0';
}

void UIModals::open_delete_set() { s_active_modal = ModalType::DeleteSetConfirmation; }

void UIModals::open_fork_set() {
	s_active_modal = ModalType::ForkSet;
	s_create_set_name_buf[0] = '\0';
}

void UIModals::open_canvas_resize(int target_w, int target_h, bool preserve_content) {
	s_active_modal = ModalType::CanvasResizeConfirmation;
	s_resize_target_w = target_w;
	s_resize_target_h = target_h;
	s_resize_preserve_content = preserve_content;
}

void UIModals::open_diff_size_save(const SaveFileInfo& save_info) {
	s_active_modal = ModalType::DiffSizeSave;
	s_pending_diff_save = save_info;
}

void UIModals::open_empty_rule_warning(uint8_t inherit_id) {
	s_active_modal = ModalType::EmptyRulePerformanceWarning;
	s_pending_empty_inherit_id = inherit_id;
}

void UIModals::open_large_neighborhood_warning(int rule_idx, uint8_t mat_id, int side) {
	s_active_modal = ModalType::LargeNeighborhoodWarning;
	s_pending_expand_rule_idx = rule_idx;
	s_pending_expand_mat_id = mat_id;
	s_pending_expand_side = side;
}

void UIModals::open_set_override_warning(const WorkshopItemClient& item, uint32_t local_version,
										 uint32_t target_version, bool is_update, bool is_version_load, bool is_switch,
										 std::function<void()> on_confirm, std::function<void()> on_keep_local) {
	s_active_modal = ModalType::SetOverrideWarning;
	s_pending_override_item = item;
	s_pending_local_version = local_version;
	s_pending_target_version = target_version;
	s_pending_is_update = is_update;
	s_pending_is_version_load = is_version_load;
	s_pending_is_switch = is_switch;
	s_pending_override_confirm = std::move(on_confirm);
	s_pending_override_keep_local = std::move(on_keep_local);
}

void UIModals::open_workshop_auth(bool is_register) {
	s_active_modal = ModalType::WorkshopAuth;
	s_auth_is_register = is_register;
	s_auth_status = "";
}

void UIModals::open_workshop_report(const std::string& item_id, const std::string& item_title) {
	s_active_modal = ModalType::WorkshopReport;
	s_report_item_id = item_id;
	s_report_item_title = item_title;
	s_report_reason_idx = 0;
	s_report_details[0] = '\0';
	s_report_status = "";
}

void UIModals::open_workshop_share_set(const std::string& set_name) {
	s_active_modal = ModalType::WorkshopShare;
	s_publish_target_set = set_name;
	s_publish_target_save = "";
	s_publish_target_stamp = "";
	s_publish_type_idx = 1;
	s_publish_status = "";
	s_publishing = false;
	s_publish_is_private = false;
	SetMetadata sm = SetManager::load_set_metadata(set_name);
	s_publish_forked_from_id = sm.forked_from_id;
	s_publish_forked_from_version = sm.forked_from_version;
	std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", set_name.c_str());
	std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username()
														   : (sm.author.empty() ? "Player" : sm.author);
	std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
	std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", sm.description.c_str());
}

void UIModals::open_workshop_share_save(const std::string& filename, const std::string& name,
										const std::string& parent_set, int w, int h) {
	s_active_modal = ModalType::WorkshopShare;
	s_publish_target_set = parent_set;
	s_publish_target_save = filename;
	s_publish_target_stamp = "";
	s_publish_target_w = w;
	s_publish_target_h = h;
	s_publish_type_idx = 0;
	s_publish_status = "";
	s_publishing = false;
	s_publish_is_private = false;

	SaveFileInfo info;
	if (SaveManager::inspect_save_file(SaveManager::get_saves_directory(parent_set) + filename, parent_set, info)) {
		s_publish_forked_from_id = info.forked_from_id;
		s_publish_forked_from_version = info.forked_from_version;
	} else {
		s_publish_forked_from_id = "";
		s_publish_forked_from_version = 0;
	}
	std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", name.c_str());
	SetMetadata sm = SetManager::load_set_metadata(parent_set);
	std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username()
														   : (sm.author.empty() ? "Player" : sm.author);
	std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
	std::string desc = "Save from set '" + parent_set + "'";
	std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", desc.c_str());
}

void UIModals::open_workshop_share_stamp(const std::string& filename, const std::string& name,
										 const std::string& parent_set, int w, int h) {
	s_active_modal = ModalType::WorkshopShare;
	s_publish_target_set = parent_set;
	s_publish_target_save = "";
	s_publish_target_stamp = filename;
	s_publish_target_w = w;
	s_publish_target_h = h;
	s_publish_type_idx = 2;
	s_publish_status = "";
	s_publishing = false;
	s_publish_is_private = false;

	StampFileInfo info;
	if (SaveManager::inspect_stamp_file(SaveManager::get_saves_directory(parent_set) + filename, parent_set, info)) {
		s_publish_forked_from_id = info.forked_from_id;
		s_publish_forked_from_version = info.forked_from_version;
	} else {
		s_publish_forked_from_id = "";
		s_publish_forked_from_version = 0;
	}
	std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", name.c_str());
	SetMetadata sm = SetManager::load_set_metadata(parent_set);
	std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username()
														   : (sm.author.empty() ? "Player" : sm.author);
	std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
	std::string desc = "Stamp from set '" + parent_set + "'";
	std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", desc.c_str());
}

void UIModals::open_workshop_share_theme() {
	s_active_modal = ModalType::WorkshopShare;
	s_publish_target_set = "";
	s_publish_target_save = "";
	s_publish_target_stamp = "";
	s_publish_forked_from_id = "";
	s_publish_forked_from_version = 0;
	s_publish_type_idx = 3;
	s_publish_status = "";
	s_publishing = false;
	s_publish_is_private = false;
	std::snprintf(s_publish_title, sizeof(s_publish_title), "My Custom Theme");
	std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username() : "Player";
	std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
	std::snprintf(s_publish_desc, sizeof(s_publish_desc), "Custom UI theme for Sand3");
}

void UIModals::open_workshop_versions(const WorkshopItemClient& item) {
	s_active_modal = ModalType::WorkshopVersionHistory;
	s_versions_item = item;
	s_current_item_versions.clear();
	s_loading_versions = true;
	s_versions_error = "";
	WorkshopClient::fetch_item_versions(item.id, [](bool success, const std::vector<WorkshopItemVersion>& versions) {
		s_loading_versions = false;
		if (success) {
			s_current_item_versions = versions;
		} else {
			s_versions_error = "Failed to load version history from server.";
		}
	});
}

void UIModals::open_migrate(bool is_save, const std::string& filename, const std::string& item_name,
							const std::string& source_set, const std::vector<std::string>& forked_sets) {
	s_active_modal = ModalType::MigrateForkedSet;
	s_migrate_is_save = is_save;
	s_migrate_filename = filename;
	s_migrate_item_name = item_name;
	s_migrate_source_set = source_set;
	s_migrate_forked_sets = forked_sets;
	s_selected_migrate_set_idx = 0;
}

void UIModals::open_fork_item(const WorkshopItemClient& item, int version) {
	s_fork_item = item;
	s_fork_version = (version > 0) ? version : item.version;

	if (item.type == "set") {
		auto do_fork_set = [item, ver = s_fork_version](const std::string& zip_file) {
			std::string base_fork =
				item.title + (ver > 0 && ver != item.version ? ("_v" + std::to_string(ver)) : "") + "_fork";
			std::string fork_name = base_fork;
			int fidx = 2;
			while (std::filesystem::exists(std::string(SETS_DIRECTORY) + fork_name)) {
				fork_name = base_fork + "_" + std::to_string(fidx++);
			}
			std::string target_dir = std::string(SETS_DIRECTORY) + fork_name;
			ZipUtil::extract_zip(zip_file, target_dir);
			SetMetadata sm = SetManager::load_set_metadata(fork_name);
			sm.name = fork_name;
			sm.workshop_id = "";
			sm.workshop_hash = "";
			sm.is_online = false;
			sm.version = 1;
			sm.forked_from_id = item.id;
			sm.forked_from_author = item.author.empty() ? "Community" : item.author;
			sm.forked_from_version = ver;
			SetManager::save_set_metadata(fork_name, sm);
			SetManager::set_current_set(fork_name);
			ToastManager::success("Forked set created: '" + fork_name + "'! You can modify and publish it separately.");
		};

		std::string cache_path =
			WorkshopCache::get_cache_path(item.id, "set", "set_v" + std::to_string(s_fork_version) + ".zip");
		if (std::filesystem::exists(cache_path)) {
			do_fork_set(cache_path);
		} else {
			WorkshopClient::download_item_version(item.id, s_fork_version, cache_path,
												  [do_fork_set, cache_path](bool success, const std::string&) {
													  if (success) {
														  do_fork_set(cache_path);
													  } else {
														  ToastManager::error("Failed to download set for forking.");
													  }
												  });
		}
		return;
	}

	if (item.type == "theme") {
		auto do_fork_theme = [item, ver = s_fork_version](const std::string& theme_file) {
			std::string themes_dir = "themes/";
			std::filesystem::create_directories(themes_dir);
			std::string base_fork = item.title + "_fork";
			std::string fork_name = base_fork;
			int fidx = 2;
			while (std::filesystem::exists(themes_dir + fork_name + ".theme")) {
				fork_name = base_fork + "_" + std::to_string(fidx++);
			}
			std::ifstream fin(theme_file);
			nlohmann::json tj;
			if (fin.is_open()) {
				try {
					tj = nlohmann::json::parse(fin);
				} catch (...) {}
				fin.close();
			}
			tj["name"] = fork_name;
			tj["forked_from_id"] = item.id;
			tj["forked_from_author"] = item.author.empty() ? "Community" : item.author;
			tj["forked_from_version"] = ver;
			std::ofstream fout(themes_dir + fork_name + ".theme");
			if (fout.is_open()) {
				fout << tj.dump(2);
				fout.close();
			}
			ToastManager::success("Forked theme created: '" + fork_name + "'!");
		};

		std::string cache_path =
			WorkshopCache::get_cache_path(item.id, "theme", "theme_v" + std::to_string(s_fork_version) + ".theme");
		if (std::filesystem::exists(cache_path)) {
			do_fork_theme(cache_path);
		} else {
			WorkshopClient::download_item_version(item.id, s_fork_version, cache_path,
												  [do_fork_theme, cache_path](bool success, const std::string&) {
													  if (success) {
														  do_fork_theme(cache_path);
													  } else {
														  ToastManager::error("Failed to download theme for forking.");
													  }
												  });
		}
		return;
	}

	s_active_modal = ModalType::ForkItem;
	s_fork_available_sets.clear();
	s_selected_fork_set_idx = 0;

	std::string parent_set = !item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
	s_fork_available_sets.push_back(parent_set);
	for (const auto& s : SetManager::get_sets()) {
		if (s != parent_set && s.rfind(parent_set + "_fork", 0) == 0) {
			s_fork_available_sets.push_back(s);
		}
	}
	if (s_fork_available_sets.size() > 1) {
		s_selected_fork_set_idx = 1;
	}
}

void UIModals::close_current() {
	s_active_modal = ModalType::None;
	ImGui::CloseCurrentPopup();
}

bool UIModals::has_active_modal() { return s_active_modal != ModalType::None; }

void UIModals::render() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	const std::string& current_set = SetManager::get_current_set();

	if (s_active_modal == ModalType::ExitConfirmation) {
		ImGui::OpenPopup("Exit Confirmation");
	} else if (s_active_modal == ModalType::UnsavedChangesSwitch) {
		ImGui::OpenPopup("Unsaved Changes Switch");
	} else if (s_active_modal == ModalType::CreateSet) {
		ImGui::OpenPopup("Create Set...");
	} else if (s_active_modal == ModalType::DeleteSetConfirmation) {
		ImGui::OpenPopup("Delete Set Confirmation");
	} else if (s_active_modal == ModalType::ForkSet) {
		ImGui::OpenPopup("Fork Set...");
	} else if (s_active_modal == ModalType::CanvasResizeConfirmation) {
		ImGui::OpenPopup("Confirm Canvas Resize");
	} else if (s_active_modal == ModalType::DiffSizeSave) {
		ImGui::OpenPopup("Differently Sized Save Detected");
	} else if (s_active_modal == ModalType::EmptyRulePerformanceWarning) {
		ImGui::OpenPopup("Performance Warning##EmptyRule");
	} else if (s_active_modal == ModalType::LargeNeighborhoodWarning) {
		ImGui::OpenPopup("Performance Warning##LargeNeighborhood");
	} else if (s_active_modal == ModalType::SetOverrideWarning) {
		ImGui::OpenPopup("Item Warning##OverrideModal");
	} else if (s_active_modal == ModalType::WorkshopAuth) {
		ImGui::OpenPopup("Workshop Authentication##Modal");
	} else if (s_active_modal == ModalType::WorkshopReport) {
		ImGui::OpenPopup("Report Item##ModalDialog");
	} else if (s_active_modal == ModalType::WorkshopShare) {
		ImGui::OpenPopup("Share to Workshop##ModalDialog");
	} else if (s_active_modal == ModalType::WorkshopVersionHistory) {
		ImGui::OpenPopup("Version History##ModalDialog");
	} else if (s_active_modal == ModalType::MigrateForkedSet) {
		ImGui::OpenPopup("Migrate to Forked Set##Modal");
	} else if (s_active_modal == ModalType::ForkItem) {
		ImGui::OpenPopup("Fork Item##ModalDialog");
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Exit Confirmation", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.00f), "WARNING: UNSAVED CHANGES");
		ImGui::Separator();
		ImGui::Spacing();

		if (!s_exit_save_as_new_set) {
			ImGui::TextWrapped("You have unsaved changes in %s. What would you like to do?", current_set.c_str());
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			std::string save_btn_lbl = "Save & Exit (" + current_set + ")";
			if (ImGui::Button(save_btn_lbl.c_str(), ImVec2(190, 30))) {
				MaterialManager::save_all_materials(std::string(SETS_DIRECTORY) + current_set);
				ConfigManager::save();
				UI::shutdown();
				Grid::shutdown();
				Window::shutdown();
				exit(0);
			}
			ImGui::SameLine();
			if (ImGui::Button("Save as New Set...", ImVec2(190, 30))) {
				s_exit_save_as_new_set = true;
			}

			if (ImGui::Button("Exit Without Saving", ImVec2(190, 30))) {
				ConfigManager::save();
				UI::shutdown();
				Grid::shutdown();
				Window::shutdown();
				exit(0);
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(190, 30))) {
				close_current();
			}
		} else {
			ImGui::Text("Enter name for the new set:");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##exit_new_set_name", s_exit_new_set_name_buf, sizeof(s_exit_new_set_name_buf),
								 ImGuiInputTextFlags_EnterReturnsTrue)) {
				std::string new_set_name = sanitize_name(s_exit_new_set_name_buf);
				if (!new_set_name.empty()) {
					SetManager::copy_set(current_set, new_set_name);
					ConfigManager::save();
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
				std::string new_set_name = sanitize_name(s_exit_new_set_name_buf);
				if (!new_set_name.empty()) {
					SetManager::copy_set(current_set, new_set_name);
					ConfigManager::save();
					UI::shutdown();
					Grid::shutdown();
					Window::shutdown();
					exit(0);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Back", ImVec2(120, 30))) {
				s_exit_save_as_new_set = false;
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

		ImGui::TextWrapped("You have unsaved changes in the current set. Do you want to save them before switching?");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Save & Switch", ImVec2(120, 30))) {
			MaterialManager::save_all_materials(std::string(SETS_DIRECTORY) + current_set);
			should_proceed_switch = true;
			close_current();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard & Switch", ImVec2(120, 30))) {
			should_proceed_switch = true;
			close_current();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			s_pending_set_switch = "";
			s_pending_save_load = "";
			close_current();
		}
		ImGui::EndPopup();
	}

	if (should_proceed_switch) {
		if (!s_pending_set_switch.empty()) {
			SetManager::set_current_set(s_pending_set_switch);
			UI::set_selected_id(0);
			s_pending_set_switch = "";
		} else if (!s_pending_save_load.empty()) {
			SaveManager::load_from_file_async(s_pending_save_load, current_set, LoadPlacement::Center,
											  [](bool success, const std::string&) {
												  if (success) {
													  UI::set_selected_id(0);
												  }
											  });
			s_pending_save_load = "";
		}
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Create Set...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::Text(s_create_set_duplicate ? "DUPLICATE CURRENT SET" : "CREATE NEW EMPTY SET");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Enter name for the new set:");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##new_set_name_input", s_create_set_name_buf, sizeof(s_create_set_name_buf),
							 ImGuiInputTextFlags_EnterReturnsTrue)) {
			std::string name = sanitize_name(s_create_set_name_buf);
			if (!name.empty()) {
				if (s_create_set_duplicate) {
					SetManager::copy_set(current_set, name);
				} else {
					SetManager::create_new_empty_set(name);
					SetManager::set_current_set(name);
				}
				close_current();
			}
		}

		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Confirm", ImVec2(120, 30))) {
			std::string name = sanitize_name(s_create_set_name_buf);
			if (!name.empty()) {
				if (s_create_set_duplicate) {
					SetManager::copy_set(current_set, name);
				} else {
					SetManager::create_new_empty_set(name);
					SetManager::set_current_set(name);
				}
				close_current();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			close_current();
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
			UI::set_selected_id(0);
			ImGui::PopStyleColor(3);
			close_current();
		} else {
			ImGui::PopStyleColor(3);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Fork Set...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::Text("FORK CURRENT SET");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Enter name for your forked set:");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##fork_set_name_input", s_create_set_name_buf, sizeof(s_create_set_name_buf),
							 ImGuiInputTextFlags_EnterReturnsTrue)) {
			std::string name = sanitize_name(s_create_set_name_buf);
			if (!name.empty()) {
				SetManager::fork_set(current_set, name);
				SetManager::set_current_set(name);
				ToastManager::success("Forked set created: '" + name + "'!");
				close_current();
			}
		}

		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Confirm Fork", ImVec2(120, 30))) {
			std::string name = sanitize_name(s_create_set_name_buf);
			if (!name.empty()) {
				SetManager::fork_set(current_set, name);
				SetManager::set_current_set(name);
				ToastManager::success("Forked set created: '" + name + "'!");
				close_current();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Confirm Canvas Resize", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		uint64_t target_cells = static_cast<uint64_t>(s_resize_target_w) * s_resize_target_h;
		ImGui::Text("Are you sure you want to resize the simulation canvas?");
		ImGui::Text("Target size: %d x %d (%lu cells)", s_resize_target_w, s_resize_target_h, target_cells);
		ImGui::Spacing();
		if (target_cells > 1024 * 1024) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.65f, 0.1f, 1.0f),
				"Performance Warning: Canvas exceeds 1,048,576 cells.\nFrame rates on CPU and GPU may drop.");
			ImGui::Spacing();
		}
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
						   "Warning: Resizing the simulation will reset undo/redo history!");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Confirm Resize", ImVec2(130, 30))) {
			UI::deselect();
			Grid::resize(static_cast<uint32_t>(s_resize_target_w), static_cast<uint32_t>(s_resize_target_h),
						 s_resize_preserve_content);
			UndoManager::clear();
			UndoManager::init();
			close_current();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 30))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Differently Sized Save Detected", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Save file: '%s'", s_pending_diff_save.name.c_str());
		ImGui::Text("Save Dimensions: %u x %u", s_pending_diff_save.width, s_pending_diff_save.height);
		ImGui::Text("Current Grid: %u x %u", Grid::get_width(), Grid::get_height());
		ImGui::Spacing();
		ImGui::Text("Choose how to load this save into the simulation:");
		ImGui::Spacing();

		if (ImGui::Button("Place in Center", ImVec2(180, 32))) {
			SaveManager::load_from_file_async(s_pending_diff_save.filename, current_set, LoadPlacement::Center,
											  [](bool success, const std::string&) {
												  if (success) {
													  UI::set_selected_id(0);
												  }
											  });
			close_current();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Places the saved content centered in the current canvas.");
		}

		ImGui::SameLine();
		if (ImGui::Button("Crop / Top-Left", ImVec2(180, 32))) {
			SaveManager::load_from_file_async(s_pending_diff_save.filename, current_set, LoadPlacement::TopLeft,
											  [](bool success, const std::string&) {
												  if (success) {
													  UI::set_selected_id(0);
												  }
											  });
			close_current();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Loads content aligned to top-left, cropping any excess outside grid.");
		}

		if (ImGui::Button("Cancel", ImVec2(368, 28))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Performance Warning##EmptyRule", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.70f, 0.20f, 1.00f), "PERFORMANCE WARNING");
		ImGui::Separator();
		ImGui::Spacing();

		if (s_pending_empty_inherit_id != 255) {
			ImGui::TextWrapped("Inheriting rules onto the 'empty' material means every empty cell on the grid will be "
							   "evaluated every frame.");
			ImGui::TextWrapped(
				"This can significantly lower simulation performance when large areas of the grid are empty.");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Inherit Anyway", ImVec2(150, 30))) {
				UndoManager::push_snapshot("Set Inheritance");
				MaterialManager::set_material_inheritance(0, s_pending_empty_inherit_id);
				s_pending_empty_inherit_id = 255;
				close_current();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 30))) {
				s_pending_empty_inherit_id = 255;
				close_current();
			}
		} else {
			ImGui::TextWrapped("Adding rules to the 'empty' material means every empty cell on the grid will be "
							   "evaluated every frame.");
			ImGui::TextWrapped(
				"This can significantly lower simulation performance when large areas of the grid are empty.");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Add Rule Anyway", ImVec2(150, 30))) {
				auto& mats = MaterialManager::get_materials();
				for (auto& m : mats) {
					if (m.id == 0) {
						UndoManager::push_snapshot("Add Rule");
						RuleDefinition new_rule;
						new_rule.init_default(m.id);
						m.rules.push_back(new_rule);
						m.rule_order.push_back({false, m.rules.size() - 1});
						MaterialManager::rebuild_compiled_rules();
						break;
					}
				}
				close_current();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 30))) {
				close_current();
			}
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Performance Warning##LargeNeighborhood", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Warning: Expanding to Large Rule Neighborhood (>100 cells)");
		ImGui::Spacing();
		ImGui::TextWrapped(
			"You are expanding the rule neighborhood beyond 100 cells.\n\n"
			"Very large neighborhoods significantly increase the computational area checked per cell, "
			"which can noticeably impact simulation performance.\n\n"
			"Would you like to enable 'Allow Large Rule Neighborhoods' in Advanced Settings and expand this rule?");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Enable & Expand", ImVec2(150, 30))) {
			ConfigManager::get_config().allow_large_neighborhoods = true;
			ConfigManager::save();
			if (s_pending_expand_rule_idx >= 0) {
				auto& mats = MaterialManager::get_materials();
				for (auto& m : mats) {
					if (m.id == s_pending_expand_mat_id) {
						if (s_pending_expand_rule_idx < static_cast<int>(m.rules.size())) {
							UndoManager::push_snapshot("Expand Rule Neighborhood");
							auto& rule = m.rules[s_pending_expand_rule_idx];
							if (s_pending_expand_side == 0)
								rule.expand_top();
							else if (s_pending_expand_side == 1)
								rule.expand_bottom();
							else if (s_pending_expand_side == 2)
								rule.expand_left();
							else if (s_pending_expand_side == 3)
								rule.expand_right();
							MaterialManager::rebuild_compiled_rules();
						}
						break;
					}
				}
			}
			s_pending_expand_rule_idx = -1;
			close_current();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 30))) {
			s_pending_expand_rule_idx = -1;
			close_current();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(460.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Item Warning##OverrideModal", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		if (s_pending_is_version_load) {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WARNING: LOCAL MODIFICATIONS DETECTED");
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextWrapped("You have made local modifications to the %s '%s'.",
							   s_pending_override_item.type.c_str(), s_pending_override_item.title.c_str());
			ImGui::Spacing();
			ImGui::BulletText("Installed Local Version: v%u (Modified)", s_pending_local_version);
			ImGui::BulletText("Target Version to Load: v%u", s_pending_target_version);
			ImGui::Spacing();
			ImGui::TextWrapped("Loading version v%u from the Workshop will overwrite your local changes.\nAre you sure "
							   "you want to proceed?",
							   s_pending_target_version);
		} else if (s_pending_is_switch) {
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "WARNING: LOCAL MODIFICATIONS DETECTED");
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextWrapped("You have made local modifications to the %s '%s'.",
							   s_pending_override_item.type.c_str(), s_pending_override_item.title.c_str());
			ImGui::Spacing();
			ImGui::BulletText("Installed Local Version: v%u (Modified)", s_pending_local_version);
			ImGui::BulletText("Workshop Latest Version: v%u", s_pending_target_version);
			ImGui::Spacing();
			ImGui::TextWrapped(
				"Loading the latest version from the Workshop will overwrite your local changes.\nYou can choose to "
				"overwrite and load the Workshop version, or keep your local modifications and switch.");
		} else if (s_pending_is_update) {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "SET UPDATE AVAILABLE");
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextWrapped("An updated version of set '%s' is available on the Workshop.",
							   s_pending_override_item.title.c_str());
			ImGui::Spacing();
			ImGui::BulletText("Installed Local Version: v%u", s_pending_local_version);
			ImGui::BulletText("Workshop Online Version: v%d", s_pending_override_item.version);
			ImGui::Spacing();
			ImGui::TextWrapped("Proceeding will replace your local set files with the online version.\nAny local "
							   "modifications made to this set's materials or configuration will be overwritten.");
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WARNING: OVERWRITING LOCAL ITEM");
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextWrapped("A local %s named '%s' (v%u) already exists on disk.",
							   s_pending_override_item.type.c_str(), s_pending_override_item.title.c_str(),
							   s_pending_local_version);
			ImGui::Spacing();
			ImGui::BulletText("Workshop Online Version: v%d", s_pending_override_item.version);
			ImGui::Spacing();
			ImGui::TextWrapped("Proceeding will replace your local files with the online version.\nAny local "
							   "modifications will be overwritten.");
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		std::string confirm_lbl = s_pending_is_version_load
									  ? ("Overwrite & Load v" + std::to_string(s_pending_target_version))
									  : (s_pending_is_update ? "Overwrite & Update" : "Overwrite & Load");
		if (ImGui::Button(confirm_lbl.c_str(), ImVec2(160, 30))) {
			if (s_pending_override_confirm) {
				s_pending_override_confirm();
			}
			close_current();
		}

		if (s_pending_is_switch && s_pending_override_keep_local) {
			ImGui::SameLine();
			if (ImGui::Button("Keep Local & Switch", ImVec2(160, 30))) {
				if (s_pending_override_keep_local) {
					s_pending_override_keep_local();
				}
				close_current();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(90, 30))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(400, 380), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal("Workshop Authentication##Modal", nullptr,
							   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.0f, 1.0f),
						   s_auth_is_register ? "Create Workshop Account" : "Log In to Workshop");
		ImGui::Separator();
		ImGui::Spacing();

		if (!s_auth_status.empty()) {
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "%s", s_auth_status.c_str());
			ImGui::Spacing();
		}

		ImGui::Text("Username:");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##auth_user", s_auth_username, sizeof(s_auth_username));

		ImGui::Spacing();
		ImGui::Text("Password:");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##auth_pass", s_auth_password, sizeof(s_auth_password), ImGuiInputTextFlags_Password);

		ImGui::Spacing();
		if (s_auth_is_register) {
			if (ImGui::SmallButton("Already have an account? Log in instead")) {
				s_auth_is_register = false;
				s_auth_status = "";
			}
		} else {
			if (ImGui::SmallButton("Don't have an account? Register new account")) {
				s_auth_is_register = true;
				s_auth_status = "";
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		std::string submit_label = s_auth_is_register ? "Create Account" : "Log In";
		if (ImGui::Button(submit_label.c_str(), ImVec2(140, 28))) {
			std::string u = s_auth_username;
			std::string p = s_auth_password;
			if (u.empty() || p.empty()) {
				s_auth_status = "Please provide both username and password.";
			} else {
				if (s_auth_is_register) {
					WorkshopClient::register_user(u, p, [u](bool success, const std::string& err) {
						if (success) {
							s_auth_password[0] = '\0';
							ConfigManager::get_config().workshop.token = WorkshopClient::get_auth_token();
							ConfigManager::get_config().workshop.username = u;
							ConfigManager::save();
							ToastManager::success("Welcome, " + u + "! Registered and logged in.");
							UIModals::close_current();
						} else {
							s_auth_status = err;
						}
					});
				} else {
					WorkshopClient::login(u, p, [u](bool success, const std::string& err) {
						if (success) {
							s_auth_password[0] = '\0';
							ConfigManager::get_config().workshop.token = WorkshopClient::get_auth_token();
							ConfigManager::get_config().workshop.username = u;
							ConfigManager::save();
							ToastManager::success("Welcome back, " + u + "!");
							UIModals::close_current();
						} else {
							s_auth_status = err;
						}
					});
				}
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 28))) {
			s_auth_password[0] = '\0';
			close_current();
		}

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Report Item##ModalDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Report item: \"%s\"", s_report_item_title.c_str());
		ImGui::Separator();
		ImGui::Spacing();

		if (!s_report_status.empty()) {
			ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "%s", s_report_status.c_str());
			ImGui::Spacing();
			if (ImGui::Button("Close", ImVec2(100, 24))) {
				close_current();
			}
		} else {
			ImGui::Text("Reason:");
			const char* reasons[] = {"Broken simulation / Game crashes", "Inappropriate or offensive content",
									 "Spam or duplicate", "Other"};
			ImGui::Combo("##report_reason", &s_report_reason_idx, reasons, IM_ARRAYSIZE(reasons));

			ImGui::Spacing();
			auto filterNewline = [](ImGuiInputTextCallbackData* data) -> int {
				return (data->EventChar == '\r' || data->EventChar == '\n');
			};

			ImGui::Text("Details (optional):");
			ImGui::InputTextMultiline(
				"##report_details", s_report_details, sizeof(s_report_details), ImVec2(-1.0f, 70.0f),
				ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackCharFilter, filterNewline);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (UIWidgets::button_with_icon("Submit Report", IconManager::get(IconID::Report), ImVec2(130, 26))) {
				const char* reason_keys[] = {"broken", "offensive", "spam", "other"};
				WorkshopClient::submit_report(s_report_item_id, reason_keys[s_report_reason_idx], s_report_details,
											  [](bool success, const std::string&) {
												  if (success) {
													  s_report_status = "Thank you! Report received.";
												  } else {
													  s_report_status =
														  "Failed to submit report. Please check internet connection.";
												  }
											  });
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(90, 24))) {
				close_current();
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Share to Workshop##ModalDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextColored(ImVec4(0.35f, 0.7f, 1.0f, 1.0f), "Publish Item to Community Workshop");
		ImGui::Separator();
		ImGui::Spacing();

		if (!s_publish_status.empty()) {
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", s_publish_status.c_str());
			ImGui::Spacing();
			if (ImGui::Button("Done", ImVec2(100, 24))) {
				close_current();
				UIWorkshop::refresh_workshop_items();
			}
		} else {
			switch (s_publish_type_idx) {
				case 0: {
					std::string cur_s =
						s_publish_target_set.empty() ? SetManager::get_current_set() : s_publish_target_set;
					if (!s_publish_target_save.empty()) {
						ImGui::TextDisabled("Target Save: %s (Set: %s)", s_publish_target_save.c_str(),
											cur_s.c_str());
						std::string fname = s_publish_target_save;
						if (fname.length() < 5 || fname.substr(fname.length() - 5) != ".save") {
							fname += ".save";
						}
						std::string fpath = SaveManager::get_saves_directory(cur_s) + fname;
						if (!SaveManager::is_file_highest_compression(fpath)) {
							ImGui::Spacing();
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
							ImGui::TextWrapped("Warning: This save does not use Highest compression. "
											   "Highest compression is required for workshop uploads and will be enforced.");
							ImGui::PopStyleColor();
						}
					} else {
						ImGui::TextDisabled("Target Save: [Current Simulation Grid] (Set: %s)",
											cur_s.c_str());
						if (ConfigManager::get_config().compression_quality != 2) {
							ImGui::Spacing();
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
							ImGui::TextWrapped("Notice: Compression setting is not High. "
											   "Highest compression is required and will be forced for this upload.");
							ImGui::PopStyleColor();
						}
					}
					break;
				}
				case 1: {
					auto all_sets = SetManager::get_sets();
					if (s_publish_target_set.empty() && !all_sets.empty()) {
						s_publish_target_set = SetManager::get_current_set();
					}
					break;
				}
				case 2: {
					std::string target_s =
						s_publish_target_set.empty() ? SetManager::get_current_set() : s_publish_target_set;
					auto stamp_files = SaveManager::get_stamp_files(target_s);
					std::vector<std::string> stamp_names;
					stamp_names.push_back("[Current Selection / Clipboard]");
					int sel_idx = 0;
					for (size_t i = 0; i < stamp_files.size(); ++i) {
						stamp_names.push_back(stamp_files[i].filename);
						if (stamp_files[i].filename == s_publish_target_stamp ||
							stamp_files[i].name == s_publish_target_stamp) {
							sel_idx = static_cast<int>(i + 1);
						}
					}
					std::vector<const char*> stamp_cstrs;
					stamp_cstrs.reserve(stamp_names.size());
					for (const auto& sn : stamp_names) {
						stamp_cstrs.push_back(sn.c_str());
					}
					ImGui::Text("Target Stamp:");
					ImGui::SameLine();
					if (ImGui::Combo("##pub_target_stamp", &sel_idx, stamp_cstrs.data(),
									 static_cast<int>(stamp_cstrs.size()))) {
						if (sel_idx == 0) {
							s_publish_target_stamp = "";
						} else {
							s_publish_target_stamp = stamp_files[sel_idx - 1].filename;
							std::snprintf(s_publish_title, sizeof(s_publish_title), "%s",
										  stamp_files[sel_idx - 1].name.c_str());
							s_publish_target_w = stamp_files[sel_idx - 1].width;
							s_publish_target_h = stamp_files[sel_idx - 1].height;
						}
					}

					if (!s_publish_target_stamp.empty()) {
						std::string fname = s_publish_target_stamp;
						if (fname.length() < 6 || fname.substr(fname.length() - 6) != ".stamp") {
							fname += ".stamp";
						}
						std::string fpath = SaveManager::get_saves_directory(target_s) + fname;
						if (!SaveManager::is_file_highest_compression(fpath)) {
							ImGui::Spacing();
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
							ImGui::TextWrapped("Warning: This stamp does not use Highest compression. "
											   "Highest compression is required for workshop uploads and will be enforced.");
							ImGui::PopStyleColor();
						}
					} else {
						if (ConfigManager::get_config().compression_quality != 2) {
							ImGui::Spacing();
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
							ImGui::TextWrapped("Notice: Compression setting is not High. "
											   "Highest compression is required and will be forced for this upload.");
							ImGui::PopStyleColor();
						}
					}
					break;
				}
				case 3: {
					ImGui::TextDisabled("Includes your current UI colors, window/frame rounding, button sizes, etc.");
					break;
				}
			}

			ImGui::Spacing();
			ImGui::Text("Item Title *");
			ImGui::InputText("##pub_title", s_publish_title, sizeof(s_publish_title));

			ImGui::Spacing();
			ImGui::Text("Author / Creator *");
			ImGui::InputText("##pub_author", s_publish_author, sizeof(s_publish_author));

			ImGui::Spacing();
			ImGui::Text("Description:");
			auto filterNewline = [](ImGuiInputTextCallbackData* data) -> int {
				return (data->EventChar == '\r' || data->EventChar == '\n');
			};

			ImGui::InputTextMultiline("##pub_desc", s_publish_desc, sizeof(s_publish_desc), ImVec2(-1.0f, 60.0f),
									  ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackCharFilter,
									  filterNewline);

			ImGui::Spacing();
			UIWidgets::custom_checkbox("Private Item", &s_publish_is_private, IconID::Lock,
									   "Make this item private so only you can see and download it.");

			ImGui::Spacing();

			std::string cur_set = s_publish_target_set.empty() ? SetManager::get_current_set() : s_publish_target_set;
			SetMetadata cur_m = SetManager::load_set_metadata(cur_set);
			std::string cur_hash = SetManager::compute_set_hash(cur_set);
			bool can_publish = true;

			if (s_publish_type_idx == 0 || s_publish_type_idx == 2) {
				const char* item_kind = (s_publish_type_idx == 0) ? "save" : "stamp";
				if (cur_m.workshop_id.empty()) {
					can_publish = false;
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
					ImGui::TextWrapped(
						"The set '%s' is not shared to the Workshop. To publish a %s, you must share this set first.",
						cur_set.c_str(), item_kind);
					ImGui::PopStyleColor();
					ImGui::Spacing();
					std::string share_btn_lbl = "Share Set '" + cur_set + "' First";
					if (UIWidgets::button_with_icon(share_btn_lbl.c_str(), IconManager::get(IconID::Transmit),
													ImVec2(0, 24))) {
						s_publish_target_set = cur_set;
						s_publish_type_idx = 1;
						std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", cur_set.c_str());
						std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", cur_m.description.c_str());
					}
				} else {
					bool set_changed = (!cur_m.workshop_hash.empty() && cur_hash != cur_m.workshop_hash);
					if (set_changed) {
						can_publish = false;
						bool is_owner = WorkshopClient::is_logged_in() && !cur_m.author.empty() &&
										(WorkshopClient::get_logged_in_username() == cur_m.author);
						if (is_owner) {
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
							ImGui::TextWrapped("Local changes detected in your set '%s'. You must update the set on "
											   "the Workshop before publishing %ss for it.",
											   cur_set.c_str(), item_kind);
							ImGui::PopStyleColor();
							ImGui::Spacing();
							if (UIWidgets::button_with_icon("Update Set on Workshop", IconManager::get(IconID::Update),
															ImVec2(0, 24))) {
								WorkshopItemClient set_item;
								set_item.id = cur_m.workshop_id;
								set_item.title = cur_set;
								set_item.description = cur_m.description;
								set_item.type = "set";
								set_item.version = cur_m.version;
								set_item.author = cur_m.author;
								close_current();
								WorkshopItemEditor::open_update_modal(set_item,
																	  []() { UIWorkshop::refresh_workshop_items(); });
							}
						} else {
							ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.2f, 1.0f));
							ImGui::TextWrapped("Set '%s' belongs to %s and has local changes. You must fork this set "
											   "to your own set before publishing %ss for it.",
											   cur_set.c_str(), cur_m.author.c_str(), item_kind);
							ImGui::PopStyleColor();
							ImGui::Spacing();
							if (UIWidgets::button_with_icon("Fork Set as Your Own", IconManager::get(IconID::Branch),
															ImVec2(0, 24))) {
								std::string base_fork = cur_set + "_fork";
								std::string fork_name = base_fork;
								int fidx = 2;
								while (std::filesystem::exists(std::string(SETS_DIRECTORY) + fork_name)) {
									fork_name = base_fork + "_" + std::to_string(fidx++);
								}
								SetManager::fork_set(cur_set, fork_name);
								SetManager::set_current_set(fork_name);
								s_publish_target_set = fork_name;
								s_publish_type_idx = 1;
								std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", fork_name.c_str());
								std::string fdesc = "Fork of '" + cur_set + "' by " + cur_m.author;
								std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", fdesc.c_str());
								ToastManager::info("Fork created: '" + fork_name +
												   "'. Publish this set to Workshop first!");
							}
						}
					} else {
						ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Attached to Online Set: '%s' (v%d)",
										   cur_set.c_str(), cur_m.version);
					}
				}
			} else if (s_publish_type_idx == 1) {
				if (!cur_m.forked_from_id.empty()) {
					ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Fork of: %s (v%d) by %s",
									   cur_m.forked_from_id.c_str(), cur_m.forked_from_version,
									   cur_m.forked_from_author.c_str());
				}
			}
			if (!s_publish_forked_from_id.empty()) {
				ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Forked from Online Item: %s (v%d)",
								   s_publish_forked_from_id.c_str(), s_publish_forked_from_version);
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (s_publishing) {
				ImGui::Text("Uploading and publishing item...");
			} else if (can_publish) {
				if (UIWidgets::button_with_icon("Upload & Publish", IconManager::get(IconID::Transmit),
												ImVec2(150, 26))) {
					s_publishing = true;
					std::string cur_s =
						s_publish_target_set.empty() ? SetManager::get_current_set() : s_publish_target_set;
					SetMetadata set_meta = SetManager::load_set_metadata(cur_s);

					if (s_publish_type_idx == 0) {
						std::vector<uint8_t> bytes;
						uint32_t sw = Grid::get_width();
						uint32_t sh = Grid::get_height();
						if (s_publish_target_w > 0)
							sw = s_publish_target_w;
						if (s_publish_target_h > 0)
							sh = s_publish_target_h;

						if (!s_publish_target_save.empty()) {
							std::string fname = s_publish_target_save;
							if (fname.length() < 5 || fname.substr(fname.length() - 5) != ".save") {
								fname += ".save";
							}
							std::string fpath = SaveManager::get_saves_directory(cur_s) + fname;
							std::ifstream fin(std::filesystem::path(fpath), std::ios::binary);
							if (fin.is_open()) {
								bytes.assign((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
								fin.close();
							}
						} else {
							SaveManager::save_to_file("__ws_temp_pub", cur_s, CompressionQuality::High);
							std::string fpath = SaveManager::get_saves_directory(cur_s) + "__ws_temp_pub.save";
							std::ifstream fin(std::filesystem::path(fpath), std::ios::binary);
							if (fin.is_open()) {
								bytes.assign((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
								fin.close();
							}
							std::error_code rm_ec;
							std::filesystem::remove(std::filesystem::path(fpath), rm_ec);
						}

						if (bytes.empty()) {
							s_publishing = false;
							ToastManager::error("No save data found to publish. Please check that the file exists.");
							return;
						}

						if (SaveManager::recompress_to_highest(bytes, cur_s)) {
							ToastManager::warning("Recompressed save to Highest compression for workshop upload.");
						}

						std::string parent_id = set_meta.workshop_id;
						std::string set_hash = SetManager::compute_set_hash(cur_s);
						std::string thumb_b64 =
							UIThumbnails::generate_thumbnail_base64(0, cur_s, s_publish_target_save);
						nlohmann::json meta;
						meta["width"] = sw;
						meta["height"] = sh;
						meta["set_name"] = cur_s;

						WorkshopClient::publish_item(
							"save", s_publish_title, s_publish_desc, s_publish_author, parent_id, bytes, ".save",
							meta.dump(), set_hash, thumb_b64,
							[cur_s, s_target_save = std::string(s_publish_target_save),
							 s_title = std::string(s_publish_title), set_hash,
							 s_author = std::string(s_publish_author)](bool success, const std::string& created_id,
																	   const std::string& err) {
								s_publishing = false;
								if (success) {
									std::string save_name = s_target_save.empty() ? s_title : s_target_save;
									SaveManager::set_save_workshop_info(save_name, cur_s, created_id, set_hash,
																		s_author, 1);
									s_publish_status = "Published successfully! Available in workshop.";
									ToastManager::success("Save published to Workshop!");
								} else {
									s_publish_status = "Upload failed: " + err;
								}
							},
							s_publish_forked_from_id, s_publish_forked_from_version, s_publish_is_private);
					} else if (s_publish_type_idx == 1) {
						if (cur_s == SetManager::get_current_set()) {
							MaterialManager::save_all_materials(std::string(SETS_DIRECTORY) + cur_s);
						}
						std::string set_dir = std::string(SETS_DIRECTORY) + cur_s;
						std::vector<std::pair<std::string, std::vector<uint8_t>>> set_files;
						std::error_code dir_ec;
						if (std::filesystem::exists(std::filesystem::path(set_dir), dir_ec)) {
							for (const auto& entry :
								 std::filesystem::directory_iterator(std::filesystem::path(set_dir), dir_ec)) {
								if (entry.is_regular_file(dir_ec)) {
									std::string fname = entry.path().filename().string();
									std::string ext = entry.path().extension().string();
									if (ext == ".mat" || fname == "set_config.ini") {
										std::ifstream in(entry.path(), std::ios::binary);
										if (in.is_open()) {
											std::vector<uint8_t> fbytes((std::istreambuf_iterator<char>(in)),
																		std::istreambuf_iterator<char>());
											set_files.emplace_back(fname, fbytes);
										}
									}
								}
							}
						}
						if (set_files.empty()) {
							s_publishing = false;
							ToastManager::error("No material or config files found in set directory '" + cur_s +
												"' to publish.");
							return;
						}
						std::vector<uint8_t> zip_bytes = ZipUtil::create_zip(set_files);
						if (zip_bytes.empty()) {
							s_publishing = false;
							ToastManager::error("Failed to generate zip file for set '" + cur_s + "'.");
							return;
						}
						std::string set_hash = SetManager::compute_set_hash(cur_s);
						std::string thumb_b64 = UIThumbnails::generate_thumbnail_base64(1, cur_s);

						nlohmann::json meta;
						meta["materials_count"] = MaterialManager::get_materials().size();
						meta["set_name"] = cur_s;

						WorkshopClient::publish_item(
							"set", s_publish_title, s_publish_desc, s_publish_author, "", zip_bytes, ".zip",
							meta.dump(), set_hash, thumb_b64,
							[cur_s, set_hash, s_author = std::string(s_publish_author)](
								bool success, const std::string& created_id, const std::string& err) {
								s_publishing = false;
								if (success) {
									SetManager::set_workshop_info(cur_s, created_id, set_hash, 1, s_author);
									SetManager::mark_set_online(cur_s, true);
									s_publish_status = "Set published successfully and linked to workshop!";
									ToastManager::success("Set published to Workshop!");
								} else {
									s_publish_status = "Upload failed: " + err;
								}
							},
							set_meta.forked_from_id, set_meta.forked_from_version, s_publish_is_private);
					} else if (s_publish_type_idx == 2) {
						std::vector<uint8_t> stamp_bytes;
						uint32_t sw = s_publish_target_w > 0 ? s_publish_target_w : 64;
						uint32_t sh = s_publish_target_h > 0 ? s_publish_target_h : 64;

						if (!s_publish_target_stamp.empty()) {
							std::string fname = s_publish_target_stamp;
							if (fname.length() < 6 || fname.substr(fname.length() - 6) != ".stamp") {
								fname += ".stamp";
							}
							std::string fpath = SaveManager::get_saves_directory(cur_s) + fname;
							std::ifstream fin(std::filesystem::path(fpath), std::ios::binary);
							if (fin.is_open()) {
								stamp_bytes.assign((std::istreambuf_iterator<char>(fin)),
												   std::istreambuf_iterator<char>());
								fin.close();
							}
						} else {
							const auto& clip = UI::get_clipboard();
							if (!clip.empty() && clip.width > 0 && clip.height > 0) {
								sw = clip.width;
								sh = clip.height;
								SaveManager::save_stamp_to_file("__ws_temp_stamp", cur_s, clip.cells, sw, sh,
																CompressionQuality::High);
								std::string fpath = SaveManager::get_saves_directory(cur_s) + "__ws_temp_stamp.stamp";
								std::ifstream fin(std::filesystem::path(fpath), std::ios::binary);
								if (fin.is_open()) {
									stamp_bytes.assign((std::istreambuf_iterator<char>(fin)),
													   std::istreambuf_iterator<char>());
									fin.close();
								}
								std::error_code rm_ec;
								std::filesystem::remove(std::filesystem::path(fpath), rm_ec);
							}
						}

						if (stamp_bytes.empty()) {
							s_publishing = false;
							ToastManager::error("No stamp file or clipboard selection found to publish.");
							return;
						}

						if (SaveManager::recompress_to_highest(stamp_bytes, cur_s)) {
							ToastManager::warning("Recompressed stamp to Highest compression for workshop upload.");
						}

						std::string parent_id = set_meta.workshop_id;
						std::string set_hash = SetManager::compute_set_hash(cur_s);
						std::string thumb_b64 =
							UIThumbnails::generate_thumbnail_base64(2, cur_s, s_publish_target_stamp);
						nlohmann::json meta;
						meta["width"] = sw;
						meta["height"] = sh;
						meta["set_name"] = cur_s;

						WorkshopClient::publish_item(
							"stamp", s_publish_title, s_publish_desc, s_publish_author, parent_id, stamp_bytes,
							".stamp", meta.dump(), set_hash, thumb_b64,
							[cur_s, s_target_stamp = std::string(s_publish_target_stamp),
							 s_title = std::string(s_publish_title), set_hash,
							 s_author = std::string(s_publish_author)](bool success, const std::string& created_id,
																	   const std::string& err) {
								s_publishing = false;
								if (success) {
									std::string stamp_name = s_target_stamp.empty() ? s_title : s_target_stamp;
									SaveManager::set_stamp_workshop_info(stamp_name, cur_s, created_id, set_hash,
																		 s_author, 1);
									s_publish_status = "Stamp published successfully!";
									ToastManager::success("Stamp published to Workshop!");
								} else {
									s_publish_status = "Upload failed: " + err;
								}
							},
							s_publish_forked_from_id, s_publish_forked_from_version, s_publish_is_private);
					} else if (s_publish_type_idx == 3) {
						auto& cfg = ConfigManager::get_config();
						nlohmann::json tj;
						tj["window_rounding"] = cfg.ui.window_rounding;
						tj["frame_rounding"] = cfg.ui.frame_rounding;
						tj["button_size"] = cfg.ui.button_size;
						tj["icon_size"] = cfg.ui.icon_size;
						tj["sidebar_width"] = cfg.ui.sidebar_width;
						tj["material_list_height"] = cfg.ui.material_list_height;
						tj["set_list_height"] = cfg.ui.set_list_height;
						tj["save_list_height"] = cfg.ui.save_list_height;
						tj["stamp_list_height"] = cfg.ui.stamp_list_height;
						tj["background_color"] = {cfg.ui.background_color.x, cfg.ui.background_color.y,
												  cfg.ui.background_color.z, cfg.ui.background_color.w};
						tj["selection_box_color"] = {cfg.ui.selection_box_color.x, cfg.ui.selection_box_color.y,
													 cfg.ui.selection_box_color.z, cfg.ui.selection_box_color.w};
						tj["selection_box_fill"] = {cfg.ui.selection_box_fill.x, cfg.ui.selection_box_fill.y,
													cfg.ui.selection_box_fill.z, cfg.ui.selection_box_fill.w};

						nlohmann::json col_map = nlohmann::json::object();
						for (const auto& [cname, hex] : ConfigManager::get_color_overrides()) {
							col_map[cname] = hex;
						}
						tj["colors"] = col_map;

						std::string dump_str = tj.dump(2);
						std::vector<uint8_t> theme_bytes(dump_str.begin(), dump_str.end());
						std::string thumb_b64 = UIThumbnails::generate_thumbnail_base64(3, "");

						nlohmann::json meta;
						meta["colors_count"] = col_map.size();

						WorkshopClient::publish_item(
							"theme", s_publish_title, s_publish_desc, s_publish_author, "", theme_bytes, ".theme",
							meta.dump(), "", thumb_b64,
							[](bool success, const std::string&, const std::string& err) {
								s_publishing = false;
								if (success) {
									s_publish_status = "Custom theme published successfully!";
									ToastManager::success("Custom theme published to Workshop!");
								} else {
									s_publish_status = "Upload failed: " + err;
								}
							},
							"", 0, s_publish_is_private);
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(90, 26))) {
					close_current();
				}
			}
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(520.0f, 360.0f), ImVec2(620.0f, 520.0f));
	if (ImGui::BeginPopupModal("Version History##ModalDialog", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(0.35f, 0.7f, 1.0f, 1.0f), "Version History: %s", s_versions_item.title.c_str());
		ImGui::TextDisabled("Type: %s | Author: %s | Latest Workshop Version: v%d", s_versions_item.type.c_str(),
							s_versions_item.author.c_str(), s_versions_item.version);
		if (!s_versions_item.forked_from_id.empty()) {
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Forked from: '%s' by %s (v%d)",
							   s_versions_item.forked_from_title.empty() ? s_versions_item.forked_from_id.c_str()
																		 : s_versions_item.forked_from_title.c_str(),
							   s_versions_item.forked_from_author.c_str(), s_versions_item.forked_from_version);
		}
		ImGui::Separator();
		ImGui::Spacing();

		if (s_loading_versions) {
			ImGui::Text("Loading version history from Workshop server...");
		} else if (!s_versions_error.empty()) {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", s_versions_error.c_str());
		} else if (s_current_item_versions.empty()) {
			ImGui::TextDisabled("No previous versions recorded for this item.");
		} else {
			ImGui::BeginChild("VersionHistoryScroll", ImVec2(0, 260.0f), true);
			for (const auto& ver : s_current_item_versions) {
				ImGui::PushID(ver.version);
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Version %d", ver.version);
				ImGui::SameLine();
				ImGui::TextDisabled("(%s)", ver.created_at.c_str());

				if (!ver.changelog.empty()) {
					ImGui::TextWrapped("Changelog: %s", ver.changelog.c_str());
				}

				if (ImGui::Button("Load this Version", ImVec2(140, 22))) {
					close_current();
				}
				ImGui::Separator();
				ImGui::PopID();
			}
			ImGui::EndChild();
		}

		ImGui::Spacing();
		if (ImGui::Button("Close", ImVec2(100, 24))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Migrate to Forked Set##Modal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Migrate %s: %s", s_migrate_is_save ? "Save" : "Stamp", s_migrate_item_name.c_str());
		ImGui::TextDisabled("Source Set: %s", s_migrate_source_set.c_str());
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Target Forked Set:");
		if (!s_migrate_forked_sets.empty()) {
			std::vector<const char*> set_names;
			set_names.reserve(s_migrate_forked_sets.size());
			for (const auto& f : s_migrate_forked_sets) {
				set_names.push_back(f.c_str());
			}
			ImGui::SetNextItemWidth(250.0f);
			ImGui::Combo("##target_forked_set", &s_selected_migrate_set_idx, set_names.data(),
						 static_cast<int>(set_names.size()));
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Migrate", ImVec2(90, 24))) {
			if (s_selected_migrate_set_idx >= 0 &&
				s_selected_migrate_set_idx < static_cast<int>(s_migrate_forked_sets.size())) {
				const std::string& target_set = s_migrate_forked_sets[s_selected_migrate_set_idx];
				bool ok = false;
				if (s_migrate_is_save) {
					ok = SaveManager::migrate_save_file(s_migrate_filename, s_migrate_source_set, target_set);
				} else {
					ok = SaveManager::migrate_stamp_file(s_migrate_filename, s_migrate_source_set, target_set);
				}
				if (ok) {
					ToastManager::success("Successfully migrated " + std::string(s_migrate_is_save ? "save" : "stamp") +
										  " to '" + target_set + "'!");
				} else {
					ToastManager::error("Failed to migrate file.");
				}
			}
			close_current();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(90, 24))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Fork Item##ModalDialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextColored(ImVec4(0.35f, 0.7f, 1.0f, 1.0f), "Fork %s: %s (v%d)", s_fork_item.type.c_str(),
						   s_fork_item.title.c_str(), s_fork_version);
		ImGui::TextDisabled("Original Author: %s", s_fork_item.author.c_str());
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Assign to Set:");
		if (!s_fork_available_sets.empty()) {
			std::vector<const char*> set_names;
			set_names.reserve(s_fork_available_sets.size());
			for (const auto& s : s_fork_available_sets) {
				set_names.push_back(s.c_str());
			}
			ImGui::SetNextItemWidth(260.0f);
			ImGui::Combo("##fork_assign_set", &s_selected_fork_set_idx, set_names.data(),
						 static_cast<int>(set_names.size()));
		}
		if (s_fork_available_sets.size() > 1 && s_selected_fork_set_idx > 0) {
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Forked set selected!");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Confirm Fork", ImVec2(110, 24))) {
			std::string chosen_set = (s_selected_fork_set_idx >= 0 &&
									  s_selected_fork_set_idx < static_cast<int>(s_fork_available_sets.size()))
										 ? s_fork_available_sets[s_selected_fork_set_idx]
										 : SetManager::get_current_set();

			auto execute_fork = [item = s_fork_item, ver = s_fork_version, chosen_set](const std::string& file_path) {
				std::string base_name = item.title + "_fork";
				std::string fork_name = base_name;
				int fidx = 2;
				if (item.type == "save") {
					std::string saves_dir = SaveManager::get_saves_directory(chosen_set);
					while (std::filesystem::exists(saves_dir + fork_name + ".save")) {
						fork_name = base_name + "_" + std::to_string(fidx++);
					}
					if (SaveManager::fork_save_to_set(file_path, chosen_set, fork_name, item.id, item.author, ver)) {
						ToastManager::success("Forked save created in set '" + chosen_set + "': " + fork_name);
					} else {
						ToastManager::error("Failed to fork save.");
					}
				} else if (item.type == "stamp") {
					std::string saves_dir = SaveManager::get_saves_directory(chosen_set);
					while (std::filesystem::exists(saves_dir + fork_name + ".stamp")) {
						fork_name = base_name + "_" + std::to_string(fidx++);
					}
					if (SaveManager::fork_stamp_to_set(file_path, chosen_set, fork_name, item.id, item.author, ver)) {
						ToastManager::success("Forked stamp created in set '" + chosen_set + "': " + fork_name);
					} else {
						ToastManager::error("Failed to fork stamp.");
					}
				}
			};

			std::string ext = (s_fork_item.type == "save" ? ".save" : ".stamp");
			std::string cache_path = WorkshopCache::get_cache_path(
				s_fork_item.id, s_fork_item.type, s_fork_item.type + "_v" + std::to_string(s_fork_version) + ext);
			if (std::filesystem::exists(cache_path)) {
				execute_fork(cache_path);
			} else {
				WorkshopClient::download_item_version(s_fork_item.id, s_fork_version, cache_path,
													  [execute_fork, cache_path](bool success, const std::string&) {
														  if (success) {
															  execute_fork(cache_path);
														  } else {
															  ToastManager::error(
																  "Failed to download item for forking.");
														  }
													  });
			}

			close_current();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(80, 24))) {
			close_current();
		}
		ImGui::EndPopup();
	}

	WorkshopItemEditor::render();
	ToastManager::render();
}