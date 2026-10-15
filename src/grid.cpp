#include "grid.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

#include "const.hpp"
#include "material_manager.hpp"
#include "set_manager.hpp"
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
int Grid::max_rule_radius_x = 2;
int Grid::max_rule_radius_y = 2;

ProcessingMode Grid::processing_mode = ProcessingMode::CPU;
std::vector<Cell> Grid::cells;
std::vector<Cell> Grid::next_cells;
std::vector<uint8_t> Grid::updated_cells;
uint32_t Grid::num_active_threads = 0;
std::unique_ptr<std::barrier<>> Grid::start_barrier;
std::unique_ptr<std::barrier<>> Grid::done_barrier;
std::unique_ptr<std::barrier<>> Grid::phase_barrier;
std::vector<std::thread> Grid::workers;
std::atomic<bool> Grid::shutdown_flag{false};
std::atomic<uint32_t> Grid::frame_changed{0};
bool Grid::gpu_data_valid = true;
bool Grid::gpu_needs_upload = true;
uint64_t Grid::sim_step_count = 0;
int Grid::heatmap_attr_index = -1;
bool Grid::heatmap_logarithmic = false;
bool Grid::heatmap_custom_range = false;
float Grid::heatmap_view_min = 0.0f;
float Grid::heatmap_view_max = 255.0f;
AttributeLayout Grid::attr_layout = AttributeLayout::Layout_3x16;
uint32_t Grid::attr_types = 0;

void Grid::init() {
	if (!workers.empty()) {
		shutdown();
	}
	num_active_threads = Grid::get_num_strips_y() / 2;
	cells.resize(Grid::get_size());
	next_cells.resize(Grid::get_size());
	updated_cells.assign(Grid::get_size(), 0);

	clear();

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

	new_width = ((new_width + 15) / 16) * 16;
	new_height = ((new_height + 15) / 16) * 16;

	if (new_width == width && new_height == height) {
		return true;
	}

	if (processing_mode == ProcessingMode::GPU && !gpu_needs_upload) {
		sync_from_gpu();
	}

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

	Cell empty_cell = make_empty_cell();
	std::vector<Cell> new_cells(new_width * new_height, empty_cell);
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
	next_cells.assign(width * height, empty_cell);
	updated_cells.assign(width * height, 0);

	MaterialManager::rebuild_compiled_rules();

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

	if (Vulkan::is_available()) {
		Vulkan::resize(width, height);
	}

	Window::resize_texture_and_buffer(width, height);

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
		processing_mode = preset;
		sync_from_gpu();
	} else if (preset == ProcessingMode::GPU) {
		processing_mode = preset;
		sync_to_gpu();
		if (Vulkan::is_available()) {
			Vulkan::refresh_display();
		}
	} else {
		processing_mode = preset;
	}
}

void Grid::sync_to_gpu() {
	if (!Vulkan::is_available()) {
		return;
	}
	Vulkan::upload_grid(cells.data(), Grid::get_size());
	gpu_needs_upload = false;
	gpu_data_valid = true;
}

void Grid::sync_from_gpu() {
	if (!Vulkan::is_available()) {
		return;
	}
	if (gpu_needs_upload) {
		Vulkan::copy_staging_to_grid();
		gpu_needs_upload = false;
	}
	Vulkan::download_grid(cells.data(), Grid::get_size());
	gpu_data_valid = true;
	gpu_needs_upload = false;
	Grid::draw();
}

void Grid::keep_awake_gpu() {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available() && Vulkan::is_prevent_downclock_enabled()) {
		Vulkan::keep_awake();
	}
}

void Grid::refresh_gpu_display() {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::refresh_display(gpu_needs_upload);
		gpu_needs_upload = false;
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
	uint32_t max_threads = std::max(1u, get_num_strips_y() / 2);
	if (thread_count == 0) {
		thread_count = max_threads;
	}
	num_active_threads = std::clamp(thread_count, 1u, max_threads);

	start_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	done_barrier = std::make_unique<std::barrier<>>(num_active_threads + 1);
	phase_barrier = std::make_unique<std::barrier<>>(num_active_threads);

	workers.reserve(num_active_threads);
	for (uint32_t t = 0; t < num_active_threads; ++t) {
		workers.emplace_back(&Grid::worker_thread, t);
	}
}

