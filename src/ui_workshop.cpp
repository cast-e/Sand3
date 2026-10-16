#include "ui_workshop.hpp"

#include <SDL3/SDL.h>
#include <fmt/format.h>
#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config_manager.hpp"
#include "const.hpp"
#include "icon_manager.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "toast_manager.hpp"
#include "ui.hpp"
#include "ui_modals.hpp"
#include "ui_theme.hpp"
#include "ui_widgets.hpp"
#include "window.hpp"
#include "workshop_cache.hpp"
#include "workshop_client.hpp"
#include "workshop_item_editor.hpp"
#include "zip_util.hpp"

std::vector<WorkshopItemClient> s_workshop_items;
bool s_workshop_loading = false;
bool s_workshop_loaded_once = false;
std::string s_workshop_error;
bool s_workshop_my_items = false;
bool s_workshop_favorites = false;

char s_workshop_search[128] = "";
int s_workshop_type_filter = 0;
int s_workshop_sort_idx = 0;
bool s_force_workshop_tab_selected = false;

std::unordered_map<std::string, SDL_Texture*> s_workshop_textures;
std::unordered_set<std::string> s_thumbnails_downloading;
std::unordered_set<std::string> s_items_activating;

std::unordered_map<std::string, std::chrono::steady_clock::time_point> s_item_last_checked;
std::unordered_set<std::string> s_item_checking_in_flight;

std::string get_item_local_path(const WorkshopItemClient& item) {
	if (item.type == "set") {
		std::string dir = std::string(SETS_DIRECTORY) + item.title;
		if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir) && !WorkshopCache::is_transient(dir)) {
			return dir;
		}
	} else if (item.type == "save") {
		std::string parent_name =
			!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
		std::string parent_dir = std::string(SETS_DIRECTORY) + parent_name;
		if (!std::filesystem::exists(parent_dir)) {
			return "";
		}
		std::string path = std::string(SETS_DIRECTORY) + parent_name + "/saves/" + item.title + ".save";
		if (std::filesystem::exists(path) && !WorkshopCache::is_transient(path)) {
			return path;
		}
		for (const auto& sf : SaveManager::get_save_files(parent_name)) {
			if (!sf.workshop_id.empty() && sf.workshop_id == item.id) {
				std::string p = std::string(SETS_DIRECTORY) + parent_name + "/saves/" + sf.filename;
				if (std::filesystem::exists(p) && !WorkshopCache::is_transient(p)) {
					return p;
				}
			}
		}
	} else if (item.type == "stamp") {
		std::string parent_name =
			!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
		std::string parent_dir = std::string(SETS_DIRECTORY) + parent_name;
		if (!std::filesystem::exists(parent_dir)) {
			return "";
		}
		std::string path = SaveManager::get_saves_directory(parent_name) + item.title + ".stamp";
		if (std::filesystem::exists(path) && !WorkshopCache::is_transient(path)) {
			return path;
		}
		for (const auto& st : SaveManager::get_stamp_files(parent_name)) {
			if (!st.workshop_id.empty() && st.workshop_id == item.id) {
				std::string p = SaveManager::get_saves_directory(parent_name) + st.filename;
				if (std::filesystem::exists(p) && !WorkshopCache::is_transient(p)) {
					return p;
				}
			}
		}
	}
	return "";
}

std::string get_item_cache_path(const WorkshopItemClient& item) {
	if (item.type == "set") {
		std::string dir = std::string(SETS_DIRECTORY) + item.title;
		if (std::filesystem::exists(dir) && WorkshopCache::is_transient(dir)) {
			return dir;
		}
		std::string p = WorkshopCache::get_cache_path(item.id, "set", "set.zip");
		if (std::filesystem::exists(p)) {
			return p;
		}
	} else if (item.type == "save") {
		std::string parent_name =
			!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
		std::string parent_dir = std::string(SETS_DIRECTORY) + parent_name;
		if (std::filesystem::exists(parent_dir)) {
			std::string path = SaveManager::get_saves_directory(parent_name) + item.title + ".save";
			if (std::filesystem::exists(path) && WorkshopCache::is_transient(path)) {
				return path;
			}
			for (const auto& sf : SaveManager::get_save_files(parent_name)) {
				if (!sf.workshop_id.empty() && sf.workshop_id == item.id) {
					std::string p = SaveManager::get_saves_directory(parent_name) + sf.filename;
					if (std::filesystem::exists(p) && WorkshopCache::is_transient(p)) {
						return p;
					}
				}
			}
		}
		std::string p = WorkshopCache::get_cache_path(item.id, "save", "item.save");
		if (std::filesystem::exists(p)) {
			return p;
		}
	} else if (item.type == "stamp") {
		std::string parent_name =
			!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
		std::string parent_dir = std::string(SETS_DIRECTORY) + parent_name;
		if (std::filesystem::exists(parent_dir)) {
			std::string path = SaveManager::get_saves_directory(parent_name) + item.title + ".stamp";
			if (std::filesystem::exists(path) && WorkshopCache::is_transient(path)) {
				return path;
			}
			for (const auto& st : SaveManager::get_stamp_files(parent_name)) {
				if (!st.workshop_id.empty() && st.workshop_id == item.id) {
					std::string p = SaveManager::get_saves_directory(parent_name) + st.filename;
					if (std::filesystem::exists(p) && WorkshopCache::is_transient(p)) {
						return p;
					}
				}
			}
		}
		std::string p = WorkshopCache::get_cache_path(item.id, "stamp", "item.stamp");
		if (std::filesystem::exists(p)) {
			return p;
		}
	} else if (item.type == "theme") {
		std::string p = WorkshopCache::get_cache_path(item.id, "theme", "item.theme");
		if (std::filesystem::exists(p)) {
			return p;
		}
	}
	return "";
}

bool is_item_present_locally(const WorkshopItemClient& item) {
	if (item.type == "theme") {
		return false;
	}
	return !get_item_local_path(item).empty();
}

