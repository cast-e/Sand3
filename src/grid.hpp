#pragma once

#include <algorithm>
#include <atomic>
#include <barrier>
#include <cmath>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "const.hpp"
#include "material_manager.hpp"

enum class AttributeLayout : uint8_t { Layout_3x16 = 0, Layout_1x32 = 1, Layout_1x32_1x16 = 2 };

inline uint16_t float_to_half(float f) {
	uint32_t x;
	std::memcpy(&x, &f, sizeof(x));
	uint32_t sign = (x >> 16) & 0x8000;
	int32_t exp = ((x >> 23) & 0xFF) - 127 + 15;
	uint32_t mant = x & 0x7FFFFF;
	if (exp <= 0) {
		if (exp < -10)
			return static_cast<uint16_t>(sign);
		mant = (mant | 0x800000) >> (1 - exp);
		return static_cast<uint16_t>(sign | ((mant + 0x1000) >> 13));
	} else if (exp >= 31) {
		return static_cast<uint16_t>(sign | 0x7C00);
	}
	return static_cast<uint16_t>(sign | (exp << 10) | ((mant + 0x1000) >> 13));
}

inline float half_to_float(uint16_t h) {
	uint32_t sign = (h & 0x8000) << 16;
	int32_t exp = (h >> 10) & 0x1F;
	uint32_t mant = h & 0x3FF;
	uint32_t x;
	if (exp == 0) {
		if (mant == 0) {
			x = sign;
		} else {
			exp = 1;
			while ((mant & 0x400) == 0) {
				mant <<= 1;
				exp--;
			}
			mant &= 0x3FF;
			x = sign | ((exp - 15 + 127) << 23) | (mant << 13);
		}
	} else if (exp == 31) {
		x = sign | 0x7F800000 | (mant << 13);
	} else {
		x = sign | ((exp - 15 + 127) << 23) | (mant << 13);
	}
	float f;
	std::memcpy(&f, &x, sizeof(f));
	return f;
}

struct alignas(8) Cell {
	uint8_t material = 0;
	int16_t a_aux = 0;
	int32_t a_prim = 0;

	[[nodiscard]] float get_attr_f(size_t idx) const;
	void set_attr_f(size_t idx, float val);

	[[nodiscard]] int32_t get_attr(size_t idx) const { return static_cast<int32_t>(std::round(get_attr_f(idx))); }
	void set_attr(size_t idx, int32_t val) { set_attr_f(idx, static_cast<float>(val)); }

	[[nodiscard]] uint64_t as_u64() const { return *reinterpret_cast<const uint64_t*>(this); }
	void set_u64(uint64_t v) { *reinterpret_cast<uint64_t*>(this) = v; }
};

static_assert(sizeof(Cell) == 8, "Cell must be exactly 8 bytes (zero padding)");
static_assert(alignof(Cell) == 8, "Cell must have 8-byte alignment");

inline uint32_t compute_cell_color(const RuntimeMaterial& rm, const Cell& cell) {
	if (!rm.has_color_modifiers || rm.color_modifiers.empty()) {
		return rm.packed_color;
	}
	float base_r = static_cast<float>(rm.base_color[0]);
	float base_g = static_cast<float>(rm.base_color[1]);
	float base_b = static_cast<float>(rm.base_color[2]);
	float r = base_r;
	float g = base_g;
	float b = base_b;
	for (const auto& cmod : rm.color_modifiers) {
		float val = cell.get_attr_f(cmod.attr_index);
		auto col = cmod.gradient.sample(val);
		float alpha = col[3];
		r = col[0] * 255.0f * alpha + r * (1.0f - alpha);
		g = col[1] * 255.0f * alpha + g * (1.0f - alpha);
		b = col[2] * 255.0f * alpha + b * (1.0f - alpha);
	}
	uint8_t cr = static_cast<uint8_t>(std::clamp(r, 0.0f, 255.0f));
	uint8_t cg = static_cast<uint8_t>(std::clamp(g, 0.0f, 255.0f));
	uint8_t cb = static_cast<uint8_t>(std::clamp(b, 0.0f, 255.0f));
	return (255u << 24) | (cb << 16) | (cg << 8) | cr;
}

enum class ProcessingMode : int { CPU = 0, GPU = 1 };

class Grid {
public:
	Grid() = delete;

	static void init();
	static void shutdown();
	static Cell make_empty_cell();

	static uint32_t get_width() { return width; }
	static uint32_t get_height() { return height; }
	static uint32_t get_size() { return width * height; }
	static uint32_t get_num_strips_y() { return (height + STRIP_HEIGHT - 1) / STRIP_HEIGHT; }

	static bool resize(uint32_t new_width, uint32_t new_height, bool preserve_content = true);

