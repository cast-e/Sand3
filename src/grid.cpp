#include "grid.hpp"

#include <algorithm>
#include <fstream>
#include <thread>

#include "const.hpp"
#include "material.hpp"

thread_local uint32_t xorshift32_state = 123456789;

inline static uint32_t xorshift32() {
	xorshift32_state ^= xorshift32_state << 13;
	xorshift32_state ^= xorshift32_state >> 17;
	xorshift32_state ^= xorshift32_state << 5;
	return xorshift32_state;
}

static unsigned int get_concurrency_threads(unsigned int total_cells) {
	unsigned int n = std::thread::hardware_concurrency();
	if (n == 0)
		n = 8;
	return std::max(1u, std::min(n, total_cells));
}

Grid::Grid() : num_active_threads(get_concurrency_threads(SIM_SIZE)) {
	cells.resize(SIM_SIZE);
	next_cells.resize(SIM_SIZE);

	clear();

	start_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	done_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	phase_barrier = std::make_unique<std::barrier<>>(num_active_threads);

	workers.reserve(num_active_threads);
	for (unsigned int t = 0; t < num_active_threads; ++t) {
		workers.emplace_back(&Grid::worker_thread, this, t);
	}
}

Grid::~Grid() {
	shutdown = true;
	if (start_barrier) {
		start_barrier->arrive_and_wait();
	}
	for (auto& worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
}

void Grid::configure_threads(unsigned int thread_count) {
	shutdown = true;
	if (start_barrier) {
		start_barrier->arrive_and_wait();
	}
	for (auto& worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	workers.clear();

	shutdown = false;
	num_active_threads = thread_count;

	if (num_active_threads > 0) {
		start_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
		done_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
		phase_barrier = std::make_unique<std::barrier<>>(num_active_threads);

		workers.reserve(num_active_threads);
		for (unsigned int t = 0; t < num_active_threads; ++t) {
			workers.emplace_back(&Grid::worker_thread, this, t);
		}
	} else {
		start_barrier.reset();
		done_barrier.reset();
		phase_barrier.reset();
	}
}

void Grid::worker_thread(const unsigned int thread_id) {
	while (true) {
		start_barrier->arrive_and_wait();
		if (shutdown) {
			break;
		}

		if (quality_preset == QualityPreset::Slow) {
			done_barrier->arrive_and_wait();
			continue;
		}

		unsigned int local_changed = 0;
		const bool reverse_x = (frame_count % 2 == 0);
		const bool reverse_y = (frame_count % 2 == 1);

		if (quality_preset == QualityPreset::Fast) {
			const bool swap_phases = (frame_count % 2 == 1);
			for (unsigned int p_idx = 0; p_idx < 2; ++p_idx) {
				const unsigned int target_sy_mod = swap_phases ? (1 - p_idx) : p_idx;

				for (int sy_idx = 0; sy_idx < NUM_STRIPS_Y; ++sy_idx) {
					const unsigned int sy = reverse_y ? (NUM_STRIPS_Y - 1 - sy_idx) : sy_idx;
					if (sy % 2 != target_sy_mod) {
						continue;
					}

					if (sy % num_active_threads != thread_id) {
						continue;
					}

					update_strip_1d(sy, reverse_x, reverse_y, local_changed);
				}
				phase_barrier->arrive_and_wait();
			}
		}

		frame_changed += local_changed;
		done_barrier->arrive_and_wait();
	}
}

inline static void apply_compiled_rules(const std::vector<CompiledUserRule>& rules, const unsigned int cx,
										const unsigned cy, const bool is_fast_path, unsigned int& local_changed,
										Grid& grid) {
	for (const CompiledUserRule& cur : rules) {
		if (cur.chance > 1 && (xorshift32() % cur.chance) != 0)
			continue;

		const size_t num_variants = cur.variants.size();
		bool match_found = false;

		if (num_variants == 1) {
			const Rule& rule = cur.variants[0];
			if (is_fast_path) {
				match_found = grid.try_apply_rule_fast(rule, cy * SIM_WIDTH + cx, local_changed);
			} else {
				match_found = grid.try_apply_rule_safe(rule, cx, cy, local_changed);
			}
		} else if (num_variants == 2) {
			const unsigned int start_idx = xorshift32() & 1;
			for (unsigned int step = 0; step < 2; ++step) {
				const Rule& rule = cur.variants[(start_idx + step) & 1];
				if (is_fast_path) {
					if (grid.try_apply_rule_fast(rule, cy * SIM_WIDTH + cx, local_changed)) {
						match_found = true;
						break;
					}
				} else {
					if (grid.try_apply_rule_safe(rule, cx, cy, local_changed)) {
						match_found = true;
						break;
					}
				}
			}
		} else if (num_variants == 4) {
			static const unsigned char perms[24][4] = {
				{0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
				{1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
				{2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
				{3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
			unsigned int perm_idx = xorshift32() % 24;
			for (unsigned int step = 0; step < 4; ++step) {
				const Rule& rule = cur.variants[perms[perm_idx][step]];
				if (is_fast_path) {
					if (grid.try_apply_rule_fast(rule, cy * SIM_WIDTH + cx, local_changed)) {
						match_found = true;
						break;
					}
				} else {
					if (grid.try_apply_rule_safe(rule, cx, cy, local_changed)) {
						match_found = true;
						break;
					}
				}
			}
		}

		if (match_found) {
			break;
		}
	}
}

void Grid::update_strip_1d(const unsigned int sy, const bool reverse_x, const bool reverse_y,
						   unsigned int& local_changed) {
	const unsigned int y_start = sy * STRIP_HEIGHT;
	const unsigned int y_end = std::min(SIM_HEIGHT, y_start + STRIP_HEIGHT);

	for (unsigned int y = 0; y < (y_end - y_start); ++y) {
		for (unsigned int x = 0; x < SIM_WIDTH; ++x) {
			const unsigned int cx = reverse_x ? (SIM_WIDTH - 1 - x) : x;
			const unsigned int cy = reverse_y ? (y_end - 1 - y) : (y_start + y);

			if (cells[cy * SIM_WIDTH + cx].first == 0 || next_cells[cy * SIM_WIDTH + cx].second)
				continue;

			const auto& rules = MaterialManager::get_material(cells[cy * SIM_WIDTH + cx].first).compiled_rules;
			if (rules.empty())
				continue;

			const bool is_fast_path = (cx >= 2 && cx < SIM_WIDTH - 2 && cy >= 2 && cy < SIM_HEIGHT - 2);

			apply_compiled_rules(rules, cx, cy, is_fast_path, local_changed, *this);
		}
	}
}

void Grid::update_sequential() {
	unsigned int local_changed = 0;
	const bool reverse_x = (frame_count % 2 == 0);
	const bool reverse_y = (frame_count % 2 == 1);

	for (int y = 0; y < SIM_HEIGHT; ++y) {
		for (unsigned int x = 0; x < SIM_WIDTH; ++x) {
			const unsigned int cx = reverse_x ? (SIM_WIDTH - 1 - x) : x;
			const unsigned int cy = reverse_y ? (SIM_HEIGHT - 1 - y) : y;

			const unsigned int center_idx = cy * SIM_WIDTH + cx;

			if (cells[center_idx].first == 0 || next_cells[center_idx].second) {
				continue;
			}

			const auto& rules = MaterialManager::get_material(cells[center_idx].first).compiled_rules;
			if (rules.empty()) {
				continue;
			}

			bool is_fast_path = (cx >= 2 && cx < SIM_WIDTH - 2 && cy >= 2 && cy < SIM_HEIGHT - 2);

			xorshift32_state = (cx + 123456789ULL) * (cy + 123456789ULL) * (frame_count + 123456789ULL);

			apply_compiled_rules(rules, cx, cy, is_fast_path, local_changed, *this);
		}
	}
	frame_changed = local_changed;
}

bool Grid::try_apply_rule_fast(const Rule& rule, const unsigned int center_idx, unsigned int& local_changed) {
	for (unsigned int n_idx = 0; n_idx < NEIGHBOR_COUNT; ++n_idx) {
		if (rule.when[n_idx] == 255) {
			continue;
		}
		if (cells[center_idx + neighbor_offsets[n_idx]].first != rule.when[n_idx]) {
			return false;
		}
	}

	for (unsigned int n_idx = 0; n_idx < NEIGHBOR_COUNT; ++n_idx) {
		if (rule.then[n_idx] != 255) {
			unsigned int target_idx = center_idx + neighbor_offsets[n_idx];
			if (next_cells[target_idx].second) {
				return false;
			}
		}
	}

	next_cells[center_idx].second = true;
	for (unsigned int n_idx = 0; n_idx < NEIGHBOR_COUNT; ++n_idx) {
		if (rule.then[n_idx] != 255) {
			unsigned int target_idx = center_idx + neighbor_offsets[n_idx];
			next_cells[target_idx].first = rule.then[n_idx];
			next_cells[target_idx].second = true;
			if (target_idx != center_idx) {
				local_changed++;
			}
		}
	}
	return true;
}

bool Grid::try_apply_rule_safe(const Rule& rule, const unsigned int x, const unsigned int y,
							   unsigned int& local_changed) {
	for (unsigned int n_idx = 0; n_idx < NEIGHBOR_COUNT; ++n_idx) {
		if (rule.when[n_idx] == 255) {
			continue;
		}

		const int dx = static_cast<int>(n_idx % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
		const int dy = static_cast<int>(n_idx / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
		const unsigned int tx = x + dx;
		const unsigned int ty = y + dy;

		if (tx >= SIM_WIDTH || ty >= SIM_HEIGHT) {
			return false;
		}

		if (cells[ty * SIM_WIDTH + tx].first != rule.when[n_idx]) {
			return false;
		}
	}

	for (unsigned int n_idx = 0; n_idx < NEIGHBOR_COUNT; ++n_idx) {
		if (rule.then[n_idx] != 255) {
			const int dx = static_cast<int>(n_idx % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_idx / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const unsigned int tx = x + dx;
			const unsigned int ty = y + dy;

			if (tx >= SIM_WIDTH || ty >= SIM_HEIGHT) {
				return false;
			}

			if (next_cells[ty * SIM_WIDTH + tx].second) {
				return false;
			}
		}
	}

	next_cells[y * SIM_WIDTH + x].second = true;
	for (unsigned int n_idx = 0; n_idx < NEIGHBOR_COUNT; ++n_idx) {
		if (rule.then[n_idx] != 255) {
			const int dx = static_cast<int>(n_idx % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_idx / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const unsigned int tx = x + dx;
			const unsigned int ty = y + dy;

			next_cells[ty * SIM_WIDTH + tx].first = rule.then[n_idx];
			next_cells[ty * SIM_WIDTH + tx].second = true;
			if (tx != x || ty != y) {
				local_changed++;
			}
		}
	}
	return true;
}

void Grid::update() {
	frame_changed = 0;
	frame_count++;

	next_cells = cells;
	for (auto& cell : next_cells) {
		cell.second = false;
	}

	if (quality_preset == QualityPreset::Slow || num_active_threads == 0) {
		update_sequential();
	} else {
		start_barrier->arrive_and_wait();
		done_barrier->arrive_and_wait();
	}

	cells = next_cells;
}

void Grid::draw(Window& window) {
	uint32_t* buffer = window.get_buffer();

	for (unsigned int idx = 0; idx < SIM_SIZE; ++idx) {
		if (!cells[idx].second) {
			continue;
		}
		unsigned char cell = cells[idx].first;
		if (cell != 0) {
			buffer[idx] = MaterialManager::get_material(cell).packed_color;
		} else {
			buffer[idx] = BG_COLOR;
		}
	}
}

unsigned char& Grid::get_cell(const unsigned int x, const unsigned int y) { return cells[y * SIM_WIDTH + x].first; }
unsigned char Grid::get_cell(const unsigned int x, const unsigned int y) const {
	return cells[y * SIM_WIDTH + x].first;
}
void Grid::set_cell(const unsigned int x, const unsigned int y, unsigned char cell) {
	cells[y * SIM_WIDTH + x].first = cell;
	cells[y * SIM_WIDTH + x].second = true;
}

unsigned int Grid::get_changed_cells() const { return frame_changed.load(); }

void Grid::remap_materials(const std::vector<unsigned char>& old_to_new) {
	for (auto& cell : cells) {
		if (cell.first < old_to_new.size()) {
			cell.first = old_to_new[cell.first];
		} else {
			cell.first = 0;
		}
	}
}

void Grid::clear() { std::fill(cells.begin(), cells.end(), std::pair<unsigned char, bool>{0, true}); }

bool Grid::save_to_file(const std::string& name, const std::string& current_set) const {
	std::string filepath = "../sets/" + current_set + "/" + name;
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}
	std::ofstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	file << current_set << "\n";

	for (unsigned int y = 0; y < SIM_HEIGHT; ++y) {
		std::string row;
		row.reserve(SIM_WIDTH + 1);
		for (unsigned int x = 0; x < SIM_WIDTH; ++x) {
			const unsigned char cell = get_cell(x, y);
			if (cell == 0) {
				row.push_back('.');
			} else {
				row.push_back(static_cast<char>('a' + cell - 1));
			}
		}
		row.push_back('\n');
		file.write(row.data(), row.size());
	}
	return true;
}

bool Grid::load_from_file(const std::string& path_or_name, const std::string& current_set, std::string& loaded_set) {
	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos) {
		filepath = "../sets/" + current_set + "/" + path_or_name;
	}
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}

	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::string set_line;
	if (!std::getline(file, set_line))
		return false;

	while (!set_line.empty() && (set_line.back() == '\r' || set_line.back() == '\n' || set_line.back() == ' ')) {
		set_line.pop_back();
	}
	loaded_set = set_line;

	MaterialManager::set_current_set(loaded_set, *this);
	unsigned int mat_count = MaterialManager::get_material_count();

	clear();

	for (unsigned int y = 0; y < SIM_HEIGHT; ++y) {
		std::string line;
		if (!std::getline(file, line))
			break;
		for (unsigned int x = 0; x < SIM_WIDTH && x < line.size(); ++x) {
			const char c = line[x];
			unsigned char cell = 0;
			if (c >= 'a' && c < 'a' + static_cast<int>(mat_count)) {
				cell = static_cast<unsigned char>(c - 'a' + 1);
			}
			set_cell(x, y, cell);
		}
	}
	return true;
}