bool is_item_modified_locally(const WorkshopItemClient& item, const std::string& target_version_hash = "") {
	if (item.type == "set") {
		if (SetManager::get_current_set() == item.title && UI::has_unsaved_changes()) {
			return true;
		}

		std::string set_dir = std::string(SETS_DIRECTORY) + item.title;
		std::error_code ec;
		if (!std::filesystem::exists(std::filesystem::path(set_dir), ec) ||
			!std::filesystem::is_directory(std::filesystem::path(set_dir), ec)) {
			return false;
		}
		if (WorkshopCache::is_transient(set_dir)) {
			return false;
		}

		std::string current_hash = SetManager::compute_set_hash(item.title);
		if (current_hash.empty()) {
			return false;
		}

		if (!target_version_hash.empty() && current_hash == target_version_hash) {
			return false;
		}

		SetMetadata meta = SetManager::load_set_metadata(item.title);
		if (!meta.workshop_hash.empty()) {
			return current_hash != meta.workshop_hash;
		}

		if (!item.set_hash.empty()) {
			return current_hash != item.set_hash;
		}

		return true;
	} else if (item.type == "save") {
		std::string local_path = get_item_local_path(item);
		if (local_path.empty()) {
			return false;
		}
		std::error_code ec;
		if (!std::filesystem::exists(std::filesystem::path(local_path), ec)) {
			return false;
		}
		if (WorkshopCache::is_transient(local_path)) {
			return false;
		}

		std::string parent_name =
			!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
		SaveFileInfo info;
		if (!SaveManager::inspect_save_file(local_path, parent_name, info)) {
			return true;
		}

		std::string current_fhash = SaveManager::compute_file_hash(local_path);
		if (!info.file_hash.empty()) {
			return current_fhash != info.file_hash;
		}

		std::string cache_p = get_item_cache_path(item);
		if (!cache_p.empty() && std::filesystem::exists(std::filesystem::path(cache_p), ec)) {
			std::string cache_fhash = SaveManager::compute_file_hash(cache_p);
			if (!cache_fhash.empty()) {
				return current_fhash != cache_fhash;
			}
		}

		if (info.workshop_id.empty()) {
			return true;
		}
		return false;
	} else if (item.type == "stamp") {
		std::string local_path = get_item_local_path(item);
		if (local_path.empty()) {
			return false;
		}
		std::error_code ec;
		if (!std::filesystem::exists(std::filesystem::path(local_path), ec)) {
			return false;
		}
		if (WorkshopCache::is_transient(local_path)) {
			return false;
		}

		std::string parent_name =
			!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
		StampFileInfo info;
		if (!SaveManager::inspect_stamp_file(local_path, parent_name, info)) {
			return true;
		}

		std::string current_fhash = SaveManager::compute_file_hash(local_path);
		if (!info.file_hash.empty()) {
			return current_fhash != info.file_hash;
		}

		std::string cache_p = get_item_cache_path(item);
		if (!cache_p.empty() && std::filesystem::exists(std::filesystem::path(cache_p), ec)) {
			std::string cache_fhash = SaveManager::compute_file_hash(cache_p);
			if (!cache_fhash.empty()) {
				return current_fhash != cache_fhash;
			}
		}

		if (info.workshop_id.empty()) {
			return true;
		}
		return false;
	}
	return false;
}

void UIWorkshop::check_item_existence(const std::string& item_id, const std::string& type,
									  const std::string& name_or_file, const std::string& set_context, bool force) {
	if (item_id.empty()) {
		return;
	}

	auto now = std::chrono::steady_clock::now();
	if (!force) {
		auto it = s_item_last_checked.find(item_id);
		if (it != s_item_last_checked.end()) {
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
			if (elapsed < 10) {
				return;
			}
		}
	}

	if (s_item_checking_in_flight.find(item_id) != s_item_checking_in_flight.end()) {
		return;
	}

	s_item_checking_in_flight.insert(item_id);
	s_item_last_checked[item_id] = now;

	WorkshopClient::check_item_exists(
		item_id, [item_id, type, name_or_file, set_context](bool exists, int http_status) {
			s_item_checking_in_flight.erase(item_id);
			if (!exists && http_status == 404) {
				if (type == "set") {
					SetManager::clear_workshop_info(name_or_file);
					ToastManager::warning(
						fmt::format("Set '{}' was deleted from the workshop. Converted to local.", name_or_file));
				} else if (type == "save") {
					SaveManager::clear_save_workshop_info(name_or_file, set_context);
					ToastManager::warning(
						fmt::format("Save '{}' was deleted from the workshop. Converted to local.", name_or_file));
				} else if (type == "stamp") {
					SaveManager::clear_stamp_workshop_info(name_or_file, set_context);
					ToastManager::warning(
						fmt::format("Stamp '{}' was deleted from the workshop. Converted to local.", name_or_file));
				}
				WorkshopCache::remove_transient_by_id(item_id);

				for (auto it = s_workshop_items.begin(); it != s_workshop_items.end();) {
					if (it->id == item_id) {
						it = s_workshop_items.erase(it);
					} else {
						++it;
					}
				}
			}
		});
}

SDL_Texture* get_or_load_thumbnail(const WorkshopItemClient& item) {
	if (item.thumbnail_path.empty())
		return nullptr;
	auto it = s_workshop_textures.find(item.id);
	if (it != s_workshop_textures.end())
		return it->second;

	if (s_thumbnails_downloading.find(item.id) == s_thumbnails_downloading.end()) {
		s_thumbnails_downloading.insert(item.id);
		std::string cache_dir = WorkshopCache::get_cache_root() + "thumbnails/";
		std::filesystem::create_directories(cache_dir);
		std::string dest_path = cache_dir + item.id + ".png";

		WorkshopClient::download_thumbnail(
			item.id, dest_path, [item_id = item.id](bool success, const std::string& path) {
				s_thumbnails_downloading.erase(item_id);
				if (success && std::filesystem::exists(path)) {
					SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
					if (io) {
						SDL_Surface* surf = SDL_LoadPNG_IO(io, true);
						if (surf) {
							SDL_Texture* tex = SDL_CreateTextureFromSurface(Window::get_renderer(), surf);
							SDL_DestroySurface(surf);
							if (tex) {
								s_workshop_textures[item_id] = tex;
							}
						}
					}
				}
			});
	}
	return nullptr;
}