	static void configure_threads(uint32_t thread_count);

private:
	static void worker_thread(uint32_t thread_id);
	static void update_strip_1d(uint32_t sy, bool reverse_x, bool reverse_y, uint32_t& local_changed);
	static bool apply_compiled_rules(const std::vector<CompiledRule>& rules, uint32_t cx, unsigned cy,
									 bool is_fast_path, uint32_t& local_changed);

public:
	static bool try_apply_rule_fast(const CompiledRuleVariant& rule, uint32_t center_id, uint32_t& local_changed);
	static bool try_apply_rule_safe(const CompiledRuleVariant& rule, uint32_t x, uint32_t y, uint32_t& local_changed);

	static void update();
	static void draw();

	static void draw_material(uint32_t id);

	static uint8_t& get_cell(uint32_t x, uint32_t y);
	static Cell& get_cell_full(uint32_t x, uint32_t y);
	static const Cell& get_cell_full_const(uint32_t x, uint32_t y);
	static void set_cell(uint32_t x, uint32_t y, uint8_t material);
	static void set_cell_full(uint32_t x, uint32_t y, const Cell& cell);
	static void clear();
	static void restore_state(const std::vector<uint8_t>& state);
	static void restore_state_full(const std::vector<Cell>& state);
	static std::vector<uint8_t> get_all_cells();
	static std::vector<Cell> get_all_cells_full();

	static ProcessingMode get_processing_mode() { return processing_mode; }
	static void set_processing_mode(ProcessingMode preset);

	static uint32_t get_thread_count() { return num_active_threads; }

	static uint32_t get_changed_cells();
	static uint64_t get_sim_step_count() { return sim_step_count; }
	static void remap_materials(const std::vector<uint8_t>& old_to_new);

	static void sync_to_gpu();
	static void sync_from_gpu();
	static void keep_awake_gpu();
	static void refresh_gpu_display();
	static bool get_gpu_needs_upload() { return gpu_needs_upload; }
	static void set_gpu_needs_upload(bool val) { gpu_needs_upload = val; }

	static void set_cell_material_only(uint32_t x, uint32_t y, uint8_t material);
	static void adjust_cell_attr(uint32_t x, uint32_t y, uint8_t attr_idx, float delta_or_val, bool set_exact = false);
	static bool is_cell_updated(uint32_t idx) { return idx < updated_cells.size() && updated_cells[idx] != 0; }
	static void set_cell_updated(uint32_t idx, bool val) {
		if (idx < updated_cells.size()) {
			updated_cells[idx] = val ? 1 : 0;
		}
	}

	static AttributeLayout get_attr_layout() { return attr_layout; }
	static uint32_t get_attr_types() { return attr_types; }
	static void update_attr_layout();
	static AttributeLayout attr_layout;
	static uint32_t attr_types;

	static int get_heatmap_mode() { return heatmap_attr_index; }
	static void set_heatmap_mode(int attr_idx);
	static bool get_heatmap_logarithmic() { return heatmap_logarithmic; }
	static bool is_heatmap_logarithmic() { return heatmap_logarithmic; }
	static void set_heatmap_logarithmic(bool log_mode);
	static bool get_heatmap_custom_range() { return heatmap_custom_range; }
	static bool is_heatmap_custom_range() { return heatmap_custom_range; }
	static void set_heatmap_custom_range(bool custom_range);
	static float get_heatmap_view_min() { return heatmap_view_min; }
	static void set_heatmap_view_min(float v_min);
	static float get_heatmap_view_max() { return heatmap_view_max; }
	static void set_heatmap_view_max(float v_max);

	static void set_max_rule_radius(int rx, int ry) {
		max_rule_radius_x = std::max(1, rx);
		max_rule_radius_y = std::max(1, ry);
	}
	static int get_max_rule_radius_x() { return max_rule_radius_x; }
	static int get_max_rule_radius_y() { return max_rule_radius_y; }

private:
	static uint32_t width;
	static uint32_t height;
	static int max_rule_radius_x;
	static int max_rule_radius_y;

	static ProcessingMode processing_mode;
	static std::vector<Cell> cells;
	static std::vector<Cell> next_cells;
	static std::vector<uint8_t> updated_cells;

	static uint32_t num_active_threads;
	static std::unique_ptr<std::barrier<>> start_barrier;
	static std::unique_ptr<std::barrier<>> done_barrier;
	static std::unique_ptr<std::barrier<>> phase_barrier;
	static std::vector<std::thread> workers;
	static std::atomic<bool> shutdown_flag;
	static std::atomic<uint32_t> frame_changed;

	static bool gpu_data_valid;
	static bool gpu_needs_upload;
	static uint64_t sim_step_count;
	static int heatmap_attr_index;
	static bool heatmap_logarithmic;
	static bool heatmap_custom_range;
	static float heatmap_view_min;
	static float heatmap_view_max;
};

