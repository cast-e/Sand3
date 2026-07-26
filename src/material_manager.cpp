#include "material_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "grid.hpp"

namespace fs = std::filesystem;

std::vector<Material> MaterialManager::materials{};
Material MaterialManager::default_empty{"empty", 0, {64, 64, 64}, (255u << 24) | (64u << 16) | (64u << 8) | 64u,
										{},		 {}};
std::array<const Material*, 256> MaterialManager::material_by_id{};

uint32_t MaterialManager::pack_color(const std::array<unsigned char, 3>& color) {
	return (255u << 24) | (color[2] << 16) | (color[1] << 8) | color[0];
}

unsigned char MaterialManager::get_unused_id() {
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
			return static_cast<unsigned char>(i);
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

static Material parse_material_from_json(const nlohmann::ordered_json& j, const std::string& default_name,
										 unsigned char fallback_id) {
	Material mat;
	mat.name = j.value("name", default_name);
	mat.id = j.value("id", fallback_id);

	if (j.contains("color")) {
		mat.color = j["color"].get<std::array<unsigned char, 3>>();
	} else {
		mat.color = {64, 64, 64};
	}
	mat.packed_color = MaterialManager::pack_color(mat.color);

	if (j.contains("rules")) {
		for (const auto& r_j : j["rules"]) {
			UserRule r;
			r.when = r_j["when"].get<std::array<std::string, NEIGHBOR_COUNT>>();
			r.then = r_j["then"].get<std::array<std::string, NEIGHBOR_COUNT>>();
			r.sym_x = r_j.contains("sym_x") && r_j["sym_x"].get<bool>();
			r.sym_y = r_j.contains("sym_y") && r_j["sym_y"].get<bool>();
			r.sym_rot = r_j.contains("sym_rot") && r_j["sym_rot"].get<bool>();
			r.chance = r_j.contains("chance") ? r_j["chance"].get<float>() : 100.0f;
			r.when[12] = mat.name;
			mat.user_rules.push_back(r);
		}
	}

	return mat;
}

void MaterialManager::load_all_materials(std::string_view directory_path) {
	materials.clear();

	Material empty_mat;
	empty_mat.name = "empty";
	empty_mat.id = 0;
	empty_mat.color = {64, 64, 64};
	empty_mat.packed_color = pack_color(empty_mat.color);

	std::vector<Material> loaded_mats;

	if (fs::exists(directory_path.data()) && fs::is_directory(directory_path.data())) {
		for (const auto& entry : fs::directory_iterator(directory_path.data())) {
			if (entry.is_regular_file() && entry.path().extension().string() == ".mat") {
				std::ifstream file(entry.path());
				if (!file.is_open())
					continue;

				try {
					nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
					Material mat = parse_material_from_json(j, entry.path().stem().string(), 255);

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

	std::sort(loaded_mats.begin(), loaded_mats.end(), [](const Material& a, const Material& b) { return a.id < b.id; });

	for (auto& mat : loaded_mats) {
		if (mat.id == 255) {
			mat.id = get_unused_id();
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
						bool is_default =
							(m.color[0] == 64 && m.color[1] == 64 && m.color[2] == 64 && m.user_rules.empty());
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
		if (mat.id == 0 && mat.color[0] == 64 && mat.color[1] == 64 && mat.color[2] == 64 && mat.user_rules.empty()) {
			continue;
		}

		nlohmann::ordered_json j;
		j["name"] = mat.name;
		j["id"] = mat.id;
		j["color"] = mat.color;
		nlohmann::ordered_json rules_arr = nlohmann::ordered_json::array();
		for (const auto& rule : mat.user_rules) {
			nlohmann::ordered_json r;
			auto when_copy = rule.when;
			when_copy[12] = "";
			r["when"] = when_copy;
			r["then"] = rule.then;
			if (rule.sym_x)
				r["sym_x"] = rule.sym_x;
			if (rule.sym_y)
				r["sym_y"] = rule.sym_y;
			if (rule.sym_rot)
				r["sym_rot"] = rule.sym_rot;
			if (rule.chance != 100.0f)
				r["chance"] = rule.chance;
			rules_arr.push_back(r);
		}
		j["rules"] = rules_arr;

		std::string filename = std::string(directory_path) + "/" + mat.name + ".mat";
		std::ofstream file(filename);
		if (file.is_open()) {
			file << j.dump(4);
		}
	}
}

void MaterialManager::add_material(const Material& mat) {
	Material m = mat;
	m.id = get_unused_id();
	m.packed_color = pack_color(m.color);
	materials.push_back(m);
	rebuild_compiled_rules();
}

void MaterialManager::edit_material(size_t index, const Material& mat) {
	if (index >= materials.size()) {
		return;
	}

	const std::string old_name = materials[index].name;

	materials[index] = mat;
	materials[index].packed_color = pack_color(mat.color);

	if (old_name != mat.name && !old_name.empty() && !mat.name.empty()) {
		for (auto& m : materials) {
			for (auto& r : m.user_rules) {
				for (auto& name : r.when) {
					if (name == old_name)
						name = mat.name;
				}
				for (auto& name : r.then) {
					if (name == old_name)
						name = mat.name;
				}
			}
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::remove_material(size_t index) {
	if (index >= materials.size() || materials[index].id == 0) {
		return;
	}

	std::string name = materials[index].name;
	unsigned char removed_id = materials[index].id;

	std::vector<unsigned char> old_to_new(256);
	for (int i = 0; i < 256; ++i) {
		old_to_new[i] = static_cast<unsigned char>(i);
	}
	old_to_new[removed_id] = 0;

	Grid::remap_materials(old_to_new);

	materials.erase(materials.begin() + index);

	for (auto& m : materials) {
		for (auto& r : m.user_rules) {
			for (auto& rule_name : r.when) {
				if (rule_name == name)
					rule_name = "";
			}
			for (auto& rule_name : r.then) {
				if (rule_name == name)
					rule_name = "";
			}
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::rebuild_compiled_rules() {
	material_by_id.fill(&default_empty);
	for (const auto& m : materials) {
		material_by_id[m.id] = &m;
	}

	auto resolve_name_to_id = [](const std::string& name) -> unsigned char {
		if (name.empty())
			return 255;
		for (const auto& m : materials) {
			if (m.name == name) {
				return m.id;
			}
		}
		return 255;
	};

	auto flip_x_array = [](const std::array<unsigned char, NEIGHBOR_COUNT>& arr) {
		std::array<unsigned char, NEIGHBOR_COUNT> result{};
		for (unsigned char x = 0; x < NEIGHBOR_SIZE; x++) {
			for (unsigned char y = 0; y < NEIGHBOR_SIZE; y++) {
				result[y * NEIGHBOR_SIZE + (NEIGHBOR_SIZE - x - 1)] = arr[y * NEIGHBOR_SIZE + x];
			}
		}
		return result;
	};

	auto flip_y_array = [](const std::array<unsigned char, NEIGHBOR_COUNT>& arr) {
		std::array<unsigned char, NEIGHBOR_COUNT> result{};
		for (unsigned char x = 0; x < NEIGHBOR_SIZE; x++) {
			for (unsigned char y = 0; y < NEIGHBOR_SIZE; y++) {
				result[(NEIGHBOR_SIZE - y - 1) * NEIGHBOR_SIZE + x] = arr[y * NEIGHBOR_SIZE + x];
			}
		}
		return result;
	};

	auto rotate_90_array = [](const std::array<unsigned char, NEIGHBOR_COUNT>& arr) {
		std::array<unsigned char, NEIGHBOR_COUNT> result{};
		for (unsigned char x = 0; x < NEIGHBOR_SIZE; x++) {
			for (unsigned char y = 0; y < NEIGHBOR_SIZE; y++) {
				result[x * NEIGHBOR_SIZE + (NEIGHBOR_SIZE - y - 1)] = arr[y * NEIGHBOR_SIZE + x];
			}
		}
		return result;
	};

	for (auto& material : materials) {
		material.compiled_rules.clear();
		for (const auto& ur : material.user_rules) {
			CompiledUserRule cur;
			cur.chance = ur.chance;

			Rule base_rule;
			for (unsigned char i = 0; i < NEIGHBOR_COUNT; ++i) {
				base_rule.when[i] = resolve_name_to_id(ur.when[i]);
				base_rule.then[i] = resolve_name_to_id(ur.then[i]);
			}
			base_rule.when[12] = 255;

			cur.variants.push_back(base_rule);

			auto add_unique_variant = [&cur](const Rule& candidate) {
				for (const auto& existing : cur.variants) {
					if (existing.when == candidate.when && existing.then == candidate.then) {
						return;
					}
				}
				cur.variants.push_back(candidate);
			};

			if (ur.sym_rot) {
				Rule r_90;
				r_90.when = rotate_90_array(base_rule.when);
				r_90.then = rotate_90_array(base_rule.then);
				add_unique_variant(r_90);

				Rule r_180;
				r_180.when = rotate_90_array(r_90.when);
				r_180.then = rotate_90_array(r_90.then);
				add_unique_variant(r_180);

				Rule r_270;
				r_270.when = rotate_90_array(r_180.when);
				r_270.then = rotate_90_array(r_180.then);
				add_unique_variant(r_270);
			}

			if (ur.sym_x || ur.sym_y) {
				size_t current_variants_count = cur.variants.size();
				for (size_t i = 0; i < current_variants_count; ++i) {
					Rule source = cur.variants[i];

					if (ur.sym_x) {
						Rule r_x;
						r_x.when = flip_x_array(source.when);
						r_x.then = flip_x_array(source.then);
						add_unique_variant(r_x);
					}
					if (ur.sym_y) {
						Rule r_y;
						r_y.when = flip_y_array(source.when);
						r_y.then = flip_y_array(source.then);
						add_unique_variant(r_y);
					}
					if (ur.sym_x && ur.sym_y) {
						Rule r_xy;
						r_xy.when = flip_y_array(flip_x_array(source.when));
						r_xy.then = flip_y_array(flip_x_array(source.then));
						add_unique_variant(r_xy);
					}
				}
			}

			material.compiled_rules.push_back(cur);
		}
	}
}

const Material& MaterialManager::get_material(unsigned char id) { return *material_by_id[id]; }

unsigned char MaterialManager::get_material_count() { return static_cast<unsigned char>(materials.size()); }

std::vector<Material>& MaterialManager::get_materials() { return materials; }