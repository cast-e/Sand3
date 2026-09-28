#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct SetMetadata {
	std::string name;
	std::string author;
	std::string description;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t target_fps = 0;
	int processing_mode = -1;
	bool prevent_downclock = true;
	uint32_t version = 1;
	std::string workshop_id;
	std::string workshop_hash;
	std::string forked_from_id;
	std::string forked_from_author;
	uint32_t forked_from_version = 0;
	bool is_online = false;
	std::unordered_map<std::string, std::string> shortcuts;
};

class SetManager {
public:
	SetManager() = delete;

	static std::vector<std::string> get_sets();
	static SetMetadata load_set_metadata(const std::string& name);
	static void save_set_metadata(const std::string& name, const SetMetadata& metadata);

	static std::string get_current_set();
	static const SetMetadata& get_current_metadata();

	static void set_current_set(const std::string& name);
	static void create_new_empty_set(const std::string& name);
	static void copy_set(const std::string& source_name, const std::string& new_name);
	static void fork_set(const std::string& source_name, const std::string& new_name);
	static bool rename_set(const std::string& old_name, const std::string& new_name);
	static void delete_set(const std::string& name);

	static void update_current_metadata(const SetMetadata& metadata);

	static std::string compute_set_hash(const std::string& set_name);
	static bool is_set_online(const std::string& set_name);
	static void mark_set_online(const std::string& set_name, bool online);
	static void set_workshop_info(const std::string& set_name, const std::string& workshop_id, const std::string& workshop_hash, uint32_t version = 1, const std::string& author = "");
	static void clear_workshop_info(const std::string& set_name);

	static std::string get_current_material_shortcut(const std::string& mat_name);
	static void set_current_material_shortcut(const std::string& mat_name, const std::string& key_combo);
	static void remove_current_material_shortcut(const std::string& mat_name);
	static void clear_current_material_shortcuts();

private:
	static std::string current_set_name;
	static SetMetadata current_metadata;
	static std::unordered_map<std::string, bool> online_set_cache;
};