void render_workshop_item_card(const WorkshopItemClient& item) {
	ImGui::PushID(item.id.c_str());

	bool is_owner = WorkshopClient::is_logged_in() && !item.author.empty() &&
					(WorkshopClient::get_logged_in_username() == item.author);
	bool is_local = is_item_present_locally(item);
	uint32_t local_version = 1;
	bool is_update = false;
	if (item.type == "set" && is_local) {
		SetMetadata local_m = SetManager::load_set_metadata(item.title);
		local_version = local_m.version;
		if (item.version > static_cast<int>(local_version)) {
			is_update = true;
		}
	}

	ImVec4 border_col = ImVec4(0.22f, 0.27f, 0.35f, 0.7f);
	ImGui::PushStyleColor(ImGuiCol_Border, border_col);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.17f, 0.6f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

	ImGui::BeginChild(("CardChild_" + item.id).c_str(), ImVec2(0, 0),
					  ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

	float card_top_y = ImGui::GetCursorPosY();
	ImVec2 thumb_sz(62, 62);
	SDL_Texture* tex = get_or_load_thumbnail(item);
	if (tex) {
		ImGui::Image((ImTextureID)tex, thumb_sz);
	} else {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 p_min = ImGui::GetCursorScreenPos();
		ImVec2 p_max(p_min.x + thumb_sz.x, p_min.y + thumb_sz.y);
		dl->AddRectFilled(p_min, p_max, IM_COL32(16, 22, 30, 255), 4.0f);
		dl->AddRect(p_min, p_max, IM_COL32(48, 54, 61, 255), 4.0f);

		IconID ph_icon = IconID::Save;
		if (item.type == "set")
			ph_icon = IconID::Package;
		else if (item.type == "stamp")
			ph_icon = IconID::Select;
		else if (item.type == "theme")
			ph_icon = IconID::Fill;
		SDL_Texture* ph_tex = IconManager::get(ph_icon);
		if (ph_tex) {
			dl->AddImage((ImTextureID)ph_tex,
						 ImVec2(p_min.x + thumb_sz.x * 0.5f - 12, p_min.y + thumb_sz.y * 0.5f - 12),
						 ImVec2(p_min.x + thumb_sz.x * 0.5f + 12, p_min.y + thumb_sz.y * 0.5f + 12));
		}
		ImGui::Dummy(thumb_sz);
	}

	ImGui::SameLine(0, 10);
	ImGui::BeginGroup();

	IconID type_icon = IconID::Save;
	ImVec4 type_col = ImVec4(0.3f, 0.8f, 0.4f, 1.0f);
	const char* type_str = "SAVE";
	if (item.type == "set") {
		type_icon = IconID::Package;
		type_col = ImVec4(0.4f, 0.65f, 1.0f, 1.0f);
		type_str = "SET";
	} else if (item.type == "stamp") {
		type_icon = IconID::Select;
		type_col = ImVec4(0.9f, 0.75f, 0.2f, 1.0f);
		type_str = "STAMP";
	} else if (item.type == "theme") {
		type_icon = IconID::Fill;
		type_col = ImVec4(0.8f, 0.45f, 0.95f, 1.0f);
		type_str = "THEME";
	}

	SDL_Texture* t_tex = IconManager::get(type_icon);
	if (t_tex) {
		ImGui::Image((ImTextureID)t_tex, ImVec2(13, 13));
		ImGui::SameLine(0, 4);
	}
	ImGui::TextColored(type_col, "[%s]", type_str);
	ImGui::SameLine(0, 5);
	if (item.version > 1) {
		ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "v%d", item.version);
		ImGui::SameLine(0, 5);
	}
	if (item.is_private) {
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "[Private]");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("This item is private and only visible to you.");
		}
		ImGui::SameLine(0, 5);
	}
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.98f, 0.98f, 1.0f));
	ImGui::TextWrapped("%s", item.title.c_str());
	ImGui::PopStyleColor();

	if (is_update) {
		ImGui::SameLine(0, 5);
		SDL_Texture* upd_tex = IconManager::get(IconID::Update);
		if (upd_tex) {
			ImGui::Image((ImTextureID)upd_tex, ImVec2(13, 13));
			ImGui::SameLine(0, 3);
		}
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "[New v%d!]", item.version);
		if (ImGui::IsItemHovered()) {
			std::string tip = fmt::format("Newer version v{} available (local is v{})", item.version, local_version);
			if (item.meta.contains("changelog") && item.meta["changelog"].is_array() &&
				!item.meta["changelog"].empty()) {
				const auto& cl = item.meta["changelog"];
				if (cl[0].contains("notes") && cl[0]["notes"].is_string()) {
					tip += "\nNotes: " + cl[0]["notes"].get<std::string>();
				}
			}
			ImGui::SetTooltip("%s", tip.c_str());
		}
	}

	ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.68f, 1.0f), "by %s", item.author.c_str());

	if (!item.forked_from_author.empty() || !item.forked_from_title.empty()) {
		ImGui::SameLine(0, 6);
		SDL_Texture* br_tex = IconManager::get(IconID::Branch);
		if (br_tex) {
			ImGui::Image((ImTextureID)br_tex, ImVec2(12, 12));
			ImGui::SameLine(0, 3);
		}
		std::string fa = item.forked_from_author.empty() ? "Community" : item.forked_from_author;
		ImGui::TextColored(ImVec4(0.45f, 0.7f, 0.95f, 1.0f), "fork of %s", fa.c_str());
		if (ImGui::IsItemHovered()) {
			std::string ft = !item.forked_from_title.empty() ? item.forked_from_title : "original item";
			ImGui::SetTooltip("Forked from '%s' by %s (version %d)", ft.c_str(), fa.c_str(), item.forked_from_version);
		}
	}

	if (!item.parent_set_title.empty()) {
		ImGui::SameLine(0, 6);
		ImGui::TextColored(ImVec4(0.5f, 0.75f, 0.95f, 1.0f), "• Set: %s", item.parent_set_title.c_str());
	}

	if (item.type == "set") {
		int mat_count = 0;
		if (item.meta.contains("materials_count") && item.meta["materials_count"].is_number()) {
			mat_count = item.meta["materials_count"].get<int>();
		}
		if (mat_count > 0) {
			ImGui::SameLine(0, 6);
			ImGui::TextColored(ImVec4(0.5f, 0.75f, 0.5f, 1.0f), "• %d materials", mat_count);
		}
	} else {
		int w = 0, h = 0;
		if (item.meta.contains("width") && item.meta["width"].is_number()) {
			w = item.meta["width"].get<int>();
		}
		if (item.meta.contains("height") && item.meta["height"].is_number()) {
			h = item.meta["height"].get<int>();
		}
		if (w > 0 && h > 0) {
			ImGui::SameLine(0, 6);
			ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.9f, 1.0f), "• %d×%d px", w, h);
		}
	}

	if (!item.description.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.75f, 0.8f, 1.0f));
		if (item.description.length() > 300) {
			std::string preview = item.description.substr(0, 297) + "...";
			ImGui::TextWrapped("%s", preview.c_str());
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", item.description.c_str());
			}
		} else {
			ImGui::TextWrapped("%s", item.description.c_str());
		}
		ImGui::PopStyleColor();
	}

	ImGui::EndGroup();

	if (ImGui::GetCursorPosY() < card_top_y + thumb_sz.y) {
		ImGui::SetCursorPosY(card_top_y + thumb_sz.y);
	}
	ImGui::Spacing();

	auto place_btn = [](float needed_w) {
		if (ImGui::GetContentRegionAvail().x >= needed_w + 4.0f) {
			ImGui::SameLine(0, 4);
		}
	};

	bool is_activating = s_items_activating.find(item.id) != s_items_activating.end();
	std::string try_label = is_activating			 ? "Loading..."
							: (item.type == "set")	 ? (is_local ? (is_update ? "Update" : "Switch") : "Switch")
							: (item.type == "stamp") ? "Use"
							: (item.type == "theme") ? "Apply"
													 : "Load";
	IconID btn_icon = is_activating			   ? IconID::RotateCW
					  : (item.type == "set")   ? (is_update ? IconID::Update : IconID::RotateCW)
					  : (item.type == "stamp") ? IconID::Select
					  : (item.type == "theme") ? IconID::Fill
											   : IconID::Folder;
	if (is_activating) {
		ImGui::BeginDisabled();
	}
	if (UIWidgets::button_with_icon(try_label.c_str(), IconManager::get(btn_icon), ImVec2(0, 22))) {
		s_items_activating.insert(item.id);
		if (item.type == "set") {
			if (is_local) {
				bool has_modifications = is_item_modified_locally(item, item.set_hash);
				if (has_modifications) {
					UIModals::open_set_override_warning(
						item, local_version, item.version, is_update, false, !is_update,
						[item]() {
							std::string target_dir = std::string(SETS_DIRECTORY) + item.title;
							std::string cache_path = get_item_cache_path(item);
							if (!cache_path.empty() && std::filesystem::exists(cache_path)) {
								ZipUtil::extract_zip(cache_path, target_dir);
								SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version,
															  item.author);
								SetManager::mark_set_online(item.title, true);
								SetManager::set_current_set(item.title);
								ToastManager::success("Set '" + item.title + "' updated to online version!");
							} else {
								std::string target_zip = WorkshopCache::get_cache_path(item.id, "set", "set.zip");
								WorkshopClient::download_item(
									item.id, target_zip, [item, target_dir](bool success, const std::string& path) {
										if (success) {
											ZipUtil::extract_zip(path, target_dir);
											SetManager::set_workshop_info(item.title, item.id, item.set_hash,
																		  item.version, item.author);
											SetManager::mark_set_online(item.title, true);
											SetManager::set_current_set(item.title);
											ToastManager::success("Set '" + item.title +
																  "' updated to online version!");
										} else {
											ToastManager::error("Failed to download updated set from workshop.");
										}
									});
							}
						},
						[item]() {
							SetManager::set_current_set(item.title);
							ToastManager::info("Switched to local set '" + item.title +
											   "' (local modifications kept).");
						});
					return;
				}

				if (is_update) {
					std::string target_dir = std::string(SETS_DIRECTORY) + item.title;
					std::string cache_path = get_item_cache_path(item);
					if (!cache_path.empty() && std::filesystem::exists(cache_path)) {
						ZipUtil::extract_zip(cache_path, target_dir);
						SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version, item.author);
						SetManager::mark_set_online(item.title, true);
						SetManager::set_current_set(item.title);
						ToastManager::success("Set '" + item.title + "' updated to online version!");
						s_items_activating.erase(item.id);
					} else {
						std::string target_zip = WorkshopCache::get_cache_path(item.id, "set", "set.zip");
						WorkshopClient::download_item(
							item.id, target_zip, [item, target_dir](bool success, const std::string& path) {
								s_items_activating.erase(item.id);
								if (success) {
									ZipUtil::extract_zip(path, target_dir);
									SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version,
																  item.author);
									SetManager::mark_set_online(item.title, true);
									SetManager::set_current_set(item.title);
									ToastManager::success("Set '" + item.title + "' updated to online version!");
								} else {
									ToastManager::error("Failed to download updated set from workshop.");
								}
							});
					}
				} else {
					SetManager::set_current_set(item.title);
					s_items_activating.erase(item.id);
				}
			} else {
				std::string target_set_dir = std::string(SETS_DIRECTORY) + item.title;
				std::string target_zip = WorkshopCache::get_cache_path(item.id, "set", "set.zip");
				WorkshopClient::download_item(
					item.id, target_zip, [item, target_set_dir](bool success, const std::string& path) {
						s_items_activating.erase(item.id);
						if (success) {
							ZipUtil::extract_zip(path, target_set_dir);
							WorkshopCache::register_transient(target_set_dir);
							SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version,
														  item.author);
							SetManager::mark_set_online(item.title, true);
							SetManager::set_current_set(item.title);
							ToastManager::info("Set '" + item.title + "' activated! (Cached until exit)");
						} else {
							ToastManager::error("Failed to download set from workshop.");
						}
					});
			}
		} else if (item.type == "save" || item.type == "stamp") {
			auto ensure_parent_set_and_run = [item](const std::function<void()>& on_ready) {
				if (!item.parent_set_id.empty() && !item.parent_set_title.empty()) {
					std::string parent_name = item.parent_set_title;
					std::string parent_dir = std::string(SETS_DIRECTORY) + parent_name;
					if (!std::filesystem::exists(parent_dir) ||
						!std::filesystem::exists(parent_dir + "/set_config.ini")) {
						std::string parent_zip = WorkshopCache::get_cache_path(item.parent_set_id, "set", "set.zip");
						if (std::filesystem::exists(parent_zip)) {
							ZipUtil::extract_zip(parent_zip, parent_dir);
							WorkshopCache::register_transient(parent_dir);
							SetManager::set_workshop_info(parent_name, item.parent_set_id, item.set_hash, 1);
							SetManager::mark_set_online(parent_name, true);
							SetManager::set_current_set(parent_name);
							on_ready();
						} else {
							WorkshopClient::download_item(
								item.parent_set_id, parent_zip,
								[parent_name, parent_dir, item, on_ready](bool set_ok, const std::string& zpath) {
									if (set_ok) {
										ZipUtil::extract_zip(zpath, parent_dir);
										WorkshopCache::register_transient(parent_dir);
										SetManager::set_workshop_info(parent_name, item.parent_set_id, item.set_hash,
																	  1);
										SetManager::mark_set_online(parent_name, true);
										SetManager::set_current_set(parent_name);
										on_ready();
									} else {
										s_items_activating.erase(item.id);
										ToastManager::error("Failed to download required set '" + parent_name + "'.");
									}
								});
						}
						return;
					} else {
						if (SetManager::get_current_set() != parent_name) {
							SetManager::set_current_set(parent_name);
						}
					}
				}
				on_ready();
			};

			ensure_parent_set_and_run([item]() {
				std::string parent_name =
					!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
				std::string saves_dir = SaveManager::get_saves_directory(parent_name);
				std::error_code ec;
				std::filesystem::create_directories(saves_dir, ec);

				std::string ext = (item.type == "save") ? ".save" : ".stamp";
				std::string target_file = saves_dir + item.title + ext;
				bool exists_on_disk = std::filesystem::exists(target_file);

				if (exists_on_disk) {
					bool is_transient = WorkshopCache::is_transient(target_file);
					if (item.type == "save") {
						SaveManager::load_from_file_async(
							target_file, parent_name, LoadPlacement::Center,
							[is_transient, item_id = item.id](bool s_ok, const std::string&) {
								s_items_activating.erase(item_id);
								if (s_ok) {
									ToastManager::info(is_transient ? "Save loaded from cache!"
																	: "Save loaded from local storage!");
								} else {
									ToastManager::error("Failed to load save file.");
								}
							});
					} else {
						SaveManager::load_stamp_from_file_async(
							target_file, parent_name,
							[is_transient, item_id = item.id](bool s_ok, const std::vector<Cell>& cells, uint32_t w,
															  uint32_t h) {
								s_items_activating.erase(item_id);
								if (s_ok) {
									UI::load_stamp(cells, w, h);
									ToastManager::info(is_transient ? "Stamp loaded into brush (cache)!"
																	: "Stamp loaded into brush (local file)!");
								} else {
									ToastManager::error("Failed to load stamp file.");
								}
							});
					}
				} else {
					WorkshopClient::download_item(
						item.id, target_file, [item, parent_name, target_file](bool success, const std::string&) {
							if (success) {
								WorkshopCache::register_transient(target_file);
								if (item.type == "save") {
									SaveManager::set_save_workshop_info(item.title, parent_name, item.id, item.set_hash,
																		item.author, item.version);
									SaveManager::load_from_file_async(
										target_file, parent_name, LoadPlacement::Center,
										[item_id = item.id](bool s_ok, const std::string&) {
											s_items_activating.erase(item_id);
											if (s_ok) {
												ToastManager::info("Save loaded into sandbox!");
											} else {
												ToastManager::error("Failed to load downloaded save.");
											}
										});
								} else {
									SaveManager::set_stamp_workshop_info(item.title, parent_name, item.id,
																		 item.set_hash, item.author, item.version);
									SaveManager::load_stamp_from_file_async(
										target_file, parent_name,
										[item_id = item.id](bool s_ok, const std::vector<Cell>& cells, uint32_t w,
															uint32_t h) {
											s_items_activating.erase(item_id);
											if (s_ok) {
												UI::load_stamp(cells, w, h);
												ToastManager::info("Stamp loaded into brush!");
											} else {
												ToastManager::error("Failed to load downloaded stamp.");
											}
										});
								}
							} else {
								s_items_activating.erase(item.id);
								ToastManager::error("Failed to download " + item.type + " from workshop server.");
							}
						});
				}
			});
		} else if (item.type == "theme") {
			std::string theme_path = WorkshopCache::get_cache_path(item.id, "theme", "item.theme");
			if (std::filesystem::exists(theme_path)) {
				s_items_activating.erase(item.id);
				UITheme::apply_theme_from_file(theme_path, item.title);
			} else {
				WorkshopClient::download_item(item.id, theme_path, [item](bool success, const std::string& p) {
					s_items_activating.erase(item.id);
					if (success) {
						WorkshopCache::register_transient(p);
						UITheme::apply_theme_from_file(p, item.title);
					} else {
						ToastManager::error("Failed to download theme.");
					}
				});
			}
		}
	}
	if (is_activating) {
		ImGui::EndDisabled();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Loads item into game.");
	}

	if (WorkshopClient::is_logged_in()) {
		place_btn(56.0f);
		if (UIWidgets::button_with_icon("Fork", IconManager::get(IconID::Branch), ImVec2(0, 22))) {
			UIModals::open_fork_item(item, item.version);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Fork this %s into a personal copy that you can modify and publish separately.",
							  item.type.c_str());
		}
	}

	place_btn(74.0f);
	if (UIWidgets::button_with_icon("Versions", IconManager::get(IconID::Tag), ImVec2(0, 22))) {
		UIModals::open_workshop_versions(item);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Browse past versions of this %s.", item.type.c_str());
	}

	char like_btn_lbl[32];
	std::snprintf(like_btn_lbl, sizeof(like_btn_lbl), "%d##like_%s", item.likes_count, item.id.c_str());
	bool pushed_like_col = item.is_liked;
	if (pushed_like_col) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
	}
	place_btn(64.0f);
	if (UIWidgets::button_with_icon(like_btn_lbl, IconManager::get(IconID::Heart), ImVec2(0, 22))) {
		if (!WorkshopClient::is_logged_in()) {
			UIModals::open_workshop_auth(false);
			ToastManager::info("Please log in to like workshop items.");
		} else {
			std::string item_id = item.id;
			bool prev_liked = item.is_liked;
			int prev_count = item.likes_count;
			bool opt_liked = !prev_liked;
			int opt_count = std::max(0, prev_count + (opt_liked ? 1 : -1));

			for (auto& it : s_workshop_items) {
				if (it.id == item_id) {
					it.is_liked = opt_liked;
					it.likes_count = opt_count;
					break;
				}
			}

			WorkshopClient::toggle_like(item_id,
										[item_id, prev_liked, prev_count](bool success, bool is_liked, int count) {
											for (auto& it : s_workshop_items) {
												if (it.id == item_id) {
													if (success) {
														it.is_liked = is_liked;
														it.likes_count = count;
													} else {
														it.is_liked = prev_liked;
														it.likes_count = prev_count;
													}
													break;
												}
											}
										});
		}
	}
	if (pushed_like_col) {
		ImGui::PopStyleColor();
	}

	char fav_btn_lbl[32];
	std::snprintf(fav_btn_lbl, sizeof(fav_btn_lbl), "%d##fav_%s", item.favorites_count, item.id.c_str());
	bool pushed_fav_col = item.is_favorited;
	if (pushed_fav_col) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
	}
	place_btn(64.0f);
	if (UIWidgets::button_with_icon(fav_btn_lbl, IconManager::get(IconID::Star), ImVec2(0, 22))) {
		if (!WorkshopClient::is_logged_in()) {
			UIModals::open_workshop_auth(false);
			ToastManager::info("Please log in to favorite workshop items.");
		} else {
			std::string item_id = item.id;
			bool prev_fav = item.is_favorited;
			int prev_count = item.favorites_count;
			bool opt_fav = !prev_fav;
			int opt_count = std::max(0, prev_count + (opt_fav ? 1 : -1));

			for (auto& it : s_workshop_items) {
				if (it.id == item_id) {
					it.is_favorited = opt_fav;
					it.favorites_count = opt_count;
					break;
				}
			}

			WorkshopClient::toggle_favorite(item_id,
											[item_id, prev_fav, prev_count](bool success, bool is_fav, int count) {
												for (auto& it : s_workshop_items) {
													if (it.id == item_id) {
														if (success) {
															it.is_favorited = is_fav;
															it.favorites_count = count;
														} else {
															it.is_favorited = prev_fav;
															it.favorites_count = prev_count;
														}
														break;
													}
												}
											});
		}
	}
	if (pushed_fav_col) {
		ImGui::PopStyleColor();
	}

	if (is_owner) {
		ImGui::Spacing();
		const char* priv_lbl = item.is_private ? "Public##priv_" : "Private##priv_";
		std::string priv_btn_id = priv_lbl + item.id;
		if (UIWidgets::button_with_icon(
				priv_btn_id.c_str(), IconManager::get(item.is_private ? IconID::World : IconID::Lock), ImVec2(0, 22))) {
			std::string item_id = item.id;
			WorkshopClient::toggle_private(item_id, [item_id](bool success, int is_private) {
				if (success) {
					for (auto& it : s_workshop_items) {
						if (it.id == item_id) {
							it.is_private = is_private;
							break;
						}
					}
					ToastManager::info(is_private ? "Item is now private." : "Item is now public.");
				} else {
					ToastManager::error("Failed to update item privacy.");
				}
			});
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(item.is_private ? "Make this item public for everyone"
											  : "Make this item private (only visible to you)");
		}

		place_btn(54.0f);
		if (UIWidgets::button_with_icon(("Edit##edit_" + item.id).c_str(), IconManager::get(IconID::Edit),
										ImVec2(0, 22))) {
			WorkshopItemEditor::open_edit_modal(item, []() { UIWorkshop::refresh_workshop_items(); });
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Edit title and description");
		}

		place_btn(66.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
		if (UIWidgets::button_with_icon(("Delete##del_" + item.id).c_str(), IconManager::get(IconID::Delete),
										ImVec2(0, 22))) {
			WorkshopItemEditor::open_delete_modal(item, []() { UIWorkshop::refresh_workshop_items(); });
		}
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Delete this item from Workshop");
		}
	}

	const float report_btn_w = 74.0f;
	float current_avail = ImGui::GetContentRegionAvail().x;
	if (current_avail >= report_btn_w + 4.0f) {
		ImGui::SameLine();
		float right_target_x = ImGui::GetWindowContentRegionMax().x - report_btn_w;
		if (right_target_x > ImGui::GetCursorPosX()) {
			ImGui::SetCursorPosX(right_target_x);
		}
	}
	if (UIWidgets::button_with_icon(("Report##rep_" + item.id).c_str(), IconManager::get(IconID::Report),
									ImVec2(report_btn_w, 22))) {
		if (!WorkshopClient::is_logged_in()) {
			UIModals::open_workshop_auth(false);
			ToastManager::info("Please log in to report items.");
		} else {
			UIModals::open_workshop_report(item.id, item.title);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Report this item");
	}

	ImGui::Dummy(ImVec2(0.0f, 4.0f));
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(2);
	ImGui::PopID();
}

