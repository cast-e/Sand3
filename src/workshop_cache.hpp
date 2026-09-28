#pragma once

#include <string>
#include <unordered_set>

class WorkshopCache {
public:
	WorkshopCache() = delete;

	static void init();
	static std::string get_cache_root();
	static std::string get_cache_path(const std::string& item_id, const std::string& type, const std::string& filename);

	static void register_transient(const std::string& path);
	static bool is_transient(const std::string& path);
	static void remove_transient(const std::string& path);
	static void remove_transient_by_id(const std::string& item_id);
	static const std::unordered_set<std::string>& get_transient_paths();
	static bool promote_to_local(const std::string& item_id, const std::string& type, const std::string& target_name,
								 const std::string& current_set = "", const std::string& workshop_hash = "");
	static void purge_transient_cache();
	static void cleanup();

private:
	static std::unordered_set<std::string> transient_paths;
};