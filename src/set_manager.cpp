#include "set_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "grid.hpp"
#include "material_manager.hpp"
#include "sanitize.hpp"
#include "undo_manager.hpp"

namespace fs = std::filesystem;

std::string SetManager::current_set_name = "";
SetMetadata SetManager::current_metadata{};

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

SetMetadata SetManager::load_set_metadata(const std::string& name) {
	SetMetadata meta;
	meta.name = name;
	meta.author = "";
	meta.description = "";

	std::string cfg_path = SETS_DIRECTORY + name + "/set.cfg";
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

void SetManager::save_set_metadata(const std::string& name, const SetMetadata& metadata) {
	std::string set_dir = SETS_DIRECTORY + name;
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

void SetManager::set_current_set(const std::string& name) {
	current_set_name = name;
	current_metadata = load_set_metadata(name);
	fs::create_directories(SETS_DIRECTORY + name);
	MaterialManager::load_all_materials(SETS_DIRECTORY + name);
	Grid::clear();
	UndoManager::init();
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
	save_set_metadata(current_set_name, current_metadata);
}
