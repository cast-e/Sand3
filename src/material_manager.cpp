#include "material_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "grid.hpp"

namespace fs = std::filesystem;

std::vector<MaterialDefinition> MaterialManager::materials{};
std::array<RuntimeMaterial, 256> MaterialManager::runtime_materials{};
std::array<uint8_t, 256> MaterialManager::material_by_id{};
MaterialDefinition MaterialManager::default_empty{"empty", 0, 255, {64, 64, 64}};
RuntimeMaterial MaterialManager::default_runtime_empty{0, MaterialManager::pack_color({64, 64, 64}), {}};

uint32_t MaterialManager::pack_color(const std::array<uint8_t, 3>& color) {
	return (255u << 24) | (color[2] << 16) | (color[1] << 8) | color[0];
}

uint8_t MaterialManager::get_unused_id() {
	std::array<bool, 256> used{};
	used[0] = true;
	used[255] = true;

	for (const auto& m : materials) {
		if (m.id < 255) {
			used[m.id] = true;
		}
	}

	for (int i = 1; i < 255; ++i) {
		if (!used[i]) {
			return static_cast<uint8_t>(i);
		}
	}

	return 254;
}

bool MaterialManager::is_valid_name(std::string_view name) {
	if (name.empty()) {
		return false;
	}
	for (const auto& m : materials) {
		if (m.name == name) {
			return false;
		}
	}
	return true;
}

void MaterialDefinition::sync_rule_order() {
	std::vector<RuleReference> valid_refs;

	const auto* parent = &MaterialManager::get_material(inherits_from);

	size_t parent_rule_count = parent ? parent->rules.size() : 0;
	size_t custom_rule_count = rules.size();

	for (const auto& reference : rule_order) {
		if (reference.is_inherited && parent && reference.index < parent_rule_count &&
			std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
			valid_refs.push_back(reference);
		} else if (reference.index < custom_rule_count &&
				   std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
			valid_refs.push_back(reference);
		}
	}

	if (parent && parent_rule_count > last_synced_parent_rule_count) {
		for (size_t i = last_synced_parent_rule_count; i < parent_rule_count; ++i) {
			RuleReference reference{true, i};
			if (std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
				if (i <= valid_refs.size()) {
					valid_refs.insert(valid_refs.begin() + i, reference);
				} else {
					valid_refs.push_back(reference);
				}
			}
		}
		last_synced_parent_rule_count = parent_rule_count;
	} else if (!parent) {
		last_synced_parent_rule_count = 0;
	}

	for (size_t i = 0; i < custom_rule_count; ++i) {
		RuleReference reference{false, i};
		if (std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
			valid_refs.push_back(reference);
		}
	}

	rule_order = valid_refs;
}

RuleDefinition MaterialDefinition::get_effective_rule(size_t order_idx) const {
	if (order_idx >= rule_order.size()) {
		RuleDefinition dummy;
		dummy.when[12] = {id};
		dummy.then.fill(255);
		return dummy;
	}

	const RuleReference& reference = rule_order[order_idx];
	if (reference.is_inherited && inherits_from != 255) {
		const auto& parent = MaterialManager::get_material(inherits_from);
		if (reference.index < parent.rules.size()) {
			RuleDefinition r = parent.rules[reference.index];
			MaterialManager::replace_self_references(r, parent.id, id);
			r.when[12] = {id};
			return r;
		}
	} else if (!reference.is_inherited) {
		if (reference.index < rules.size()) {
			RuleDefinition r = rules[reference.index];
			r.when[12] = {id};
			return r;
		}
	}

	RuleDefinition dummy;
	dummy.when[12] = {id};
	dummy.then.fill(255);
	return dummy;
}

void MaterialManager::replace_self_references(RuleDefinition& rule, uint8_t old_id, uint8_t new_id) {
	for (auto& when_cell : rule.when) {
		for (auto& id : when_cell) {
			if (id == old_id) {
				id = new_id;
			}
		}
	}
	for (auto& then_cell : rule.then) {
		if (then_cell == old_id) {
			then_cell = new_id;
		}
	}
}

