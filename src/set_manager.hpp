#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SetMetadata {
	std::string name;
	std::string author;
	std::string description;
	uint32_t width = 0;           // Recommended canvas width (0 = inherit/use global)
	uint32_t height = 0;          // Recommended canvas height (0 = inherit/use global)
	uint32_t target_fps = 0;      // Target FPS for this set (0 = inherit/use global)
	int processing_mode = -1;     // Recommended mode (-1 = inherit, 0 = CPU, 1 = GPU)
	bool prevent_downclock = true;
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
	static bool rename_set(const std::string& old_name, const std::string& new_name);
	static void delete_set(const std::string& name);

	static void update_current_metadata(const SetMetadata& metadata);

private:
	static std::string current_set_name;
	static SetMetadata current_metadata;
};