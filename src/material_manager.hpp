#pragma once

#include <stdint.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "const.hpp"

struct Rule {
	std::array<unsigned char, NEIGHBOR_COUNT> when;
	std::array<unsigned char, NEIGHBOR_COUNT> then;
};

struct CompiledUserRule {
	std::vector<Rule> variants;
	unsigned char chance = 100;
};

struct UserRule {
	std::array<std::string, NEIGHBOR_COUNT> when;
	std::array<std::string, NEIGHBOR_COUNT> then;
	bool sym_x = false;
	bool sym_y = false;
	bool sym_rot = false;
	unsigned char chance = 100;
};

struct Material {
	std::string name;
	unsigned char id = 0;
	std::array<unsigned char, 3> color = {0, 0, 0};
	uint32_t packed_color = 0;
	std::vector<UserRule> user_rules;
	std::vector<CompiledUserRule> compiled_rules;
};

class MaterialManager {
public:
	MaterialManager() = delete;

	static void load_all_materials(std::string_view directory_path);
	static void save_all_materials(std::string_view directory_path);

	static void add_material(const Material& mat);
	static void edit_material(unsigned char id, const Material& mat);
	static void remove_material(unsigned char id);

	static void rebuild_compiled_rules();
	static unsigned char get_unused_id();

	static const Material& get_material(unsigned char id);
	static unsigned char get_material_count();
	static std::vector<Material>& get_materials();

private:
	static std::vector<Material> materials;
};