#pragma once

#include <array>
#include <atomic>
#include <barrier>
#include <memory>
#include <thread>
#include <vector>

#include "const.hpp"
#include "material_manager.hpp"

inline constexpr std::array<int, NEIGHBOR_COUNT> compute_neighbor_offsets() {
	std::array<int, NEIGHBOR_COUNT> offsets{};
	unsigned int id = 0;
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

	static void configure_threads(unsigned int thread_count);

private:
	static void worker_thread(const unsigned int thread_id);
	static void update_strip_1d(const unsigned int sy, const bool reverse_x, const bool reverse_y,
								unsigned int& local_changed);
	static void update_sequential();

public:
	static bool try_apply_rule_fast(const Rule& rule, const unsigned int center_id, unsigned int& local_changed);
	static bool try_apply_rule_safe(const Rule& rule, const unsigned int x, const unsigned int y,
									unsigned int& local_changed);

	static void update();
	static void draw();

	static void draw_material(unsigned int id);

	static unsigned char& get_cell(const unsigned int x, const unsigned int y);
	static void set_cell(const unsigned int x, const unsigned int y, const unsigned char material);
	static void clear();

	static QualityPreset get_quality_preset() { return quality_preset; }
	static void set_quality_preset(QualityPreset preset) { quality_preset = preset; }

	static unsigned int get_thread_count() { return num_active_threads; }

	static unsigned int get_changed_cells();
	static void remap_materials(const std::vector<unsigned char>& old_to_new);

private:
	static constexpr std::array<int, NEIGHBOR_COUNT> neighbor_offsets = compute_neighbor_offsets();

	static QualityPreset quality_preset;
	static std::vector<std::pair<unsigned char, bool>> cells;
	static std::vector<std::pair<unsigned char, bool>> next_cells;

	static unsigned int num_active_threads;
	static std::unique_ptr<std::barrier<>> start_barrier;
	static std::unique_ptr<std::barrier<>> done_barrier;
	static std::unique_ptr<std::barrier<>> phase_barrier;
	static std::vector<std::thread> workers;
	static std::atomic<bool> shutdown_flag;
	static std::atomic<unsigned int> frame_changed;

	static unsigned int frame_count;

	static constexpr uint32_t BG_COLOR = (255u << 24) | (64u << 16) | (64u << 8) | 64u;
};