#include "grid.hpp"

#include <algorithm>
#include <thread>

#include "const.hpp"
#include "material_manager.hpp"
#include "window.hpp"

thread_local uint32_t xorshift32_state = 123456789;

inline static uint32_t xorshift32() {
	xorshift32_state ^= xorshift32_state << 13;
	xorshift32_state ^= xorshift32_state >> 17;
	xorshift32_state ^= xorshift32_state << 5;
	return xorshift32_state;
}

QualityPreset Grid::quality_preset = QualityPreset::Fast;
std::vector<Cell> Grid::cells;
std::vector<Cell> Grid::next_cells;
unsigned int Grid::num_active_threads = 0;
std::unique_ptr<std::barrier<>> Grid::start_barrier;
std::unique_ptr<std::barrier<>> Grid::done_barrier;
std::unique_ptr<std::barrier<>> Grid::phase_barrier;
std::vector<std::thread> Grid::workers;
std::atomic<bool> Grid::shutdown_flag{false};
std::atomic<unsigned int> Grid::frame_changed{0};

void Grid::init() {
	num_active_threads = NUM_STRIPS_Y / 2;
	cells.resize(SIM_SIZE);
	next_cells.resize(SIM_SIZE);

	clear();

	shutdown_flag = false;
	frame_changed = 0;

	start_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	done_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	phase_barrier = std::make_unique<std::barrier<>>(num_active_threads);

	workers.reserve(num_active_threads);
	for (unsigned int t = 0; t < num_active_threads; ++t) {
		workers.emplace_back(&Grid::worker_thread, t);
	}
}

