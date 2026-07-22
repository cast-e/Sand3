#pragma once

#include <array>
#include <atomic>
#include <barrier>
#include <memory>
#include <thread>
#include <vector>

#include "const.hpp"
#include "material.hpp"
#include "window.hpp"

inline constexpr std::array<int, NEIGHBOR_COUNT> compute_neighbor_offsets() {
	std::array<int, NEIGHBOR_COUNT> offsets{};
	unsigned int idx = 0;
	for (int dy = -2; dy <= 2; ++dy) {
		for (int dx = -2; dx <= 2; ++dx) {
			offsets[idx++] = dy * SIM_WIDTH + dx;
		}
	}
	return offsets;
}

enum class QualityPreset : int { Slow = 0, Fast = 1 };

class Grid {
public:
	Grid();
	~Grid();

	void configure_threads(unsigned int thread_count);

private:
	void worker_thread(const unsigned int thread_id);
	void update_strip_1d(const unsigned int sy, const bool reverse_x, const bool reverse_y,
						 unsigned int& local_changed);
	void update_sequential();

public:
	bool try_apply_rule_fast(const Rule& rule, const unsigned int center_idx, unsigned int& local_changed);
	bool try_apply_rule_safe(const Rule& rule, const unsigned int x, const unsigned int y, unsigned int& local_changed);

	void update();
	void draw(Window& window);

	unsigned char& get_cell(const unsigned int x, const unsigned int y);
	unsigned char get_cell(const unsigned int x, const unsigned int y) const;
	void set_cell(const unsigned int x, const unsigned int y, const unsigned char material);
	void clear();

	bool save_to_file(const std::string& name, const std::string& current_set) const;
	bool load_from_file(const std::string& path_or_name, const std::string& current_set, std::string& loaded_set);

	QualityPreset get_quality_preset() const { return quality_preset; }
	void set_quality_preset(QualityPreset preset) { quality_preset = preset; }

	unsigned int get_thread_count() const { return num_active_threads; }

	unsigned int get_changed_cells() const;
	void remap_materials(const std::vector<unsigned char>& old_to_new);

private:
	static constexpr std::array<int, NEIGHBOR_COUNT> neighbor_offsets = compute_neighbor_offsets();

	QualityPreset quality_preset = QualityPreset::Fast;
	std::vector<std::pair<unsigned char, bool>> cells;
	std::vector<std::pair<unsigned char, bool>> next_cells;

	unsigned int num_active_threads;
	std::unique_ptr<std::barrier<>> start_barrier;
	std::unique_ptr<std::barrier<>> done_barrier;
	std::unique_ptr<std::barrier<>> phase_barrier;
	std::vector<std::thread> workers;
	std::atomic<bool> shutdown{false};
	std::atomic<unsigned int> frame_changed{0};

	unsigned int frame_count = 0;

	static constexpr uint32_t BG_COLOR = (255u << 24) | (64u << 16) | (64u << 8) | 64u;
};