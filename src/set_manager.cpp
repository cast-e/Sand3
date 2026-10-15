#include "set_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "grid.hpp"
#include "material_manager.hpp"
#include "sanitize.hpp"
#include "sha256.hpp"
#include "ui.hpp"
#include "undo_manager.hpp"
#include "window.hpp"
#include "workshop_cache.hpp"

namespace fs = std::filesystem;

std::string SetManager::current_set_name;
SetMetadata SetManager::current_metadata{};
std::unordered_map<std::string, bool> SetManager::online_set_cache;

static bool is_valid_or_non_empty_set(const fs::path& dir_path) {
	std::error_code ec;
	for (const auto& sub_entry :
		 fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied, ec)) {
		if (sub_entry.is_regular_file(ec)) {
			std::string ext = sub_entry.path().extension().string();
			std::string fn = sub_entry.path().filename().string();
			if (ext == ".mat" || ext == ".save" || ext == ".stamp" || fn == "set_config.ini") {
				return true;
			}
		}
	}
	return false;
}

std::vector<std::string> SetManager::get_sets() {
	std::vector<std::string> sets;
	std::error_code ec;
	fs::create_directories(SETS_DIRECTORY, ec);

	for (const auto& entry : fs::directory_iterator(SETS_DIRECTORY, ec)) {
		if (entry.is_directory(ec)) {
			std::string name = entry.path().filename().string();
			if (!is_valid_or_non_empty_set(entry.path()) && name != current_set_name) {
				fs::remove_all(entry.path(), ec);
				continue;
			}
			sets.push_back(name);
		}
	}

	if (sets.empty()) {
		create_new_empty_set("new_set");
		sets.push_back("new_set");
	}

	std::sort(sets.begin(), sets.end());

	return sets;
}

