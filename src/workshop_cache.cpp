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

void WorkshopCache::remove_transient(const std::string& path) {
	transient_paths.erase(path);
}

void WorkshopCache::remove_transient_by_id(const std::string& item_id) {
	if (item_id.empty()) {
		return;
	}
	std::vector<std::string> to_remove;
	for (const auto& p : transient_paths) {
		if (p.find(item_id) != std::string::npos) {
			to_remove.push_back(p);
			continue;
		}
		std::string ini_path = p + ".ini";
		if (!fs::exists(ini_path) && fs::is_directory(p)) {
			ini_path = p + "/set_config.ini";
		}
		if (fs::exists(ini_path)) {
			std::ifstream inif(ini_path);
			std::string line;
			while (std::getline(inif, line)) {
				if (line.find("workshop_id") != std::string::npos && line.find(item_id) != std::string::npos) {
					to_remove.push_back(p);
					break;
				}
			}
		}
	}
	for (const auto& p : to_remove) {
		transient_paths.erase(p);
		try {
			if (fs::exists(p)) {
				fs::remove_all(p);
			}
			std::string ini = p + ".ini";
			if (fs::exists(ini)) {
				fs::remove(ini);
			}
		} catch (...) {}
	}
}

const std::unordered_set<std::string>& WorkshopCache::get_transient_paths() {
	return transient_paths;
}

bool WorkshopCache::promote_to_local(const std::string& item_id, const std::string& type,
									 const std::string& target_name, const std::string& current_set,
									 const std::string& workshop_hash) {
	try {
		std::string cset = current_set.empty() ? SetManager::get_current_set() : current_set;

		if (type == "set") {
			std::string dest_dir = std::string(SETS_DIRECTORY) + target_name;
			if (fs::exists(dest_dir)) {
				transient_paths.erase(dest_dir);
			} else {
				std::string src_path;
				for (const auto& p : transient_paths) {
					if ((!item_id.empty() && p.find(item_id) != std::string::npos) ||
						(!target_name.empty() && p.find(target_name) != std::string::npos)) {
						src_path = p;
						break;
					}
				}
				if (src_path.empty()) {
					std::string cached_zip = get_cache_path(item_id, "set", "set.zip");
					if (fs::exists(cached_zip)) {
						src_path = cached_zip;
					}
				}
				if (!src_path.empty() && fs::exists(src_path)) {
					fs::create_directories(dest_dir);
					if (fs::is_directory(src_path)) {
						fs::copy(src_path, dest_dir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
					} else {
						ZipUtil::extract_zip(src_path, dest_dir);
					}
					transient_paths.erase(src_path);
				} else {
					return false;
				}
			}

			std::vector<std::string> to_remove;
			for (const auto& p : transient_paths) {
				if (p == dest_dir || (!item_id.empty() && p.find(item_id) != std::string::npos)) {
					to_remove.push_back(p);
				}
			}
			for (const auto& p : to_remove) {
				transient_paths.erase(p);
			}

			SetManager::set_workshop_info(target_name, item_id, workshop_hash);
			SetManager::mark_set_online(target_name, true);
			return true;
		} else if (type == "save") {
			std::string fn = target_name;
			if (fn.length() < 5 || fn.substr(fn.length() - 5) != ".save") {
				fn += ".save";
			}
			std::string dest_dir = SaveManager::get_saves_directory(cset);
			fs::create_directories(dest_dir);
			std::string dest_file = dest_dir + fn;

			if (fs::exists(dest_file)) {
				transient_paths.erase(dest_file);
			} else {
				std::string src_path;
				for (const auto& p : transient_paths) {
					if ((!item_id.empty() && p.find(item_id) != std::string::npos) ||
						(!fn.empty() && p.find(fn) != std::string::npos)) {
						src_path = p;
						break;
					}
				}
				if (src_path.empty()) {
					std::string cached_file = get_cache_path(item_id, "save", "item.save");
					if (fs::exists(cached_file)) {
						src_path = cached_file;
					}
				}
				if (!src_path.empty() && fs::exists(src_path)) {
					fs::copy_file(src_path, dest_file, fs::copy_options::overwrite_existing);
					transient_paths.erase(src_path);
				} else {
					return false;
				}
			}

			std::vector<std::string> to_remove;
			for (const auto& p : transient_paths) {
				if (p == dest_file || (!item_id.empty() && p.find(item_id) != std::string::npos)) {
					to_remove.push_back(p);
				}
			}
			for (const auto& p : to_remove) {
				transient_paths.erase(p);
			}

			SaveManager::set_save_workshop_info(target_name, cset, item_id, workshop_hash, "", 1);
			return true;
		} else if (type == "stamp") {
			std::string fn = target_name;
			if (fn.length() < 6 || fn.substr(fn.length() - 6) != ".stamp") {
				fn += ".stamp";
			}
			std::string dest_dir = SaveManager::get_saves_directory(cset);
			fs::create_directories(dest_dir);
			std::string dest_file = dest_dir + fn;

			if (fs::exists(dest_file)) {
				transient_paths.erase(dest_file);
			} else {
				std::string src_path;
				for (const auto& p : transient_paths) {
					if ((!item_id.empty() && p.find(item_id) != std::string::npos) ||
						(!fn.empty() && p.find(fn) != std::string::npos)) {
						src_path = p;
						break;
					}
				}
				if (src_path.empty()) {
					std::string cached_file = get_cache_path(item_id, "stamp", "item.stamp");
					if (fs::exists(cached_file)) {
						src_path = cached_file;
					}
				}
				if (!src_path.empty() && fs::exists(src_path)) {
					fs::copy_file(src_path, dest_file, fs::copy_options::overwrite_existing);
					transient_paths.erase(src_path);
				} else {
					return false;
				}
			}

			std::vector<std::string> to_remove;
			for (const auto& p : transient_paths) {
				if (p == dest_file || (!item_id.empty() && p.find(item_id) != std::string::npos)) {
					to_remove.push_back(p);
				}
			}
			for (const auto& p : to_remove) {
				transient_paths.erase(p);
			}

			SaveManager::set_stamp_workshop_info(target_name, cset, item_id, workshop_hash, "", 1);
			return true;
		}

		return false;
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
			std::string ini = path + ".ini";
			if (fs::exists(ini)) {
				fs::remove(ini);
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

void WorkshopCache::cleanup() {
	try {
		purge_transient_cache();
		std::string root = get_cache_root();
		if (fs::exists(root)) {
			fs::remove_all(root);
		}
		if (fs::exists("./cache")) {
			fs::remove_all("./cache");
		}
	} catch (const std::exception& e) {
		std::cerr << "WorkshopCache::cleanup error: " << e.what() << std::endl;
	}
}