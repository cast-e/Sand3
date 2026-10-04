#include "material_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "grid.hpp"
#include "vulkan.hpp"

namespace fs = std::filesystem;

std::vector<MaterialDefinition> MaterialManager::materials{};
std::array<RuntimeMaterial, 256> MaterialManager::runtime_materials{};
std::array<uint8_t, 256> MaterialManager::material_by_id{};
MaterialDefinition MaterialManager::default_empty{"empty", 255, 255, {64, 64, 64}};
RuntimeMaterial MaterialManager::default_runtime_empty{255, MaterialManager::pack_color({64, 64, 64}), {}};

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

void RuleDefinition::expand_top() {
	int old_w = width();
	int old_h = height();
	min_dy--;
	int new_h = height();
	std::vector<std::vector<uint8_t>> new_when(old_w * new_h);
	std::vector<uint8_t> new_then(old_w * new_h, 255);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			new_when[(y + 1) * old_w + x] = std::move(when[y * old_w + x]);
			new_then[(y + 1) * old_w + x] = then[y * old_w + x];
		}
	}
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::shrink_top() {
	if (min_dy >= 0 || height() <= 1)
		return;
	int old_w = width();
	min_dy++;
	int new_h = height();
	std::vector<std::vector<uint8_t>> new_when(old_w * new_h);
	std::vector<uint8_t> new_then(old_w * new_h, 255);
	for (int y = 0; y < new_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			new_when[y * old_w + x] = std::move(when[(y + 1) * old_w + x]);
			new_then[y * old_w + x] = then[(y + 1) * old_w + x];
		}
	}
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::expand_bottom() {
	int old_w = width();
	max_dy++;
	int new_h = height();
	when.resize(old_w * new_h);
	then.resize(old_w * new_h, 255);
}

void RuleDefinition::shrink_bottom() {
	if (max_dy <= 0 || height() <= 1)
		return;
	int old_w = width();
	max_dy--;
	int new_h = height();
	when.resize(old_w * new_h);
	then.resize(old_w * new_h, 255);
}