void UIWorkshop::refresh_workshop_items() {
	validate_online_items(true);
	s_workshop_loading = true;
	s_workshop_error = "";

	std::string type_str = "all";
	if (s_workshop_type_filter == 1)
		type_str = "set";
	else if (s_workshop_type_filter == 2)
		type_str = "save";
	else if (s_workshop_type_filter == 3)
		type_str = "stamp";
	else if (s_workshop_type_filter == 4)
		type_str = "theme";

	std::string sort_str = "popular";
	if (s_workshop_sort_idx == 1)
		sort_str = "liked";
	else if (s_workshop_sort_idx == 2)
		sort_str = "favorites";
	else if (s_workshop_sort_idx == 3)
		sort_str = "newest";
	else if (s_workshop_sort_idx == 4)
		sort_str = "downloads";

	std::string author_str;
	if (s_workshop_my_items && WorkshopClient::is_logged_in()) {
		author_str = WorkshopClient::get_logged_in_username();
	}

	WorkshopClient::fetch_items(
		type_str, sort_str, s_workshop_search, author_str, s_workshop_favorites,
		[](bool success, const std::vector<WorkshopItemClient>& items, int) {
			s_workshop_loading = false;
			s_workshop_loaded_once = true;
			if (success) {
				s_workshop_items = items;
				s_workshop_error = "";
			} else {
				s_workshop_error = "Could not reach workshop server. Please ensure you are connected to the internet.";
			}
		});
}

