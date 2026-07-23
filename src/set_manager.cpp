#include "set_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "grid.hpp"
#include "material_manager.hpp"

namespace fs = std::filesystem;

std::string SetManager::current_set_name = "";
SetMetadata SetManager::current_metadata{};

static std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

std::vector<std::string> SetManager::get_sets() {
	std::vector<std::string> sets;
	fs::create_directories(SETS_DIRECTORY);

	for (const auto& entry : fs::directory_iterator(SETS_DIRECTORY)) {
		if (entry.is_directory()) {
			sets.push_back(entry.path().filename().string());
		}
	}

	if (sets.empty()) {
		create_new_empty_set("new_set");
		sets.push_back("new_set");
	}

	std::sort(sets.begin(), sets.end());

	return sets;
}

SetMetadata SetManager::load_set_metadata(const std::string& set_name) {
	SetMetadata meta;
	meta.name = set_name;
	meta.author = "";
	meta.description = "";

	std::string cfg_path = SETS_DIRECTORY + set_name + "/set.cfg";
	std::ifstream file(cfg_path);
	if (!file.is_open()) {
		return meta;
	}

	std::string line;
	while (std::getline(file, line)) {
		std::string trimmed = trim(line);
		if (trimmed.empty() || trimmed[0] == '#' || trimmed.rfind("//", 0) == 0) {
			continue;
		}
		size_t eq_pos = trimmed.find('=');
		if (eq_pos != std::string::npos) {
			std::string key = trim(trimmed.substr(0, eq_pos));
			std::string val = trim(trimmed.substr(eq_pos + 1));

			if (key == "author") {
				meta.author = val;
			} else if (key == "description") {
				meta.description = val;
			}
		}
	}

	return meta;
}

void SetManager::save_set_metadata(const std::string& set_name, const SetMetadata& metadata) {
	std::string set_dir = SETS_DIRECTORY + set_name;
	fs::create_directories(set_dir);
	std::string cfg_path = set_dir + "/set.cfg";

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
}

std::string SetManager::get_current_set() { return current_set_name; }

const SetMetadata& SetManager::get_current_metadata() { return current_metadata; }

void SetManager::set_current_set(const std::string& set_name) {
	current_set_name = set_name;
	current_metadata = load_set_metadata(set_name);
	MaterialManager::load_all_materials(SETS_DIRECTORY + set_name);
	Grid::clear();
}

void SetManager::create_new_empty_set(const std::string& set_name) {
	std::string set_path = SETS_DIRECTORY + set_name;
	fs::create_directories(set_path);

	SetMetadata meta;
	meta.name = set_name;
	save_set_metadata(set_name, meta);

	current_set_name = set_name;
	current_metadata = meta;
	MaterialManager::get_materials().clear();
	MaterialManager::rebuild_compiled_rules();
	Grid::clear();
}

void SetManager::copy_set(const std::string& source_set_name, const std::string& new_set_name) {
	std::string src_path = SETS_DIRECTORY + source_set_name;
	std::string dst_path = SETS_DIRECTORY + new_set_name;

	if (fs::exists(src_path)) {
		try {
			fs::copy(src_path, dst_path, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
		} catch (...) {}
	} else {
		fs::create_directories(dst_path);
	}

	SetMetadata meta = load_set_metadata(source_set_name);
	meta.name = new_set_name;
	save_set_metadata(new_set_name, meta);

	set_current_set(new_set_name);
}

bool SetManager::rename_set(const std::string& old_set_name, const std::string& new_set_name) {
	if (old_set_name.empty() || new_set_name.empty() || old_set_name == new_set_name) {
		return false;
	}

	std::string old_path = SETS_DIRECTORY + old_set_name;
	std::string new_path = SETS_DIRECTORY + new_set_name;

	if (!fs::exists(old_path) || fs::exists(new_path)) {
		return false;
	}

	try {
		fs::rename(old_path, new_path);
	} catch (...) {
		return false;
	}

	if (current_set_name == old_set_name) {
		current_set_name = new_set_name;
		current_metadata.name = new_set_name;
		save_set_metadata(new_set_name, current_metadata);
		MaterialManager::load_all_materials(SETS_DIRECTORY + new_set_name);
	} else {
		SetMetadata meta = load_set_metadata(new_set_name);
		meta.name = new_set_name;
		save_set_metadata(new_set_name, meta);
	}

	return true;
}

void SetManager::delete_set(const std::string& set_name) {
	std::string set_path = SETS_DIRECTORY + set_name;
	if (fs::exists(set_path)) {
		try {
			fs::remove_all(set_path);
		} catch (...) {}
	}

	auto remaining_sets = get_sets();
	if (!remaining_sets.empty()) {
		set_current_set(remaining_sets[0]);
	} else {
		create_new_empty_set("new_set");
	}
}

void SetManager::update_current_metadata(const SetMetadata& metadata) {
	current_metadata = metadata;
	save_set_metadata(current_set_name, current_metadata);
}