void RuleDefinition::expand_left() {
	int old_w = width();
	int old_h = height();
	min_dx--;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			new_when[y * new_w + (x + 1)] = std::move(when[y * old_w + x]);
			new_then[y * new_w + (x + 1)] = then[y * old_w + x];
		}
	}
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::shrink_left() {
	if (min_dx >= 0 || width() <= 1)
		return;
	int old_w = width();
	int old_h = height();
	min_dx++;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < new_w; ++x) {
			new_when[y * new_w + x] = std::move(when[y * old_w + (x + 1)]);
			new_then[y * new_w + x] = then[y * old_w + (x + 1)];
		}
	}
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::expand_right() {
	int old_w = width();
	int old_h = height();
	max_dx++;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < old_w; ++x) {
			new_when[y * new_w + x] = std::move(when[y * old_w + x]);
			new_then[y * new_w + x] = then[y * old_w + x];
		}
	}
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::shrink_right() {
	if (max_dx <= 0 || width() <= 1)
		return;
	int old_w = width();
	int old_h = height();
	max_dx--;
	int new_w = width();
	std::vector<std::vector<uint8_t>> new_when(new_w * old_h);
	std::vector<uint8_t> new_then(new_w * old_h, 255);
	for (int y = 0; y < old_h; ++y) {
		for (int x = 0; x < new_w; ++x) {
			new_when[y * new_w + x] = std::move(when[y * old_w + x]);
			new_then[y * new_w + x] = then[y * old_w + x];
		}
	}
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::flip_x() {
	int new_min_dx = -max_dx;
	int new_max_dx = -min_dx;
	int new_w = new_max_dx - new_min_dx + 1;
	int new_h = height();
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);

	for (int dy = min_dy; dy <= max_dy; ++dy) {
		for (int dx = min_dx; dx <= max_dx; ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = -dx;
			int new_dy = dy;
			int new_idx = (new_dy - min_dy) * new_w + (new_dx - new_min_dx);
			new_when[new_idx] = std::move(when[old_idx]);
			new_then[new_idx] = then[old_idx];
		}
	}
	min_dx = new_min_dx;
	max_dx = new_max_dx;
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::flip_y() {
	int new_min_dy = -max_dy;
	int new_max_dy = -min_dy;
	int new_w = width();
	int new_h = new_max_dy - new_min_dy + 1;
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);

	for (int dy = min_dy; dy <= max_dy; ++dy) {
		for (int dx = min_dx; dx <= max_dx; ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = dx;
			int new_dy = -dy;
			int new_idx = (new_dy - new_min_dy) * new_w + (new_dx - min_dx);
			new_when[new_idx] = std::move(when[old_idx]);
			new_then[new_idx] = then[old_idx];
		}
	}
	min_dy = new_min_dy;
	max_dy = new_max_dy;
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::rotate_cw() {
	int new_min_dx = -max_dy;
	int new_max_dx = -min_dy;
	int new_min_dy = min_dx;
	int new_max_dy = max_dx;
	int new_w = new_max_dx - new_min_dx + 1;
	int new_h = new_max_dy - new_min_dy + 1;
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);

	for (int dy = min_dy; dy <= max_dy; ++dy) {
		for (int dx = min_dx; dx <= max_dx; ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = -dy;
			int new_dy = dx;
			int new_idx = (new_dy - new_min_dy) * new_w + (new_dx - new_min_dx);
			new_when[new_idx] = std::move(when[old_idx]);
			new_then[new_idx] = then[old_idx];
		}
	}
	min_dx = new_min_dx;
	max_dx = new_max_dx;
	min_dy = new_min_dy;
	max_dy = new_max_dy;
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::rotate_ccw() {
	int new_min_dx = min_dy;
	int new_max_dx = max_dy;
	int new_min_dy = -max_dx;
	int new_max_dy = -min_dx;
	int new_w = new_max_dx - new_min_dx + 1;
	int new_h = new_max_dy - new_min_dy + 1;
	std::vector<std::vector<uint8_t>> new_when(new_w * new_h);
	std::vector<uint8_t> new_then(new_w * new_h, 255);

	for (int dy = min_dy; dy <= max_dy; ++dy) {
		for (int dx = min_dx; dx <= max_dx; ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_dx = dy;
			int new_dy = -dx;
			int new_idx = (new_dy - new_min_dy) * new_w + (new_dx - new_min_dx);
			new_when[new_idx] = std::move(when[old_idx]);
			new_then[new_idx] = then[old_idx];
		}
	}
	min_dx = new_min_dx;
	max_dx = new_max_dx;
	min_dy = new_min_dy;
	max_dy = new_max_dy;
	when = std::move(new_when);
	then = std::move(new_then);
}

void RuleDefinition::reset_to_3x3(uint8_t center_id) {
	std::vector<std::vector<uint8_t>> new_when(9, std::vector<uint8_t>{});
	std::vector<uint8_t> new_then(9, 255);
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			int old_idx = get_cell_idx(dx, dy);
			int new_idx = (dy + 1) * 3 + (dx + 1);
			if (old_idx >= 0 && old_idx < static_cast<int>(when.size())) {
				new_when[new_idx] = std::move(when[old_idx]);
				new_then[new_idx] = then[old_idx];
			}
		}
	}
	min_dx = -1;
	max_dx = 1;
	min_dy = -1;
	max_dy = 1;
	when = std::move(new_when);
	then = std::move(new_then);
	if (when[4].empty()) {
		when[4] = {center_id};
	}
}

