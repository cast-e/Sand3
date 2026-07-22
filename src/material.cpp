#include "material.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "grid.hpp"

namespace fs = std::filesystem;

std::vector<Material> MaterialManager::materials{};
std::string MaterialManager::current_set = "default";

void MaterialManager::load_all_materials(std::string_view directory_path) {
	materials.clear();

	std::vector<fs::path> json_files;
	if (fs::exists(directory_path.data()) && fs::is_directory(directory_path.data())) {
		for (const auto& entry : fs::directory_iterator(directory_path.data())) {
			if (entry.is_regular_file() && entry.path().extension() == ".json") {
				std::string filename = entry.path().filename().string();
				json_files.push_back(entry.path());
			}
		}
	}
	if (!json_files.empty()) {
		for (const auto& filepath : json_files) {
			std::ifstream file(filepath);
			nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
			Material mat;
			mat.name = j["name"].get<std::string>();
			mat.color = j["color"].get<std::array<unsigned char, 3>>();
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
			materials.push_back(mat);
		}
	}

	rebuild_compiled_rules();
}

void MaterialManager::save_all_materials(std::string_view directory_path) {
	fs::create_directories(directory_path.data());
	for (const auto& entry : fs::directory_iterator(directory_path.data())) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			std::string filename = entry.path().filename().string();
			bool found = false;
			for (const auto& m : materials) {
				if (m.name + ".json" == filename) {
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
		nlohmann::ordered_json j;
		j["name"] = mat.name;
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

		std::string filename = std::string(directory_path) + "/" + mat.name + ".json";
		std::ofstream file(filename);
		file << j.dump(4);
	}
}

void MaterialManager::add_material(const Material& mat, Grid& grid) {
	Material m = mat;
	m.packed_color = (255u << 24) | (mat.color[2] << 16) | (mat.color[1] << 8) | mat.color[0];
	materials.push_back(m);
	rebuild_compiled_rules();
}

void MaterialManager::edit_material(size_t index, const Material& mat, Grid& grid) {
	std::string old_name = materials[index].name;
	std::string new_name = mat.name;

	materials[index] = mat;
	materials[index].packed_color = (255u << 24) | (mat.color[2] << 16) | (mat.color[1] << 8) | mat.color[0];

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

void MaterialManager::remove_material(size_t index, Grid& grid) {
	std::string name = materials[index].name;

	std::vector<unsigned char> old_to_new;
	old_to_new.push_back(0);

	for (size_t i = 0; i < materials.size(); ++i) {
		if (i == index) {
			old_to_new.push_back(0);
		} else if (i < index) {
			old_to_new.push_back(static_cast<unsigned char>(i + 1));
		} else {
			old_to_new.push_back(static_cast<unsigned char>(i));
		}
	}

	grid.remap_materials(old_to_new);

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
	auto resolve_name_to_id = [](const std::string& name) -> unsigned char {
		if (name.empty())
			return 255;
		if (name == "air")
			return 0;
		for (size_t idx = 0; idx < materials.size(); ++idx) {
			if (materials[idx].name == name) {
				return static_cast<unsigned char>(idx + 1);
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
			for (size_t i = 0; i < NEIGHBOR_COUNT; ++i) {
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

const Material& MaterialManager::get_material(unsigned char id) { return materials[id - 1]; }

unsigned char MaterialManager::get_material_count() { return materials.size(); }

std::vector<Material>& MaterialManager::get_materials() { return materials; }

std::vector<std::string> MaterialManager::get_sets() {
	std::vector<std::string> sets;
	std::string base_dir = "../sets";
	fs::create_directories(base_dir);

	for (const auto& entry : fs::directory_iterator(base_dir)) {
		if (entry.is_directory()) {
			sets.push_back(entry.path().filename().string());
		}
	}

	if (sets.empty()) {
		std::string default_path = base_dir + "/default";
		fs::create_directories(default_path);

		std::string parent_dir = "../sets";
		if (fs::exists(parent_dir) && fs::is_directory(parent_dir)) {
			for (const auto& entry : fs::directory_iterator(parent_dir)) {
				if (entry.is_regular_file() && entry.path().extension() == ".json") {
					try {
						fs::copy_file(entry.path(), default_path + "/" + entry.path().filename().string(),
									  fs::copy_options::overwrite_existing);
					} catch (...) {}
				}
			}
		}
		sets.push_back("default");
	}

	std::sort(sets.begin(), sets.end());
	return sets;
}

std::string MaterialManager::get_current_set() { return current_set; }

void MaterialManager::set_current_set(const std::string& set_name, Grid& grid) {
	current_set = set_name;
	grid.clear();
	load_all_materials("../sets/" + set_name);
}

void MaterialManager::copy_set(const std::string& set_name, Grid& grid) {
	std::string set_path = "../sets/" + set_name;
	fs::create_directories(set_path);
	current_set = set_name;
}

void MaterialManager::create_new_empty_set(const std::string& set_name, Grid& grid) {
	std::string set_path = "../sets/" + set_name;
	fs::create_directories(set_path);
	current_set = set_name;
	materials.clear();
	rebuild_compiled_rules();
	grid.clear();
}

void MaterialManager::delete_set(const std::string& set_name, Grid& grid) {
	std::string set_path = "../sets/" + set_name;
	if (fs::exists(set_path)) {
		fs::remove_all(set_path);
	}
	set_current_set("default", grid);
}