void MaterialManager::replace_self_references_in_material(MaterialDefinition& mat, uint8_t old_id, uint8_t new_id) {
	for (auto& r : mat.rules) {
		replace_self_references(r, old_id, new_id);
	}
}

void MaterialManager::sync_inherited_rules(uint8_t index) {
	if (index < materials.size()) {
		materials[index].sync_rule_order();
	}
}

static MaterialDefinition parse_material_from_json(const nlohmann::ordered_json& j, const std::string& default_name,
												   uint8_t fallback_id) {
	MaterialDefinition mat;
	mat.name = j.value("name", default_name);
	mat.id = j.value("id", fallback_id);
	mat.inherits_from = j.value("inherits_from", 255);

	if (j.contains("color")) {
		mat.color = j["color"].get<std::array<uint8_t, 3>>();
	} else {
		mat.color = {64, 64, 64};
	}

	if (j.contains("rules")) {
		for (const auto& r_j : j["rules"]) {
			if (r_j.contains("inherited_rule")) {
				size_t parent_idx = r_j["inherited_rule"].get<size_t>();
				mat.rule_order.push_back({true, parent_idx});
			} else {
				RuleDefinition r;
				const auto& when_arr = r_j["when"];
				for (uint32_t i = 0; i < NEIGHBOR_COUNT; ++i) {
					r.when[i] = when_arr[i].get<std::vector<uint8_t>>();
				}

				r.then.fill(255);
				if (r_j.contains("then")) {
					const auto& then_arr = r_j["then"];
					for (uint32_t i = 0; i < NEIGHBOR_COUNT; ++i) {
						r.then[i] = then_arr[i].get<uint8_t>();
					}
				}

				r.symmetry.flip_x = r_j.contains("sym_x") && r_j["sym_x"].get<bool>();
				r.symmetry.flip_y = r_j.contains("sym_y") && r_j["sym_y"].get<bool>();
				r.symmetry.rotate = r_j.contains("sym_rot") && r_j["sym_rot"].get<bool>();
				r.chance = r_j.contains("chance") ? r_j["chance"].get<float>() : 100.0f;
				r.when[12] = {mat.id};

				mat.rules.push_back(r);
				mat.rule_order.push_back({false, mat.rules.size() - 1});
			}
		}
	}

	mat.last_synced_parent_rule_count = j.value("last_synced_parent_rule_count", (size_t)0);

	return mat;
}

void MaterialManager::load_all_materials(std::string_view directory_path) {
	materials.clear();

	MaterialDefinition empty_mat = default_empty;

	std::vector<MaterialDefinition> loaded_mats;

	if (fs::exists(directory_path.data()) && fs::is_directory(directory_path.data())) {
		for (const auto& entry : fs::directory_iterator(directory_path.data())) {
			if (entry.is_regular_file() && entry.path().extension().string() == ".mat") {
				std::ifstream file(entry.path());
				if (!file.is_open())
					continue;

				try {
					nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
					MaterialDefinition mat = parse_material_from_json(j, entry.path().stem().string(), 255);

					if (mat.id == 0) {
						empty_mat = mat;
					} else {
						loaded_mats.push_back(mat);
					}
				} catch (...) {}
			}
		}
	}

	materials.push_back(empty_mat);

	std::sort(loaded_mats.begin(), loaded_mats.end(),
			  [](const MaterialDefinition& a, const MaterialDefinition& b) { return a.id < b.id; });

	for (auto& mat : loaded_mats) {
		if (mat.id == 255) {
			mat.id = get_unused_id();
		}
		for (auto& r : mat.rules) {
			r.when[12] = {mat.id};
		}

		materials.push_back(mat);
	}

	rebuild_compiled_rules();
}