void MaterialDefinition::sync_rule_order() {
	std::vector<RuleReference> valid_refs;

	const MaterialDefinition* parent = nullptr;
	if (inherits_from != 255) {
		const auto& p = MaterialManager::get_material(inherits_from);
		if (p.id == inherits_from) {
			parent = &p;
		}
	}

	size_t parent_rule_count = parent ? parent->rules.size() : 0;
	size_t custom_rule_count = rules.size();

	for (const auto& reference : rule_order) {
		if (reference.is_inherited) {
			if (parent && reference.index < parent_rule_count &&
				std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
				valid_refs.push_back(reference);
			}
		} else {
			if (reference.index < custom_rule_count &&
				std::find(valid_refs.begin(), valid_refs.end(), reference) == valid_refs.end()) {
				valid_refs.push_back(reference);
			}
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
		dummy.init_default(id);
		return dummy;
	}

	const RuleReference& reference = rule_order[order_idx];
	if (reference.is_inherited && inherits_from != 255) {
		const auto& parent = MaterialManager::get_material(inherits_from);
		if (parent.id == inherits_from && reference.index < parent.rules.size()) {
			RuleDefinition r = parent.rules[reference.index];
			MaterialManager::replace_self_references(r, parent.id, id);
			int c_idx = r.get_center_idx();
			if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
				r.when[c_idx] = {id};
			}
			return r;
		}
	} else if (!reference.is_inherited) {
		if (reference.index < rules.size()) {
			RuleDefinition r = rules[reference.index];
			int c_idx = r.get_center_idx();
			if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
				r.when[c_idx] = {id};
			}
			return r;
		}
	}

	RuleDefinition dummy;
	dummy.init_default(id);
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

void MaterialManager::sync_inherited_rules(uint8_t id) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		materials[idx].sync_rule_order();
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
				int8_t min_dx = -1, max_dx = 1, min_dy = -1, max_dy = 1;
				if (r_j.contains("bounds") && r_j["bounds"].is_array() && r_j["bounds"].size() >= 4) {
					min_dx = r_j["bounds"][0].get<int8_t>();
					max_dx = r_j["bounds"][1].get<int8_t>();
					min_dy = r_j["bounds"][2].get<int8_t>();
					max_dy = r_j["bounds"][3].get<int8_t>();
				} else if (r_j.contains("when") && r_j["when"].is_array() && r_j["when"].size() == 25) {
					min_dx = -2;
					max_dx = 2;
					min_dy = -2;
					max_dy = 2;
				}

				r.min_dx = min_dx;
				r.max_dx = max_dx;
				r.min_dy = min_dy;
				r.max_dy = max_dy;
				int total = r.total_cells();
				r.when.assign(total, std::vector<uint8_t>{});
				r.then.assign(total, 255);

				if (r_j.contains("when") && r_j["when"].is_array()) {
					const auto& when_arr = r_j["when"];
					if (when_arr.size() == 25 && total != 25) {
						for (int dy = min_dy; dy <= max_dy; ++dy) {
							for (int dx = min_dx; dx <= max_dx; ++dx) {
								if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) {
									int old_idx = (dy + 2) * 5 + (dx + 2);
									int new_idx = r.get_cell_idx(dx, dy);
									r.when[new_idx] = when_arr[old_idx].get<std::vector<uint8_t>>();
								}
							}
						}
					} else {
						size_t count = std::min(when_arr.size(), static_cast<size_t>(total));
						for (size_t i = 0; i < count; ++i) {
							r.when[i] = when_arr[i].get<std::vector<uint8_t>>();
						}
					}
				}

				if (r_j.contains("then") && r_j["then"].is_array()) {
					const auto& then_arr = r_j["then"];
					if (then_arr.size() == 25 && total != 25) {
						for (int dy = min_dy; dy <= max_dy; ++dy) {
							for (int dx = min_dx; dx <= max_dx; ++dx) {
								if (dx >= -2 && dx <= 2 && dy >= -2 && dy <= 2) {
									int old_idx = (dy + 2) * 5 + (dx + 2);
									int new_idx = r.get_cell_idx(dx, dy);
									r.then[new_idx] = then_arr[old_idx].get<uint8_t>();
								}
							}
						}
					} else {
						size_t count = std::min(then_arr.size(), static_cast<size_t>(total));
						for (size_t i = 0; i < count; ++i) {
							r.then[i] = then_arr[i].get<uint8_t>();
						}
					}
				}

				r.symmetry.flip_x = r_j.contains("sym_x") && r_j["sym_x"].get<bool>();
				r.symmetry.flip_y = r_j.contains("sym_y") && r_j["sym_y"].get<bool>();
				r.symmetry.rotate = r_j.contains("sym_rot") && r_j["sym_rot"].get<bool>();
				r.chance = r_j.contains("chance") ? r_j["chance"].get<float>() : 100.0f;
				int c_idx = r.get_center_idx();
				if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
					r.when[c_idx] = {mat.id};
				}

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
	empty_mat.id = 0;

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

	std::array<bool, 256> id_taken{};
	id_taken[0] = true;

	for (auto& mat : loaded_mats) {
		if (mat.id == 0 || mat.id == 255 || id_taken[mat.id]) {
			mat.id = get_unused_id();
		}
		id_taken[mat.id] = true;
		for (auto& r : mat.rules) {
			int c_idx = r.get_center_idx();
			if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
				r.when[c_idx] = {mat.id};
			}
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
					int c_idx = rule.get_center_idx();
					if (c_idx >= 0 && c_idx < static_cast<int>(when_copy.size())) {
						when_copy[c_idx].clear();
					}
					r["when"] = when_copy;
					r["then"] = rule.then;
					r["bounds"] = {rule.min_dx, rule.max_dx, rule.min_dy, rule.max_dy};
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
		int c_idx = r.get_center_idx();
		if (c_idx >= 0 && c_idx < static_cast<int>(r.when.size())) {
			r.when[c_idx] = {m.id};
		}
	}
	materials.push_back(m);
	rebuild_compiled_rules();
	return m.id;
}