void UIWorkshop::remove_workshop_item(const std::string& item_id) {
	for (auto it = s_workshop_items.begin(); it != s_workshop_items.end();) {
		if (it->id == item_id) {
			it = s_workshop_items.erase(it);
		} else {
			++it;
		}
	}
}

void UIWorkshop::validate_online_items(bool force) {
	std::string cur_set = SetManager::get_current_set();

	for (const auto& s : SetManager::get_sets()) {
		SetMetadata sm = SetManager::load_set_metadata(s);
		if ((sm.is_online || SetManager::is_set_online(s)) && !sm.workshop_id.empty()) {
			check_item_existence(sm.workshop_id, "set", s, cur_set, force);
		}
	}

	auto saves = SaveManager::get_save_files(cur_set);
	for (const auto& sf : saves) {
		if (sf.is_online && !sf.workshop_id.empty()) {
			check_item_existence(sf.workshop_id, "save", sf.filename, cur_set, force);
		}
	}

	auto stamps = SaveManager::get_stamp_files(cur_set);
	for (const auto& st : stamps) {
		if (st.is_online && !st.workshop_id.empty()) {
			check_item_existence(st.workshop_id, "stamp", st.filename, cur_set, force);
		}
	}

	for (const auto& path : WorkshopCache::get_transient_paths()) {
		size_t slash = path.find_last_of("/\\");
		std::string fname = (slash != std::string::npos) ? path.substr(slash + 1) : path;
		size_t underscore = fname.find('_');
		if (underscore != std::string::npos) {
			std::string tid = fname.substr(0, underscore);
			if (tid.length() >= 8) {
				check_item_existence(tid, "cache", fname, cur_set, force);
			}
		}
	}
}