void Grid::worker_thread(const uint32_t thread_id) {
	while (true) {
		start_barrier->arrive_and_wait();
		if (shutdown_flag) {
			break;
		}

		uint32_t local_changed = 0;
		const bool reverse_x = (sim_step_count % 2 == 0);
		const bool reverse_y = (sim_step_count % 2 == 1);

		const bool swap_phases = (sim_step_count % 2 == 1);
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

bool Grid::apply_compiled_rules(const std::vector<CompiledRule>& rules, const uint32_t cx, const unsigned cy,
								const bool is_fast_path, uint32_t& local_changed) {
	bool any_match_found = false;
	const uint32_t center_id = cy * Grid::get_width() + cx;

	for (const CompiledRule& cur : rules) {
		if (xorshift32() % (100 * 1000) >= static_cast<uint32_t>(cur.chance * 1000))
			continue;

		const size_t num_variants = cur.variants.size();
		bool match_found = false;

		if (num_variants == 1) {
			const CompiledRuleVariant& rule = cur.variants[0];
			if (is_fast_path) {
				match_found = Grid::try_apply_rule_fast(rule, center_id, local_changed);
			} else {
				match_found = Grid::try_apply_rule_safe(rule, cx, cy, local_changed);
			}
		} else if (num_variants == 2) {
			const uint32_t start_id = xorshift32() & 1;
			for (uint32_t step = 0; step < 2; ++step) {
				const CompiledRuleVariant& rule = cur.variants[(start_id + step) & 1];
				if (is_fast_path) {
					if (Grid::try_apply_rule_fast(rule, center_id, local_changed)) {
						match_found = true;
						if (!cur.multi_apply) {
							break;
						}
					}
				} else {
					if (Grid::try_apply_rule_safe(rule, cx, cy, local_changed)) {
						match_found = true;
						if (!cur.multi_apply) {
							break;
						}
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
					if (Grid::try_apply_rule_fast(rule, center_id, local_changed)) {
						match_found = true;
						if (!cur.multi_apply) {
							break;
						}
					}
				} else {
					if (Grid::try_apply_rule_safe(rule, cx, cy, local_changed)) {
						match_found = true;
						if (!cur.multi_apply) {
							break;
						}
					}
				}
			}
		} else if (num_variants > 0) {
			const uint32_t start_id = xorshift32() % num_variants;
			for (uint32_t step = 0; step < num_variants; ++step) {
				const CompiledRuleVariant& rule = cur.variants[(start_id + step) % num_variants];
				if (is_fast_path) {
					if (Grid::try_apply_rule_fast(rule, center_id, local_changed)) {
						match_found = true;
						if (!cur.multi_apply) {
							break;
						}
					}
				} else {
					if (Grid::try_apply_rule_safe(rule, cx, cy, local_changed)) {
						match_found = true;
						if (!cur.multi_apply) {
							break;
						}
					}
				}
			}
		}

		if (match_found) {
			any_match_found = true;
			if (!cur.continue_to_next) {
				break;
			}
		}
	}

	return any_match_found;
}

void Grid::update_strip_1d(const uint32_t sy, const bool reverse_x, const bool reverse_y, uint32_t& local_changed) {
	const uint32_t y_start = sy * STRIP_HEIGHT;
	const uint32_t y_end = std::min(Grid::get_height(), y_start + STRIP_HEIGHT);

	for (uint32_t y = 0; y < (y_end - y_start); ++y) {
		for (uint32_t x = 0; x < Grid::get_width(); ++x) {
			const uint32_t cx = reverse_x ? (Grid::get_width() - 1 - x) : x;
			const uint32_t cy = reverse_y ? (y_end - 1 - y) : (y_start + y);
			const uint32_t center_id = cy * Grid::get_width() + cx;

			if (updated_cells[center_id])
				continue;

			const auto& rules =
				MaterialManager::get_runtime_material(cells[center_id].material).rules;
			if (rules.empty())
				continue;

			const bool is_fast_path = (cx >= static_cast<uint32_t>(max_rule_radius_x) &&
									   cx < Grid::get_width() - static_cast<uint32_t>(max_rule_radius_x) &&
									   cy >= static_cast<uint32_t>(max_rule_radius_y) &&
									   cy < Grid::get_height() - static_cast<uint32_t>(max_rule_radius_y));

			apply_compiled_rules(rules, cx, cy, is_fast_path, local_changed);
		}
	}
}

inline static bool eval_attr_condition(const AttributeCondition& ac, const Cell& target_cell, const Cell& center_cell) {
	float val = target_cell.get_attr_f(ac.attr_index);
	float target_val = 0.0f;
	switch (ac.target) {
		case CompareTarget::Constant:
			target_val = static_cast<float>(ac.const_value);
			break;
		case CompareTarget::CenterCell:
			target_val = center_cell.get_attr_f(ac.attr_index);
			break;
	}
	switch (ac.op) {
		case CompareOp::Equal:
			return std::abs(val - target_val) < 1e-4f;
		case CompareOp::NotEqual:
			return std::abs(val - target_val) >= 1e-4f;
		case CompareOp::Less:
			return val < target_val;
		case CompareOp::LessOrEqual:
			return val <= target_val;
		case CompareOp::Greater:
			return val > target_val;
		case CompareOp::GreaterOrEqual:
			return val >= target_val;
		default:
			break;
	}
	return true;
}

bool Grid::try_apply_rule_fast(const CompiledRuleVariant& rule, const uint32_t center_id, uint32_t& local_changed) {
	const uint32_t gw = Grid::get_width();

	for (const auto& cond : rule.conditions) {
		const Cell& c = cells[center_id + cond.offset];
		if (!cond.allowed.test(c.material)) {
			return false;
		}
		if (!cond.attr_conditions.empty()) {
			const Cell& center_c = next_cells[center_id];
			const Cell& target_c = next_cells[center_id + cond.offset];
			for (const auto& ac : cond.attr_conditions) {
				if (!eval_attr_condition(ac, target_c, center_c)) {
					return false;
				}
			}
		}
	}

	for (const auto& act : rule.actions) {
		uint32_t target_id = center_id + act.offset;
		if (updated_cells[target_id]) {
			return false;
		}
	}

	Cell saved_center = next_cells[center_id];
	const auto& set_attrs = SetManager::get_current_metadata().attributes;
	const uint32_t gh = Grid::get_height();
	uint32_t cx = center_id % gw;
	uint32_t cy = center_id / gw;
	auto get_attr = [&](int8_t dx, int8_t dy, uint8_t attr_i) -> float {
		if (dx == 0 && dy == 0) {
			return saved_center.get_attr_f(attr_i);
		}
		int64_t sx = static_cast<int64_t>(cx) + dx;
		int64_t sy = static_cast<int64_t>(cy) + dy;
		if (sx >= 0 && sx < static_cast<int64_t>(gw) && sy >= 0 && sy < static_cast<int64_t>(gh)) {
			uint32_t s_id = static_cast<uint32_t>(sy) * gw + static_cast<uint32_t>(sx);
			return next_cells[s_id].get_attr_f(attr_i);
		}
		return (attr_i < set_attrs.size()) ? static_cast<float>(set_attrs[attr_i].default_val) : 0.0f;
	};

	struct StagedAction {
		uint32_t target_id;
		uint8_t target_material;
		float attr_vals[3];
		bool has_attr[3];
		bool updated;
	};
	std::array<StagedAction, 100> staged;
	size_t num_staged = 0;

	for (const auto& act : rule.actions) {
		if (num_staged >= staged.size())
			break;
		uint32_t target_id = center_id + act.offset;
		StagedAction& sa = staged[num_staged++];
		sa.target_id = target_id;
		sa.target_material = act.target_material;
		sa.updated = act.updated;

		for (size_t a = 0; a < 3; ++a) {
			sa.has_attr[a] = false;
			if (act.attr_expr_ops[a].empty())
				continue;
			float min_v = (a < set_attrs.size()) ? static_cast<float>(set_attrs[a].min_val) : -32768.0f;
			float max_v = (a < set_attrs.size()) ? static_cast<float>(set_attrs[a].max_val) : 32767.0f;
			float s_val = next_cells[target_id].get_attr_f(a);
			float c_val = saved_center.get_attr_f(a);
			sa.attr_vals[a] =
				eval_expr(act.attr_expr_ops[a], s_val, c_val, min_v, max_v, static_cast<uint8_t>(a), get_attr);
			sa.has_attr[a] = true;
		}
	}

	for (size_t i = 0; i < num_staged; ++i) {
		const auto& sa = staged[i];
		if (sa.target_material != 255) {
			if (sa.target_material != next_cells[sa.target_id].material) {
				next_cells[sa.target_id].material = sa.target_material;
				const auto& def = MaterialManager::get_material(sa.target_material);
				for (size_t a = 0; a < 3; ++a) {
					next_cells[sa.target_id].set_attr_f(a, static_cast<float>(def.default_attributes[a]));
				}
			}
		}
		for (size_t a = 0; a < 3; ++a) {
			if (sa.has_attr[a]) {
				next_cells[sa.target_id].set_attr_f(a, sa.attr_vals[a]);
			}
		}
		if (sa.updated) {
			updated_cells[sa.target_id] = 1;
		}
		if (sa.target_id != center_id) {
			local_changed++;
		}
	}
	return true;
}

bool Grid::try_apply_rule_safe(const CompiledRuleVariant& rule, const uint32_t x, const uint32_t y,
							   uint32_t& local_changed) {
	const uint32_t gw = Grid::get_width();
	const uint32_t gh = Grid::get_height();
	const uint32_t center_id = y * gw + x;

	for (const auto& cond : rule.conditions) {
		const uint32_t tx = x + cond.dx;
		const uint32_t ty = y + cond.dy;

		if (tx >= gw || ty >= gh) {
			return false;
		}

		const Cell& c = cells[ty * gw + tx];
		if (!cond.allowed.test(c.material)) {
			return false;
		}
		if (!cond.attr_conditions.empty()) {
			const Cell& center_c = next_cells[center_id];
			const Cell& target_c = next_cells[ty * gw + tx];
			for (const auto& ac : cond.attr_conditions) {
				if (!eval_attr_condition(ac, target_c, center_c)) {
					return false;
				}
			}
		}
	}

	for (const auto& act : rule.actions) {
		const uint32_t tx = x + act.dx;
		const uint32_t ty = y + act.dy;

		if (tx >= gw || ty >= gh) {
			continue;
		}

		const uint32_t target_id = ty * gw + tx;
		if (updated_cells[target_id]) {
			return false;
		}
	}

	Cell saved_center = next_cells[center_id];
	const auto& set_attrs = SetManager::get_current_metadata().attributes;
	auto get_attr = [&](int8_t dx, int8_t dy, uint8_t attr_i) -> float {
		if (dx == 0 && dy == 0) {
			return saved_center.get_attr_f(attr_i);
		}
		int64_t sx = static_cast<int64_t>(x) + dx;
		int64_t sy = static_cast<int64_t>(y) + dy;
		if (sx >= 0 && sx < static_cast<int64_t>(gw) && sy >= 0 && sy < static_cast<int64_t>(gh)) {
			return next_cells[static_cast<uint32_t>(sy) * gw + static_cast<uint32_t>(sx)].get_attr_f(attr_i);
		}
		return (attr_i < set_attrs.size()) ? static_cast<float>(set_attrs[attr_i].default_val) : 0.0f;
	};

	struct StagedAction {
		uint32_t target_id;
		uint32_t tx;
		uint32_t ty;
		uint8_t target_material;
		float attr_vals[3];
		bool has_attr[3];
		bool updated;
	};
	std::array<StagedAction, 100> staged;
	size_t num_staged = 0;

	for (const auto& act : rule.actions) {
		const uint32_t tx = x + act.dx;
		const uint32_t ty = y + act.dy;

		if (tx >= gw || ty >= gh || num_staged >= staged.size()) {
			continue;
		}

		const uint32_t target_id = ty * gw + tx;
		StagedAction& sa = staged[num_staged++];
		sa.target_id = target_id;
		sa.tx = tx;
		sa.ty = ty;
		sa.target_material = act.target_material;
		sa.updated = act.updated;

		for (size_t a = 0; a < 3; ++a) {
			sa.has_attr[a] = false;
			if (act.attr_expr_ops[a].empty())
				continue;
			float min_v = (a < set_attrs.size()) ? static_cast<float>(set_attrs[a].min_val) : -32768.0f;
			float max_v = (a < set_attrs.size()) ? static_cast<float>(set_attrs[a].max_val) : 32767.0f;
			float s_val = next_cells[target_id].get_attr_f(a);
			float c_val = saved_center.get_attr_f(a);
			sa.attr_vals[a] =
				eval_expr(act.attr_expr_ops[a], s_val, c_val, min_v, max_v, static_cast<uint8_t>(a), get_attr);
			sa.has_attr[a] = true;
		}
	}

	for (size_t i = 0; i < num_staged; ++i) {
		const auto& sa = staged[i];
		if (sa.target_material != 255) {
			if (sa.target_material != next_cells[sa.target_id].material) {
				next_cells[sa.target_id].material = sa.target_material;
				const auto& def = MaterialManager::get_material(sa.target_material);
				for (size_t a = 0; a < 3; ++a) {
					next_cells[sa.target_id].set_attr_f(a, static_cast<float>(def.default_attributes[a]));
				}
			}
		}
		for (size_t a = 0; a < 3; ++a) {
			if (sa.has_attr[a]) {
				next_cells[sa.target_id].set_attr_f(a, sa.attr_vals[a]);
			}
		}
		if (sa.updated) {
			updated_cells[sa.target_id] = 1;
		}
		if (sa.tx != x || sa.ty != y) {
			local_changed++;
		}
	}
	return true;
}

void Grid::update() {
	frame_changed = 0;

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::step(static_cast<uint32_t>(sim_step_count), gpu_needs_upload);
		Cell* staging = Vulkan::get_staging_cells();
		if (staging) {
			std::memcpy(cells.data(), staging, Grid::get_size() * sizeof(Cell));
		}
		gpu_data_valid = true;
		gpu_needs_upload = false;
		frame_changed = Vulkan::get_changed_cells();
		sim_step_count++;
		return;
	}

	next_cells = cells;
	if (updated_cells.size() != Grid::get_size()) {
		updated_cells.assign(Grid::get_size(), 0);
	} else {
		std::memset(updated_cells.data(), 0, updated_cells.size());
	}

	if (num_active_threads > 0) {
		start_barrier->arrive_and_wait();
		done_barrier->arrive_and_wait();
	}

	cells = next_cells;
	sim_step_count++;
}

void Grid::update_attr_layout() {
	const auto& attrs = SetManager::get_current_metadata().attributes;
	attr_types = 0;
	if (attrs.size() <= 1) {
		attr_layout = AttributeLayout::Layout_1x32;
	} else if (attrs.size() == 2) {
		attr_layout = AttributeLayout::Layout_1x32_1x16;
	} else {
		attr_layout = AttributeLayout::Layout_3x16;
	}

	for (size_t i = 0; i < attrs.size() && i < 3; ++i) {
		if (attrs[i].type == AttributeType::Float) {
			attr_types |= (1u << i);
		}
		if (attrs[i].min_val >= 0.0) {
			attr_types |= (1u << (i + 4));
		}
	}
}

void Grid::set_heatmap_mode(int attr_idx) {
	heatmap_attr_index = attr_idx;
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::set_heatmap_mode(attr_idx);
		Vulkan::refresh_display(gpu_needs_upload);
		return;
	}
	draw();
}

void Grid::set_heatmap_logarithmic(bool log_mode) {
	heatmap_logarithmic = log_mode;
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::refresh_display(gpu_needs_upload);
		return;
	}
	draw();
}