void MaterialManager::save_all_materials(std::string_view directory_path) {
	fs::create_directories(directory_path.data());

	for (const auto& entry : fs::directory_iterator(directory_path.data())) {
		if (entry.is_regular_file() && entry.path().extension().string() == ".mat") {
			std::string stem = entry.path().stem().string();
			bool found = false;
			for (const auto& m : materials) {
				if (m.name == stem) {
					if (m.id == 0) {
						bool is_default = (m.color[0] == 64 && m.color[1] == 64 && m.color[2] == 64 && m.rules.empty());
						if (is_default) {
							break;
						}
					}
					found = true;
					break;
				}
			}
			if (!found) {
				fs::remove(entry.path());
			}
		}
	}

	for (const auto& mat : materials) {
		if (mat.id == 0 && mat.color[0] == 64 && mat.color[1] == 64 && mat.color[2] == 64 && mat.rules.empty()) {
			continue;
		}

		nlohmann::ordered_json j;
		j["name"] = mat.name;
		j["id"] = mat.id;
		j["color"] = mat.color;
		if (mat.inherits_from != 255) {
			j["inherits_from"] = mat.inherits_from;
			j["last_synced_parent_rule_count"] = mat.last_synced_parent_rule_count;
		}
		nlohmann::ordered_json rules_arr = nlohmann::ordered_json::array();
		for (const auto& reference : mat.rule_order) {
			if (reference.is_inherited) {
				nlohmann::ordered_json r;
				r["inherited_rule"] = reference.index;
				rules_arr.push_back(r);
			} else {
				if (reference.index < mat.rules.size()) {
					const auto& rule = mat.rules[reference.index];
					nlohmann::ordered_json r;
					auto when_copy = rule.when;
					when_copy[12].clear();
					nlohmann::ordered_json when_json = nlohmann::ordered_json::array();
					for (uint32_t i = 0; i < NEIGHBOR_COUNT; ++i) {
						when_json.push_back(when_copy[i]);
					}
					r["when"] = when_json;
					r["then"] = rule.then;
					if (rule.symmetry.flip_x)
						r["sym_x"] = rule.symmetry.flip_x;
					if (rule.symmetry.flip_y)
						r["sym_y"] = rule.symmetry.flip_y;
					if (rule.symmetry.rotate)
						r["sym_rot"] = rule.symmetry.rotate;
					if (rule.chance != 100.0f)
						r["chance"] = rule.chance;
					rules_arr.push_back(r);
				}
			}
		}
		j["rules"] = rules_arr;

		std::string filename = std::string(directory_path) + "/" + mat.name + ".mat";
		std::ofstream file(filename);
		if (file.is_open()) {
			file << j.dump(4);
		}
	}
}

uint8_t MaterialManager::add_material(const MaterialDefinition& mat) {
	MaterialDefinition m = mat;
	uint8_t old_id = mat.id;
	m.id = get_unused_id();
	replace_self_references_in_material(m, old_id, m.id);
	for (auto& r : m.rules) {
		r.when[12] = {m.id};
	}
	materials.push_back(m);
	rebuild_compiled_rules();
	return m.id;
}

void MaterialManager::update_material_name(uint8_t index, std::string_view name) { materials[index].name = name; }

void MaterialManager::update_material_rules(uint8_t index, const MaterialDefinition& mat) {
	materials[index].rules = mat.rules;
	rebuild_compiled_rules();
}

void MaterialManager::set_material_inheritance(uint8_t index, uint8_t parent_id) {
	if (index >= materials.size())
		return;
	auto& mat = materials[index];
	mat.inherits_from = parent_id;

	std::erase_if(mat.rule_order, [](const RuleReference& r) { return r.is_inherited; });
	mat.last_synced_parent_rule_count = 0;

	if (parent_id != 255) {
		const auto& parent = get_material(parent_id);
		std::vector<RuleReference> new_order;
		for (size_t i = 0; i < parent.rules.size(); ++i) {
			new_order.push_back({true, i});
		}
		for (const auto& reference : mat.rule_order) {
			if (!reference.is_inherited) {
				new_order.push_back(reference);
			}
		}
		mat.rule_order = new_order;
		mat.last_synced_parent_rule_count = parent.rules.size();
	}

	mat.sync_rule_order();
	rebuild_compiled_rules();
}