SetMetadata SetManager::load_set_metadata(const std::string& name) {
	SetMetadata meta;
	meta.name = name;
	meta.author = "";
	meta.description = "";
	meta.width = 0;
	meta.height = 0;
	meta.target_fps = 0;
	meta.processing_mode = -1;
	meta.prevent_downclock = true;
	meta.workshop_id = "";
	meta.workshop_hash = "";
	meta.is_online = false;

	if (online_set_cache.find(name) != online_set_cache.end()) {
		meta.is_online = online_set_cache[name];
	}

	std::string cfg_path = SETS_DIRECTORY + name + "/set_config.ini";
	std::ifstream file(cfg_path);
	if (!file.is_open()) {
		return meta;
	}

	std::string line;
	std::string current_section;
	while (std::getline(file, line)) {
		std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';' || trimmed.rfind("//", 0) == 0) {
			continue;
		}
		if (trimmed.front() == '[' && trimmed.back() == ']') {
			current_section = trim(trimmed.substr(1, trimmed.length() - 2));
			continue;
		}
		size_t eq_pos = trimmed.find('=');
		if (eq_pos != std::string::npos) {
			std::string key = trim(trimmed.substr(0, eq_pos));
			std::string val = trim(trimmed.substr(eq_pos + 1));

			if (current_section == "Shortcuts") {
				if (!key.empty() && !val.empty()) {
					meta.shortcuts[key] = val;
				}
			} else if (current_section == "Workshop") {
				if (key == "workshop_id") {
					meta.workshop_id = val;
					meta.is_online = true;
				} else if (key == "workshop_hash") {
					meta.workshop_hash = val;
				} else if (key == "forked_from_id") {
					meta.forked_from_id = val;
				} else if (key == "forked_from_author") {
					meta.forked_from_author = val;
				} else if (key == "forked_from_version") {
					try {
						meta.forked_from_version = static_cast<uint32_t>(std::stoul(val));
					} catch (...) {}
				}
			} else if (current_section == "Attributes") {
				if (key == "count") {
					try {
						int cnt = std::clamp(std::stoi(val), 0, 3);
						if (meta.attributes.size() < static_cast<size_t>(cnt)) {
							meta.attributes.resize(cnt);
						}
					} catch (...) {}
				} else {
					for (int i = 0; i < 3; ++i) {
						std::string prefix = "attr" + std::to_string(i) + "_";
						if (key.rfind(prefix, 0) == 0) {
							if (meta.attributes.size() <= static_cast<size_t>(i)) {
								meta.attributes.resize(i + 1);
							}
							std::string field = key.substr(prefix.length());
							if (field == "name") {
								meta.attributes[i].name = val;
							} else if (field == "type") {
								meta.attributes[i].type = (val == "float") ? AttributeType::Float : AttributeType::Int;
							} else if (field == "min") {
								try {
									meta.attributes[i].min_val = std::stod(val);
								} catch (...) {}
							} else if (field == "max") {
								try {
									meta.attributes[i].max_val = std::stod(val);
								} catch (...) {}
							} else if (field == "default") {
								try {
									meta.attributes[i].default_val = std::stod(val);
								} catch (...) {}
							}
						}
					}
				}
			} else if (current_section == "Heatmap") {
				if (key == "stop_count") {
					try {
						int cnt = std::clamp(std::stoi(val), 1, 16);
						meta.heatmap_gradient.stops.resize(cnt);
					} catch (...) {}
				} else {
					for (int i = 0; i < 16; ++i) {
						std::string prefix = "stop" + std::to_string(i) + "_";
						if (key.rfind(prefix, 0) == 0) {
							if (meta.heatmap_gradient.stops.size() <= static_cast<size_t>(i)) {
								meta.heatmap_gradient.stops.resize(i + 1);
							}
							std::string field = key.substr(prefix.length());
							if (field == "pos") {
								try {
									meta.heatmap_gradient.stops[i].pos = std::stof(val);
								} catch (...) {}
							} else if (field == "color") {
								std::stringstream ss(val);
								std::string item;
								int c_idx = 0;
								while (std::getline(ss, item, ',') && c_idx < 4) {
									try {
										meta.heatmap_gradient.stops[i].color[c_idx++] = std::stof(trim(item));
									} catch (...) {}
								}
							}
						}
					}
				}
			} else {
				try {
					if (key == "author") {
						meta.author = val;
					} else if (key == "description") {
						meta.description = val;
					} else if (key == "version") {
						meta.version = static_cast<uint32_t>(std::stoul(val));
					} else if (key == "width") {
						meta.width = static_cast<uint32_t>(std::stoul(val));
					} else if (key == "height") {
						meta.height = static_cast<uint32_t>(std::stoul(val));
					} else if (key == "target_fps") {
						meta.target_fps = static_cast<uint32_t>(std::stoul(val));
					} else if (key == "processing_mode") {
						meta.processing_mode = std::stoi(val);
					} else if (key == "prevent_downclock") {
						meta.prevent_downclock = (val == "true" || val == "1");
					}
				} catch (...) {}
			}
		}
	}

	for (auto& attr : meta.attributes) {
		if (attr.min_val > attr.max_val) {
			std::swap(attr.min_val, attr.max_val);
		}
		attr.default_val = std::clamp(attr.default_val, attr.min_val, attr.max_val);
	}

	if (!meta.attributes.empty()) {
		float a_min = static_cast<float>(meta.attributes[0].min_val);
		float a_max = static_cast<float>(meta.attributes[0].max_val);
		if (a_max <= a_min) {
			a_max = a_min + 1.0f;
		}
		if (meta.heatmap_gradient.stops.empty()) {
			meta.heatmap_gradient = ColorGradient::default_heatmap(a_min, a_max);
		} else {
			bool all_01 = true;
			for (const auto& s : meta.heatmap_gradient.stops) {
				if (s.pos < 0.0f || s.pos > 1.0f) {
					all_01 = false;
					break;
				}
			}
			if (all_01 && (a_max - a_min) > 1.5f) {
				for (auto& s : meta.heatmap_gradient.stops) {
					s.pos = a_min + s.pos * (a_max - a_min);
				}
			}
		}
	}

	meta.heatmap_gradient.sort_stops();
	return meta;
}