void Grid::set_heatmap_custom_range(bool custom_range) {
	heatmap_custom_range = custom_range;
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::refresh_display(gpu_needs_upload);
		return;
	}
	draw();
}

void Grid::set_heatmap_view_min(float v_min) {
	heatmap_view_min = v_min;
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::refresh_display(gpu_needs_upload);
		return;
	}
	draw();
}

void Grid::set_heatmap_view_max(float v_max) {
	heatmap_view_max = v_max;
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::refresh_display(gpu_needs_upload);
		return;
	}
	draw();
}

void Grid::draw() {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		return;
	}

	uint32_t* buffer = Window::get_buffer();
	if (!buffer) {
		return;
	}

	if (heatmap_attr_index >= 0) {
		const auto& meta = SetManager::get_current_metadata();
		if (heatmap_attr_index < static_cast<int>(meta.attributes.size())) {
			const auto& attr_def = meta.attributes[heatmap_attr_index];
			float min_v = static_cast<float>(attr_def.min_val);
			float max_v = static_cast<float>(attr_def.max_val);
			float range = (max_v > min_v) ? (max_v - min_v) : 1.0f;

			for (uint32_t id = 0; id < Grid::get_size(); ++id) {
				float val = cells[id].get_attr_f(heatmap_attr_index);
				auto col = meta.heatmap_gradient.sample(val);
				uint8_t r = static_cast<uint8_t>(std::clamp(col[0] * 255.0f, 0.0f, 255.0f));
				uint8_t g = static_cast<uint8_t>(std::clamp(col[1] * 255.0f, 0.0f, 255.0f));
				uint8_t b = static_cast<uint8_t>(std::clamp(col[2] * 255.0f, 0.0f, 255.0f));
				buffer[id] = (255u << 24) | (b << 16) | (g << 8) | r;
			}
			return;
		}
	}

	for (uint32_t id = 0; id < Grid::get_size(); ++id) {
		uint8_t cell = cells[id].material;
		const auto& rm = MaterialManager::get_runtime_material(cell);
		buffer[id] = compute_cell_color(rm, cells[id]);
	}
}