inline float Cell::get_attr_f(size_t idx) const {
	bool is_float = ((Grid::attr_types >> idx) & 1u) != 0u;
	bool is_unsigned = ((Grid::attr_types >> (idx + 4u)) & 1u) != 0u;

	if (Grid::attr_layout == AttributeLayout::Layout_1x32) {
		if (idx != 0)
			return 0.0f;
		if (is_float) {
			float f;
			std::memcpy(&f, &a_prim, sizeof(f));
			return f;
		} else if (is_unsigned) {
			return static_cast<float>(static_cast<uint32_t>(a_prim));
		} else {
			return static_cast<float>(a_prim);
		}
	} else if (Grid::attr_layout == AttributeLayout::Layout_1x32_1x16) {
		if (idx == 0) {
			if (is_float) {
				float f;
				std::memcpy(&f, &a_prim, sizeof(f));
				return f;
			} else if (is_unsigned) {
				return static_cast<float>(static_cast<uint32_t>(a_prim));
			} else {
				return static_cast<float>(a_prim);
			}
		} else if (idx == 1) {
			auto u16 = static_cast<uint16_t>(a_aux);
			if (is_float) {
				return half_to_float(u16);
			} else if (is_unsigned) {
				return static_cast<float>(u16);
			} else {
				return static_cast<float>(a_aux);
			}
		}
		return 0.0f;
	} else {
		uint16_t u16;
		int16_t s16;
		if (idx == 0) {
			u16 = static_cast<uint16_t>(a_prim & 0xFFFF);
			s16 = static_cast<int16_t>(a_prim & 0xFFFF);
		} else if (idx == 1) {
			u16 = static_cast<uint16_t>((a_prim >> 16) & 0xFFFF);
			s16 = static_cast<int16_t>((a_prim >> 16) & 0xFFFF);
		} else {
			u16 = static_cast<uint16_t>(a_aux);
			s16 = a_aux;
		}

		if (is_float) {
			return half_to_float(u16);
		} else if (is_unsigned) {
			return static_cast<float>(u16);
		} else {
			return static_cast<float>(s16);
		}
	}
}

inline void Cell::set_attr_f(size_t idx, float val) {
	bool is_float = ((Grid::attr_types >> idx) & 1u) != 0u;
	bool is_unsigned = ((Grid::attr_types >> (idx + 4u)) & 1u) != 0u;

	if (Grid::attr_layout == AttributeLayout::Layout_1x32) {
		if (idx != 0)
			return;
		if (is_float) {
			std::memcpy(&a_prim, &val, sizeof(a_prim));
		} else if (is_unsigned) {
			a_prim = static_cast<int32_t>(static_cast<uint32_t>(std::clamp(std::nearbyint(val), 0.0f, 4294967295.0f)));
		} else {
			a_prim = static_cast<int32_t>(std::clamp(std::nearbyint(val), -2147483648.0f, 2147483647.0f));
		}
	} else if (Grid::attr_layout == AttributeLayout::Layout_1x32_1x16) {
		if (idx == 0) {
			if (is_float) {
				std::memcpy(&a_prim, &val, sizeof(a_prim));
			} else if (is_unsigned) {
				a_prim =
					static_cast<int32_t>(static_cast<uint32_t>(std::clamp(std::nearbyint(val), 0.0f, 4294967295.0f)));
			} else {
				a_prim = static_cast<int32_t>(std::clamp(std::nearbyint(val), -2147483648.0f, 2147483647.0f));
			}
		} else if (idx == 1) {
			if (is_float) {
				a_aux = static_cast<int16_t>(float_to_half(val));
			} else if (is_unsigned) {
				a_aux = static_cast<int16_t>(static_cast<uint16_t>(std::clamp(std::nearbyint(val), 0.0f, 65535.0f)));
			} else {
				a_aux = static_cast<int16_t>(std::clamp(std::nearbyint(val), -32768.0f, 32767.0f));
			}
		}
	} else {
		uint16_t pack16;
		if (is_float) {
			pack16 = float_to_half(val);
		} else if (is_unsigned) {
			pack16 = static_cast<uint16_t>(std::clamp(std::nearbyint(val), 0.0f, 65535.0f));
		} else {
			pack16 = static_cast<uint16_t>(static_cast<int16_t>(std::clamp(std::nearbyint(val), -32768.0f, 32767.0f)));
		}

		if (idx == 0) {
			a_prim = (a_prim & ~0xFFFF) | static_cast<int32_t>(pack16);
		} else if (idx == 1) {
			a_prim = (a_prim & 0xFFFF) | (static_cast<int32_t>(pack16) << 16);
		} else {
			a_aux = static_cast<int16_t>(pack16);
		}
	}
}