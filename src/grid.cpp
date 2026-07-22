#include "grid.hpp"

#include <algorithm>
#include <thread>

#include "const.hpp"
#include "material_manager.hpp"
#include "save_manager.hpp"

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
			for (unsigned int p_id = 0; p_id < 2; ++p_id) {
				const unsigned int target_sy_mod = swap_phases ? (1 - p_id) : p_id;

				for (int sy_id = 0; sy_id < NUM_STRIPS_Y; ++sy_id) {
					const unsigned int sy = reverse_y ? (NUM_STRIPS_Y - 1 - sy_id) : sy_id;
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
			const unsigned int start_id = xorshift32() & 1;
			for (unsigned int step = 0; step < 2; ++step) {
				const Rule& rule = cur.variants[(start_id + step) & 1];
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
			unsigned int perm_id = xorshift32() % 24;
			for (unsigned int step = 0; step < 4; ++step) {
				const Rule& rule = cur.variants[perms[perm_id][step]];
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

			const unsigned int center_id = cy * SIM_WIDTH + cx;

			if (cells[center_id].first == 0 || next_cells[center_id].second) {
				continue;
			}

			const auto& rules = MaterialManager::get_material(cells[center_id].first).compiled_rules;
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

bool Grid::try_apply_rule_fast(const Rule& rule, const unsigned int center_id, unsigned int& local_changed) {
	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.when[n_id] == 255) {
			continue;
		}
		if (cells[center_id + neighbor_offsets[n_id]].first != rule.when[n_id]) {
			return false;
		}
	}

	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			unsigned int target_id = center_id + neighbor_offsets[n_id];
			if (next_cells[target_id].second) {
				return false;
			}
		}
	}

	next_cells[center_id].second = true;
	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			unsigned int target_id = center_id + neighbor_offsets[n_id];
			next_cells[target_id].first = rule.then[n_id];
			next_cells[target_id].second = true;
			if (target_id != center_id) {
				local_changed++;
			}
		}
	}
	return true;
}

bool Grid::try_apply_rule_safe(const Rule& rule, const unsigned int x, const unsigned int y,
							   unsigned int& local_changed) {
	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.when[n_id] == 255) {
			continue;
		}

		const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
		const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
		const unsigned int tx = x + dx;
		const unsigned int ty = y + dy;

		if (tx >= SIM_WIDTH || ty >= SIM_HEIGHT) {
			return false;
		}

		if (cells[ty * SIM_WIDTH + tx].first != rule.when[n_id]) {
			return false;
		}
	}

	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
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
	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const unsigned int tx = x + dx;
			const unsigned int ty = y + dy;

			next_cells[ty * SIM_WIDTH + tx].first = rule.then[n_id];
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

	for (unsigned int id = 0; id < SIM_SIZE; ++id) {
		if (!cells[id].second) {
			continue;
		}
		unsigned char cell = cells[id].first;
		if (cell != 0) {
			buffer[id] = MaterialManager::get_material(cell).packed_color;
		} else {
			buffer[id] = BG_COLOR;
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