#pragma once

#include <stdint.h>

#include <array>
#include <bitset>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

enum class CompareOp : uint8_t { None = 0, Equal, NotEqual, Less, LessOrEqual, Greater, GreaterOrEqual };

enum class CompareTarget : uint8_t { CenterCell = 0, Constant };

struct AttributeCondition {
	uint8_t attr_index = 0;
	CompareOp op = CompareOp::None;
	CompareTarget target = CompareTarget::CenterCell;
	double const_value = 0.0;

	bool operator==(const AttributeCondition& other) const {
		return attr_index == other.attr_index && op == other.op && target == other.target &&
			   const_value == other.const_value;
	}
};

enum class ExprOpType : uint8_t {
	PushConst = 1,
	LoadSelf = 2,
	LoadCenter = 3,
	LoadCellAttr = 4,
	LoadMin = 5,
	LoadMax = 6,
	Add = 7,
	Sub = 8,
	Mul = 9,
	Div = 10,
	Mod = 11,
	Pow = 12,
	Neg = 13,
	Abs = 14,
	Min = 15,
	Max = 16,
	Clamp = 17,
	Floor = 18,
	Ceil = 19,
	Round = 20
};

struct ExprOp {
	ExprOpType type = ExprOpType::PushConst;
	int8_t dx = 0;
	int8_t dy = 0;
	uint8_t attr = 0;
	float val = 0.0f;

	bool operator==(const ExprOp& other) const {
		return type == other.type && dx == other.dx && dy == other.dy && attr == other.attr && val == other.val;
	}
};

struct ThenCellAction {
	std::array<std::string, 3> attr_exprs = {"", "", ""};
	bool updated = true;

	bool operator==(const ThenCellAction& other) const {
		return attr_exprs == other.attr_exprs && updated == other.updated;
	}
	bool operator!=(const ThenCellAction& other) const { return !(*this == other); }
};

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
	std::vector<std::vector<AttributeCondition>> when_attrs;
	std::vector<uint8_t> then;
	std::vector<ThenCellAction> then_actions;
	SymmetryFlags symmetry;
	float chance = 100.0f;
	bool enabled = true;
	bool multi_apply = false;
	bool continue_to_next = false;

	RuleDefinition() { init_default(0); }

	void init_default(uint8_t center_id = 0) {
		min_dx = -1;
		max_dx = 1;
		min_dy = -1;
		max_dy = 1;
		when.assign(9, std::vector<uint8_t>{});
		when_attrs.assign(9, std::vector<AttributeCondition>{});
		then.assign(9, 255);
		then_actions.assign(9, ThenCellAction{});
		when[4] = {center_id};
		chance = 100.0f;
		symmetry = SymmetryFlags{};
		enabled = true;
		multi_apply = false;
		continue_to_next = false;
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

#include "gradient.hpp"

struct AttributeColorModifier {
	bool enabled = false;
	ColorGradient gradient;
};

struct MaterialDefinition {
	std::string name;
	uint8_t id = 0;
	uint8_t inherits_from = 255;
	std::array<uint8_t, 3> color = {0, 0, 0};
	std::array<double, 3> default_attributes = {0.0, 0.0, 0.0};
	std::array<AttributeColorModifier, 3> attr_color_modifiers = {};
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
	std::vector<AttributeCondition> attr_conditions;
};

struct OutputAction {
	int offset = 0;
	int8_t dx = 0;
	int8_t dy = 0;
	uint8_t target_material = 255;
	std::array<std::vector<ExprOp>, 3> attr_expr_ops = {};
	bool updated = true;
};

struct CompiledRuleVariant {
	std::vector<ConditionCheck> conditions;
	std::vector<OutputAction> actions;
	int8_t min_dx = 0;
	int8_t max_dx = 0;
	int8_t min_dy = 0;
	int8_t max_dy = 0;
	bool has_attribute_logic = false;
};

struct CompiledRule {
	std::vector<CompiledRuleVariant> variants;
	float chance = 1.0f;
	bool multi_apply = false;
	bool continue_to_next = false;
};

struct RuntimeColorMod {
	uint8_t attr_index = 0;
	float attr_min = 0.0f;
	float attr_max = 1.0f;
	ColorGradient gradient;
};

struct RuntimeMaterial {
	uint8_t id = 0;
	uint32_t packed_color = 0;
	std::vector<CompiledRule> rules;
	bool has_color_modifiers = false;
	std::array<uint8_t, 3> base_color = {0, 0, 0};
	std::vector<RuntimeColorMod> color_modifiers;
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

bool parse_and_compile_expr(const std::string& expr, uint8_t target_attr, std::vector<ExprOp>& out_ops,
							std::string* error_out = nullptr, int cell_dx = 0, int cell_dy = 0);
std::string transform_expr_coords(const std::string& expr, int mode /* 0: flip_x, 1: flip_y, 2: rot_cw, 3: rot_ccw */);
std::string shift_expr_coords(const std::string& expr, int delta_x, int delta_y);
float eval_expr(const std::vector<ExprOp>& ops, float self_val, float center_val, float min_val, float max_val,
				uint8_t cur_attr, const std::function<float(int8_t dx, int8_t dy, uint8_t attr)>& get_cell_attr);
