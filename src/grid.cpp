#include "grid.hpp"

#include <algorithm>
#include <thread>

#include "const.hpp"
#include "material_manager.hpp"
#include "vulkan.hpp"
#include "window.hpp"

thread_local uint32_t xorshift32_state = 123456789;

inline static uint32_t xorshift32() {
	xorshift32_state ^= xorshift32_state << 13;
	xorshift32_state ^= xorshift32_state >> 17;
	xorshift32_state ^= xorshift32_state << 5;
	return xorshift32_state;
}

uint32_t Grid::width = DEFAULT_SIM_WIDTH;
uint32_t Grid::height = DEFAULT_SIM_HEIGHT;
std::array<int, NEIGHBOR_COUNT> Grid::neighbor_offsets{};

ProcessingMode Grid::processing_mode = ProcessingMode::CPU;
std::vector<Cell> Grid::cells;
std::vector<Cell> Grid::next_cells;
uint32_t Grid::num_active_threads = 0;
std::unique_ptr<std::barrier<>> Grid::start_barrier;
std::unique_ptr<std::barrier<>> Grid::done_barrier;
std::unique_ptr<std::barrier<>> Grid::phase_barrier;
std::vector<std::thread> Grid::workers;
std::atomic<bool> Grid::shutdown_flag{false};
std::atomic<uint32_t> Grid::frame_changed{0};
bool Grid::gpu_data_valid = true;
bool Grid::gpu_needs_upload = true;

void Grid::recompute_neighbor_offsets() {
	for (int dy = -static_cast<int>(HALF_NEIGHBOR_SIZE); dy <= static_cast<int>(HALF_NEIGHBOR_SIZE); ++dy) {
		for (int dx = -static_cast<int>(HALF_NEIGHBOR_SIZE); dx <= static_cast<int>(HALF_NEIGHBOR_SIZE); ++dx) {
			uint32_t idx = (dy + HALF_NEIGHBOR_SIZE) * NEIGHBOR_SIZE + (dx + HALF_NEIGHBOR_SIZE);
			neighbor_offsets[idx] = dy * static_cast<int>(width) + dx;
		}
	}
}