void Grid::draw_material(uint32_t id) {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		refresh_gpu_display();
		return;
	}

	uint32_t* buffer = Window::get_buffer();

	for (uint32_t i = 0; i < Grid::get_size(); ++i) {
		if (cells[i].material == id) {
			const auto& rm = MaterialManager::get_runtime_material(cells[i].material);
			buffer[i] = compute_cell_color(rm, cells[i]);
		}
	}
}

uint8_t& Grid::get_cell(const uint32_t x, const uint32_t y) { return cells[y * Grid::get_width() + x].material; }

Cell& Grid::get_cell_full(const uint32_t x, const uint32_t y) { return cells[y * width + x]; }

const Cell& Grid::get_cell_full_const(const uint32_t x, const uint32_t y) { return cells[y * width + x]; }

void Grid::set_cell(const uint32_t x, const uint32_t y, uint8_t material) {
	if (x >= width || y >= height) {
		return;
	}
	const uint32_t idx = y * width + x;
	cells[idx].material = material;
	const auto& def = MaterialManager::get_material(material);
	for (size_t a = 0; a < 3; ++a) {
		cells[idx].set_attr_f(a, static_cast<float>(def.default_attributes[a]));
	}
	gpu_needs_upload = true;

	const auto& rm = MaterialManager::get_runtime_material(material);
	uint32_t cell_col = compute_cell_color(rm, cells[idx]);

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		uint32_t* disp = Vulkan::get_display_buffer();
		if (disp) {
			disp[idx] = cell_col;
		}
		Cell* staging = Vulkan::get_staging_cells();
		if (staging) {
			staging[idx] = cells[idx];
		}
	} else {
		uint32_t* buffer = Window::get_buffer();
		if (buffer) {
			buffer[idx] = cell_col;
		}
	}
}

