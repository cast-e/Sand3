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

inline constexpr std::array<int, NEIGHBOR_COUNT> compute_neighbor_offsets() {
	std::array<int, NEIGHBOR_COUNT> offsets{};
	uint32_t id = 0;
	for (int dy = -2; dy <= 2; ++dy) {
		for (int dx = -2; dx <= 2; ++dx) {
			offsets[id++] = dy * SIM_WIDTH + dx;
		}
	}
	return offsets;
}

enum class QualityPreset : int { Slow = 0, Fast = 1 };

class Grid {
public:
	Grid() = delete;

	static void init();
	static void shutdown();

	static void configure_threads(uint32_t thread_count);

private:
	static void worker_thread(const uint32_t thread_id);
	static void update_strip_1d(const uint32_t sy, const bool reverse_x, const bool reverse_y, uint32_t& local_changed);
	static void update_sequential();

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

	static QualityPreset get_quality_preset() { return quality_preset; }
	static void set_quality_preset(QualityPreset preset) { quality_preset = preset; }

	static uint32_t get_thread_count() { return num_active_threads; }

	static uint32_t get_changed_cells();
	static void remap_materials(const std::vector<uint8_t>& old_to_new);

private:
	static constexpr std::array<int, NEIGHBOR_COUNT> neighbor_offsets = compute_neighbor_offsets();

	static QualityPreset quality_preset;
	static std::vector<Cell> cells;
	static std::vector<Cell> next_cells;

	static uint32_t num_active_threads;
	static std::unique_ptr<std::barrier<>> start_barrier;
	static std::unique_ptr<std::barrier<>> done_barrier;
	static std::unique_ptr<std::barrier<>> phase_barrier;
	static std::vector<std::thread> workers;
	static std::atomic<bool> shutdown_flag;
	static std::atomic<uint32_t> frame_changed;

	static constexpr uint32_t BG_COLOR = (255u << 24) | (64u << 16) | (64u << 8) | 64u;
};