void MaterialManager::update_material_name(uint8_t id, std::string_view name) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		materials[idx].name = name;
	}
}

void MaterialManager::update_material_rules(uint8_t id, const MaterialDefinition& mat) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		materials[idx].rules = mat.rules;
		materials[idx].rule_order = mat.rule_order;
		materials[idx].last_synced_parent_rule_count = mat.last_synced_parent_rule_count;
		rebuild_compiled_rules();
	}
}

void MaterialManager::set_material_inheritance(uint8_t id, uint8_t parent_id) {
	uint8_t idx = material_by_id[id];
	if (idx >= materials.size() || materials[idx].id != id)
		return;

	if (parent_id != 255) {
		uint8_t curr = parent_id;
		while (curr != 255) {
			if (curr == id) {
				return;
			}
			const auto& p = get_material(curr);
			if (p.id != curr)
				break;
			curr = p.inherits_from;
		}
	}

	auto& mat = materials[idx];
	mat.inherits_from = parent_id;

	std::erase_if(mat.rule_order, [](const RuleReference& r) { return r.is_inherited; });
	mat.last_synced_parent_rule_count = 0;

	if (parent_id != 255) {
		const auto& parent = get_material(parent_id);
		if (parent.id == parent_id) {
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
	}

	mat.sync_rule_order();
	rebuild_compiled_rules();
}

void MaterialManager::update_material_color(uint8_t id, const MaterialDefinition& mat) {
	uint8_t idx = material_by_id[id];
	if (idx >= materials.size() || materials[idx].id != id)
		return;
	materials[idx].color = mat.color;
	runtime_materials[id].packed_color = pack_color(mat.color);

	if (Vulkan::is_available()) {
		Vulkan::update_rules();
	}

	Grid::draw_material(id);
}

void MaterialManager::remove_material(uint8_t id) {
	uint8_t idx = material_by_id[id];
	if (idx >= materials.size() || materials[idx].id != id)
		return;

	std::vector<uint8_t> old_to_new(256);
	for (int i = 0; i < 256; ++i) {
		old_to_new[i] = static_cast<uint8_t>(i);
	}
	old_to_new[id] = 0;

	Grid::remap_materials(old_to_new);
	Grid::draw_material(0);

	materials.erase(materials.begin() + idx);

	for (auto& m : materials) {
		if (m.inherits_from == id) {
			m.inherits_from = 255;
			std::erase_if(m.rule_order, [](const RuleReference& r) { return r.is_inherited; });
			m.last_synced_parent_rule_count = 0;
		}
		for (auto& r : m.rules) {
			for (auto& when_cell : r.when) {
				std::erase(when_cell, id);
			}
			for (auto& then_cell : r.then) {
				if (then_cell == id) {
					then_cell = 255;
				}
			}
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::rebuild_compiled_rules() {
	material_by_id.fill(255);
	for (size_t i = 0; i < materials.size(); ++i) {
		material_by_id[materials[i].id] = static_cast<uint8_t>(i);
	}

	for (size_t i = 0; i < materials.size(); ++i) {
		materials[i].sync_rule_order();
	}

	runtime_materials.fill(default_runtime_empty);

	int max_rx = 1;
	int max_ry = 1;
	const int gw = Grid::get_width();

	auto transform_variant = [gw](const CompiledRuleVariant& src, auto coord_func) {
		CompiledRuleVariant dst;
		dst.min_dx = 0;
		dst.max_dx = 0;
		dst.min_dy = 0;
		dst.max_dy = 0;
		for (const auto& cond : src.conditions) {
			auto [ndx, ndy] = coord_func(cond.dx, cond.dy);
			dst.min_dx = std::min(dst.min_dx, static_cast<int8_t>(ndx));
			dst.max_dx = std::max(dst.max_dx, static_cast<int8_t>(ndx));
			dst.min_dy = std::min(dst.min_dy, static_cast<int8_t>(ndy));
			dst.max_dy = std::max(dst.max_dy, static_cast<int8_t>(ndy));
			dst.conditions.push_back(
				{ndy * gw + ndx, static_cast<int8_t>(ndx), static_cast<int8_t>(ndy), cond.allowed});
		}
		for (const auto& act : src.actions) {
			auto [ndx, ndy] = coord_func(act.dx, act.dy);
			dst.min_dx = std::min(dst.min_dx, static_cast<int8_t>(ndx));
			dst.max_dx = std::max(dst.max_dx, static_cast<int8_t>(ndx));
			dst.min_dy = std::min(dst.min_dy, static_cast<int8_t>(ndy));
			dst.max_dy = std::max(dst.max_dy, static_cast<int8_t>(ndy));
			dst.actions.push_back(
				{ndy * gw + ndx, static_cast<int8_t>(ndx), static_cast<int8_t>(ndy), act.target_material});
		}
		std::sort(dst.conditions.begin(), dst.conditions.end(), [](const ConditionCheck& a, const ConditionCheck& b) {
			return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
		});
		std::sort(dst.actions.begin(), dst.actions.end(), [](const OutputAction& a, const OutputAction& b) {
			return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
		});
		return dst;
	};

	auto are_variants_equal = [](const CompiledRuleVariant& a, const CompiledRuleVariant& b) {
		if (a.conditions.size() != b.conditions.size() || a.actions.size() != b.actions.size())
			return false;
		for (size_t i = 0; i < a.conditions.size(); ++i) {
			if (a.conditions[i].dx != b.conditions[i].dx || a.conditions[i].dy != b.conditions[i].dy ||
				a.conditions[i].allowed != b.conditions[i].allowed)
				return false;
		}
		for (size_t i = 0; i < a.actions.size(); ++i) {
			if (a.actions[i].dx != b.actions[i].dx || a.actions[i].dy != b.actions[i].dy ||
				a.actions[i].target_material != b.actions[i].target_material)
				return false;
		}
		return true;
	};

	for (const auto& m : materials) {
		RuntimeMaterial rm;
		rm.id = m.id;
		rm.packed_color = pack_color(m.color);

		for (size_t r_idx = 0; r_idx < m.rule_order.size(); ++r_idx) {
			RuleDefinition ur = m.get_effective_rule(r_idx);
			CompiledRule cur;
			cur.chance = ur.chance;

			CompiledRuleVariant base_rule;
			base_rule.min_dx = ur.min_dx;
			base_rule.max_dx = ur.max_dx;
			base_rule.min_dy = ur.min_dy;
			base_rule.max_dy = ur.max_dy;

			for (int dy = ur.min_dy; dy <= ur.max_dy; ++dy) {
				for (int dx = ur.min_dx; dx <= ur.max_dx; ++dx) {
					int idx = ur.get_cell_idx(dx, dy);
					if (idx < 0 || idx >= static_cast<int>(ur.when.size()))
						continue;
					const auto& w_list = ur.when[idx];
					uint8_t then_val = (idx < static_cast<int>(ur.then.size())) ? ur.then[idx] : 255;

					if (dx == 0 && dy == 0) {
						if (then_val != 255) {
							base_rule.actions.push_back({0, 0, 0, then_val});
						}
						continue;
					}

					if (!w_list.empty()) {
						ConditionCheck cond;
						cond.offset = dy * gw + dx;
						cond.dx = static_cast<int8_t>(dx);
						cond.dy = static_cast<int8_t>(dy);
						cond.allowed.reset();
						for (uint8_t wid : w_list) {
							cond.allowed.set(wid);
						}
						base_rule.conditions.push_back(cond);
					}

					if (then_val != 255) {
						OutputAction act;
						act.offset = dy * gw + dx;
						act.dx = static_cast<int8_t>(dx);
						act.dy = static_cast<int8_t>(dy);
						act.target_material = then_val;
						base_rule.actions.push_back(act);
					}
				}
			}

			std::sort(base_rule.conditions.begin(), base_rule.conditions.end(),
					  [](const ConditionCheck& a, const ConditionCheck& b) {
						  return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
					  });
			std::sort(base_rule.actions.begin(), base_rule.actions.end(),
					  [](const OutputAction& a, const OutputAction& b) {
						  return std::tie(a.dy, a.dx) < std::tie(b.dy, b.dx);
					  });

			cur.variants.push_back(base_rule);

			auto add_unique_variant = [&cur, &are_variants_equal](const CompiledRuleVariant& candidate) {
				for (const auto& existing : cur.variants) {
					if (are_variants_equal(existing, candidate)) {
						return;
					}
				}
				cur.variants.push_back(candidate);
			};

			if (ur.symmetry.rotate) {
				CompiledRuleVariant r_90 =
					transform_variant(base_rule, [](int dx, int dy) { return std::pair(-dy, dx); });
				add_unique_variant(r_90);

				CompiledRuleVariant r_180 = transform_variant(r_90, [](int dx, int dy) { return std::pair(-dy, dx); });
				add_unique_variant(r_180);

				CompiledRuleVariant r_270 = transform_variant(r_180, [](int dx, int dy) { return std::pair(-dy, dx); });
				add_unique_variant(r_270);
			}

			if (ur.symmetry.flip_x || ur.symmetry.flip_y) {
				std::vector<CompiledRuleVariant> current_variants = cur.variants;
				for (const auto& src : current_variants) {
					if (ur.symmetry.flip_x) {
						add_unique_variant(transform_variant(src, [](int dx, int dy) { return std::pair(-dx, dy); }));
					}
					if (ur.symmetry.flip_y) {
						add_unique_variant(transform_variant(src, [](int dx, int dy) { return std::pair(dx, -dy); }));
					}
					if (ur.symmetry.flip_x && ur.symmetry.flip_y) {
						add_unique_variant(transform_variant(src, [](int dx, int dy) { return std::pair(-dx, -dy); }));
					}
				}
			}

			for (const auto& v : cur.variants) {
				max_rx = std::max({max_rx, std::abs(static_cast<int>(v.min_dx)), std::abs(static_cast<int>(v.max_dx))});
				max_ry = std::max({max_ry, std::abs(static_cast<int>(v.min_dy)), std::abs(static_cast<int>(v.max_dy))});
			}

			rm.rules.push_back(cur);
		}

		runtime_materials[m.id] = rm;
	}

	Grid::set_max_rule_radius(max_rx, max_ry);

	if (Vulkan::is_available()) {
		Vulkan::update_rules();
		if (Grid::get_processing_mode() == ProcessingMode::GPU) {
			Grid::refresh_gpu_display();
		}
	}
}

const MaterialDefinition& MaterialManager::get_material(uint8_t id) {
	uint8_t idx = material_by_id[id];
	if (idx < materials.size() && materials[idx].id == id) {
		return materials[idx];
	}
	return default_empty;
}

const RuntimeMaterial& MaterialManager::get_runtime_material(uint8_t id) { return runtime_materials[id]; }

uint8_t MaterialManager::get_material_count() { return static_cast<uint8_t>(materials.size()); }

std::vector<MaterialDefinition>& MaterialManager::get_materials() { return materials; }