void Grid::shutdown() {
	shutdown_flag = true;
	if (start_barrier) {
		start_barrier->arrive_and_wait();
	}
	for (auto& worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	workers.clear();
	start_barrier.reset();
	done_barrier.reset();
	phase_barrier.reset();
}

void Grid::configure_threads(unsigned int thread_count) {
	shutdown_flag = true;
	if (start_barrier) {
		start_barrier->arrive_and_wait();
	}
	for (auto& worker : workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	workers.clear();

	shutdown_flag = false;
	num_active_threads = thread_count;

	if (num_active_threads > 0) {
		start_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
		done_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
		phase_barrier = std::make_unique<std::barrier<>>(num_active_threads);

		workers.reserve(num_active_threads);
		for (unsigned int t = 0; t < num_active_threads; ++t) {
			workers.emplace_back(&Grid::worker_thread, t);
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
		if (shutdown_flag) {
			break;
		}

		if (quality_preset == QualityPreset::Slow) {
			done_barrier->arrive_and_wait();
			continue;
		}

		unsigned int local_changed = 0;
		const bool reverse_x = (Window::get_frame_count() % 2 == 0);
		const bool reverse_y = (Window::get_frame_count() % 2 == 1);

		if (quality_preset == QualityPreset::Fast) {
			const bool swap_phases = (Window::get_frame_count() % 2 == 1);
			for (unsigned int p_id = 0; p_id < 2; ++p_id) {
				const unsigned int target_sy_mod = swap_phases ? (1 - p_id) : p_id;

				unsigned int strip_idx_in_phase = 0;
				for (int sy_id = 0; sy_id < NUM_STRIPS_Y; ++sy_id) {
					const unsigned int sy = reverse_y ? (NUM_STRIPS_Y - 1 - sy_id) : sy_id;
					if (sy % 2 != target_sy_mod) {
						continue;
					}

					if (strip_idx_in_phase % num_active_threads == thread_id) {
						update_strip_1d(sy, reverse_x, reverse_y, local_changed);
					}
					strip_idx_in_phase++;
				}
				phase_barrier->arrive_and_wait();
			}
		}

		frame_changed += local_changed;
		done_barrier->arrive_and_wait();
	}
}

inline static void apply_compiled_rules(const std::vector<CompiledUserRule>& rules, const unsigned int cx,
										const unsigned cy, const bool is_fast_path, unsigned int& local_changed) {
	for (const CompiledUserRule& cur : rules) {
		if (xorshift32() % (100 * 1000) >= static_cast<uint32_t>(cur.chance * 1000))
			continue;

		const size_t num_variants = cur.variants.size();
		bool match_found = false;

		if (num_variants == 1) {
			const Rule& rule = cur.variants[0];
			if (is_fast_path) {
				match_found = Grid::try_apply_rule_fast(rule, cy * SIM_WIDTH + cx, local_changed);
			} else {
				match_found = Grid::try_apply_rule_safe(rule, cx, cy, local_changed);
			}
		} else if (num_variants == 2) {
			const unsigned int start_id = xorshift32() & 1;
			for (unsigned int step = 0; step < 2; ++step) {
				const Rule& rule = cur.variants[(start_id + step) & 1];
				if (is_fast_path) {
					if (Grid::try_apply_rule_fast(rule, cy * SIM_WIDTH + cx, local_changed)) {
						match_found = true;
						break;
					}
				} else {
					if (Grid::try_apply_rule_safe(rule, cx, cy, local_changed)) {
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
					if (Grid::try_apply_rule_fast(rule, cy * SIM_WIDTH + cx, local_changed)) {
						match_found = true;
						break;
					}
				} else {
					if (Grid::try_apply_rule_safe(rule, cx, cy, local_changed)) {
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

			if (next_cells[cy * SIM_WIDTH + cx].updated)
				continue;

			const auto& rules = MaterialManager::get_material(cells[cy * SIM_WIDTH + cx].material).compiled_rules;
			if (rules.empty())
				continue;

			const bool is_fast_path = (cx >= 2 && cx < SIM_WIDTH - 2 && cy >= 2 && cy < SIM_HEIGHT - 2);

			apply_compiled_rules(rules, cx, cy, is_fast_path, local_changed);
		}
	}
}

void Grid::update_sequential() {
	unsigned int local_changed = 0;
	const bool reverse_x = (Window::get_frame_count() % 2 == 0);
	const bool reverse_y = (Window::get_frame_count() % 2 == 1);

	for (int y = 0; y < SIM_HEIGHT; ++y) {
		for (unsigned int x = 0; x < SIM_WIDTH; ++x) {
			const unsigned int cx = reverse_x ? (SIM_WIDTH - 1 - x) : x;
			const unsigned int cy = reverse_y ? (SIM_HEIGHT - 1 - y) : y;

			const unsigned int center_id = cy * SIM_WIDTH + cx;

			if (next_cells[center_id].updated) {
				continue;
			}

			const auto& rules = MaterialManager::get_material(cells[center_id].material).compiled_rules;
			if (rules.empty()) {
				continue;
			}

			bool is_fast_path = (cx >= 2 && cx < SIM_WIDTH - 2 && cy >= 2 && cy < SIM_HEIGHT - 2);

			xorshift32_state = (cx + 123456789ULL) * (cy + 123456789ULL) * (Window::get_frame_count() + 123456789ULL);

			apply_compiled_rules(rules, cx, cy, is_fast_path, local_changed);
		}
	}
	frame_changed = local_changed;
}

bool Grid::try_apply_rule_fast(const Rule& rule, const unsigned int center_id, unsigned int& local_changed) {
	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.when[n_id] == 255) {
			continue;
		}
		if (cells[center_id + neighbor_offsets[n_id]].material != rule.when[n_id]) {
			return false;
		}
	}

	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			unsigned int target_id = center_id + neighbor_offsets[n_id];
			if (next_cells[target_id].updated) {
				return false;
			}
		}
	}

	next_cells[center_id].updated = true;
	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			unsigned int target_id = center_id + neighbor_offsets[n_id];
			next_cells[target_id].material = rule.then[n_id];
			next_cells[target_id].updated = true;
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

		if (cells[ty * SIM_WIDTH + tx].material != rule.when[n_id]) {
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

			if (next_cells[ty * SIM_WIDTH + tx].updated) {
				return false;
			}
		}
	}

	next_cells[y * SIM_WIDTH + x].updated = true;
	for (unsigned int n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const unsigned int tx = x + dx;
			const unsigned int ty = y + dy;

			next_cells[ty * SIM_WIDTH + tx].material = rule.then[n_id];
			next_cells[ty * SIM_WIDTH + tx].updated = true;
			if (tx != x || ty != y) {
				local_changed++;
			}
		}
	}
	return true;
}

void Grid::update() {
	frame_changed = 0;

	next_cells = cells;
	for (auto& cell : next_cells) {
		cell.updated = false;
	}

	if (quality_preset == QualityPreset::Slow || num_active_threads == 0) {
		update_sequential();
	} else {
		start_barrier->arrive_and_wait();
		done_barrier->arrive_and_wait();
	}

	cells = next_cells;
}

void Grid::draw() {
	uint32_t* buffer = Window::get_buffer();

	for (unsigned int id = 0; id < SIM_SIZE; ++id) {
		if (!cells[id].updated) {
			continue;
		}
		unsigned char cell = cells[id].material;
		buffer[id] = MaterialManager::get_material(cell).packed_color;
	}
}

void Grid::draw_material(unsigned int id) {
	uint32_t* buffer = Window::get_buffer();

	for (unsigned int id = 0; id < SIM_SIZE; ++id) {
		if (cells[id].material == id) {
			continue;
		}
		unsigned char cell = cells[id].material;
		buffer[id] = MaterialManager::get_material(cell).packed_color;
	}
}

unsigned char& Grid::get_cell(const unsigned int x, const unsigned int y) { return cells[y * SIM_WIDTH + x].material; }
void Grid::set_cell(const unsigned int x, const unsigned int y, unsigned char cell) {
	cells[y * SIM_WIDTH + x].material = cell;
	cells[y * SIM_WIDTH + x].updated = true;
}

unsigned int Grid::get_changed_cells() { return frame_changed.load(); }

void Grid::remap_materials(const std::vector<unsigned char>& old_to_new) {
	for (auto& cell : cells) {
		if (cell.material < old_to_new.size()) {
			cell.material = old_to_new[cell.material];
		} else {
			cell.material = 0;
		}
	}
}

void Grid::clear() {
	for (auto& cell : cells) {
		cell.material = 0;
		cell.updated = true;
	}
}