void MaterialManager::update_material_color(uint8_t id, const MaterialDefinition& mat) {
	uint8_t idx = material_by_id[id];
	materials[idx].color = mat.color;
	runtime_materials[idx].packed_color = pack_color(mat.color);

	Grid::draw_material(id);
}

void MaterialManager::remove_material(uint8_t id) {
	uint8_t idx = material_by_id[id];

	std::vector<uint8_t> old_to_new(256);
	for (int i = 0; i < 256; ++i) {
		old_to_new[i] = static_cast<uint8_t>(i);
	}
	old_to_new[idx] = 0;

	Grid::remap_materials(old_to_new);
	Grid::draw_material(0);

	materials.erase(materials.begin() + idx);

	for (auto& m : materials) {
		for (auto& r : m.rules) {
			for (auto& when_cell : r.when) {
				std::erase(when_cell, idx);
			}
			for (auto& then_cell : r.then) {
				if (then_cell == idx) {
					then_cell = 255;
				}
			}
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::rebuild_compiled_rules() {
	material_by_id.fill(0);
	for (size_t i = 0; i < materials.size(); ++i) {
		material_by_id[materials[i].id] = static_cast<uint8_t>(i);
	}

	for (size_t i = 0; i < materials.size(); ++i) {
		sync_inherited_rules(static_cast<uint8_t>(i));
	}

	runtime_materials.fill(default_runtime_empty);

	for (const auto& m : materials) {
		RuntimeMaterial rm;
		rm.id = m.id;
		rm.packed_color = pack_color(m.color);

		using WhenArray = std::array<std::bitset<256>, NEIGHBOR_COUNT>;
		using ThenArray = std::array<uint8_t, NEIGHBOR_COUNT>;

		auto flip_x_when = [](const WhenArray& arr) {
			WhenArray result{};
			for (uint8_t x = 0; x < NEIGHBOR_SIZE; x++) {
				for (uint8_t y = 0; y < NEIGHBOR_SIZE; y++) {
					result[y * NEIGHBOR_SIZE + (NEIGHBOR_SIZE - x - 1)] = arr[y * NEIGHBOR_SIZE + x];
				}
			}
			return result;
		};

		auto flip_x_then = [](const ThenArray& arr) {
			ThenArray result{};
			for (uint8_t x = 0; x < NEIGHBOR_SIZE; x++) {
				for (uint8_t y = 0; y < NEIGHBOR_SIZE; y++) {
					result[y * NEIGHBOR_SIZE + (NEIGHBOR_SIZE - x - 1)] = arr[y * NEIGHBOR_SIZE + x];
				}
			}
			return result;
		};

		auto flip_y_when = [](const WhenArray& arr) {
			WhenArray result{};
			for (uint8_t x = 0; x < NEIGHBOR_SIZE; x++) {
				for (uint8_t y = 0; y < NEIGHBOR_SIZE; y++) {
					result[(NEIGHBOR_SIZE - y - 1) * NEIGHBOR_SIZE + x] = arr[y * NEIGHBOR_SIZE + x];
				}
			}
			return result;
		};

		auto flip_y_then = [](const ThenArray& arr) {
			ThenArray result{};
			for (uint8_t x = 0; x < NEIGHBOR_SIZE; x++) {
				for (uint8_t y = 0; y < NEIGHBOR_SIZE; y++) {
					result[(NEIGHBOR_SIZE - y - 1) * NEIGHBOR_SIZE + x] = arr[y * NEIGHBOR_SIZE + x];
				}
			}
			return result;
		};

		auto rotate_90_when = [](const WhenArray& arr) {
			WhenArray result{};
			for (uint8_t x = 0; x < NEIGHBOR_SIZE; x++) {
				for (uint8_t y = 0; y < NEIGHBOR_SIZE; y++) {
					result[x * NEIGHBOR_SIZE + (NEIGHBOR_SIZE - y - 1)] = arr[y * NEIGHBOR_SIZE + x];
				}
			}
			return result;
		};

		auto rotate_90_then = [](const ThenArray& arr) {
			ThenArray result{};
			for (uint8_t x = 0; x < NEIGHBOR_SIZE; x++) {
				for (uint8_t y = 0; y < NEIGHBOR_SIZE; y++) {
					result[x * NEIGHBOR_SIZE + (NEIGHBOR_SIZE - y - 1)] = arr[y * NEIGHBOR_SIZE + x];
				}
			}
			return result;
		};

		for (size_t r_idx = 0; r_idx < m.rule_order.size(); ++r_idx) {
			RuleDefinition ur = m.get_effective_rule(r_idx);
			CompiledRule cur;
			cur.chance = ur.chance;

			CompiledRuleVariant base_rule;
			for (uint8_t i = 0; i < NEIGHBOR_COUNT; ++i) {
				if (ur.when[i].empty()) {
					base_rule.when[i].set();
				} else {
					base_rule.when[i].reset();
					for (uint8_t id : ur.when[i]) {
						base_rule.when[i].set(id);
					}
				}
				base_rule.then[i] = ur.then[i];
			}
			base_rule.when[12].set();

			cur.variants.push_back(base_rule);

			auto add_unique_variant = [&cur](const CompiledRuleVariant& candidate) {
				for (const auto& existing : cur.variants) {
					if (existing.when == candidate.when && existing.then == candidate.then) {
						return;
					}
				}
				cur.variants.push_back(candidate);
			};

			if (ur.symmetry.rotate) {
				CompiledRuleVariant r_90;
				r_90.when = rotate_90_when(base_rule.when);
				r_90.then = rotate_90_then(base_rule.then);
				add_unique_variant(r_90);

				CompiledRuleVariant r_180;
				r_180.when = rotate_90_when(r_90.when);
				r_180.then = rotate_90_then(r_90.then);
				add_unique_variant(r_180);

				CompiledRuleVariant r_270;
				r_270.when = rotate_90_when(r_180.when);
				r_270.then = rotate_90_then(r_180.then);
				add_unique_variant(r_270);
			}

			if (ur.symmetry.flip_x || ur.symmetry.flip_y) {
				size_t current_variants_count = cur.variants.size();
				for (size_t i = 0; i < current_variants_count; ++i) {
					CompiledRuleVariant source = cur.variants[i];

					if (ur.symmetry.flip_x) {
						CompiledRuleVariant r_x;
						r_x.when = flip_x_when(source.when);
						r_x.then = flip_x_then(source.then);
						add_unique_variant(r_x);
					}
					if (ur.symmetry.flip_y) {
						CompiledRuleVariant r_y;
						r_y.when = flip_y_when(source.when);
						r_y.then = flip_y_then(source.then);
						add_unique_variant(r_y);
					}
					if (ur.symmetry.flip_x && ur.symmetry.flip_y) {
						CompiledRuleVariant r_xy;
						r_xy.when = flip_y_when(flip_x_when(source.when));
						r_xy.then = flip_y_then(flip_x_then(source.then));
						add_unique_variant(r_xy);
					}
				}
			}

			rm.rules.push_back(cur);
		}

		runtime_materials[m.id] = rm;
	}
}

const MaterialDefinition& MaterialManager::get_material(uint8_t id) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size()) {
		return materials[idx];
	}
	return default_empty;
}

const RuntimeMaterial& MaterialManager::get_runtime_material(uint8_t id) { return runtime_materials[id]; }

uint8_t MaterialManager::get_material_count() { return static_cast<uint8_t>(materials.size()); }

std::vector<MaterialDefinition>& MaterialManager::get_materials() { return materials; }