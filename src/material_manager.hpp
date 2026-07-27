#pragma once

#include <stdint.h>

#include <array>
#include <bitset>
#include <string>
#include <string_view>
#include <vector>

#include "const.hpp"

struct SymmetryFlags {
	bool flip_x = false;
	bool flip_y = false;
	bool rotate = false;
};

struct RuleDefinition {
	std::array<std::vector<uint8_t>, NEIGHBOR_COUNT> when;
	std::array<uint8_t, NEIGHBOR_COUNT> then;
	SymmetryFlags symmetry;
	float chance = 1.0f;
	bool is_inherited = false;
};

struct RuleRef {
	bool is_inherited = false;
	size_t index = 0;

	bool operator==(const RuleRef& other) const { return is_inherited == other.is_inherited && index == other.index; }
};

struct MaterialDefinition {
	std::string name;
	uint8_t id = 0;
	uint8_t inherits_from = 255;
	std::array<uint8_t, 3> color = {0, 0, 0};
	std::vector<RuleDefinition> rules;
	std::vector<RuleRef> rule_order;
	size_t last_synced_parent_rule_count = 0;

	void sync_rule_order();
	RuleDefinition get_effective_rule(size_t order_idx) const;
};

struct CompiledRuleVariant {
	std::array<std::bitset<256>, NEIGHBOR_COUNT> when;
	std::array<uint8_t, NEIGHBOR_COUNT> then;
};

struct CompiledRule {
	std::vector<CompiledRuleVariant> variants;
	float chance = 1.0f;
};

struct RuntimeMaterial {
	uint8_t id = 0;
	uint32_t packed_color = 0;
	std::vector<CompiledRule> rules;
};

class MaterialManager {
public:
	MaterialManager() = delete;

	static void load_all_materials(std::string_view directory_path);
	static void save_all_materials(std::string_view directory_path);

	static uint8_t add_material(const MaterialDefinition& mat);
	static void update_material_name(uint8_t index, std::string_view name);
	static void update_material_rules(uint8_t index, const MaterialDefinition& mat);
	static void update_material_color(uint8_t index, const MaterialDefinition& mat);
	static void set_material_inheritance(uint8_t index, uint8_t parent_id);
	static void remove_material(uint8_t index);

	static void rebuild_compiled_rules();
	static uint8_t get_unused_id();

	static void replace_self_references(RuleDefinition& rule, uint8_t old_id, uint8_t new_id);
	static void replace_self_references_in_material(MaterialDefinition& mat, uint8_t old_id, uint8_t new_id);
	static void sync_inherited_rules(uint8_t index);

	static bool is_valid_name(std::string_view name);
	static uint32_t pack_color(const std::array<uint8_t, 3>& color);

	static const MaterialDefinition& get_material(uint8_t id);
	static const RuntimeMaterial& get_runtime_material(uint8_t id);
	static uint8_t get_material_count();
	static std::vector<MaterialDefinition>& get_materials();

private:
	static std::vector<MaterialDefinition> materials;
	static std::array<RuntimeMaterial, 256> runtime_materials;
	static std::array<uint8_t, 256> material_by_id;
	static MaterialDefinition default_empty;
	static RuntimeMaterial default_runtime_empty;
};