void Grid::set_cell_material_only(const uint32_t x, const uint32_t y, const uint8_t material) {
	if (x >= width || y >= height) {
		return;
	}
	const uint32_t idx = y * width + x;
	cells[idx].material = material;
	gpu_needs_upload = true;

	const auto& rm = MaterialManager::get_runtime_material(material);
	uint32_t cell_col = compute_cell_color(rm, cells[idx]);

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		uint32_t* disp = Vulkan::get_display_buffer();
		if (disp) {
			disp[idx] = cell_col;
		}
		Cell* staging = Vulkan::get_staging_cells();
		if (staging) {
			staging[idx] = cells[idx];
		}
	} else {
		uint32_t* buffer = Window::get_buffer();
		if (buffer) {
			buffer[idx] = cell_col;
		}
	}
}

void Grid::adjust_cell_attr(const uint32_t x, const uint32_t y, const uint8_t attr_idx, const float delta_or_val,
							const bool set_exact) {
	if (x >= width || y >= height) {
		return;
	}
	const uint32_t idx = y * width + x;
	const auto& meta = SetManager::get_current_metadata();
	if (attr_idx >= meta.attributes.size()) {
		return;
	}

	float min_v = static_cast<float>(meta.attributes[attr_idx].min_val);
	float max_v = static_cast<float>(meta.attributes[attr_idx].max_val);

	float cur_val = cells[idx].get_attr_f(attr_idx);
	float new_val = set_exact ? delta_or_val : (cur_val + delta_or_val);
	new_val = std::clamp(new_val, min_v, max_v);
	cells[idx].set_attr_f(attr_idx, new_val);
	gpu_needs_upload = true;

	uint32_t cell_col;
	if (heatmap_attr_index == static_cast<int>(attr_idx)) {
		auto col = meta.heatmap_gradient.sample(new_val);
		uint8_t r = static_cast<uint8_t>(std::clamp(col[0] * 255.0f, 0.0f, 255.0f));
		uint8_t g = static_cast<uint8_t>(std::clamp(col[1] * 255.0f, 0.0f, 255.0f));
		uint8_t b = static_cast<uint8_t>(std::clamp(col[2] * 255.0f, 0.0f, 255.0f));
		cell_col = (255u << 24) | (b << 16) | (g << 8) | r;
	} else {
		const auto& rm = MaterialManager::get_runtime_material(cells[idx].material);
		cell_col = compute_cell_color(rm, cells[idx]);
	}

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		uint32_t* disp = Vulkan::get_display_buffer();
		if (disp) {
			disp[idx] = cell_col;
		}
		Cell* staging = Vulkan::get_staging_cells();
		if (staging) {
			staging[idx] = cells[idx];
		}
	} else {
		uint32_t* buffer = Window::get_buffer();
		if (buffer) {
			buffer[idx] = cell_col;
		}
	}
}