void Grid::init() {
	recompute_neighbor_offsets();
	num_active_threads = Grid::get_num_strips_y() / 2;
	cells.resize(Grid::get_size());
	next_cells.resize(Grid::get_size());

	clear();

	// Initialize GPU simulation (Vulkan)
	if (Vulkan::init(width, height)) {
		sync_to_gpu();
	}

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

bool Grid::resize(uint32_t new_width, uint32_t new_height, bool preserve_content) {
	if (new_width == 0 || new_height == 0) {
		return false;
	}
	// Snap to multiple of 16 for GPU workgroup (16x16) and CPU strips (STRIP_HEIGHT=16)
	new_width = ((new_width + 15) / 16) * 16;
	new_height = ((new_height + 15) / 16) * 16;

	if (new_width == width && new_height == height) {
		return true;
	}

	// 1. If currently using GPU and GPU has updated data, sync down first so CPU has current cells
	if (processing_mode == ProcessingMode::GPU && !gpu_needs_upload) {
		sync_from_gpu();
	}

	// 2. Stop CPU worker threads
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

	// 3. Prepare new cell buffers
	std::vector<Cell> new_cells(new_width * new_height, Cell{0, false});
	if (preserve_content) {
		uint32_t copy_w = std::min(width, new_width);
		uint32_t copy_h = std::min(height, new_height);
		for (uint32_t y = 0; y < copy_h; ++y) {
			for (uint32_t x = 0; x < copy_w; ++x) {
				new_cells[y * new_width + x] = cells[y * width + x];
			}
		}
	}

	width = new_width;
	height = new_height;
	cells = std::move(new_cells);
	next_cells.assign(width * height, Cell{0, false});

	// 4. Recompute neighbor offsets for new width
	recompute_neighbor_offsets();

	// 5. Restart CPU worker threads
	shutdown_flag = false;
	frame_changed = 0;
	uint32_t max_threads = get_num_strips_y() / 2;
	if (num_active_threads == 0 || num_active_threads > max_threads) {
		num_active_threads = std::max(1u, max_threads);
	}

	start_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	done_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	phase_barrier = std::make_unique<std::barrier<>>(num_active_threads);

	workers.reserve(num_active_threads);
	for (uint32_t t = 0; t < num_active_threads; ++t) {
		workers.emplace_back(&Grid::worker_thread, t);
	}

	// 6. Resize Vulkan buffers
	if (Vulkan::is_available()) {
		Vulkan::resize(width, height);
	}

	// 7. Resize Window texture and buffer
	Window::resize_texture_and_buffer(width, height);

	// 8. Sync display
	if (processing_mode == ProcessingMode::GPU) {
		sync_to_gpu();
		if (Vulkan::is_available()) {
			Vulkan::refresh_display();
		}
	} else {
		draw();
	}

	return true;
}

void Grid::shutdown() {
	Vulkan::shutdown();

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

void Grid::set_processing_mode(ProcessingMode preset) {
	if (preset == processing_mode) {
		return;
	}
	if (processing_mode == ProcessingMode::GPU && preset != ProcessingMode::GPU) {
		if (!gpu_data_valid) {
			sync_from_gpu();
		}
	} else if (preset == ProcessingMode::GPU) {
		sync_to_gpu();
		if (Vulkan::is_available()) {
			Vulkan::refresh_display();
		}
	}
	processing_mode = preset;
}

void Grid::sync_to_gpu() {
	if (!Vulkan::is_available()) {
		return;
	}
	std::vector<uint8_t> mat_data(Grid::get_size());
	for (uint32_t i = 0; i < Grid::get_size(); ++i) {
		mat_data[i] = cells[i].material;
	}
	Vulkan::upload_grid(mat_data.data(), Grid::get_size());
	gpu_needs_upload = false;
	gpu_data_valid = true;
}

void Grid::sync_from_gpu() {
	if (!Vulkan::is_available()) {
		return;
	}
	std::vector<uint8_t> mat_data(Grid::get_size());
	Vulkan::download_grid(mat_data.data(), Grid::get_size());
	for (uint32_t i = 0; i < Grid::get_size(); ++i) {
		cells[i].material = mat_data[i];
		cells[i].updated = true;
	}
	gpu_data_valid = true;
	gpu_needs_upload = false;
	Grid::draw();
}

void Grid::keep_awake_gpu() {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available() && Vulkan::is_prevent_downclock_enabled()) {
		Vulkan::keep_awake();
	}
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

		uint32_t local_changed = 0;
		const bool reverse_x = (Window::get_frame_count() % 2 == 0);
		const bool reverse_y = (Window::get_frame_count() % 2 == 1);

		const bool swap_phases = (Window::get_frame_count() % 2 == 1);
		for (uint32_t p_id = 0; p_id < 2; ++p_id) {
			const uint32_t target_sy_mod = swap_phases ? (1 - p_id) : p_id;

			uint32_t strip_idx_in_phase = 0;
			for (int sy_id = 0; sy_id < Grid::get_num_strips_y(); ++sy_id) {
				const uint32_t sy = reverse_y ? (Grid::get_num_strips_y() - 1 - sy_id) : sy_id;
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
				match_found = Grid::try_apply_rule_fast(rule, cy * Grid::get_width() + cx, local_changed);
			} else {
				match_found = Grid::try_apply_rule_safe(rule, cx, cy, local_changed);
			}
		} else if (num_variants == 2) {
			const uint32_t start_id = xorshift32() & 1;
			for (uint32_t step = 0; step < 2; ++step) {
				const CompiledRuleVariant& rule = cur.variants[(start_id + step) & 1];
				if (is_fast_path) {
					if (Grid::try_apply_rule_fast(rule, cy * Grid::get_width() + cx, local_changed)) {
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
					if (Grid::try_apply_rule_fast(rule, cy * Grid::get_width() + cx, local_changed)) {
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
	const uint32_t y_end = std::min(Grid::get_height(), y_start + STRIP_HEIGHT);

	for (uint32_t y = 0; y < (y_end - y_start); ++y) {
		for (uint32_t x = 0; x < Grid::get_width(); ++x) {
			const uint32_t cx = reverse_x ? (Grid::get_width() - 1 - x) : x;
			const uint32_t cy = reverse_y ? (y_end - 1 - y) : (y_start + y);

			if (next_cells[cy * Grid::get_width() + cx].updated)
				continue;

			const auto& rules =
				MaterialManager::get_runtime_material(cells[cy * Grid::get_width() + cx].material).rules;
			if (rules.empty())
				continue;

			const bool is_fast_path = (cx >= 2 && cx < Grid::get_width() - 2 && cy >= 2 && cy < Grid::get_height() - 2);

			apply_compiled_rules(rules, cx, cy, is_fast_path, local_changed);
		}
	}
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

		if (tx >= Grid::get_width() || ty >= Grid::get_height()) {
			return false;
		}

		if (!rule.when[n_id].test(cells[ty * Grid::get_width() + tx].material)) {
			return false;
		}
	}

	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const uint32_t tx = x + dx;
			const uint32_t ty = y + dy;

			if (tx >= Grid::get_width() || ty >= Grid::get_height()) {
				continue;
			}

			if (next_cells[ty * Grid::get_width() + tx].updated) {
				return false;
			}
		}
	}

	next_cells[y * Grid::get_width() + x].updated = true;
	for (uint32_t n_id = 0; n_id < NEIGHBOR_COUNT; ++n_id) {
		if (rule.then[n_id] != 255) {
			const int dx = static_cast<int>(n_id % NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const int dy = static_cast<int>(n_id / NEIGHBOR_SIZE) - HALF_NEIGHBOR_SIZE;
			const uint32_t tx = x + dx;
			const uint32_t ty = y + dy;

			if (tx >= Grid::get_width() || ty >= Grid::get_height()) {
				continue;
			}

			const uint32_t target_id = ty * Grid::get_width() + tx;
			next_cells[target_id].material = rule.then[n_id];
			next_cells[target_id].updated = true;
			if (tx != x || ty != y) {
				local_changed++;
			}
		}
	}
	return true;
}

void Grid::update() {
	frame_changed = 0;

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::step(static_cast<uint32_t>(Window::get_frame_count()), gpu_needs_upload);
		uint32_t* staging = Vulkan::get_staging_buffer();
		if (staging) {
			for (size_t i = 0; i < Grid::get_size(); ++i) {
				cells[i].material = static_cast<uint8_t>(staging[i] & 0xFFu);
			}
		}
		gpu_data_valid = true;
		gpu_needs_upload = false;
		frame_changed = Vulkan::get_changed_cells();
		return;
	}

	next_cells = cells;
	for (auto& cell : next_cells) {
		cell.updated = false;
	}

	if (num_active_threads > 0) {
		start_barrier->arrive_and_wait();
		done_barrier->arrive_and_wait();
	}

	cells = next_cells;
}

void Grid::draw() {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		return;
	}

	uint32_t* buffer = Window::get_buffer();

	for (uint32_t id = 0; id < Grid::get_size(); ++id) {
		if (!cells[id].updated) {
			continue;
		}
		uint8_t cell = cells[id].material;
		buffer[id] = MaterialManager::get_runtime_material(cell).packed_color;
	}
}

void Grid::draw_material(uint32_t id) {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::refresh_display();
		return;
	}

	uint32_t* buffer = Window::get_buffer();

	for (uint32_t i = 0; i < Grid::get_size(); ++i) {
		if (cells[i].material == id) {
			buffer[i] = MaterialManager::get_runtime_material(cells[i].material).packed_color;
		}
	}
}

uint8_t& Grid::get_cell(const uint32_t x, const uint32_t y) { return cells[y * Grid::get_width() + x].material; }

void Grid::set_cell(const uint32_t x, const uint32_t y, uint8_t cell) {
	const uint32_t idx = y * Grid::get_width() + x;
	cells[idx].material = cell;
	cells[idx].updated = true;
	gpu_needs_upload = true;

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		uint32_t* disp = Vulkan::get_display_buffer();
		if (disp) {
			disp[idx] = MaterialManager::get_runtime_material(cell).packed_color;
		}
		uint32_t* staging = Vulkan::get_staging_buffer();
		if (staging) {
			staging[idx] = static_cast<uint32_t>(cell);
		}
	}
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
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		sync_to_gpu();
	}
}

void Grid::clear() {
	for (auto& cell : cells) {
		cell.material = 0;
		cell.updated = true;
	}
	if (Vulkan::is_available()) {
		Vulkan::clear();
	}
	gpu_needs_upload = false;
	gpu_data_valid = true;
}

void Grid::restore_state(const std::vector<uint8_t>& state) {
	if (state.size() != Grid::get_size()) {
		return;
	}

	for (size_t i = 0; i < Grid::get_size(); ++i) {
		cells[i].material = state[i];
		cells[i].updated = true;
	}

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::upload_grid(state.data(), Grid::get_size());
		gpu_needs_upload = false;
		gpu_data_valid = true;
	} else {
		uint32_t* buffer = Window::get_buffer();
		if (buffer) {
			for (size_t i = 0; i < Grid::get_size(); ++i) {
				buffer[i] = MaterialManager::get_runtime_material(state[i]).packed_color;
			}
		}
	}
}

std::vector<uint8_t> Grid::get_all_cells() {
	std::vector<uint8_t> state(Grid::get_size());
	for (size_t i = 0; i < Grid::get_size(); ++i) {
		state[i] = cells[i].material;
	}
	return state;
}