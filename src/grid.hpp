#pragma once

#include <array>
#include <atomic>
#include <barrier>
#include <memory>
#include <thread>
#include <vector>

#include "const.hpp"
#include "material_manager.hpp"

struct Cell {
	uint8_t material = 0;
	bool updated = false;
};

enum class ProcessingMode : int { CPU = 0, GPU = 1 };

class Grid {
public:
	Grid() = delete;

	static void init();
	static void shutdown();

	static uint32_t get_width() { return width; }
	static uint32_t get_height() { return height; }
	static uint32_t get_size() { return width * height; }
	static uint32_t get_num_strips_y() { return (height + STRIP_HEIGHT - 1) / STRIP_HEIGHT; }

	static bool resize(uint32_t new_width, uint32_t new_height, bool preserve_content = true);

	static void configure_threads(uint32_t thread_count);

private:
	static void worker_thread(const uint32_t thread_id);
	static void update_strip_1d(const uint32_t sy, const bool reverse_x, const bool reverse_y, uint32_t& local_changed);

public:
	static bool try_apply_rule_fast(const CompiledRuleVariant& rule, const uint32_t center_id, uint32_t& local_changed);
	static bool try_apply_rule_safe(const CompiledRuleVariant& rule, const uint32_t x, const uint32_t y,
									uint32_t& local_changed);

	static void update();
	static void draw();

	static void draw_material(uint32_t id);

	static uint8_t& get_cell(const uint32_t x, const uint32_t y);
	static void set_cell(const uint32_t x, const uint32_t y, const uint8_t material);
	static void clear();
	static void restore_state(const std::vector<uint8_t>& state);
	static std::vector<uint8_t> get_all_cells();

	static ProcessingMode get_processing_mode() { return processing_mode; }
	static void set_processing_mode(ProcessingMode preset);

	static uint32_t get_thread_count() { return num_active_threads; }

	static uint32_t get_changed_cells();
	static void remap_materials(const std::vector<uint8_t>& old_to_new);

	static void sync_to_gpu();
	static void sync_from_gpu();
	static void keep_awake_gpu();

private:
	static uint32_t width;
	static uint32_t height;
	static std::array<int, NEIGHBOR_COUNT> neighbor_offsets;
	static void recompute_neighbor_offsets();

	static ProcessingMode processing_mode;
	static std::vector<Cell> cells;
	static std::vector<Cell> next_cells;

	static uint32_t num_active_threads;
	static std::unique_ptr<std::barrier<>> start_barrier;
	static std::unique_ptr<std::barrier<>> done_barrier;
	static std::unique_ptr<std::barrier<>> phase_barrier;
	static std::vector<std::thread> workers;
	static std::atomic<bool> shutdown_flag;
	static std::atomic<uint32_t> frame_changed;

	static bool gpu_data_valid;
	static bool gpu_needs_upload;

	static constexpr uint32_t BG_COLOR = (255u << 24) | (64u << 16) | (64u << 8) | 64u;
};