void SetManager::save_set_metadata(const std::string& name, const SetMetadata& metadata) {
	std::string set_dir = SETS_DIRECTORY + name;
	fs::create_directories(set_dir);
	std::string cfg_path = set_dir + "/set_config.ini";

	bool has_meta_overrides = !metadata.author.empty() || !metadata.description.empty() || metadata.width > 0 ||
							  metadata.height > 0 || metadata.target_fps > 0 || metadata.processing_mode >= 0 ||
							  !metadata.prevent_downclock || metadata.version > 1;
	bool has_shortcuts = !metadata.shortcuts.empty();
	bool has_workshop =
		!metadata.workshop_id.empty() || !metadata.workshop_hash.empty() || !metadata.forked_from_id.empty();
	bool has_attributes = !metadata.attributes.empty();
	bool has_heatmap = !metadata.attributes.empty() && !metadata.heatmap_gradient.stops.empty();

	if (!has_meta_overrides && !has_shortcuts && !has_workshop && !has_attributes && !has_heatmap) {
		if (fs::exists(cfg_path)) {
			std::error_code ec;
			fs::remove(cfg_path, ec);
		}
		return;
	}

	std::ofstream file(cfg_path);
	if (!file.is_open()) {
		return;
	}

	if (!metadata.author.empty()) {
		file << "author = " << metadata.author << "\n";
	}
	if (!metadata.description.empty()) {
		file << "description = " << metadata.description << "\n";
	}
	if (metadata.version > 0) {
		file << "version = " << metadata.version << "\n";
	}
	if (metadata.width > 0) {
		file << "width = " << metadata.width << "\n";
	}
	if (metadata.height > 0) {
		file << "height = " << metadata.height << "\n";
	}
	if (metadata.target_fps > 0) {
		file << "target_fps = " << metadata.target_fps << "\n";
	}
	if (metadata.processing_mode >= 0) {
		file << "processing_mode = " << metadata.processing_mode << "\n";
	}
	if (!metadata.prevent_downclock) {
		file << "prevent_downclock = false\n";
	}

	if (has_shortcuts) {
		if (has_meta_overrides) {
			file << "\n";
		}
		file << "[Shortcuts]\n";
		for (const auto& [mat_name, sc] : metadata.shortcuts) {
			if (!sc.empty()) {
				file << mat_name << " = " << sc << "\n";
			}
		}
	}

	if (has_workshop) {
		file << "\n[Workshop]\n";
		if (!metadata.workshop_id.empty()) {
			file << "workshop_id = " << metadata.workshop_id << "\n";
		}
		if (!metadata.workshop_hash.empty()) {
			file << "workshop_hash = " << metadata.workshop_hash << "\n";
		}
		if (!metadata.forked_from_id.empty()) {
			file << "forked_from_id = " << metadata.forked_from_id << "\n";
		}
		if (!metadata.forked_from_author.empty()) {
			file << "forked_from_author = " << metadata.forked_from_author << "\n";
		}
		if (metadata.forked_from_version > 0) {
			file << "forked_from_version = " << metadata.forked_from_version << "\n";
		}
	}

	if (has_attributes) {
		file << "\n[Attributes]\n";
		file << "count = " << metadata.attributes.size() << "\n";
		for (size_t i = 0; i < metadata.attributes.size(); ++i) {
			const auto& attr = metadata.attributes[i];
			file << "attr" << i << "_name = " << attr.name << "\n";
			file << "attr" << i << "_type = " << (attr.type == AttributeType::Float ? "float" : "int") << "\n";
			if (attr.type == AttributeType::Int) {
				file << "attr" << i << "_min = " << static_cast<int64_t>(std::round(attr.min_val)) << "\n";
				file << "attr" << i << "_max = " << static_cast<int64_t>(std::round(attr.max_val)) << "\n";
				file << "attr" << i << "_default = " << static_cast<int64_t>(std::round(attr.default_val)) << "\n";
			} else {
				file << "attr" << i << "_min = " << attr.min_val << "\n";
				file << "attr" << i << "_max = " << attr.max_val << "\n";
				file << "attr" << i << "_default = " << attr.default_val << "\n";
			}
		}
	}

	if (has_heatmap) {
		file << "\n[Heatmap]\n";
		file << "stop_count = " << metadata.heatmap_gradient.stops.size() << "\n";
		for (size_t i = 0; i < metadata.heatmap_gradient.stops.size(); ++i) {
			const auto& s = metadata.heatmap_gradient.stops[i];
			file << "stop" << i << "_pos = " << s.pos << "\n";
			file << "stop" << i << "_color = " << s.color[0] << "," << s.color[1] << "," << s.color[2] << "," << s.color[3] << "\n";
		}
	}
}