void UIWorkshop::handle_uri(const std::string& uri) {
	std::string item_id;
	std::string item_type;

	std::string s = uri;
	while (!s.empty() && (s.front() == '"' || s.front() == '\'' || s.front() == ' '))
		s.erase(0, 1);
	while (!s.empty() &&
		   (s.back() == '"' || s.back() == '\'' || s.back() == ' ' || s.back() == '\n' || s.back() == '\r'))
		s.pop_back();

	size_t qmark = s.find('?');
	if (qmark != std::string::npos) {
		std::string query = s.substr(qmark + 1);
		size_t pos = 0;
		while (pos < query.size()) {
			size_t next_amp = query.find('&', pos);
			std::string param = (next_amp == std::string::npos) ? query.substr(pos) : query.substr(pos, next_amp - pos);
			size_t eq = param.find('=');
			if (eq != std::string::npos) {
				std::string key = param.substr(0, eq);
				std::string val = param.substr(eq + 1);
				if (key == "id") {
					item_id = val;
				} else if (key == "type") {
					item_type = val;
				}
			}
			if (next_amp == std::string::npos)
				break;
			pos = next_amp + 1;
		}
	}

	if (item_id.empty()) {
		std::string path_part = (qmark != std::string::npos) ? s.substr(0, qmark) : s;
		if (path_part.rfind("sand3://", 0) == 0) {
			path_part = path_part.substr(8);
		}
		if (path_part.rfind("open/", 0) == 0 || path_part.rfind("item/", 0) == 0)
			path_part = path_part.substr(5);
		else if (path_part == "open")
			path_part = "";
		else if (path_part.rfind("items/", 0) == 0)
			path_part = path_part.substr(6);

		while (!path_part.empty() && path_part.back() == '/')
			path_part.pop_back();
		while (!path_part.empty() && path_part.front() == '/')
			path_part.erase(0, 1);

		if (!path_part.empty()) {
			item_id = path_part;
		}
	}

	if (!item_id.empty()) {
		s_force_workshop_tab_selected = true;

		std::memset(s_workshop_search, 0, sizeof(s_workshop_search));
		std::strncpy(s_workshop_search, item_id.c_str(), sizeof(s_workshop_search) - 1);

		if (item_type == "set") {
			s_workshop_type_filter = 1;
		} else if (item_type == "save") {
			s_workshop_type_filter = 2;
		} else if (item_type == "stamp") {
			s_workshop_type_filter = 3;
		} else if (item_type == "theme") {
			s_workshop_type_filter = 4;
		} else {
			s_workshop_type_filter = 0;
		}

		s_workshop_my_items = false;
		s_workshop_favorites = false;
		s_workshop_sort_idx = 0;

		refresh_workshop_items();

		if (Window::get_window()) {
			SDL_ShowWindow(Window::get_window());
			SDL_RaiseWindow(Window::get_window());
		}
	}
}

