#include "workshop_cache.hpp"

#include <filesystem>
#include <iostream>

#include "const.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "zip_util.hpp"

namespace fs = std::filesystem;

std::unordered_set<std::string> WorkshopCache::transient_paths;

void WorkshopCache::init() {
	try {
		fs::create_directories(get_cache_root() + "sets/");
		fs::create_directories(get_cache_root() + "saves/");
		fs::create_directories(get_cache_root() + "stamps/");
		purge_transient_cache();
	} catch (const std::exception& e) {
		std::cerr << "WorkshopCache::init error: " << e.what() << std::endl;
	}
}

std::string WorkshopCache::get_cache_root() { return "./cache/workshop/"; }

std::string WorkshopCache::get_cache_path(const std::string& item_id, const std::string& type,
										  const std::string& filename) {
	std::string sub = "saves/";
	if (type == "set")
		sub = "sets/";
	else if (type == "stamp")
		sub = "stamps/";
	return get_cache_root() + sub + item_id + "_" + filename;
}

void WorkshopCache::register_transient(const std::string& path) {
	if (!path.empty()) {
		transient_paths.insert(path);
	}
}

bool WorkshopCache::is_transient(const std::string& path) {
	return transient_paths.find(path) != transient_paths.end();
}

bool WorkshopCache::promote_to_local(const std::string& item_id, const std::string& type,
									 const std::string& target_name, const std::string& current_set,
									 const std::string& workshop_hash) {
	try {
		std::string src_path;
		for (const auto& p : transient_paths) {
			if (p.find(item_id) != std::string::npos) {
				src_path = p;
				break;
			}
		}

		if (src_path.empty() || !fs::exists(src_path)) {
			return false;
		}

		if (type == "set") {
			std::string dest_dir = std::string(SETS_DIRECTORY) + target_name;
			fs::create_directories(dest_dir);
			if (fs::is_directory(src_path)) {
				fs::copy(src_path, dest_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
			} else if (src_path.size() >= 4 && src_path.substr(src_path.size() - 4) == ".zip") {
				ZipUtil::extract_zip(src_path, dest_dir);
			} else {
				fs::copy_file(src_path, dest_dir + "/set_config.ini", fs::copy_options::overwrite_existing);
			}
			SetManager::set_workshop_info(target_name, item_id, workshop_hash);
			SetManager::mark_set_online(target_name, true);
		} else if (type == "save") {
			std::string dest_dir =
				std::string(SETS_DIRECTORY) + (current_set.empty() ? "crystals" : current_set) + "/saves/";
			fs::create_directories(dest_dir);
			std::string dest_file = dest_dir + target_name + ".save";
			fs::copy_file(src_path, dest_file, fs::copy_options::overwrite_existing);
			SaveManager::set_save_workshop_info(target_name, current_set.empty() ? "crystals" : current_set, item_id,
												workshop_hash, "", 1);
		} else if (type == "stamp") {
			std::string dest_dir =
				std::string(SETS_DIRECTORY) + (current_set.empty() ? "crystals" : current_set) + "/stamps/";
			fs::create_directories(dest_dir);
			std::string dest_file = dest_dir + target_name + ".stamp";
			fs::copy_file(src_path, dest_file, fs::copy_options::overwrite_existing);
			SaveManager::set_stamp_workshop_info(target_name, current_set.empty() ? "crystals" : current_set, item_id,
												 workshop_hash, "", 1);
		}

		transient_paths.erase(src_path);
		return true;
	} catch (const std::exception& e) {
		std::cerr << "WorkshopCache::promote_to_local error: " << e.what() << std::endl;
		return false;
	}
}

void WorkshopCache::purge_transient_cache() {
	try {
		for (const auto& path : transient_paths) {
			if (fs::exists(path)) {
				fs::remove_all(path);
			}
		}
		transient_paths.clear();

		std::string root = get_cache_root();
		if (fs::exists(root)) {
			for (const auto& entry : fs::recursive_directory_iterator(root)) {
				if (entry.is_regular_file()) {
					fs::remove(entry.path());
				}
			}
		}
	} catch (const std::exception& e) {
		std::cerr << "WorkshopCache::purge_transient_cache error: " << e.what() << std::endl;
	}
}