void Grid::set_cell_full(const uint32_t x, const uint32_t y, const Cell& cell) {
	if (x >= width || y >= height) {
		return;
	}
	const uint32_t idx = y * width + x;
	cells[idx] = cell;
	gpu_needs_upload = true;

	const auto& rm = MaterialManager::get_runtime_material(cell.material);
	uint32_t cell_col = compute_cell_color(rm, cells[idx]);

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		uint32_t* disp = Vulkan::get_display_buffer();
		if (disp) {
			disp[idx] = cell_col;
		}
		Cell* staging = Vulkan::get_staging_cells();
		if (staging) {
			staging[idx] = cells[idx];
		}
	} else {
		uint32_t* buffer = Window::get_buffer();
		if (buffer) {
			buffer[idx] = cell_col;
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
	} else {
		draw();
	}
}

Cell Grid::make_empty_cell() {
	Cell cell{};
	cell.material = 0;
	const auto& def = MaterialManager::get_material(0);
	for (size_t a = 0; a < 3; ++a) {
		cell.set_attr_f(a, static_cast<float>(def.default_attributes[a]));
	}
	return cell;
}

void Grid::clear() {
	Cell empty_cell = make_empty_cell();
	for (auto& cell : cells) {
		cell = empty_cell;
	}
	for (auto& cell : next_cells) {
		cell = empty_cell;
	}
	updated_cells.assign(Grid::get_size(), 0);
	if (Vulkan::is_available()) {
		Vulkan::clear();
	}
	gpu_needs_upload = false;
	gpu_data_valid = true;
	draw();
	frame_changed = 0;
}