void SetManager::fork_set(const std::string& source_name, const std::string& new_name) {
	copy_set(source_name, new_name);
	SetMetadata src_meta = load_set_metadata(source_name);
	SetMetadata new_meta = load_set_metadata(new_name);

	if (!src_meta.workshop_id.empty()) {
		new_meta.forked_from_id = src_meta.workshop_id;
		new_meta.forked_from_author = src_meta.author.empty() ? "Community" : src_meta.author;
		new_meta.forked_from_version = src_meta.version;
	} else if (!src_meta.forked_from_id.empty()) {
		new_meta.forked_from_id = src_meta.forked_from_id;
		new_meta.forked_from_author = src_meta.forked_from_author;
		new_meta.forked_from_version = src_meta.forked_from_version;
	}
	new_meta.name = new_name;
	new_meta.workshop_id = "";
	new_meta.workshop_hash = "";
	new_meta.is_online = false;
	new_meta.version = 1;
	save_set_metadata(new_name, new_meta);
}

std::string SetManager::get_current_set() { return current_set_name; }

const SetMetadata& SetManager::get_current_metadata() { return current_metadata; }

void SetManager::set_current_set(const std::string& name) {
	Window::set_cursor_wait(true);
	current_set_name = name;
	current_metadata = load_set_metadata(name);
	Grid::update_attr_layout();
	fs::create_directories(SETS_DIRECTORY + name);
	MaterialManager::load_all_materials(SETS_DIRECTORY + name);

	if (current_metadata.width > 0 && current_metadata.height > 0) {
		if (current_metadata.width != Grid::get_width() || current_metadata.height != Grid::get_height()) {
			Grid::resize(current_metadata.width, current_metadata.height, false);
		}
	}

	Grid::clear();
	UndoManager::init();
	UI::clear_clipboard();
	UI::deselect();
	Window::set_cursor_wait(false);
}

void SetManager::create_new_empty_set(const std::string& name) {
	std::string set_path = SETS_DIRECTORY + name;
	fs::create_directories(set_path);

	SetMetadata meta;
	meta.name = name;
	save_set_metadata(name, meta);

	current_set_name = name;
	current_metadata = meta;
	MaterialManager::load_all_materials(set_path);
	Grid::clear();
	UndoManager::init();
	UI::clear_clipboard();
	UI::deselect();
}

