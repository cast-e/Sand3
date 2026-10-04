#pragma once

#include <stdint.h>

#include <array>
#include <bitset>
#include <string>
#include <string_view>
#include <vector>

struct SymmetryFlags {
	bool flip_x = false;
	bool flip_y = false;
	bool rotate = false;
};

struct RuleDefinition {
	int8_t min_dx = -1;
	int8_t max_dx = 1;
	int8_t min_dy = -1;
	int8_t max_dy = 1;
	std::vector<std::vector<uint8_t>> when;
	std::vector<uint8_t> then;
	SymmetryFlags symmetry;
	float chance = 100.0f;

	RuleDefinition() { init_default(0); }

	void init_default(uint8_t center_id = 0) {
		min_dx = -1;
		max_dx = 1;
		min_dy = -1;
		max_dy = 1;
		when.assign(9, std::vector<uint8_t>{});
		then.assign(9, 255);
		when[4] = {center_id};
		chance = 100.0f;
		symmetry = SymmetryFlags{};
	}

	int width() const { return max_dx - min_dx + 1; }
	int height() const { return max_dy - min_dy + 1; }
	int total_cells() const { return width() * height(); }

	int get_cell_idx(int dx, int dy) const {
		if (dx < min_dx || dx > max_dx || dy < min_dy || dy > max_dy)
			return -1;
		return (dy - min_dy) * width() + (dx - min_dx);
	}
	int get_dx(int idx) const { return min_dx + (idx % width()); }
	int get_dy(int idx) const { return min_dy + (idx / width()); }
	int get_center_idx() const { return get_cell_idx(0, 0); }

	void expand_top();
	void shrink_top();
	void expand_bottom();
	void shrink_bottom();
	void expand_left();
	void shrink_left();
	void expand_right();
	void shrink_right();
	void flip_x();
	void flip_y();
	void rotate_cw();
	void rotate_ccw();
	void reset_to_3x3(uint8_t center_id);
};

struct RuleReference {
	bool is_inherited = false;
	size_t index = 0;

	bool operator==(const RuleReference& other) const {
		return is_inherited == other.is_inherited && index == other.index;
	}
};

struct MaterialDefinition {
	std::string name;
	uint8_t id = 0;
	uint8_t inherits_from = 255;
	std::array<uint8_t, 3> color = {0, 0, 0};
	std::vector<RuleDefinition> rules;
	std::vector<RuleReference> rule_order;
	size_t last_synced_parent_rule_count = 0;

	void sync_rule_order();
	RuleDefinition get_effective_rule(size_t order_idx) const;
};

struct ConditionCheck {
	int offset = 0;
	int8_t dx = 0;
	int8_t dy = 0;
	std::bitset<256> allowed;
};

struct OutputAction {
	int offset = 0;
	int8_t dx = 0;
	int8_t dy = 0;
	uint8_t target_material = 255;
};

struct CompiledRuleVariant {
	std::vector<ConditionCheck> conditions;
	std::vector<OutputAction> actions;
	int8_t min_dx = 0;
	int8_t max_dx = 0;
	int8_t min_dy = 0;
	int8_t max_dy = 0;
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
	static void update_material_name(uint8_t id, std::string_view name);
	static void update_material_rules(uint8_t id, const MaterialDefinition& mat);
	static void update_material_color(uint8_t id, const MaterialDefinition& mat);
	static void set_material_inheritance(uint8_t id, uint8_t parent_id);
	static void remove_material(uint8_t id);

	static void rebuild_compiled_rules();
	static uint8_t get_unused_id();

	static void replace_self_references(RuleDefinition& rule, uint8_t old_id, uint8_t new_id);
	static void replace_self_references_in_material(MaterialDefinition& mat, uint8_t old_id, uint8_t new_id);
	static void sync_inherited_rules(uint8_t id);

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