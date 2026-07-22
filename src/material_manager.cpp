#include "material_manager.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "grid.hpp"

namespace fs = std::filesystem;

std::vector<Material> MaterialManager::materials{};

unsigned char MaterialManager::get_unused_id() {
	std::array<bool, 256> used{};
	used[0] = true;
	used[255] = true;

	for (const auto& m : materials) {
		if (m.id > 0 && m.id < 255) {
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

void MaterialManager::load_all_materials(std::string_view directory_path) {
	materials.clear();

	std::vector<fs::path> mat_files;
	if (fs::exists(directory_path.data()) && fs::is_directory(directory_path.data())) {
		for (const auto& entry : fs::directory_iterator(directory_path.data())) {
			if (entry.is_regular_file()) {
				std::string ext = entry.path().extension().string();
				if (ext == ".mat" || ext == ".json") {
					mat_files.push_back(entry.path());
				}
			}
		}
	}

	if (!mat_files.empty()) {
		materials.resize(mat_files.size());
		for (const auto& filepath : mat_files) {
			std::ifstream file(filepath);
			if (!file.is_open())
				continue;

			try {
				nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
				Material mat;
				mat.name = j.value("name", "");

				if (j.contains("id") && j["id"].is_number()) {
					mat.id = static_cast<unsigned char>(j["id"].get<int>());
				} else {
					mat.id = get_unused_id();
				}

				if (j.contains("color")) {
					mat.color = j["color"].get<std::array<unsigned char, 3>>();
				}
				mat.packed_color = (255u << 24) | (mat.color[2] << 16) | (mat.color[1] << 8) | mat.color[0];

				if (j.contains("rules")) {
					for (auto& r_j : j["rules"]) {
						UserRule r;
						r.when = r_j["when"].get<std::array<std::string, NEIGHBOR_COUNT>>();
						r.then = r_j["then"].get<std::array<std::string, NEIGHBOR_COUNT>>();
						r.sym_x = r_j.contains("sym_x") && r_j["sym_x"].get<bool>();
						r.sym_y = r_j.contains("sym_y") && r_j["sym_y"].get<bool>();
						r.sym_rot = r_j.contains("sym_rot") && r_j["sym_rot"].get<bool>();
						r.chance = r_j.contains("chance") ? r_j["chance"].get<unsigned char>() : 1;
						r.when[12] = mat.name;
						mat.user_rules.push_back(r);
					}
				}

				materials[mat.id - 1] = mat;
			} catch (...) {}
		}
	}

	for (size_t i = 0; i < materials.size(); ++i) {
		if (materials[i].id == 0) {
			materials[i].id = get_unused_id();
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::save_all_materials(std::string_view directory_path) {
	fs::create_directories(directory_path.data());
	for (const auto& entry : fs::directory_iterator(directory_path.data())) {
		if (entry.is_regular_file()) {
			std::string ext = entry.path().extension().string();
			if (ext == ".mat" || ext == ".json") {
				std::string stem = entry.path().stem().string();
				bool found = false;
				for (const auto& m : materials) {
					if (m.name == stem) {
						found = true;
						break;
					}
				}
				if (!found) {
					fs::remove(entry.path());
				}
			}
		}
	}

	for (const auto& mat : materials) {
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
			if (rule.chance != 1)
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

void MaterialManager::add_material(const Material& mat, Grid& grid) {
	Material m = mat;
	if (m.id == 0) {
		m.id = get_unused_id();
	} else {
		for (const auto& existing : materials) {
			if (existing.id == m.id) {
				m.id = get_unused_id();
				break;
			}
		}
	}

	m.packed_color = (255u << 24) | (m.color[2] << 16) | (m.color[1] << 8) | m.color[0];
	materials.push_back(m);
	rebuild_compiled_rules();
}

void MaterialManager::edit_material(unsigned char id, const Material& mat, Grid& grid) {
	if (id >= materials.size()) {
		return;
	}

	std::string old_name = materials[id].name;
	std::string new_name = mat.name;

	materials[id] = mat;
	materials[id].packed_color = (255u << 24) | (mat.color[2] << 16) | (mat.color[1] << 8) | mat.color[0];

	if (old_name != new_name) {
		for (auto& m : materials) {
			for (auto& r : m.user_rules) {
				for (auto& name : r.when) {
					if (name == old_name)
						name = new_name;
				}
				for (auto& name : r.then) {
					if (name == old_name)
						name = new_name;
				}
			}
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::remove_material(unsigned char id, Grid& grid) {
	std::string name = materials[id].name;
	unsigned char removed_id = materials[id].id;

	std::vector<unsigned char> old_to_new(256);
	for (int i = 0; i < 256; ++i) {
		old_to_new[i] = static_cast<unsigned char>(i);
	}
	old_to_new[removed_id] = 0;

	grid.remap_materials(old_to_new);

	materials.erase(materials.begin() + id);

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
	auto resolve_name_to_id = [](const std::string& name) -> unsigned char {
		if (name.empty())
			return 255;
		if (name == "air")
			return 0;
		for (size_t id = 0; id < materials.size(); ++id) {
			if (materials[id].name == name) {
				return materials[id].id;
			}
		}
		return 255;
	};

	auto flip_x_array =
		[](const std::array<unsigned char, NEIGHBOR_COUNT>& arr) -> std::array<unsigned char, NEIGHBOR_COUNT> {
		std::array<unsigned char, NEIGHBOR_COUNT> result{};
		for (unsigned char x = 0; x < NEIGHBOR_SIZE; x++) {
			for (unsigned char y = 0; y < NEIGHBOR_SIZE; y++) {
				result[y * NEIGHBOR_SIZE + (NEIGHBOR_SIZE - x - 1)] = arr[y * NEIGHBOR_SIZE + x];
			}
		}
		return result;
	};

	auto flip_y_array =
		[](const std::array<unsigned char, NEIGHBOR_COUNT>& arr) -> std::array<unsigned char, NEIGHBOR_COUNT> {
		std::array<unsigned char, NEIGHBOR_COUNT> result{};
		for (unsigned char x = 0; x < NEIGHBOR_SIZE; x++) {
			for (unsigned char y = 0; y < NEIGHBOR_SIZE; y++) {
				result[(NEIGHBOR_SIZE - y - 1) * NEIGHBOR_SIZE + x] = arr[y * NEIGHBOR_SIZE + x];
			}
		}
		return result;
	};

	auto rotate_90_array =
		[](const std::array<unsigned char, NEIGHBOR_COUNT>& arr) -> std::array<unsigned char, NEIGHBOR_COUNT> {
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

const Material& MaterialManager::get_material(unsigned char id) {
	if (id <= materials.size()) {
		return materials[id - 1];
	}
	static Material empty_mat;
	return empty_mat;
}

unsigned char MaterialManager::get_material_count() { return static_cast<unsigned char>(materials.size()); }

std::vector<Material>& MaterialManager::get_materials() { return materials; }