void UIWorkshop::render() {
	ImGuiTabItemFlags tab_flags = 0;
	if (s_force_workshop_tab_selected) {
		tab_flags |= ImGuiTabItemFlags_SetSelected;
		s_force_workshop_tab_selected = false;
	}
	if (ImGui::BeginTabItem("Workshop", nullptr, tab_flags)) {
		if (!s_workshop_loaded_once && !s_workshop_loading) {
			refresh_workshop_items();
		}

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.0f, 1.0f), "Community Workshop");
		ImGui::TextDisabled("Browse, load, and share community items.");

		if (WorkshopClient::is_logged_in()) {
			SDL_Texture* u_icon = IconManager::get(IconID::User);
			if (u_icon) {
				ImGui::Image((ImTextureID)u_icon, ImVec2(14, 14));
				ImGui::SameLine(0, 4);
			}
			ImGui::Text("User: %s", WorkshopClient::get_logged_in_username().c_str());
			ImGui::SameLine(0, 8);
			if (ImGui::SmallButton("Log out")) {
				WorkshopClient::logout();
				ConfigManager::get_config().workshop.token = "";
				ConfigManager::get_config().workshop.username = "";
				ConfigManager::save();
				s_workshop_my_items = false;
				s_workshop_favorites = false;
				refresh_workshop_items();
			}

			ImGui::Spacing();
			float filter_row_w = ImGui::GetContentRegionAvail().x;
			float half_btn_w = (filter_row_w - 4.0f) * 0.5f;

			bool my_items_active = s_workshop_my_items;
			if (my_items_active) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.7f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.8f, 1.0f));
			}
			if (ImGui::Button("My Items", ImVec2(half_btn_w, 22.0f))) {
				s_workshop_my_items = !s_workshop_my_items;
				refresh_workshop_items();
			}
			if (my_items_active) {
				ImGui::PopStyleColor(2);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Show only items created by you");
			}

			ImGui::SameLine(0, 4.0f);

			bool fav_active = s_workshop_favorites;
			if (fav_active) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.4f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.5f, 0.2f, 1.0f));
			}
			if (UIWidgets::button_with_icon("Favorites##fav_filter", IconManager::get(IconID::Star),
											ImVec2(half_btn_w, 22.0f))) {
				s_workshop_favorites = !s_workshop_favorites;
				refresh_workshop_items();
			}
			if (fav_active) {
				ImGui::PopStyleColor(2);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Show items you have favorited");
			}
		} else {
			if (UIWidgets::button_with_icon("Log in##login_guest", IconManager::get(IconID::User), ImVec2(0, 20.0f))) {
				UIModals::open_workshop_auth(false);
			}
			ImGui::SameLine(0, 8);
			bool fav_active = s_workshop_favorites;
			if (fav_active) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.4f, 0.15f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.5f, 0.2f, 1.0f));
			}
			if (UIWidgets::button_with_icon("Favorites##fav_guest", IconManager::get(IconID::Star), ImVec2(0, 20.0f))) {
				s_workshop_favorites = !s_workshop_favorites;
				refresh_workshop_items();
			}
			if (fav_active) {
				ImGui::PopStyleColor(2);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Show items you have favorited");
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float avail_w = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(avail_w - 28.0f);
		if (ImGui::InputTextWithHint("##ws_sidebar_search", "Search items...", s_workshop_search,
									 sizeof(s_workshop_search))) {
			refresh_workshop_items();
		}
		ImGui::SameLine(0, 4.0f);
		if (UIWidgets::button_with_icon("##ws_refresh_btn", IconManager::get(IconID::Refresh), ImVec2(28, 28))) {
			refresh_workshop_items();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Refresh workshop items");
		}

		const char* type_labels[] = {"All Types", "Sets", "Saves", "Stamps", "Themes"};
		float half_w = (avail_w - 6.0f) * 0.5f;
		ImGui::SetNextItemWidth(half_w);
		if (ImGui::Combo("##ws_type_filter", &s_workshop_type_filter, type_labels, IM_ARRAYSIZE(type_labels))) {
			refresh_workshop_items();
		}
		ImGui::SameLine(0, 6.0f);
		const char* sort_labels[] = {"Popular", "Most Liked", "Favorites", "Newest", "Downloads"};
		ImGui::SetNextItemWidth(half_w);
		if (ImGui::Combo("##ws_sort_filter", &s_workshop_sort_idx, sort_labels, IM_ARRAYSIZE(sort_labels))) {
			refresh_workshop_items();
		}

		ImGui::Spacing();
		ImGui::BeginChild("WorkshopSidebarScroll", ImVec2(0, -25), true);
		if (s_workshop_loading) {
			ImGui::TextDisabled("Loading items...");
		} else if (!s_workshop_error.empty()) {
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "%s", s_workshop_error.c_str());
			if (ImGui::Button("Retry")) {
				refresh_workshop_items();
			}
		} else if (s_workshop_items.empty()) {
			ImGui::TextDisabled("No items found.");
		} else {
			for (const auto& it : s_workshop_items) {
				render_workshop_item_card(it);
				ImGui::Spacing();
			}
		}
		ImGui::EndChild();

		ImGui::EndTabItem();
	}
}