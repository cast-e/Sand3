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

void MaterialManager::load_all_materials(std::string_view directory_path) {
	materials.clear();

	Material empty_mat;
	empty_mat.name = "empty";
	empty_mat.id = 0;
	empty_mat.color = {64, 64, 64};
	empty_mat.packed_color = (255u << 24) | (64u << 16) | (64u << 8) | 64u;

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

	std::vector<Material> other_mats;

	for (const auto& filepath : mat_files) {
		std::ifstream file(filepath);
		if (!file.is_open())
			continue;

		try {
			nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
			Material mat;
			mat.name = j.value("name", "");

			if (mat.name == "empty" || (j.contains("id") && j["id"].is_number() && j["id"].get<int>() == 0)) {
				mat.name = "empty";
				mat.id = 0;
				if (j.contains("color")) {
					mat.color = j["color"].get<std::array<unsigned char, 3>>();
				} else {
					mat.color = {64, 64, 64};
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
				empty_mat = mat;
				continue;
			}

			if (j.contains("id") && j["id"].is_number()) {
				mat.id = static_cast<unsigned char>(j["id"].get<int>());
			} else {
				mat.id = 0;
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

			other_mats.push_back(mat);
		} catch (...) {}
	}

	std::sort(other_mats.begin(), other_mats.end(), [](const Material& a, const Material& b) { return a.id < b.id; });

	for (auto& mat : other_mats) {
		if (mat.id == 0) {
			mat.id = get_unused_id();
		}
		materials.push_back(mat);
	}

	materials.push_back(empty_mat);

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
						if (m.id == 0 || m.name == "empty") {
							bool is_changed =
								(m.color[0] != 64 || m.color[1] != 64 || m.color[2] != 64 || !m.user_rules.empty());
							if (!is_changed) {
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
	}

	for (const auto& mat : materials) {
		if (mat.id == 0 || mat.name == "empty") {
			bool is_changed =
				(mat.color[0] != 64 || mat.color[1] != 64 || mat.color[2] != 64 || !mat.user_rules.empty());
			if (!is_changed) {
				continue;
			}
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

void MaterialManager::add_material(const Material& mat) {
	Material m = mat;
	m.id = get_unused_id();

	m.packed_color = (255u << 24) | (m.color[2] << 16) | (m.color[1] << 8) | m.color[0];
	materials.push_back(m);
	rebuild_compiled_rules();

	std::swap(materials[materials.size() - 2], materials[materials.size() - 1]);
}

void MaterialManager::edit_material(unsigned char id, const Material& mat) {
	if (id >= materials.size()) {
		return;
	}

	if (materials[id].id == 0 || materials[id].name == "empty") {
		Material updated = mat;
		updated.name = "empty";
		updated.id = 0;
		materials[id] = updated;
		materials[id].packed_color =
			(255u << 24) | (updated.color[2] << 16) | (updated.color[1] << 8) | updated.color[0];
		rebuild_compiled_rules();
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

void MaterialManager::remove_material(unsigned char id) {
	if (id >= materials.size() || materials[id].id == 0 || materials[id].name == "empty") {
		return;
	}

	std::string name = materials[id].name;
	unsigned char removed_id = materials[id].id;

	std::vector<unsigned char> old_to_new(256);
	for (int i = 0; i < 256; ++i) {
		old_to_new[i] = static_cast<unsigned char>(i);
	}
	old_to_new[removed_id] = 0;

	Grid::remap_materials(old_to_new);

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
		if (name == "empty")
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
	for (const auto& m : materials) {
		if (m.id == id) {
			return m;
		}
	}
	static Material default_empty{"empty", 0, {64, 64, 64}, (255u << 24) | (64u << 16) | (64u << 8) | 64u, {}, {}};
	return default_empty;
}

unsigned char MaterialManager::get_material_count() { return static_cast<unsigned char>(materials.size()); }

std::vector<Material>& MaterialManager::get_materials() { return materials; }