void Grid::restore_state(const std::vector<uint8_t>& state) {
	if (state.size() != Grid::get_size()) {
		return;
	}

	const auto& meta = SetManager::get_current_metadata();
	for (size_t i = 0; i < Grid::get_size(); ++i) {
		cells[i].material = state[i];
		const auto& def = MaterialManager::get_material(state[i]);
		for (size_t a = 0; a < 3; ++a) {
			float min_v = (a < meta.attributes.size()) ? static_cast<float>(meta.attributes[a].min_val) : -1e9f;
			float max_v = (a < meta.attributes.size()) ? static_cast<float>(meta.attributes[a].max_val) : 1e9f;
			float val = std::clamp(static_cast<float>(def.default_attributes[a]), min_v, max_v);
			cells[i].set_attr_f(a, val);
		}
	}

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::upload_grid(cells.data(), Grid::get_size());
		gpu_needs_upload = false;
		gpu_data_valid = true;
	} else {
		uint32_t* buffer = Window::get_buffer();
		if (buffer) {
			for (size_t i = 0; i < Grid::get_size(); ++i) {
				const auto& rm = MaterialManager::get_runtime_material(state[i]);
				buffer[i] = compute_cell_color(rm, cells[i]);
			}
		}
	}
}

void Grid::restore_state_full(const std::vector<Cell>& state) {
	if (state.size() != Grid::get_size()) {
		return;
	}

	cells = state;
	const auto& meta = SetManager::get_current_metadata();
	for (auto& cell : cells) {
		for (size_t a = 0; a < 3; ++a) {
			float min_v = (a < meta.attributes.size()) ? static_cast<float>(meta.attributes[a].min_val) : -1e9f;
			float max_v = (a < meta.attributes.size()) ? static_cast<float>(meta.attributes[a].max_val) : 1e9f;
			cell.set_attr_f(a, std::clamp(cell.get_attr_f(a), min_v, max_v));
		}
	}

	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available()) {
		Vulkan::upload_grid(cells.data(), Grid::get_size());
		gpu_needs_upload = false;
		gpu_data_valid = true;
	} else {
		draw();
	}
}

std::vector<uint8_t> Grid::get_all_cells() {
	std::vector<uint8_t> state(Grid::get_size());
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available() && !gpu_needs_upload) {
		Cell* staging = Vulkan::get_staging_cells();
		if (staging) {
			for (size_t i = 0; i < Grid::get_size(); ++i) {
				state[i] = staging[i].material;
			}
			return state;
		}
	}
	for (size_t i = 0; i < Grid::get_size(); ++i) {
		state[i] = cells[i].material;
	}
	return state;
}

std::vector<Cell> Grid::get_all_cells_full() {
	if (processing_mode == ProcessingMode::GPU && Vulkan::is_available() && !gpu_needs_upload) {
		Cell* staging = Vulkan::get_staging_cells();
		if (staging) {
			std::memcpy(cells.data(), staging, Grid::get_size() * sizeof(Cell));
		}
	}
	return cells;
}