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
uint32_t Grid::num_active_threads = 0;
std::unique_ptr<std::barrier<>> Grid::start_barrier;
std::unique_ptr<std::barrier<>> Grid::done_barrier;
std::unique_ptr<std::barrier<>> Grid::phase_barrier;
std::vector<std::thread> Grid::workers;
std::atomic<bool> Grid::shutdown_flag{false};
std::atomic<uint32_t> Grid::frame_changed{0};

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
	for (uint32_t t = 0; t < num_active_threads; ++t) {
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

void Grid::configure_threads(uint32_t thread_count) {
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
		for (uint32_t t = 0; t < num_active_threads; ++t) {
			workers.emplace_back(&Grid::worker_thread, t);
		}
	} else {
		start_barrier.reset();
		done_barrier.reset();
		phase_barrier.reset();
	}
}

void Grid::worker_thread(const uint32_t thread_id) {
	while (true) {
		start_barrier->arrive_and_wait();
		if (shutdown_flag) {
			break;
		}

		if (quality_preset == QualityPreset::Slow) {
			done_barrier->arrive_and_wait();
			continue;
		}

		uint32_t local_changed = 0;
		const bool reverse_x = (Window::get_frame_count() % 2 == 0);
		const bool reverse_y = (Window::get_frame_count() % 2 == 1);

		if (quality_preset == QualityPreset::Fast) {
			const bool swap_phases = (Window::get_frame_count() % 2 == 1);
			for (uint32_t p_id = 0; p_id < 2; ++p_id) {
				const uint32_t target_sy_mod = swap_phases ? (1 - p_id) : p_id;

				uint32_t strip_idx_in_phase = 0;
				for (int sy_id = 0; sy_id < NUM_STRIPS_Y; ++sy_id) {
					const uint32_t sy = reverse_y ? (NUM_STRIPS_Y - 1 - sy_id) : sy_id;
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

inline static void apply_compiled_rules(const std::vector<CompiledRule>& rules, const uint32_t cx, const unsigned cy,
										const bool is_fast_path, uint32_t& local_changed) {
	for (const CompiledRule& cur : rules) {
		if (xorshift32() % (100 * 1000) >= static_cast<uint32_t>(cur.chance * 1000))
			continue;

		const size_t num_variants = cur.variants.size();
		bool match_found = false;

		if (num_variants == 1) {
			const CompiledRuleVariant& rule = cur.variants[0];
			if (is_fast_path) {
				match_found = Grid::try_apply_rule_fast(rule, cy * SIM_WIDTH + cx, local_changed);
			} else {
				match_found = Grid::try_apply_rule_safe(rule, cx, cy, local_changed);
			}
		} else if (num_variants == 2) {
			const uint32_t start_id = xorshift32() & 1;
			for (uint32_t step = 0; step < 2; ++step) {
				const CompiledRuleVariant& rule = cur.variants[(start_id + step) & 1];
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
			static const uint8_t perms[24][4] = {{0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2},
												 {0, 3, 2, 1}, {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0},
												 {1, 3, 0, 2}, {1, 3, 2, 0}, {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3},
												 {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0}, {3, 0, 1, 2}, {3, 0, 2, 1},
												 {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
			uint32_t perm_id = xorshift32() % 24;
			for (uint32_t step = 0; step < 4; ++step) {
				const CompiledRuleVariant& rule = cur.variants[perms[perm_id][step]];
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

void Grid::update_strip_1d(const uint32_t sy, const bool reverse_x, const bool reverse_y, uint32_t& local_changed) {
	const uint32_t y_start = sy * STRIP_HEIGHT;
	const uint32_t y_end = std::min(SIM_HEIGHT, y_start + STRIP_HEIGHT);

	for (uint32_t y = 0; y < (y_end - y_start); ++y) {
		for (uint32_t x = 0; x < SIM_WIDTH; ++x) {
			const uint32_t cx = reverse_x ? (SIM_WIDTH - 1 - x) : x;
			const uint32_t cy = reverse_y ? (y_end - 1 - y) : (y_start + y);

			if (next_cells[cy * SIM_WIDTH + cx].updated)
				continue;

			const auto& rules = MaterialManager::get_runtime_material(cells[cy * SIM_WIDTH + cx].material).rules;
			if (rules.empty())
				continue;

			const bool is_fast_path = (cx >= 2 && cx < SIM_WIDTH - 2 && cy >= 2 && cy < SIM_HEIGHT - 2);

			apply_compiled_rules(rules, cx, cy, is_fast_path, local_changed);
		}
	}
}

void Grid::update_sequential() {
	uint32_t local_changed = 0;
	const bool reverse_x = (Window::get_frame_count() % 2 == 0);
	const bool reverse_y = (Window::get_frame_count() % 2 == 1);

	for (int y = 0; y < SIM_HEIGHT; ++y) {
		for (uint32_t x = 0; x < SIM_WIDTH; ++x) {
			const uint32_t cx = reverse_x ? (SIM_WIDTH - 1 - x) : x;
			const uint32_t cy = reverse_y ? (SIM_HEIGHT - 1 - y) : y;

			const uint32_t center_id = cy * SIM_WIDTH + cx;

			if (next_cells[center_id].updated) {
				continue;
			}

			const auto& rules = MaterialManager::get_runtime_material(cells[center_id].material).rules;
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

bool Grid::try_apply_rule_fast(const CompiledRuleVariant& rule, const uint32_t center_id, uint32_t& local_changed) {
	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.when[n_id].all()) {
			continue;
		}
		if (!rule.when[n_id].test(cells[center_id + neighbor_offsets[n_id]].material)) {
			return false;
		}
	}

	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			uint32_t target_id = center_id + neighbor_offsets[n_id];
			if (next_cells[target_id].updated) {
				return false;
			}
		}
	}

	next_cells[center_id].updated = true;
	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			uint32_t target_id = center_id + neighbor_offsets[n_id];
			next_cells[target_id].material = rule.then[n_id];
			next_cells[target_id].updated = true;
			if (target_id != center_id) {
				local_changed++;
			}
		}
	}
	return true;
}

bool Grid::try_apply_rule_safe(const CompiledRuleVariant& rule, const uint32_t x, const uint32_t y,
							   uint32_t& local_changed) {
	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.when[n_id].all()) {
			continue;
		}

		const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
		const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
		const uint32_t tx = x + dx;
		const uint32_t ty = y + dy;

		if (tx >= SIM_WIDTH || ty >= SIM_HEIGHT) {
			return false;
		}

		if (!rule.when[n_id].test(cells[ty * SIM_WIDTH + tx].material)) {
			return false;
		}
	}

	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const uint32_t tx = x + dx;
			const uint32_t ty = y + dy;

			if (tx >= SIM_WIDTH || ty >= SIM_HEIGHT) {
				return false;
			}

			if (next_cells[ty * SIM_WIDTH + tx].updated) {
				return false;
			}
		}
	}

	next_cells[y * SIM_WIDTH + x].updated = true;
	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const uint32_t tx = x + dx;
			const uint32_t ty = y + dy;

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

	for (uint32_t id = 0; id < SIM_SIZE; ++id) {
		if (!cells[id].updated) {
			continue;
		}
		uint8_t cell = cells[id].material;
		buffer[id] = MaterialManager::get_runtime_material(cell).packed_color;
	}
}

void Grid::draw_material(uint32_t id) {
	uint32_t* buffer = Window::get_buffer();

	for (uint32_t i = 0; i < SIM_SIZE; ++i) {
		if (cells[i].material == id) {
			buffer[i] = MaterialManager::get_runtime_material(cells[i].material).packed_color;
		}
	}
}

uint8_t& Grid::get_cell(const uint32_t x, const uint32_t y) { return cells[y * SIM_WIDTH + x].material; }
void Grid::set_cell(const uint32_t x, const uint32_t y, uint8_t cell) {
	cells[y * SIM_WIDTH + x].material = cell;
	cells[y * SIM_WIDTH + x].updated = true;
}

uint32_t Grid::get_changed_cells() { return frame_changed.load(); }

void Grid::remap_materials(const std::vector<uint8_t>& old_to_new) {
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