void SetManager::copy_set(const std::string& source_name, const std::string& new_name) {
	std::string src_path = SETS_DIRECTORY + source_name;
	std::string dst_path = SETS_DIRECTORY + new_name;

	if (fs::exists(dst_path)) {
		return;
	}

	if (source_name == current_set_name) {
		save_set_metadata(source_name, current_metadata);
		MaterialManager::save_all_materials(src_path);
	}

	if (fs::exists(src_path)) {
		try {
			fs::copy(src_path, dst_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
		} catch (...) {}
	} else {
		fs::create_directories(dst_path);
	}

	SetMetadata meta = load_set_metadata(source_name);
	meta.name = new_name;
	save_set_metadata(new_name, meta);

	set_current_set(new_name);
}

bool SetManager::rename_set(const std::string& old_name, const std::string& new_name) {
	std::string old_path = SETS_DIRECTORY + old_name;
	std::string new_path = SETS_DIRECTORY + new_name;

	if (!fs::exists(old_path) || fs::exists(new_path)) {
		return false;
	}

	try {
		fs::rename(old_path, new_path);
	} catch (...) {
		return false;
	}

	if (current_set_name == old_name) {
		current_set_name = new_name;
		current_metadata.name = new_name;
		save_set_metadata(new_name, current_metadata);
		MaterialManager::save_all_materials(SETS_DIRECTORY + new_name);
	} else {
		SetMetadata meta = load_set_metadata(new_name);
		meta.name = new_name;
		save_set_metadata(new_name, meta);
	}

	return true;
}

void SetManager::delete_set(const std::string& name) {
	std::string set_path = SETS_DIRECTORY + name;
	if (fs::exists(set_path)) {
		try {
			fs::remove_all(set_path);
		} catch (...) {}
	}

	if (current_set_name == name) {
		auto remaining_sets = get_sets();
		if (!remaining_sets.empty()) {
			set_current_set(remaining_sets[0]);
		} else {
			create_new_empty_set("new_set");
		}
	}
}

void SetManager::update_current_metadata(const SetMetadata& metadata) {
	current_metadata = metadata;
	for (auto& attr : current_metadata.attributes) {
		if (attr.min_val > attr.max_val) {
			std::swap(attr.min_val, attr.max_val);
		}
		attr.default_val = std::clamp(attr.default_val, attr.min_val, attr.max_val);
	}
	Grid::update_attr_layout();
	save_set_metadata(current_set_name, current_metadata);
}

std::string SetManager::compute_set_hash(const std::string& set_name) {
	std::string set_dir = SETS_DIRECTORY + set_name;
	if (!fs::exists(set_dir) || !fs::is_directory(set_dir)) {
		return "";
	}

	std::vector<std::string> filenames;
	for (const auto& entry : fs::directory_iterator(set_dir)) {
		if (entry.is_regular_file()) {
			std::string fname = entry.path().filename().string();
			std::string ext = entry.path().extension().string();
			if (ext == ".mat" && fname != "set_config.ini") {
				filenames.push_back(fname);
			}
		}
	}
	std::sort(filenames.begin(), filenames.end());

	std::vector<uint8_t> combined_bytes;
	for (const auto& fname : filenames) {
		std::string fpath = set_dir;
		fpath += '/';
		fpath += fname;
		for (char c : fname)
			combined_bytes.push_back(static_cast<uint8_t>(c));
		combined_bytes.push_back(0);

		std::ifstream in(fpath, std::ios::binary);
		if (in.is_open()) {
			std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			for (char c : content) {
				if (c != '\r') {
					combined_bytes.push_back(static_cast<uint8_t>(c));
				}
			}
		}
	}

	if (combined_bytes.empty()) {
		return "";
	}

	return sha256(combined_bytes.data(), combined_bytes.size());
}

bool SetManager::is_set_online(const std::string& set_name) {
	if (online_set_cache.find(set_name) != online_set_cache.end()) {
		return online_set_cache[set_name];
	}
	SetMetadata m = load_set_metadata(set_name);
	return !m.workshop_id.empty();
}

void SetManager::mark_set_online(const std::string& set_name, bool online) { online_set_cache[set_name] = online; }

void SetManager::set_workshop_info(const std::string& set_name, const std::string& workshop_id,
								   const std::string& workshop_hash, uint32_t version, const std::string& author) {
	SetMetadata m = load_set_metadata(set_name);
	m.workshop_id = workshop_id;
	m.workshop_hash = workshop_hash;
	if (version > 0) {
		m.version = version;
	}
	if (!author.empty()) {
		m.author = author;
	}
	m.is_online = true;
	online_set_cache[set_name] = true;
	save_set_metadata(set_name, m);
	if (current_set_name == set_name) {
		current_metadata = m;
	}
}

void SetManager::clear_workshop_info(const std::string& set_name) {
	SetMetadata m = load_set_metadata(set_name);
	m.workshop_id = "";
	m.workshop_hash = "";
	m.is_online = false;
	online_set_cache[set_name] = false;
	save_set_metadata(set_name, m);
	if (current_set_name == set_name) {
		current_metadata = m;
	}
	WorkshopCache::remove_transient(std::string(SETS_DIRECTORY) + set_name);
}

std::string SetManager::get_current_material_shortcut(const std::string& mat_name) {
	auto it = current_metadata.shortcuts.find(mat_name);
	if (it != current_metadata.shortcuts.end()) {
		return it->second;
	}
	return "";
}

void SetManager::set_current_material_shortcut(const std::string& mat_name, const std::string& key_combo) {
	if (key_combo.empty()) {
		current_metadata.shortcuts.erase(mat_name);
	} else {
		current_metadata.shortcuts[mat_name] = key_combo;
	}
	save_set_metadata(current_set_name, current_metadata);
}

void SetManager::remove_current_material_shortcut(const std::string& mat_name) {
	current_metadata.shortcuts.erase(mat_name);
	save_set_metadata(current_set_name, current_metadata);
}

void SetManager::clear_current_material_shortcuts() {
	current_metadata.shortcuts.clear();
	save_set_metadata(current_set_name, current_metadata);
}