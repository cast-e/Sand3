#include "ui.hpp"

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "config_manager.hpp"
#include "const.hpp"
#include "gradient.hpp"
#include "grid.hpp"
#include "icon_manager.hpp"
#include "material_manager.hpp"
#include "resources/roboto_ttf.h"
#include "sanitize.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "shortcut_manager.hpp"
#include "toast_manager.hpp"
#include "ui_advanced.hpp"
#include "ui_modals.hpp"
#include "ui_shortcuts.hpp"
#include "ui_theme.hpp"
#include "ui_thumbnails.hpp"
#include "ui_widgets.hpp"
#include "ui_workshop.hpp"
#include "undo_manager.hpp"
#include "vulkan.hpp"
#include "window.hpp"
#include "workshop_cache.hpp"
#include "workshop_client.hpp"
#include "workshop_item_editor.hpp"
#include "zip_util.hpp"

void UI::apply_theme_style(ImGuiStyle& style, const UIConfig& cfg) { UITheme::apply_theme_style(style, cfg); }

float UI::get_spacing(float offset) { return UITheme::get_spacing(offset); }

const ImVec4* UI::get_default_colors() { return UITheme::get_default_colors(); }

void UI::reset_theme_colors() { UITheme::reset_theme_colors(); }

struct GridRect {
	int x, y, w, h;
};

static int s_active_eq_rule_id = -1;
static int s_active_eq_cell_id = -1;
static int s_active_eq_attr_idx = 0;
static ImGuiID s_active_eq_popup_imgui_id = 0;
static std::string s_active_eq_popup_id;
static ImRect s_active_popup_rect;
static int s_active_raw_edit_attr = -1;
static bool s_keep_eq_popup_open = false;
static std::unordered_map<int, ImRect> s_rule_when_rect;
static std::unordered_map<int, ImRect> s_rule_then_rect;

struct ScreenRect {
	float x1, y1, x2, y2;
};

static ImVec2 screen_to_grid_pos(ImVec2 screen_pos) {
	SDL_FRect dst_rect = Window::get_dst_rect();
	float norm_x = (screen_pos.x - dst_rect.x) / dst_rect.w;
	float norm_y = (screen_pos.y - dst_rect.y) / dst_rect.h;
	return ImVec2(norm_x * Grid::get_width(), norm_y * Grid::get_height());
}

static std::pair<float, float> screen_to_grid(float sx, float sy) {
	ImVec2 g = screen_to_grid_pos(ImVec2(sx, sy));
	return {g.x, g.y};
}

static ScreenRect grid_to_screen_rect(int start_x, int start_y, int size) {
	SDL_FRect dst_rect = Window::get_dst_rect();
	float sx1 = dst_rect.x + (static_cast<float>(start_x) / Grid::get_width()) * dst_rect.w;
	float sy1 = dst_rect.y + (static_cast<float>(start_y) / Grid::get_height()) * dst_rect.h;
	float sx2 = dst_rect.x + (static_cast<float>(start_x + size) / Grid::get_width()) * dst_rect.w;
	float sy2 = dst_rect.y + (static_cast<float>(start_y + size) / Grid::get_height()) * dst_rect.h;
	return {sx1, sy1, sx2, sy2};
}

static ScreenRect grid_to_screen_rect_wh(int start_x, int start_y, int w, int h) {
	SDL_FRect dst_rect = Window::get_dst_rect();
	float sx1 = dst_rect.x + (static_cast<float>(start_x) / Grid::get_width()) * dst_rect.w;
	float sy1 = dst_rect.y + (static_cast<float>(start_y) / Grid::get_height()) * dst_rect.h;
	float sx2 = dst_rect.x + (static_cast<float>(start_x + w) / Grid::get_width()) * dst_rect.w;
	float sy2 = dst_rect.y + (static_cast<float>(start_y + h) / Grid::get_height()) * dst_rect.h;
	return {sx1, sy1, sx2, sy2};
}

static GridRect calculate_brush_bounds(ImVec2 grid_pos, int brush_size) {
	float half = static_cast<float>(brush_size) * 0.5f;
	int x_start = static_cast<int>(std::floor(grid_pos.x - half + 0.5f));
	int y_start = static_cast<int>(std::floor(grid_pos.y - half + 0.5f));
	return {x_start, y_start, brush_size, brush_size};
}

static ResizeHandle get_hovered_resize_handle(const ScreenRect& srect, ImVec2 mouse_pos) {
	ImVec2 handle_pts[8] = {ImVec2(srect.x1, srect.y1), ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y1),
							ImVec2(srect.x2, srect.y1), ImVec2(srect.x2, (srect.y1 + srect.y2) * 0.5f),
							ImVec2(srect.x2, srect.y2), ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y2),
							ImVec2(srect.x1, srect.y2), ImVec2(srect.x1, (srect.y1 + srect.y2) * 0.5f)};

	ResizeHandle handle_enums[8] = {ResizeHandle::TopLeft,	  ResizeHandle::Top,		 ResizeHandle::TopRight,
									ResizeHandle::Right,	  ResizeHandle::BottomRight, ResizeHandle::Bottom,
									ResizeHandle::BottomLeft, ResizeHandle::Left};

	const float hit_dist = 12.0f;
	float min_d = 1e9f;
	ResizeHandle closest = ResizeHandle::None;

	for (int i = 0; i < 8; ++i) {
		float d = std::hypot(mouse_pos.x - handle_pts[i].x, mouse_pos.y - handle_pts[i].y);
		if (d < min_d) {
			min_d = d;
			closest = handle_enums[i];
		}
	}

	if (min_d <= hit_dist) {
		return closest;
	}
	return ResizeHandle::None;
}

static ImGuiMouseCursor get_cursor_for_resize_handle(ResizeHandle h) {
	switch (h) {
		case ResizeHandle::TopLeft:
		case ResizeHandle::BottomRight:
			return ImGuiMouseCursor_ResizeNWSE;
		case ResizeHandle::TopRight:
		case ResizeHandle::BottomLeft:
			return ImGuiMouseCursor_ResizeNESW;
		case ResizeHandle::Top:
		case ResizeHandle::Bottom:
			return ImGuiMouseCursor_ResizeNS;
		case ResizeHandle::Left:
		case ResizeHandle::Right:
			return ImGuiMouseCursor_ResizeEW;
		default:
			return ImGuiMouseCursor_Arrow;
	}
}

struct LineSpan {
	int y;
	int x0;
	int x1;
};

static bool s_line_brush_active = false;
static ImVec2 s_line_brush_start_grid(0.0f, 0.0f);

static std::vector<LineSpan> compute_line_spans(ImVec2 start_grid, ImVec2 end_grid, int brush_size, BrushShape shape) {
	GridRect b_start = calculate_brush_bounds(start_grid, brush_size);
	GridRect b_end = calculate_brush_bounds(end_grid, brush_size);

	float cx_start = static_cast<float>(b_start.x) + static_cast<float>(brush_size) * 0.5f;
	float cy_start = static_cast<float>(b_start.y) + static_cast<float>(brush_size) * 0.5f;
	float cx_end = static_cast<float>(b_end.x) + static_cast<float>(brush_size) * 0.5f;
	float cy_end = static_cast<float>(b_end.y) + static_cast<float>(brush_size) * 0.5f;

	float dx = cx_end - cx_start;
	float dy = cy_end - cy_start;
	float dist = std::max(std::abs(dx), std::abs(dy));

	int min_y = std::min(b_start.y, b_end.y);
	int max_y = std::max(b_start.y + brush_size, b_end.y + brush_size);

	int total_rows = max_y - min_y;
	if (total_rows <= 0)
		return {};

	std::vector<int> row_min_x(total_rows, std::numeric_limits<int>::max());
	std::vector<int> row_max_x(total_rows, std::numeric_limits<int>::min());

	auto stamp = [&](const GridRect& b) {
		if (shape == BrushShape::Square || brush_size <= 1) {
			for (int y = b.y; y < b.y + brush_size; ++y) {
				int r_idx = y - min_y;
				if (r_idx >= 0 && r_idx < total_rows) {
					row_min_x[r_idx] = std::min(row_min_x[r_idx], b.x);
					row_max_x[r_idx] = std::max(row_max_x[r_idx], b.x + brush_size);
				}
			}
		} else {
			float cx = static_cast<float>(b.x) + static_cast<float>(brush_size) * 0.5f;
			float cy = static_cast<float>(b.y) + static_cast<float>(brush_size) * 0.5f;
			float r = static_cast<float>(brush_size) * 0.5f;
			float r_sq = r * r;

			for (int i = 0; i < brush_size; ++i) {
				int y = b.y + i;
				int r_idx = y - min_y;
				if (r_idx < 0 || r_idx >= total_rows)
					continue;

				float cdy = (static_cast<float>(y) + 0.5f) - cy;
				float dy_sq = cdy * cdy;
				if (dy_sq <= r_sq) {
					float dx_max = std::sqrt(r_sq - dy_sq);
					int x0 = static_cast<int>(std::ceil(cx - dx_max - 0.5f));
					int x1 = static_cast<int>(std::floor(cx + dx_max - 0.5f)) + 1;
					if (x0 < x1) {
						row_min_x[r_idx] = std::min(row_min_x[r_idx], x0);
						row_max_x[r_idx] = std::max(row_max_x[r_idx], x1);
					}
				}
			}
		}
	};

	if (dist < 0.001f) {
		stamp(b_start);
	} else {
		int steps = static_cast<int>(std::ceil(dist));
		int last_bx = std::numeric_limits<int>::min();
		int last_by = std::numeric_limits<int>::min();

		for (int i = 0; i <= steps; ++i) {
			float t = static_cast<float>(i) / static_cast<float>(steps);
			ImVec2 pos(cx_start + t * dx, cy_start + t * dy);
			GridRect b = calculate_brush_bounds(pos, brush_size);
			if (b.x != last_bx || b.y != last_by) {
				stamp(b);
				last_bx = b.x;
				last_by = b.y;
			}
		}
	}

	std::vector<LineSpan> spans;
	spans.reserve(total_rows);
	for (int i = 0; i < total_rows; ++i) {
		int y = min_y + i;
		if (row_min_x[i] < row_max_x[i]) {
			spans.push_back({y, row_min_x[i], row_max_x[i]});
		} else {
			spans.push_back({y, 0, 0});
		}
	}
	return spans;
}

static void render_pixel_spans(ImDrawList* draw_list, const std::vector<LineSpan>& spans, ImU32 fill_color,
							   ImU32 line_color, ImU32 inner_line_color) {
	SDL_FRect dst_rect = Window::get_dst_rect();

	for (const auto& span : spans) {
		if (span.x0 < span.x1) {
			ScreenRect r_srect = grid_to_screen_rect_wh(span.x0, span.y, span.x1 - span.x0, 1);
			draw_list->AddRectFilled(ImVec2(r_srect.x1, r_srect.y1), ImVec2(r_srect.x2, r_srect.y2), fill_color);
		}
	}

	struct GridSeg {
		int x1, y1, x2, y2;
	};
	std::vector<GridSeg> segs;
	segs.reserve(spans.size() * 4);

	for (size_t k = 0; k < spans.size(); ++k) {
		int y = spans[k].y;
		int x0 = spans[k].x0;
		int x1 = spans[k].x1;
		if (x0 >= x1)
			continue;

		segs.push_back({x0, y, x0, y + 1});
		segs.push_back({x1, y, x1, y + 1});

		int prev_x0 = (k > 0) ? spans[k - 1].x0 : 0;
		int prev_x1 = (k > 0) ? spans[k - 1].x1 : 0;
		if (prev_x0 >= prev_x1) {
			segs.push_back({x0, y, x1, y});
		} else {
			if (x0 < prev_x0)
				segs.push_back({x0, y, prev_x0, y});
			if (x1 > prev_x1)
				segs.push_back({prev_x1, y, x1, y});
		}

		int next_x0 = (k + 1 < spans.size()) ? spans[k + 1].x0 : 0;
		int next_x1 = (k + 1 < spans.size()) ? spans[k + 1].x1 : 0;
		if (next_x0 >= next_x1) {
			segs.push_back({x0, y + 1, x1, y + 1});
		} else {
			if (x0 < next_x0)
				segs.push_back({x0, y + 1, next_x0, y + 1});
			if (x1 > next_x1)
				segs.push_back({next_x1, y + 1, x1, y + 1});
		}
	}

	for (const auto& seg : segs) {
		float sx1 = dst_rect.x + (static_cast<float>(seg.x1) / Grid::get_width()) * dst_rect.w;
		float sy1 = dst_rect.y + (static_cast<float>(seg.y1) / Grid::get_height()) * dst_rect.h;
		float sx2 = dst_rect.x + (static_cast<float>(seg.x2) / Grid::get_width()) * dst_rect.w;
		float sy2 = dst_rect.y + (static_cast<float>(seg.y2) / Grid::get_height()) * dst_rect.h;

		draw_list->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2), line_color, 3.0f);
		draw_list->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2), inner_line_color, 1.5f);
	}
}

template <typename Func>
static void parallel_for_rows(int min_y, int max_y, Func&& func) {
	int count = max_y - min_y;
	if (count <= 0)
		return;

	unsigned int num_threads = std::thread::hardware_concurrency();
	if (num_threads == 0)
		num_threads = 4;

	if (count < 16 || num_threads <= 1) {
		for (int y = min_y; y < max_y; ++y) {
			func(y);
		}
		return;
	}

	num_threads = std::min<unsigned int>(num_threads, static_cast<unsigned int>(count));
	std::vector<std::thread> workers;
	workers.reserve(num_threads);

	int rows_per_thread = count / num_threads;
	int extra = count % num_threads;

	int start_y = min_y;
	for (unsigned int t = 0; t < num_threads; ++t) {
		int end_y = start_y + rows_per_thread + (t < static_cast<unsigned int>(extra) ? 1 : 0);
		workers.emplace_back([start_y, end_y, &func]() {
			for (int y = start_y; y < end_y; ++y) {
				func(y);
			}
		});
		start_y = end_y;
	}

	for (auto& worker : workers) {
		worker.join();
	}
}

static void paint_brush_at(int start_x, int start_y, int brush_size, uint8_t mat_id, BrushShape shape) {
	int min_x = std::clamp(start_x, 0, static_cast<int>(Grid::get_width()));
	int max_x = std::clamp(start_x + brush_size, 0, static_cast<int>(Grid::get_width()));
	int min_y = std::clamp(start_y, 0, static_cast<int>(Grid::get_height()));
	int max_y = std::clamp(start_y + brush_size, 0, static_cast<int>(Grid::get_height()));

	if (min_x >= max_x || min_y >= max_y)
		return;

	if (shape == BrushShape::Square || brush_size <= 1) {
		parallel_for_rows(min_y, max_y, [min_x, max_x, mat_id](int y) {
			for (int x = min_x; x < max_x; ++x) {
				if (UI::brush_overwrite_attributes) {
					Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
				} else {
					Grid::set_cell_material_only(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
				}
			}
		});
	} else {
		float cx = static_cast<float>(start_x) + static_cast<float>(brush_size) * 0.5f;
		float cy = static_cast<float>(start_y) + static_cast<float>(brush_size) * 0.5f;
		float r = static_cast<float>(brush_size) * 0.5f;
		float r_sq = r * r;

		parallel_for_rows(min_y, max_y, [min_x, max_x, cx, cy, r_sq, mat_id](int y) {
			float dy = (static_cast<float>(y) + 0.5f) - cy;
			float dy_sq = dy * dy;
			if (dy_sq <= r_sq) {
				float dx_max = std::sqrt(r_sq - dy_sq);
				int rx_min = static_cast<int>(std::ceil(cx - dx_max - 0.5f));
				int rx_max = static_cast<int>(std::floor(cx + dx_max - 0.5f)) + 1;
				rx_min = std::clamp(rx_min, min_x, max_x);
				rx_max = std::clamp(rx_max, min_x, max_x);
				for (int x = rx_min; x < rx_max; ++x) {
					if (UI::brush_overwrite_attributes) {
						Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
					} else {
						Grid::set_cell_material_only(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
					}
				}
			}
		});
	}
}

static void paint_line(ImVec2 start_grid, ImVec2 end_grid, int brush_size, uint8_t mat_id, BrushShape shape) {
	std::vector<LineSpan> spans = compute_line_spans(start_grid, end_grid, brush_size, shape);
	for (const auto& span : spans) {
		if (span.x0 >= span.x1)
			continue;
		int y = span.y;
		if (y < 0 || y >= static_cast<int>(Grid::get_height()))
			continue;
		int x0 = std::clamp(span.x0, 0, static_cast<int>(Grid::get_width()));
		int x1 = std::clamp(span.x1, 0, static_cast<int>(Grid::get_width()));
		for (int x = x0; x < x1; ++x) {
			if (UI::brush_overwrite_attributes) {
				Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
			} else {
				Grid::set_cell_material_only(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
			}
		}
	}
}

static uint32_t s_attr_stroke_id = 0;
static std::vector<uint32_t> s_attr_cell_stroke;

static void paint_attribute_brush_at(int start_x, int start_y, int brush_size, uint8_t attr_idx, float val,
									 bool set_exact, BrushShape shape) {
	int min_x = std::clamp(start_x, 0, static_cast<int>(Grid::get_width()));
	int max_x = std::clamp(start_x + brush_size, 0, static_cast<int>(Grid::get_width()));
	int min_y = std::clamp(start_y, 0, static_cast<int>(Grid::get_height()));
	int max_y = std::clamp(start_y + brush_size, 0, static_cast<int>(Grid::get_height()));

	if (min_x >= max_x || min_y >= max_y)
		return;

	uint32_t grid_sz = Grid::get_size();
	if (s_attr_cell_stroke.size() != grid_sz) {
		s_attr_cell_stroke.assign(grid_sz, 0);
	}
	uint32_t w = Grid::get_width();

	if (shape == BrushShape::Square || brush_size <= 1) {
		parallel_for_rows(min_y, max_y, [min_x, max_x, w, attr_idx, val, set_exact](int y) {
			for (int x = min_x; x < max_x; ++x) {
				uint32_t idx = static_cast<uint32_t>(y) * w + static_cast<uint32_t>(x);
				if (s_attr_stroke_id != 0 && s_attr_cell_stroke[idx] == s_attr_stroke_id) {
					continue;
				}
				if (s_attr_stroke_id != 0) {
					s_attr_cell_stroke[idx] = s_attr_stroke_id;
				}
				Grid::adjust_cell_attr(static_cast<uint32_t>(x), static_cast<uint32_t>(y), attr_idx, val, set_exact);
			}
		});
	} else {
		float cx = static_cast<float>(start_x) + static_cast<float>(brush_size) * 0.5f;
		float cy = static_cast<float>(start_y) + static_cast<float>(brush_size) * 0.5f;
		float r = static_cast<float>(brush_size) * 0.5f;
		float r_sq = r * r;

		parallel_for_rows(min_y, max_y, [min_x, max_x, cx, cy, r_sq, w, attr_idx, val, set_exact](int y) {
			float dy = (static_cast<float>(y) + 0.5f) - cy;
			float dy_sq = dy * dy;
			if (dy_sq <= r_sq) {
				float dx_max = std::sqrt(r_sq - dy_sq);
				int rx_min = static_cast<int>(std::ceil(cx - dx_max - 0.5f));
				int rx_max = static_cast<int>(std::floor(cx + dx_max - 0.5f)) + 1;
				rx_min = std::clamp(rx_min, min_x, max_x);
				rx_max = std::clamp(rx_max, min_x, max_x);
				for (int x = rx_min; x < rx_max; ++x) {
					uint32_t idx = static_cast<uint32_t>(y) * w + static_cast<uint32_t>(x);
					if (s_attr_stroke_id != 0 && s_attr_cell_stroke[idx] == s_attr_stroke_id) {
						continue;
					}
					if (s_attr_stroke_id != 0) {
						s_attr_cell_stroke[idx] = s_attr_stroke_id;
					}
					Grid::adjust_cell_attr(static_cast<uint32_t>(x), static_cast<uint32_t>(y), attr_idx, val,
										   set_exact);
				}
			}
		});
	}
}

static void paint_attribute_line(ImVec2 start_grid, ImVec2 end_grid, int brush_size, uint8_t attr_idx, float val,
								 bool set_exact, BrushShape shape) {
	std::vector<LineSpan> spans = compute_line_spans(start_grid, end_grid, brush_size, shape);
	uint32_t grid_sz = Grid::get_size();
	if (s_attr_cell_stroke.size() != grid_sz) {
		s_attr_cell_stroke.assign(grid_sz, 0);
	}
	uint32_t w = Grid::get_width();

	for (const auto& span : spans) {
		if (span.x0 >= span.x1)
			continue;
		int y = span.y;
		if (y < 0 || y >= static_cast<int>(Grid::get_height()))
			continue;
		int x0 = std::clamp(span.x0, 0, static_cast<int>(w));
		int x1 = std::clamp(span.x1, 0, static_cast<int>(w));
		for (int x = x0; x < x1; ++x) {
			uint32_t idx = static_cast<uint32_t>(y) * w + static_cast<uint32_t>(x);
			if (s_attr_stroke_id != 0 && s_attr_cell_stroke[idx] == s_attr_stroke_id) {
				continue;
			}
			if (s_attr_stroke_id != 0) {
				s_attr_cell_stroke[idx] = s_attr_stroke_id;
			}
			Grid::adjust_cell_attr(static_cast<uint32_t>(x), static_cast<uint32_t>(y), attr_idx, val, set_exact);
		}
	}
}

void UI::flood_fill(uint32_t start_x, uint32_t start_y, uint8_t fill_mat) {
	uint32_t width = Grid::get_width();
	uint32_t height = Grid::get_height();
	if (start_x >= width || start_y >= height)
		return;
	uint8_t target_mat = Grid::get_cell(start_x, start_y);
	if (target_mat == fill_mat && !UI::brush_overwrite_attributes)
		return;

	static std::vector<uint32_t> s_visited;
	static uint32_t s_fill_gen = 0;

	if (s_visited.size() != static_cast<size_t>(width) * height) {
		s_visited.assign(static_cast<size_t>(width) * height, 0);
		s_fill_gen = 0;
	}
	s_fill_gen++;
	if (s_fill_gen == 0) {
		s_visited.assign(static_cast<size_t>(width) * height, 0);
		s_fill_gen = 1;
	}

	uint32_t start_idx = start_y * width + start_x;
	s_visited[start_idx] = s_fill_gen;

	std::vector<std::pair<uint32_t, uint32_t>> queue;
	queue.reserve(4096);
	queue.push_back({start_x, start_y});

	if (UI::brush_overwrite_attributes) {
		Grid::set_cell(start_x, start_y, fill_mat);
	} else {
		Grid::set_cell_material_only(start_x, start_y, fill_mat);
	}

	size_t head = 0;
	while (head < queue.size()) {
		auto [x, y] = queue[head++];

		const int dx[4] = {-1, 1, 0, 0};
		const int dy[4] = {0, 0, -1, 1};

		for (int i = 0; i < 4; ++i) {
			int nx = static_cast<int>(x) + dx[i];
			int ny = static_cast<int>(y) + dy[i];

			if (nx >= 0 && nx < static_cast<int>(width) && ny >= 0 && ny < static_cast<int>(height)) {
				uint32_t unx = static_cast<uint32_t>(nx);
				uint32_t uny = static_cast<uint32_t>(ny);
				uint32_t nidx = uny * width + unx;

				if (s_visited[nidx] != s_fill_gen && Grid::get_cell(unx, uny) == target_mat) {
					s_visited[nidx] = s_fill_gen;
					if (UI::brush_overwrite_attributes) {
						Grid::set_cell(unx, uny, fill_mat);
					} else {
						Grid::set_cell_material_only(unx, uny, fill_mat);
					}
					queue.push_back({unx, uny});
				}
			}
		}
	}
}

void UI::flood_fill_attribute(uint32_t start_x, uint32_t start_y, uint8_t attr_idx, float val, bool set_exact) {
	uint32_t width = Grid::get_width();
	uint32_t height = Grid::get_height();
	if (start_x >= width || start_y >= height)
		return;
	uint8_t target_mat = Grid::get_cell(start_x, start_y);

	static std::vector<uint32_t> s_visited_attr;
	static uint32_t s_fill_attr_gen = 0;

	if (s_visited_attr.size() != static_cast<size_t>(width) * height) {
		s_visited_attr.assign(static_cast<size_t>(width) * height, 0);
		s_fill_attr_gen = 0;
	}
	s_fill_attr_gen++;
	if (s_fill_attr_gen == 0) {
		s_visited_attr.assign(static_cast<size_t>(width) * height, 0);
		s_fill_attr_gen = 1;
	}

	uint32_t start_idx = start_y * width + start_x;
	s_visited_attr[start_idx] = s_fill_attr_gen;

	std::vector<std::pair<uint32_t, uint32_t>> queue;
	queue.reserve(4096);
	queue.push_back({start_x, start_y});

	Grid::adjust_cell_attr(start_x, start_y, attr_idx, val, set_exact);

	size_t head = 0;
	while (head < queue.size()) {
		auto [x, y] = queue[head++];

		const int dx[4] = {-1, 1, 0, 0};
		const int dy[4] = {0, 0, -1, 1};

		for (int i = 0; i < 4; ++i) {
			int nx = static_cast<int>(x) + dx[i];
			int ny = static_cast<int>(y) + dy[i];

			if (nx >= 0 && nx < static_cast<int>(width) && ny >= 0 && ny < static_cast<int>(height)) {
				uint32_t unx = static_cast<uint32_t>(nx);
				uint32_t uny = static_cast<uint32_t>(ny);
				uint32_t nidx = uny * width + unx;

				if (s_visited_attr[nidx] != s_fill_attr_gen && Grid::get_cell(unx, uny) == target_mat) {
					s_visited_attr[nidx] = s_fill_attr_gen;
					Grid::adjust_cell_attr(unx, uny, attr_idx, val, set_exact);
					queue.push_back({unx, uny});
				}
			}
		}
	}
}
void UI::trigger_exit() { UIModals::open_exit(); }

char UI::save_file_name_buf[128] = "";
char UI::save_as_buf[64] = "";
char UI::new_set_name_buf[64] = "";

int UI::selected_save_id = -1;
int UI::selected_stamp_id = -1;
uint8_t UI::selected_id = 0;
int UI::mouse_size = 5;
BrushShape UI::brush_shape = BrushShape::Square;

enum class ExpandSide { Top, Bottom, Left, Right };
static int s_pending_expand_rule_idx = -1;
static uint8_t s_pending_expand_mat_id = 0;
static ExpandSide s_pending_expand_side = ExpandSide::Top;

bool UI::duplicate_set_checkbox = false;
bool UI::exit_save_as_new_set = false;

bool UI::update = false;
bool UI::step_frame = false;
bool UI::stepping_sequence_active = false;

bool UI::unsaved_changes = false;
std::string UI::pending_set_switch;
std::string UI::pending_save_load;
bool UI::ui_compact = false;

float UI::zoom = 1.0f;
float UI::pan_x = 0.0f;
float UI::pan_y = 0.0f;
float UI::target_zoom = 1.0f;
float UI::target_pan_x = 0.0f;
float UI::target_pan_y = 0.0f;

ToolMode UI::current_tool = ToolMode::Brush;
int UI::attr_tool_target_index = 0;
int UI::attr_tool_mode = 0;
float UI::attr_tool_value = 10.0f;
bool UI::brush_overwrite_attributes = true;
SelectionState UI::selection_state = SelectionState::None;
ResizeHandle UI::active_resize_handle = ResizeHandle::None;
SelectionBox UI::selection_box = {};
ClipboardData UI::clipboard = {};
std::vector<Cell> UI::floating_cells = {};
std::vector<Cell> UI::move_initial_cells = {};
int UI::floating_width = 0;
int UI::floating_height = 0;
int UI::move_initial_width = 0;
int UI::move_initial_height = 0;
int UI::move_origin_x = 0;
int UI::move_origin_y = 0;
int UI::move_grab_offset_x = 0;
int UI::move_grab_offset_y = 0;
int UI::current_floating_x = 0;
int UI::current_floating_y = 0;
bool UI::transparent_mode = false;

void UI::init() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigInputTrickleEventQueue = false;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	const auto& cfg = ConfigManager::get_config();
	float font_sz = std::clamp(cfg.ui.font_size, 10.0f, 48.0f);
	ImFontConfig font_cfg;
	font_cfg.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(roboto_ttf), roboto_ttf_len, font_sz, &font_cfg);

	UITheme::init_style();

	ImGui_ImplSDL3_InitForSDLRenderer(Window::get_window(), Window::get_renderer());
	ImGui_ImplSDLRenderer3_Init(Window::get_renderer());

	UndoManager::init();
	IconManager::init();
	WorkshopCache::init();
	WorkshopClient::init();

	if (!cfg.workshop.token.empty()) {
		WorkshopClient::set_auth_token(cfg.workshop.token);
		WorkshopClient::check_auth([](bool ok, const std::string& username) {
			if (!ok) {
				ConfigManager::get_config().workshop.token = "";
				ConfigManager::get_config().workshop.username = "";
				ConfigManager::save();
			} else {
				ConfigManager::get_config().workshop.username = username;
				ConfigManager::save();
			}
		});
	}
}

void UI::shutdown() {
	ConfigManager::save();
	IconManager::shutdown();
	WorkshopClient::shutdown();
	WorkshopCache::cleanup();
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
}

void UI::render() {
	WorkshopClient::update();
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	if (Window::is_busy()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Wait);
		Window::set_cursor_wait(true);
	} else {
		Window::set_cursor_wait(false);
	}

	ImGuiIO& io = ImGui::GetIO();
	auto& cfg = ConfigManager::get_config();

	if (ui_compact) {
		ImGui::SetNextWindowPos(ImVec2(static_cast<float>(cfg.ui.compact_x), static_cast<float>(cfg.ui.compact_y)),
								ImGuiCond_Once);
		ImGui::SetNextWindowSize(
			ImVec2(static_cast<float>(cfg.ui.compact_width), static_cast<float>(cfg.ui.compact_height)),
			ImGuiCond_Once);
		if (ImGui::Begin("Sand3 - Simulation Controls")) {
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz = ImGui::GetWindowSize();
			cfg.ui.compact_x = static_cast<int>(pos.x);
			cfg.ui.compact_y = static_cast<int>(pos.y);
			cfg.ui.compact_width = static_cast<int>(sz.x);
			cfg.ui.compact_height = static_cast<int>(sz.y);
			render_sim_content();
		}
		ImGui::End();
	} else {
		float screen_w = io.DisplaySize.x > 0.0f ? io.DisplaySize.x : static_cast<float>(Window::get_size().first);
		float screen_h = io.DisplaySize.y > 0.0f ? io.DisplaySize.y : static_cast<float>(Window::get_size().second);
		float sidebar_y = (cfg.ui.sidebar_y <= 2) ? 0.0f : static_cast<float>(cfg.ui.sidebar_y);
		float sidebar_x = (cfg.ui.sidebar_x <= 2) ? 0.0f : static_cast<float>(cfg.ui.sidebar_x);
		float target_h = std::max(100.0f, screen_h - sidebar_y);

		float max_sb_w = std::max(1200.0f, screen_w * 0.85f);
		ImGui::SetNextWindowPos(ImVec2(sidebar_x, sidebar_y), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(cfg.ui.sidebar_width), target_h), ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, target_h), ImVec2(max_sb_w, target_h));
		if (ImGui::Begin("##simulation_editor", nullptr,
						 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
							 ImGuiWindowFlags_NoResize)) {
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz = ImGui::GetWindowSize();
			cfg.ui.sidebar_x = static_cast<int>(pos.x);
			cfg.ui.sidebar_y = static_cast<int>(pos.y);
			cfg.ui.sidebar_width = static_cast<int>(sz.x);
			render_header(io);

			if (ImGui::BeginTabBar("SidebarTabs")) {
				render_material_editor();
				render_save_load();
				render_manage_sets();
				UITheme::render_theme_editor();
				UIWorkshop::render();
				UIShortcuts::render();
				UIAdvanced::render();
				ImGui::EndTabBar();
			}

			ImGui::End();
		}

		float bar_x = sidebar_x + static_cast<float>(cfg.ui.sidebar_width);
		float hit_w = 4.0f;

		ImGui::SetNextWindowPos(ImVec2(bar_x - 3.0f, sidebar_y));
		ImGui::SetNextWindowSize(ImVec2(hit_w, target_h));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		constexpr ImGuiWindowFlags kSplitterFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize;
		if (ImGui::Begin("##SidebarResizeOverlay", nullptr, kSplitterFlags)) {
			ImGui::InvisibleButton("##SidebarWidthResizeSplitter", ImVec2(hit_w, target_h));
			bool bar_hovered = ImGui::IsItemHovered();
			bool bar_held = ImGui::IsItemActive();
			if (bar_hovered || bar_held) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
			if (bar_held && io.MouseDelta.x != 0.0f) {
				cfg.ui.sidebar_width = std::clamp(cfg.ui.sidebar_width + static_cast<int>(io.MouseDelta.x), 260,
												  static_cast<int>(screen_w * 0.85f));
			}
			if (ImGui::IsItemDeactivated()) {
				ConfigManager::save();
			}
			if (bar_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				cfg.ui.sidebar_width = 520;
				ConfigManager::save();
			}

			ImDrawList* dl = ImGui::GetWindowDrawList();
			if (bar_hovered || bar_held) {
				ImU32 glow_col =
					bar_held ? ImGui::GetColorU32(ImGuiCol_ButtonActive) : ImGui::GetColorU32(ImGuiCol_SeparatorActive);
				dl->AddLine(ImVec2(bar_x, sidebar_y), ImVec2(bar_x, sidebar_y + target_h), glow_col,
							bar_held ? 3.0f : 2.0f);
				float mid_y = sidebar_y + target_h * 0.5f;
				dl->AddRectFilled(ImVec2(bar_x - 3.0f, mid_y - 20.0f), ImVec2(bar_x + 3.0f, mid_y + 20.0f), glow_col,
								  3.0f);
			} else {
				dl->AddLine(ImVec2(bar_x, sidebar_y), ImVec2(bar_x, sidebar_y + target_h),
							ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	render_mouse_overlay();
	render_floating_toolbar();

	UIModals::render();

	ImGuiContext& g = *GImGui;
	if (g.HoveredIdIsDisabled) {
		if (s_active_eq_cell_id < 0) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
		}
	} else if (g.HoveredId != 0 && ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
}

void UI::set_tool_mode(ToolMode mode) {
	if (mode == ToolMode::Attribute && SetManager::get_current_metadata().attributes.empty()) {
		return;
	}
	if (current_tool == mode)
		return;
	if (current_tool == ToolMode::Select) {
		deselect();
	}
	active_resize_handle = ResizeHandle::None;
	selection_state = SelectionState::None;
	s_line_brush_active = false;
	current_tool = mode;
	std::string tool_name = (current_tool == ToolMode::Brush)	 ? "Brush"
							: (current_tool == ToolMode::Select) ? "Select"
																 : "Attribute Tool";
	UndoManager::push_snapshot("Switched to " + tool_name);
}

void UI::deselect() {
	if (selection_state == SelectionState::Moving) {
		for (int y = 0; y < move_initial_height; ++y) {
			for (int x = 0; x < move_initial_width; ++x) {
				int gx = move_origin_x + x;
				int gy = move_origin_y + y;
				if (gx >= 0 && gx < static_cast<int>(Grid::get_width()) && gy >= 0 &&
					gy < static_cast<int>(Grid::get_height())) {
					Grid::set_cell_full(gx, gy, move_initial_cells[y * move_initial_width + x]);
				}
			}
		}
		floating_cells.clear();
		move_initial_cells.clear();
		floating_width = 0;
		floating_height = 0;
		move_initial_width = 0;
		move_initial_height = 0;
	}
	active_resize_handle = ResizeHandle::None;
	selection_state = SelectionState::None;
}

void UI::restore_selection_state(ToolMode mode, SelectionState state, const SelectionBox& box) {
	if (selection_state == SelectionState::Moving) {
		floating_cells.clear();
		move_initial_cells.clear();
		floating_width = 0;
		floating_height = 0;
		move_initial_width = 0;
		move_initial_height = 0;
	}
	active_resize_handle = ResizeHandle::None;
	current_tool = mode;
	if (state == SelectionState::Moving || state == SelectionState::Pasting || state == SelectionState::Resizing) {
		selection_state = SelectionState::Selected;
	} else {
		selection_state = state;
	}
	selection_box = box;
}

void UI::copy_selection() {
	if (selection_state != SelectionState::Selected)
		return;
	if (Grid::get_processing_mode() == ProcessingMode::GPU) {
		Grid::sync_from_gpu();
	}
	int min_x = selection_box.min_x();
	int min_y = selection_box.min_y();
	int bw = selection_box.width();
	int bh = selection_box.height();
	clipboard.width = bw;
	clipboard.height = bh;
	clipboard.cells.resize(bw * bh);
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			clipboard.cells[y * bw + x] = Grid::get_cell_full_const(min_x + x, min_y + y);
		}
	}
}

void UI::cut_selection() {
	if (selection_state != SelectionState::Selected)
		return;
	if (Grid::get_processing_mode() == ProcessingMode::GPU) {
		Grid::sync_from_gpu();
	}
	copy_selection();
	int min_x = selection_box.min_x();
	int min_y = selection_box.min_y();
	int bw = selection_box.width();
	int bh = selection_box.height();
	Cell empty_cell{};
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			Grid::set_cell_full(min_x + x, min_y + y, empty_cell);
		}
	}
	UndoManager::push_snapshot("Cut Selection");
	deselect();
}

void UI::fill_selection(uint8_t id) {
	if (selection_state != SelectionState::Selected)
		return;
	int min_x = selection_box.min_x();
	int min_y = selection_box.min_y();
	int bw = selection_box.width();
	int bh = selection_box.height();
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			if (UI::brush_overwrite_attributes) {
				Grid::set_cell(min_x + x, min_y + y, id);
			} else {
				Grid::set_cell_material_only(min_x + x, min_y + y, id);
			}
		}
	}
	UndoManager::push_snapshot("Filled Selection");
}

void UI::rotate_selection(bool clockwise) {
	if (selection_state == SelectionState::Selected) {
		int min_x = selection_box.min_x();
		int min_y = selection_box.min_y();
		int w = selection_box.width();
		int h = selection_box.height();
		if (w <= 0 || h <= 0)
			return;

		if (Grid::get_processing_mode() == ProcessingMode::GPU) {
			Grid::sync_from_gpu();
		}
		std::vector<Cell> old_cells(w * h);
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				old_cells[y * w + x] = Grid::get_cell_full_const(min_x + x, min_y + y);
			}
		}

		Cell empty_cell{};
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				if (!transparent_mode || old_cells[y * w + x].material != 0) {
					Grid::set_cell_full(min_x + x, min_y + y, empty_cell);
				}
			}
		}

		int new_w = h;
		int new_h = w;
		std::vector<Cell> new_cells(new_w * new_h);
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				int nx, ny;
				if (clockwise) {
					nx = (h - 1) - y;
					ny = x;
				} else {
					nx = y;
					ny = (w - 1) - x;
				}
				new_cells[ny * new_w + nx] = old_cells[y * w + x];
			}
		}

		int cx = min_x + w / 2;
		int cy = min_y + h / 2;
		int new_min_x = std::clamp(cx - new_w / 2, 0, std::max(0, static_cast<int>(Grid::get_width()) - new_w));
		int new_min_y = std::clamp(cy - new_h / 2, 0, std::max(0, static_cast<int>(Grid::get_height()) - new_h));

		for (int ny = 0; ny < new_h; ++ny) {
			for (int nx = 0; nx < new_w; ++nx) {
				const auto& c = new_cells[ny * new_w + nx];
				if (!transparent_mode || c.material != 0) {
					Grid::set_cell_full(new_min_x + nx, new_min_y + ny, c);
				}
			}
		}

		selection_box.start_x = new_min_x;
		selection_box.start_y = new_min_y;
		selection_box.current_x = new_min_x + new_w - 1;
		selection_box.current_y = new_min_y + new_h - 1;

		UndoManager::push_snapshot(clockwise ? "Rotate Selection CW" : "Rotate Selection CCW");
	} else if (selection_state == SelectionState::Moving) {
		int w = floating_width;
		int h = floating_height;
		if (w <= 0 || h <= 0 || floating_cells.size() < static_cast<size_t>(w * h))
			return;

		int new_w = h;
		int new_h = w;
		std::vector<Cell> new_cells(new_w * new_h);
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				int nx, ny;
				if (clockwise) {
					nx = (h - 1) - y;
					ny = x;
				} else {
					nx = y;
					ny = (w - 1) - x;
				}
				new_cells[ny * new_w + nx] = floating_cells[y * w + x];
			}
		}
		floating_cells = std::move(new_cells);
		floating_width = new_w;
		floating_height = new_h;

		int old_gx = move_grab_offset_x;
		int old_gy = move_grab_offset_y;
		if (clockwise) {
			move_grab_offset_x = (h - 1) - old_gy;
			move_grab_offset_y = old_gx;
		} else {
			move_grab_offset_x = old_gy;
			move_grab_offset_y = (w - 1) - old_gx;
		}

		int cx = current_floating_x + w / 2;
		int cy = current_floating_y + h / 2;
		current_floating_x = std::clamp(cx - new_w / 2, 0, std::max(0, static_cast<int>(Grid::get_width()) - new_w));
		current_floating_y = std::clamp(cy - new_h / 2, 0, std::max(0, static_cast<int>(Grid::get_height()) - new_h));

		move_grab_offset_x = std::clamp(move_grab_offset_x, 0, new_w - 1);
		move_grab_offset_y = std::clamp(move_grab_offset_y, 0, new_h - 1);

		selection_box.start_x = current_floating_x;
		selection_box.start_y = current_floating_y;
		selection_box.current_x = current_floating_x + new_w - 1;
		selection_box.current_y = current_floating_y + new_h - 1;
	} else if (selection_state == SelectionState::Pasting && !clipboard.empty()) {
		int w = clipboard.width;
		int h = clipboard.height;
		if (w <= 0 || h <= 0 || clipboard.cells.size() < static_cast<size_t>(w * h))
			return;

		int new_w = h;
		int new_h = w;
		std::vector<Cell> new_cells(new_w * new_h);
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				int nx, ny;
				if (clockwise) {
					nx = (h - 1) - y;
					ny = x;
				} else {
					nx = y;
					ny = (w - 1) - x;
				}
				new_cells[ny * new_w + nx] = clipboard.cells[y * w + x];
			}
		}
		clipboard.cells = std::move(new_cells);
		clipboard.width = new_w;
		clipboard.height = new_h;

		int cx = current_floating_x + w / 2;
		int cy = current_floating_y + h / 2;
		current_floating_x = std::clamp(cx - new_w / 2, 0, std::max(0, static_cast<int>(Grid::get_width()) - new_w));
		current_floating_y = std::clamp(cy - new_h / 2, 0, std::max(0, static_cast<int>(Grid::get_height()) - new_h));
	}
}

void UI::paste_clipboard() {
	if (clipboard.empty())
		return;
	current_tool = ToolMode::Select;
	selection_state = SelectionState::Pasting;
}

void UI::clear_clipboard() {
	clipboard.cells.clear();
	clipboard.width = 0;
	clipboard.height = 0;
}

void UI::load_stamp(const std::vector<Cell>& cells, uint32_t w, uint32_t h) {
	if (cells.empty() || w == 0 || h == 0)
		return;
	clipboard.cells = cells;
	clipboard.width = static_cast<int>(w);
	clipboard.height = static_cast<int>(h);
	paste_clipboard();
}

void UI::load_stamp(const std::vector<uint8_t>& cells, uint32_t w, uint32_t h) {
	if (cells.empty() || w == 0 || h == 0)
		return;
	clipboard.cells.resize(cells.size());
	for (size_t i = 0; i < cells.size(); ++i) {
		clipboard.cells[i].material = cells[i];
		clipboard.cells[i].a_aux = 0;
		clipboard.cells[i].a_prim = 0;
	}
	clipboard.width = static_cast<int>(w);
	clipboard.height = static_cast<int>(h);
	paste_clipboard();
}

void UI::start_moving_selection(int gx, int gy) {
	if (selection_state != SelectionState::Selected)
		return;
	if (Grid::get_processing_mode() == ProcessingMode::GPU) {
		Grid::sync_from_gpu();
	}
	int min_x = selection_box.min_x();
	int min_y = selection_box.min_y();
	int bw = selection_box.width();
	int bh = selection_box.height();
	floating_width = bw;
	floating_height = bh;
	move_initial_width = bw;
	move_initial_height = bh;
	floating_cells.resize(bw * bh);
	move_initial_cells.resize(bw * bh);
	Cell empty_cell{};
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			Cell c = Grid::get_cell_full_const(min_x + x, min_y + y);
			floating_cells[y * bw + x] = c;
			move_initial_cells[y * bw + x] = c;
			if (!transparent_mode || c.material != 0) {
				Grid::set_cell_full(min_x + x, min_y + y, empty_cell);
			}
		}
	}
	move_origin_x = min_x;
	move_origin_y = min_y;
	move_grab_offset_x = gx - min_x;
	move_grab_offset_y = gy - min_y;
	current_floating_x = min_x;
	current_floating_y = min_y;
	selection_state = SelectionState::Moving;
}

void UI::move_floating_selection(int gx, int gy) {
	if (selection_state != SelectionState::Moving)
		return;
	int bw = floating_width;
	int bh = floating_height;
	current_floating_x = std::clamp(gx - move_grab_offset_x, 0, std::max(0, static_cast<int>(Grid::get_width()) - bw));
	current_floating_y = std::clamp(gy - move_grab_offset_y, 0, std::max(0, static_cast<int>(Grid::get_height()) - bh));
	selection_box.start_x = current_floating_x;
	selection_box.start_y = current_floating_y;
	selection_box.current_x = current_floating_x + bw - 1;
	selection_box.current_y = current_floating_y + bh - 1;
}

void UI::drop_moving_selection() {
	if (selection_state != SelectionState::Moving)
		return;
	int bw = floating_width;
	int bh = floating_height;
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			int fgx = current_floating_x + x;
			int fgy = current_floating_y + y;
			if (fgx >= 0 && fgx < static_cast<int>(Grid::get_width()) && fgy >= 0 &&
				fgy < static_cast<int>(Grid::get_height())) {
				const Cell& cell = floating_cells[y * bw + x];
				if (!transparent_mode || cell.material != 0) {
					Grid::set_cell_full(fgx, fgy, cell);
				}
			}
		}
	}
	floating_cells.clear();
	move_initial_cells.clear();
	floating_width = 0;
	floating_height = 0;
	move_initial_width = 0;
	move_initial_height = 0;
	if (current_floating_x != move_origin_x || current_floating_y != move_origin_y) {
		UndoManager::push_snapshot("Move Selection");
	}
	selection_box.start_x = current_floating_x;
	selection_box.start_y = current_floating_y;
	selection_box.current_x = current_floating_x + bw - 1;
	selection_box.current_y = current_floating_y + bh - 1;
	selection_state = SelectionState::Selected;
}

bool UI::button_with_icon(const char* label, SDL_Texture* icon, const ImVec2& size_arg) {
	return UIWidgets::button_with_icon(label, icon, size_arg);
}

bool UI::selectable_with_color(const char* label, const ImVec4& color, bool selected, ImGuiSelectableFlags flags,
							   const ImVec2& size_arg, bool show_checkbox) {
	return UIWidgets::selectable_with_color(label, color, selected, flags, size_arg, show_checkbox);
}

bool UI::custom_checkbox(const char* label, bool* val, IconID icon_id, const char* tooltip, const ImVec2& size) {
	return UIWidgets::custom_checkbox(label, val, icon_id, tooltip, size);
}

bool UI::gradient_editor(const char* label, ColorGradient& gradient, float min_val, float max_val, float width) {
	return UIWidgets::gradient_editor(label, gradient, min_val, max_val, width);
}

void UI::render_floating_toolbar() {
	static bool s_options_expanded = false;

	ImGuiViewport* vp = ImGui::GetMainViewport();
	float screen_w = vp->WorkSize.x;
	float screen_h = vp->WorkSize.y;
	ImVec2 pivot(1.0f, 0.0f);
	ImVec2 target_pos(vp->WorkPos.x + screen_w - 12.0f, vp->WorkPos.y + 12.0f);

	ImGui::SetNextWindowPos(target_pos, ImGuiCond_Always, pivot);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
							 ImGuiWindowFlags_NoCollapse;

	const auto& cfg = ConfigManager::get_config();
	float btn_sz = (cfg.ui.button_size > 0) ? static_cast<float>(cfg.ui.button_size) : 32.0f;
	const auto& set_meta = SetManager::get_current_metadata();
	bool has_attrs = !set_meta.attributes.empty();

	if (current_tool == ToolMode::Attribute && !has_attrs) {
		set_tool_mode(ToolMode::Brush);
	}

	const auto& style = ImGui::GetStyle();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, style.WindowRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);

	if (ImGui::Begin("Sand3 - Toolbar", nullptr, flags)) {
		bool is_brush = (current_tool == ToolMode::Brush);
		bool is_select = (current_tool == ToolMode::Select);
		bool is_attr = (current_tool == ToolMode::Attribute);

		const ImVec4& btn_active_col = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
		ImVec4 btn_active_hover =
			ImVec4(std::min(1.0f, btn_active_col.x * 1.15f + 0.05f), std::min(1.0f, btn_active_col.y * 1.15f + 0.05f),
				   std::min(1.0f, btn_active_col.z * 1.15f + 0.05f), btn_active_col.w);

		if (is_brush) {
			ImGui::PushStyleColor(ImGuiCol_Button, btn_active_col);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_active_hover);
		}
		if (button_with_icon("##BrushTool", IconManager::get(IconID::Brush), ImVec2(btn_sz, btn_sz))) {
			set_tool_mode(ToolMode::Brush);
		}
		if (is_brush) {
			ImGui::PopStyleColor(2);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Drawing brush tool (%s)\nDraw cells on canvas",
							  ShortcutManager::get_key_string(ShortcutAction::ToolBrush).c_str());
		}

		ImGui::SameLine(0.0f, get_spacing(0.0f));

		if (is_select) {
			ImGui::PushStyleColor(ImGuiCol_Button, btn_active_col);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_active_hover);
		}
		if (button_with_icon("##SelectTool", IconManager::get(IconID::Select), ImVec2(btn_sz, btn_sz))) {
			set_tool_mode(ToolMode::Select);
		}
		if (is_select) {
			ImGui::PopStyleColor(2);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Box selection and transform tool (%s)\nSelect rectangular regions",
							  ShortcutManager::get_key_string(ShortcutAction::ToolSelect).c_str());
		}

		ImGui::SameLine(0.0f, get_spacing(0.0f));

		if (!has_attrs) {
			ImGui::BeginDisabled();
		}
		if (is_attr) {
			ImGui::PushStyleColor(ImGuiCol_Button, btn_active_col);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_active_hover);
		}
		if (button_with_icon("##AttrTool", IconManager::get(IconID::Edit), ImVec2(btn_sz, btn_sz))) {
			set_tool_mode(ToolMode::Attribute);
		}
		if (is_attr) {
			ImGui::PopStyleColor(2);
		}
		if (!has_attrs) {
			ImGui::EndDisabled();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			if (!has_attrs) {
				ImGui::SetTooltip("Attribute brush tool (Disabled: current set has no attributes)");
			} else {
				ImGui::SetTooltip("Attribute brush tool (%s)\nApply continuous physics attributes on canvas",
								  ShortcutManager::get_key_string(ShortcutAction::ToolAttribute).c_str());
			}
		}

		ImGui::SameLine(0.0f, get_spacing(0.0f));

		bool was_options_expanded = s_options_expanded;
		if (was_options_expanded) {
			ImGui::PushStyleColor(ImGuiCol_Button, btn_active_col);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_active_hover);
		}
		if (button_with_icon("##ToolOptions", IconManager::get(s_options_expanded ? IconID::Update : IconID::ArrowDown),
							 ImVec2(btn_sz, btn_sz))) {
			s_options_expanded = !s_options_expanded;
		}
		if (was_options_expanded) {
			ImGui::PopStyleColor(2);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s tool options drawer", s_options_expanded ? "Collapse" : "Expand");
		}

		if (s_options_expanded) {
			ImGui::Separator();
			ImGui::Spacing();

			if (current_tool == ToolMode::Brush) {
				ImGui::TextDisabled("Brush Settings");
				ImGui::SetNextItemWidth(130.0f);
				if (ImGui::InputInt("Size##_brush_sz", &mouse_size, 1, 5)) {
					mouse_size = std::clamp(mouse_size, 1, static_cast<int>(Grid::get_width() * 0.5f));
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Brush diameter in cells (Mouse Scroll)");
				}

				const char* shape_names[] = {"Square", "Circle"};
				int current_shape = static_cast<int>(brush_shape);
				ImGui::SetNextItemWidth(130.0f);
				if (ImGui::Combo("Shape##_brush_shape", &current_shape, shape_names, IM_ARRAYSIZE(shape_names))) {
					brush_shape = static_cast<BrushShape>(current_shape);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Toggle shape (%s)",
									  ShortcutManager::get_key_string(ShortcutAction::BrushShape).c_str());
				}

				custom_checkbox(
					"Overwrite##_ow_brush", &brush_overwrite_attributes, IconID::Cog,
					"When unchecked, painting or filling changes the cell material without resetting its attributes.");
			} else if (current_tool == ToolMode::Select) {
				ImGui::TextDisabled("Selection Settings");
				bool has_selection = (selection_state == SelectionState::Selected);
				bool has_clipboard = !clipboard.empty();
				bool can_rotate = (has_selection || selection_state == SelectionState::Moving ||
								   (selection_state == SelectionState::Pasting && !clipboard.empty()));

				ImVec2 act_btn_sz(28.0f, 28.0f);

				if (!has_selection)
					ImGui::BeginDisabled();
				if (button_with_icon("##CopySel", IconManager::get(IconID::Copy), act_btn_sz))
					copy_selection();
				if (!has_selection)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Copy (%s)", ShortcutManager::get_key_string(ShortcutAction::Copy).c_str());

				ImGui::SameLine(0.0f, get_spacing(-2.0f));
				if (!has_selection)
					ImGui::BeginDisabled();
				if (button_with_icon("##CutSel", IconManager::get(IconID::Cut), act_btn_sz))
					cut_selection();
				if (!has_selection)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Cut (%s)", ShortcutManager::get_key_string(ShortcutAction::Cut).c_str());

				ImGui::SameLine(0.0f, get_spacing(-2.0f));
				if (!has_clipboard)
					ImGui::BeginDisabled();
				if (button_with_icon("##PasteSel", IconManager::get(IconID::Paste), act_btn_sz))
					paste_clipboard();
				if (!has_clipboard)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Paste (%s)", ShortcutManager::get_key_string(ShortcutAction::Paste).c_str());

				ImGui::SameLine(0.0f, get_spacing(-2.0f));
				if (!has_selection)
					ImGui::BeginDisabled();
				if (button_with_icon("##DeleteSel", IconManager::get(IconID::Delete), act_btn_sz)) {
					fill_selection(0);
					deselect();
				}
				if (!has_selection)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Delete (%s)", ShortcutManager::get_key_string(ShortcutAction::Delete).c_str());

				if (!has_selection)
					ImGui::BeginDisabled();
				if (button_with_icon("##FillSel", IconManager::get(IconID::Fill), act_btn_sz))
					fill_selection(selected_id);
				if (!has_selection)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Fill (%s)", ShortcutManager::get_key_string(ShortcutAction::Fill).c_str());

				ImGui::SameLine(0.0f, get_spacing(-2.0f));
				if (!has_selection)
					ImGui::BeginDisabled();
				if (button_with_icon("##TransparentSel", IconManager::get(IconID::Select), act_btn_sz))
					transparent_mode = !transparent_mode;
				if (!has_selection)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("When enabled, air/empty cells in clipboard or moved selection will not "
									  "overwrite existing cells. (%s)",
									  ShortcutManager::get_key_string(ShortcutAction::ToggleTransparent).c_str());

				ImGui::SameLine(0.0f, get_spacing(-2.0f));
				if (!can_rotate)
					ImGui::BeginDisabled();
				if (button_with_icon("##RotateCWSel", IconManager::get(IconID::RotateCW), act_btn_sz))
					rotate_selection(true);
				if (!can_rotate)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Rotate CW (%s)",
									  ShortcutManager::get_key_string(ShortcutAction::RotateCW).c_str());

				ImGui::SameLine(0.0f, get_spacing(-2.0f));
				if (!can_rotate)
					ImGui::BeginDisabled();
				if (button_with_icon("##RotateCCWSel", IconManager::get(IconID::RotateCCW), act_btn_sz))
					rotate_selection(false);
				if (!can_rotate)
					ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("Rotate CCW (%s)",
									  ShortcutManager::get_key_string(ShortcutAction::RotateCCW).c_str());
			} else if (current_tool == ToolMode::Attribute) {
				ImGui::TextDisabled("Attribute Brush Settings");
				ImGui::SetNextItemWidth(130.0f);
				if (ImGui::InputInt("Size##_attr_sz", &mouse_size, 1, 5)) {
					mouse_size = std::clamp(mouse_size, 1, static_cast<int>(Grid::get_width() * 0.5f));
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Brush diameter in cells (Mouse Scroll)");
				}

				const char* shape_names[] = {"Square", "Circle"};
				int current_shape = static_cast<int>(brush_shape);
				ImGui::SetNextItemWidth(130.0f);
				if (ImGui::Combo("Shape##_attr_shape", &current_shape, shape_names, IM_ARRAYSIZE(shape_names))) {
					brush_shape = static_cast<BrushShape>(current_shape);
				}

				std::string current_attr_name = (attr_tool_target_index >= 0 &&
												 attr_tool_target_index < static_cast<int>(set_meta.attributes.size()))
													? set_meta.attributes[attr_tool_target_index].name
													: "Select Attribute";
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::BeginCombo("Attr##_target_attr", current_attr_name.c_str())) {
					for (size_t a = 0; a < set_meta.attributes.size(); ++a) {
						if (ImGui::Selectable(set_meta.attributes[a].name.c_str(),
											  attr_tool_target_index == static_cast<int>(a))) {
							attr_tool_target_index = static_cast<int>(a);
						}
					}
					ImGui::EndCombo();
				}

				const char* tool_modes[] = {"Add / Subtract", "Set Exact Value"};
				ImGui::SetNextItemWidth(150.0f);
				ImGui::Combo("Mode##_attr_mode", &attr_tool_mode, tool_modes, IM_ARRAYSIZE(tool_modes));

				ImGui::SetNextItemWidth(150.0f);
				ImGui::InputFloat(attr_tool_mode == 0 ? "Delta##_val" : "Value##_val", &attr_tool_value, 1.0f, 10.0f,
								  "%.2f");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Shortcut keys: [ to decrease, ] to increase");
				}
			}
		}

		ImGui::End();
	}
	ImGui::PopStyleVar(2);
}

void UI::render_mouse_overlay() {
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

	if (current_tool == ToolMode::Select) {
		if (selection_state == SelectionState::Selecting || selection_state == SelectionState::Selected ||
			selection_state == SelectionState::Resizing) {
			int min_x = selection_box.min_x();
			int min_y = selection_box.min_y();
			int bw = selection_box.width();
			int bh = selection_box.height();
			ScreenRect srect = grid_to_screen_rect_wh(min_x, min_y, bw, bh);

			const auto& ui_cfg = ConfigManager::get_config().ui;
			ImU32 fill_col = ImGui::ColorConvertFloat4ToU32(ui_cfg.selection_box_fill);
			ImU32 border_col = ImGui::ColorConvertFloat4ToU32(ui_cfg.selection_box_color);

			draw_list->AddRectFilled(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), fill_col);

			draw_list->AddRect(ImVec2(srect.x1 - 1.0f, srect.y1 - 1.0f), ImVec2(srect.x2 + 1.0f, srect.y2 + 1.0f),
							   IM_COL32(0, 0, 0, 220), 0.0f, 0, 2.0f);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), border_col, 0.0f, 0, 1.5f);

			if (selection_state == SelectionState::Selected || selection_state == SelectionState::Resizing) {
				float handle_size = 5.0f;
				ImVec2 handle_pts[8] = {ImVec2(srect.x1, srect.y1), ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y1),
										ImVec2(srect.x2, srect.y1), ImVec2(srect.x2, (srect.y1 + srect.y2) * 0.5f),
										ImVec2(srect.x2, srect.y2), ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y2),
										ImVec2(srect.x1, srect.y2), ImVec2(srect.x1, (srect.y1 + srect.y2) * 0.5f)};
				ResizeHandle handle_enums[8] = {
					ResizeHandle::TopLeft,	   ResizeHandle::Top,	 ResizeHandle::TopRight,   ResizeHandle::Right,
					ResizeHandle::BottomRight, ResizeHandle::Bottom, ResizeHandle::BottomLeft, ResizeHandle::Left};
				ResizeHandle hovered_handle = (selection_state == SelectionState::Resizing)
												  ? active_resize_handle
												  : get_hovered_resize_handle(srect, io.MousePos);

				for (int i = 0; i < 8; ++i) {
					bool is_active = (hovered_handle == handle_enums[i]);
					float sz = is_active ? (handle_size + 1.5f) : handle_size;
					ImU32 handle_fill_col = is_active ? border_col : IM_COL32(255, 255, 255, 255);
					ImU32 handle_border_col = is_active ? IM_COL32(0, 50, 100, 255) : IM_COL32(0, 0, 0, 255);
					draw_list->AddRectFilled(ImVec2(handle_pts[i].x - sz, handle_pts[i].y - sz),
											 ImVec2(handle_pts[i].x + sz, handle_pts[i].y + sz), handle_fill_col);
					draw_list->AddRect(ImVec2(handle_pts[i].x - sz, handle_pts[i].y - sz),
									   ImVec2(handle_pts[i].x + sz, handle_pts[i].y + sz), handle_border_col, 0.0f, 0,
									   is_active ? 1.5f : 1.0f);
				}

				char dim_buf[32];
				std::snprintf(dim_buf, sizeof(dim_buf), "%d x %d", bw, bh);
				ImVec2 text_size = ImGui::CalcTextSize(dim_buf);
				float badge_pad_x = 4.0f;
				float badge_pad_y = 2.0f;
				float badge_x = srect.x1;
				float badge_y = srect.y1 - text_size.y - badge_pad_y * 2.0f - 4.0f;
				if (badge_y < 10.0f) {
					badge_y = srect.y2 + 4.0f;
				}
				draw_list->AddRectFilled(
					ImVec2(badge_x, badge_y),
					ImVec2(badge_x + text_size.x + badge_pad_x * 2.0f, badge_y + text_size.y + badge_pad_y * 2.0f),
					IM_COL32(20, 20, 20, 220), 3.0f);
				draw_list->AddText(ImVec2(badge_x + badge_pad_x, badge_y + badge_pad_y), IM_COL32(220, 220, 220, 255),
								   dim_buf);
			}
		} else if (selection_state == SelectionState::Moving) {
			int bw = floating_width;
			int bh = floating_height;
			ScreenRect srect = grid_to_screen_rect_wh(current_floating_x, current_floating_y, bw, bh);

			for (int y = 0; y < bh; ++y) {
				int x = 0;
				while (x < bw) {
					uint8_t m = floating_cells[y * bw + x].material;
					if (m == 0 && transparent_mode) {
						x++;
						continue;
					}
					int start_x = x;
					while (x < bw && floating_cells[y * bw + x].material == m) {
						x++;
					}
					int len = x - start_x;
					ScreenRect r = grid_to_screen_rect_wh(current_floating_x + start_x, current_floating_y + y, len, 1);
					if (m != 0) {
						const auto& mat_def = MaterialManager::get_material(m);
						draw_list->AddRectFilled(ImVec2(r.x1, r.y1), ImVec2(r.x2, r.y2),
												 IM_COL32(mat_def.color[0], mat_def.color[1], mat_def.color[2], 210));
					} else {
						draw_list->AddRectFilled(ImVec2(r.x1, r.y1), ImVec2(r.x2, r.y2), IM_COL32(40, 40, 40, 160));
					}
				}
			}

			draw_list->AddRect(ImVec2(srect.x1 - 1.0f, srect.y1 - 1.0f), ImVec2(srect.x2 + 1.0f, srect.y2 + 1.0f),
							   IM_COL32(0, 0, 0, 220), 0.0f, 0, 2.0f);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), IM_COL32(255, 200, 50, 255),
							   0.0f, 0, 1.5f);
		} else if (selection_state == SelectionState::Pasting && !clipboard.empty()) {
			int bw = clipboard.width;
			int bh = clipboard.height;
			ScreenRect srect = grid_to_screen_rect_wh(current_floating_x, current_floating_y, bw, bh);

			for (int y = 0; y < bh; ++y) {
				int x = 0;
				while (x < bw) {
					uint8_t m = clipboard.cells[y * bw + x].material;
					if (m == 0 && transparent_mode) {
						x++;
						continue;
					}
					int start_x = x;
					while (x < bw && clipboard.cells[y * bw + x].material == m) {
						x++;
					}
					int len = x - start_x;
					ScreenRect r = grid_to_screen_rect_wh(current_floating_x + start_x, current_floating_y + y, len, 1);
					if (m != 0) {
						const auto& mat_def = MaterialManager::get_material(m);
						draw_list->AddRectFilled(ImVec2(r.x1, r.y1), ImVec2(r.x2, r.y2),
												 IM_COL32(mat_def.color[0], mat_def.color[1], mat_def.color[2], 210));
					} else {
						draw_list->AddRectFilled(ImVec2(r.x1, r.y1), ImVec2(r.x2, r.y2), IM_COL32(40, 40, 40, 160));
					}
				}
			}

			draw_list->AddRect(ImVec2(srect.x1 - 1.0f, srect.y1 - 1.0f), ImVec2(srect.x2 + 1.0f, srect.y2 + 1.0f),
							   IM_COL32(0, 0, 0, 220), 0.0f, 0, 2.0f);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), IM_COL32(50, 255, 120, 255),
							   0.0f, 0, 1.5f);

			char paste_buf[64];
			std::snprintf(paste_buf, sizeof(paste_buf), "Paste (%dx%d) - Left Click to Stamp", bw, bh);
			ImVec2 text_size = ImGui::CalcTextSize(paste_buf);
			float badge_x = srect.x1;
			float badge_y = srect.y1 - text_size.y - 8.0f;
			if (badge_y < 10.0f)
				badge_y = srect.y2 + 4.0f;
			draw_list->AddRectFilled(ImVec2(badge_x, badge_y),
									 ImVec2(badge_x + text_size.x + 8.0f, badge_y + text_size.y + 4.0f),
									 IM_COL32(20, 20, 20, 220), 3.0f);
			draw_list->AddText(ImVec2(badge_x + 4.0f, badge_y + 2.0f), IM_COL32(100, 255, 150, 255), paste_buf);
		}

		if (!io.WantCaptureMouse) {
			if (selection_state == SelectionState::Moving) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
			} else if (selection_state == SelectionState::Pasting) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
			} else if (selection_state == SelectionState::Selecting) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
			} else if (selection_state == SelectionState::Resizing) {
				ImGui::SetMouseCursor(get_cursor_for_resize_handle(active_resize_handle));
			} else if (selection_state == SelectionState::Selected) {
				ScreenRect srect = grid_to_screen_rect_wh(selection_box.min_x(), selection_box.min_y(),
														  selection_box.width(), selection_box.height());
				ResizeHandle h = get_hovered_resize_handle(srect, io.MousePos);
				if (h != ResizeHandle::None) {
					ImGui::SetMouseCursor(get_cursor_for_resize_handle(h));
				} else {
					ImVec2 gp = screen_to_grid_pos(io.MousePos);
					if (selection_box.contains(static_cast<int>(gp.x), static_cast<int>(gp.y))) {
						ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
					}
				}
			} else if (selection_state == SelectionState::None) {
				ImVec2 cur_mouse = io.MousePos;
				float ch_size = 8.0f;
				draw_list->AddLine(ImVec2(cur_mouse.x - ch_size, cur_mouse.y),
								   ImVec2(cur_mouse.x + ch_size, cur_mouse.y), IM_COL32(255, 255, 255, 200), 1.5f);
				draw_list->AddLine(ImVec2(cur_mouse.x, cur_mouse.y - ch_size),
								   ImVec2(cur_mouse.x, cur_mouse.y + ch_size), IM_COL32(255, 255, 255, 200), 1.5f);
			}
		}
		s_line_brush_active = false;
		return;
	}

	if (io.WantCaptureMouse) {
		s_line_brush_active = false;
		return;
	}

	ImVec2 cur_mouse = io.MousePos;

	bool is_shift_down = io.KeyShift;
	bool is_alt_down = io.KeyAlt;
	bool is_left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool is_right_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);

	if (is_shift_down && is_alt_down) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	} else if (is_shift_down || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
	}

	const auto& mat = MaterialManager::get_material(is_right_down ? 0 : selected_id);
	uint8_t r = mat.color[0];
	uint8_t g = mat.color[1];
	uint8_t b = mat.color[2];

	float lum = 0.299f * r + 0.587f * g + 0.114f * b;
	uint8_t cr, cg, cb;
	if (lum > 128.0f) {
		cr = static_cast<uint8_t>(r * 0.25f);
		cg = static_cast<uint8_t>(g * 0.25f);
		cb = static_cast<uint8_t>(b * 0.25f);
	} else {
		cr = static_cast<uint8_t>(std::min(255.0f, r + (255.0f - r) * 0.75f + 40.0f));
		cg = static_cast<uint8_t>(std::min(255.0f, g + (255.0f - g) * 0.75f + 40.0f));
		cb = static_cast<uint8_t>(std::min(255.0f, b + (255.0f - b) * 0.75f + 40.0f));
	}

	ImU32 fill_color;
	ImU32 line_color;
	ImU32 inner_line_color;
	if (current_tool == ToolMode::Attribute) {
		fill_color = IM_COL32(255, 175, 50, 70);
		line_color = IM_COL32(255, 210, 80, 255);
		inner_line_color = IM_COL32(255, 140, 20, 240);
	} else {
		fill_color = IM_COL32(r, g, b, 70);
		line_color = IM_COL32(cr, cg, cb, 255);
		inner_line_color = IM_COL32(r, g, b, 240);
	}

	if (is_shift_down && is_alt_down) {
		float cross_size = 14.0f;

		draw_list->AddLine(ImVec2(cur_mouse.x - cross_size - 1.0f, cur_mouse.y),
						   ImVec2(cur_mouse.x + cross_size + 1.0f, cur_mouse.y), line_color, 4.0f);
		draw_list->AddLine(ImVec2(cur_mouse.x, cur_mouse.y - cross_size - 1.0f),
						   ImVec2(cur_mouse.x, cur_mouse.y + cross_size + 1.0f), line_color, 4.0f);

		draw_list->AddLine(ImVec2(cur_mouse.x - cross_size, cur_mouse.y), ImVec2(cur_mouse.x + cross_size, cur_mouse.y),
						   inner_line_color, 2.0f);
		draw_list->AddLine(ImVec2(cur_mouse.x, cur_mouse.y - cross_size), ImVec2(cur_mouse.x, cur_mouse.y + cross_size),
						   inner_line_color, 2.0f);

		ImVec2 grid_pos = screen_to_grid_pos(cur_mouse);
		int cx = static_cast<int>(std::floor(grid_pos.x));
		int cy = static_cast<int>(std::floor(grid_pos.y));
		ScreenRect scell = grid_to_screen_rect(cx, cy, 1);
		draw_list->AddRect(ImVec2(scell.x1, scell.y1), ImVec2(scell.x2, scell.y2), line_color, 0.0f, 0, 3.0f);
		draw_list->AddRect(ImVec2(scell.x1, scell.y1), ImVec2(scell.x2, scell.y2), inner_line_color, 0.0f, 0, 1.5f);
	} else if (is_shift_down && (is_left_down || is_right_down)) {
		ImGuiMouseButton btn = is_left_down ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
		if (!s_line_brush_active) {
			s_line_brush_start_grid = screen_to_grid_pos(io.MouseClickedPos[btn]);
			s_line_brush_active = true;
		}

		ImVec2 start_grid = s_line_brush_start_grid;
		ImVec2 end_grid = screen_to_grid_pos(cur_mouse);

		std::vector<LineSpan> spans = compute_line_spans(start_grid, end_grid, mouse_size, brush_shape);
		render_pixel_spans(draw_list, spans, fill_color, line_color, inner_line_color);
	} else {
		if (!is_left_down && !is_right_down && !ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
			!ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
			s_line_brush_active = false;
		}
		ImVec2 grid_pos = screen_to_grid_pos(cur_mouse);
		GridRect brush = calculate_brush_bounds(grid_pos, mouse_size);

		if (brush_shape == BrushShape::Square || mouse_size <= 1) {
			ScreenRect srect = grid_to_screen_rect_wh(brush.x, brush.y, brush.w, brush.w);
			draw_list->AddRectFilled(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), fill_color, 0.0f, 0);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), line_color, 0.0f, 0, 3.0f);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), inner_line_color, 0.0f, 0, 1.5f);
		} else {
			std::vector<LineSpan> spans = compute_line_spans(grid_pos, grid_pos, mouse_size, BrushShape::Circle);
			render_pixel_spans(draw_list, spans, fill_color, line_color, inner_line_color);
		}
	}
}

void UI::handle_zoom_and_pan(ImGuiIO& io) {
	auto [win_w, win_h] = Window::get_size();
	float rem_w = static_cast<float>(win_w);
	float rem_h = static_cast<float>(win_h);
	float sim_aspect = static_cast<float>(Grid::get_width()) / Grid::get_height();

	float target_w = rem_w;
	float target_h = rem_w / sim_aspect;
	if (target_h > rem_h) {
		target_h = rem_h;
		target_w = rem_h * sim_aspect;
	}

	if (!io.WantCaptureMouse && io.MouseWheel != 0.0f && io.KeyShift) {
		target_zoom += io.MouseWheel * 0.2f * target_zoom;
	}

	float dt = std::min(io.DeltaTime, 0.1f);
	if (dt <= 0.0f)
		dt = 0.016f;

	if (!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt) {
		float pan_dir_x = 0.0f;
		float pan_dir_y = 0.0f;
		if (ShortcutManager::is_action_down(ShortcutAction::CameraUp))
			pan_dir_y -= 1.0f;
		if (ShortcutManager::is_action_down(ShortcutAction::CameraDown))
			pan_dir_y += 1.0f;
		if (ShortcutManager::is_action_down(ShortcutAction::CameraLeft))
			pan_dir_x -= 1.0f;
		if (ShortcutManager::is_action_down(ShortcutAction::CameraRight))
			pan_dir_x += 1.0f;

		if (pan_dir_x != 0.0f || pan_dir_y != 0.0f) {
			float len = std::hypot(pan_dir_x, pan_dir_y);
			if (len > 0.0f) {
				pan_dir_x /= len;
				pan_dir_y /= len;
			}
			float speed = 800.0f / std::max(zoom, 0.05f);
			if (io.KeyShift) {
				speed *= 2.5f;
			}
			target_pan_x += pan_dir_x * speed * dt;
			target_pan_y += pan_dir_y * speed * dt;
			target_pan_x =
				std::clamp(target_pan_x, -static_cast<float>(Grid::get_width()), static_cast<float>(Grid::get_width()));
			target_pan_y = std::clamp(target_pan_y, -static_cast<float>(Grid::get_height()),
									  static_cast<float>(Grid::get_height()));
		}
	}

	float t = 1.0f - std::exp(-10.0f * dt);
	zoom = zoom + (target_zoom - zoom) * t;
	pan_x = pan_x + (target_pan_x - pan_x) * t;
	pan_y = pan_y + (target_pan_y - pan_y) * t;

	float w_new = target_w * zoom;
	float h_new = target_h * zoom;

	if (!io.WantCaptureMouse && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
		ImVec2 delta = io.MouseDelta;
		target_pan_x -= delta.x * (Grid::get_width() / w_new);
		target_pan_y -= delta.y * (Grid::get_height() / h_new);
		target_pan_x =
			std::clamp(target_pan_x, -static_cast<float>(Grid::get_width()), static_cast<float>(Grid::get_width()));
		target_pan_y =
			std::clamp(target_pan_y, -static_cast<float>(Grid::get_height()), static_cast<float>(Grid::get_height()));
	}

	pan_x = pan_x + (target_pan_x - pan_x) * 0.15f;
	pan_y = pan_y + (target_pan_y - pan_y) * 0.15f;

	float offset_screen_x = pan_x * (w_new / Grid::get_width());
	float offset_screen_y = pan_y * (h_new / Grid::get_height());
	float center_x_shifted = rem_w / 2.0f - offset_screen_x;
	float center_y_shifted = rem_h / 2.0f - offset_screen_y;

	SDL_FRect dst_rect;
	dst_rect.x = center_x_shifted - w_new / 2.0f;
	dst_rect.y = center_y_shifted - h_new / 2.0f;
	dst_rect.w = w_new;
	dst_rect.h = h_new;
	Window::set_dst_rect(dst_rect);
}

void UI::handle_keyboard_shortcuts(ImGuiIO& io) {
	if (io.WantTextInput)
		return;

	if (ShortcutManager::is_action_pressed(ShortcutAction::Undo)) {
		UndoManager::undo();
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::Redo)) {
		UndoManager::redo();
	}

	if (ShortcutManager::is_action_pressed(ShortcutAction::Copy)) {
		if (current_tool != ToolMode::Select) {
			set_tool_mode(ToolMode::Select);
		}
		copy_selection();
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::Cut)) {
		if (selection_state != SelectionState::Selected) {
			set_tool_mode(ToolMode::Select);
		}
		cut_selection();
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::Paste)) {
		if (selection_state != SelectionState::Selected) {
			set_tool_mode(ToolMode::Select);
		}
		paste_clipboard();
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::ToolBrush)) {
		set_tool_mode(ToolMode::Brush);
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::ToolSelect)) {
		set_tool_mode(ToolMode::Select);
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::ToolAttribute)) {
		const auto& set_meta = SetManager::get_current_metadata();
		if (!set_meta.attributes.empty()) {
			set_tool_mode(ToolMode::Attribute);
		}
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::IncreaseAttrValue, true)) {
		if (current_tool == ToolMode::Attribute) {
			const auto& set_meta = SetManager::get_current_metadata();
			float step = 1.0f;
			if (attr_tool_target_index >= 0 && attr_tool_target_index < static_cast<int>(set_meta.attributes.size())) {
				const auto& attr_def = set_meta.attributes[attr_tool_target_index];
				if (attr_def.type == AttributeType::Float && (attr_def.max_val - attr_def.min_val <= 2.0)) {
					step = 0.05f;
				} else if (attr_def.type == AttributeType::Float && (attr_def.max_val - attr_def.min_val <= 10.0)) {
					step = 0.5f;
				}
			}
			if (io.KeyShift)
				step *= 5.0f;
			attr_tool_value += step;
			if (attr_tool_mode == 1 && attr_tool_target_index >= 0 &&
				attr_tool_target_index < static_cast<int>(set_meta.attributes.size())) {
				const auto& attr_def = set_meta.attributes[attr_tool_target_index];
				attr_tool_value = std::clamp(attr_tool_value, static_cast<float>(attr_def.min_val),
											 static_cast<float>(attr_def.max_val));
			}
			ToastManager::info(fmt::format("Attribute Value: {:.2f}", attr_tool_value));
		}
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::DecreaseAttrValue, true)) {
		if (current_tool == ToolMode::Attribute) {
			const auto& set_meta = SetManager::get_current_metadata();
			float step = 1.0f;
			if (attr_tool_target_index >= 0 && attr_tool_target_index < static_cast<int>(set_meta.attributes.size())) {
				const auto& attr_def = set_meta.attributes[attr_tool_target_index];
				if (attr_def.type == AttributeType::Float && (attr_def.max_val - attr_def.min_val <= 2.0)) {
					step = 0.05f;
				} else if (attr_def.type == AttributeType::Float && (attr_def.max_val - attr_def.min_val <= 10.0)) {
					step = 0.5f;
				}
			}
			if (io.KeyShift)
				step *= 5.0f;
			attr_tool_value -= step;
			if (attr_tool_mode == 1 && attr_tool_target_index >= 0 &&
				attr_tool_target_index < static_cast<int>(set_meta.attributes.size())) {
				const auto& attr_def = set_meta.attributes[attr_tool_target_index];
				attr_tool_value = std::clamp(attr_tool_value, static_cast<float>(attr_def.min_val),
											 static_cast<float>(attr_def.max_val));
			}
			ToastManager::info(fmt::format("Attribute Value: {:.2f}", attr_tool_value));
		}
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::CycleDisplayMode)) {
		const auto& set_meta = SetManager::get_current_metadata();
		if (!set_meta.attributes.empty()) {
			int cur_hm = Grid::get_heatmap_mode();
			int total_attrs = static_cast<int>(set_meta.attributes.size());
			int next_hm = cur_hm + 1;
			if (next_hm >= total_attrs) {
				next_hm = -1;
			}
			Grid::set_heatmap_mode(next_hm);
			if (next_hm == -1) {
				ToastManager::info("Display Mode: Normal Colors");
			} else {
				ToastManager::info(fmt::format("Display Mode: Heatmap ({})", set_meta.attributes[next_hm].name));
			}
		}
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::Delete)) {
		if (selection_state == SelectionState::Selected) {
			fill_selection(0);
			deselect();
		}
	} else if (!update && ShortcutManager::is_action_pressed(ShortcutAction::StepFrame, true)) {
		step_frame = true;
	} else if (selection_state == SelectionState::Selected) {
		if (ShortcutManager::is_action_pressed(ShortcutAction::Fill)) {
			fill_selection(selected_id);
		}

		int nudge_x = 0;
		int nudge_y = 0;
		int step = io.KeyShift ? 10 : 1;
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
			nudge_x -= step;
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
			nudge_x += step;
		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			nudge_y -= step;
		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			nudge_y += step;

		if (nudge_x != 0 || nudge_y != 0) {
			int min_x = selection_box.min_x();
			int min_y = selection_box.min_y();
			int bw = selection_box.width();
			int bh = selection_box.height();
			int new_x = std::clamp(min_x + nudge_x, 0, static_cast<int>(Grid::get_width()) - bw);
			int new_y = std::clamp(min_y + nudge_y, 0, static_cast<int>(Grid::get_height()) - bh);
			if (new_x != min_x || new_y != min_y) {
				std::vector<uint8_t> temp(bw * bh);
				for (int y = 0; y < bh; ++y) {
					for (int x = 0; x < bw; ++x) {
						temp[y * bw + x] = Grid::get_cell(min_x + x, min_y + y);
						Grid::set_cell(min_x + x, min_y + y, 0);
					}
				}
				for (int y = 0; y < bh; ++y) {
					for (int x = 0; x < bw; ++x) {
						Grid::set_cell(new_x + x, new_y + y, temp[y * bw + x]);
					}
				}
				selection_box.start_x = new_x;
				selection_box.start_y = new_y;
				selection_box.current_x = new_x + bw - 1;
				selection_box.current_y = new_y + bh - 1;
			}
			UndoManager::push_snapshot("Nudge Selection");
		}
	}

	if (ShortcutManager::is_action_pressed(ShortcutAction::ToggleSimulation)) {
		update = !update;
		if (update) {
			UndoManager::push_snapshot("Resume Simulation");
		}
		return;
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::BrushShape)) {
		brush_shape = static_cast<BrushShape>((static_cast<int>(brush_shape) + 1) % static_cast<int>(BrushShape::Size));
		return;
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::ToggleCompact)) {
		ui_compact = !ui_compact;
		return;
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::ToggleTransparent)) {
		transparent_mode = !transparent_mode;
		return;
	}

	bool can_rotate = (selection_state == SelectionState::Selected || selection_state == SelectionState::Moving ||
					   (selection_state == SelectionState::Pasting && !clipboard.empty()));
	if (can_rotate) {
		if (ShortcutManager::is_action_pressed(ShortcutAction::RotateCW)) {
			rotate_selection(true);
			return;
		}
		if (ShortcutManager::is_action_pressed(ShortcutAction::RotateCCW)) {
			rotate_selection(false);
			return;
		}
	}

	if (ShortcutManager::is_action_pressed(ShortcutAction::ClearGrid)) {
		Grid::clear();
		UndoManager::push_snapshot("Clear Grid");
		return;
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::ZoomIn)) {
		target_zoom *= 1.2f;
		return;
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::ZoomOut)) {
		target_zoom /= 1.2f;
		return;
	}

	if (ShortcutManager::is_action_pressed(ShortcutAction::Fullscreen)) {
		SDL_Window* window = Window::get_window();
		SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
		return;
	} else if (ShortcutManager::is_action_pressed(ShortcutAction::CancelOrQuit)) {
		if (selection_state == SelectionState::Moving || selection_state == SelectionState::Selected ||
			selection_state == SelectionState::Selecting || selection_state == SelectionState::Resizing) {
			deselect();
		} else if (selection_state == SelectionState::Pasting) {
			selection_state = SelectionState::None;
		} else {
			UIModals::open_exit();
		}
		return;
	}

	auto mat_shortcuts = get_all_material_shortcuts();
	for (const auto& item : mat_shortcuts) {
		if (item.effective_shortcut.empty())
			continue;
		ImGuiKey k;
		bool c, s, a;
		if (ShortcutManager::parse_key_combo(item.effective_shortcut, k, c, s, a)) {
			if (k != ImGuiKey_None && io.KeyCtrl == c && io.KeyShift == s && io.KeyAlt == a) {
				if (ImGui::IsKeyPressed(k, false)) {
					selected_id = item.id;
					break;
				}
			}
		}
	}
}

void UI::handle_mouse_wheel_brush_size(ImGuiIO& io) {
	if (!io.WantCaptureMouse && io.MouseWheel != 0.0f && !io.KeyShift) {
		bool fast = io.KeyCtrl;
		mouse_size += io.MouseWheel * std::ceil(mouse_size * (fast ? 0.2f : 0.05f));
		mouse_size = std::clamp(mouse_size, 1, static_cast<int>(Grid::get_width() * 0.5f));
	}
}

static bool s_is_canvas_dragging = false;
static ImVec2 s_prev_canvas_grid(0, 0);

static void finalize_canvas_drag() {
	if (s_is_canvas_dragging) {
		s_is_canvas_dragging = false;
		UndoManager::commit_grid_snapshot_if_changed("Paint Brush");
	}
	s_line_brush_active = false;
}

void UI::handle_canvas_interaction() {
	ImGuiIO& io = ImGui::GetIO();

	static bool s_middle_dragged = false;
	static ImVec2 s_middle_down_pos(0, 0);
	static bool s_middle_down_on_canvas = false;

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
		s_middle_down_pos = io.MousePos;
		s_middle_dragged = false;
		s_middle_down_on_canvas = !io.WantCaptureMouse;
	}
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		float dx = io.MousePos.x - s_middle_down_pos.x;
		float dy = io.MousePos.y - s_middle_down_pos.y;
		if (dx * dx + dy * dy > 1.0f) {
			s_middle_dragged = true;
		}
	}

	if (io.WantCaptureMouse && selection_state != SelectionState::Moving &&
		selection_state != SelectionState::Resizing) {
		finalize_canvas_drag();
		s_line_brush_active = false;
		return;
	}

	ImVec2 grid_pos = screen_to_grid_pos(io.MousePos);

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
		if (!s_middle_dragged && s_middle_down_on_canvas && !io.WantCaptureMouse) {
			float dx = io.MousePos.x - s_middle_down_pos.x;
			float dy = io.MousePos.y - s_middle_down_pos.y;
			if (dx * dx + dy * dy <= 1.0f) {
				uint32_t x_cell = static_cast<uint32_t>(grid_pos.x);
				uint32_t y_cell = static_cast<uint32_t>(grid_pos.y);
				if (x_cell < Grid::get_width() && y_cell < Grid::get_height()) {
					uint8_t cell = Grid::get_cell(x_cell, y_cell);
					selected_id = MaterialManager::get_material(cell).id;
				}
			}
		}
		s_middle_dragged = false;
		s_middle_down_on_canvas = false;
	}

	if (current_tool == ToolMode::Select) {
		if (selection_state == SelectionState::Pasting) {
			if (clipboard.empty()) {
				selection_state = SelectionState::None;
				return;
			}
			int px = static_cast<int>(grid_pos.x) - clipboard.width / 2;
			int py = static_cast<int>(grid_pos.y) - clipboard.height / 2;
			current_floating_x = std::clamp(px, 0, std::max(0, static_cast<int>(Grid::get_width()) - clipboard.width));
			current_floating_y =
				std::clamp(py, 0, std::max(0, static_cast<int>(Grid::get_height()) - clipboard.height));

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				for (int y = 0; y < clipboard.height; ++y) {
					for (int x = 0; x < clipboard.width; ++x) {
						int gx = current_floating_x + x;
						int gy = current_floating_y + y;
						if (gx >= 0 && gx < static_cast<int>(Grid::get_width()) && gy >= 0 &&
							gy < static_cast<int>(Grid::get_height())) {
							const Cell& cell = clipboard.cells[y * clipboard.width + x];
							if (!transparent_mode || cell.material != 0) {
								Grid::set_cell_full(gx, gy, cell);
							}
						}
					}
				}
				UndoManager::push_snapshot("Paste");
				selection_box.start_x = current_floating_x;
				selection_box.start_y = current_floating_y;
				selection_box.current_x = current_floating_x + clipboard.width - 1;
				selection_box.current_y = current_floating_y + clipboard.height - 1;
				selection_state = SelectionState::Selected;
			} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				selection_state = SelectionState::None;
			}
			return;
		}

		if (selection_state == SelectionState::Moving) {
			int gx = static_cast<int>(grid_pos.x);
			int gy = static_cast<int>(grid_pos.y);
			move_floating_selection(gx, gy);

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				drop_moving_selection();
			} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				deselect();
			}
			return;
		}

		if (selection_state == SelectionState::Resizing) {
			int gx = std::clamp(static_cast<int>(grid_pos.x), 0, static_cast<int>(Grid::get_width()) - 1);
			int gy = std::clamp(static_cast<int>(grid_pos.y), 0, static_cast<int>(Grid::get_height()) - 1);

			if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				if (active_resize_handle == ResizeHandle::Top || active_resize_handle == ResizeHandle::Bottom) {
					selection_box.current_y = gy;
				} else if (active_resize_handle == ResizeHandle::Left || active_resize_handle == ResizeHandle::Right) {
					selection_box.current_x = gx;
				} else {
					selection_box.current_x = gx;
					selection_box.current_y = gy;
				}
			} else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				if (active_resize_handle == ResizeHandle::Top || active_resize_handle == ResizeHandle::Bottom) {
					selection_box.current_y = gy;
				} else if (active_resize_handle == ResizeHandle::Left || active_resize_handle == ResizeHandle::Right) {
					selection_box.current_x = gx;
				} else {
					selection_box.current_x = gx;
					selection_box.current_y = gy;
				}
				int min_x = selection_box.min_x();
				int min_y = selection_box.min_y();
				int max_x = selection_box.max_x();
				int max_y = selection_box.max_y();
				selection_box.start_x = min_x;
				selection_box.start_y = min_y;
				selection_box.current_x = max_x;
				selection_box.current_y = max_y;
				selection_state = SelectionState::Selected;
				active_resize_handle = ResizeHandle::None;
				UndoManager::push_snapshot("Resize Selection");
			} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				int min_x = selection_box.min_x();
				int min_y = selection_box.min_y();
				int max_x = selection_box.max_x();
				int max_y = selection_box.max_y();
				selection_box.start_x = min_x;
				selection_box.start_y = min_y;
				selection_box.current_x = max_x;
				selection_box.current_y = max_y;
				selection_state = SelectionState::Selected;
				active_resize_handle = ResizeHandle::None;
			}
			return;
		}

		int gx = std::clamp(static_cast<int>(grid_pos.x), 0, static_cast<int>(Grid::get_width()) - 1);
		int gy = std::clamp(static_cast<int>(grid_pos.y), 0, static_cast<int>(Grid::get_height()) - 1);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			if (selection_state == SelectionState::Selected) {
				ScreenRect srect = grid_to_screen_rect_wh(selection_box.min_x(), selection_box.min_y(),
														  selection_box.width(), selection_box.height());
				ResizeHandle h = get_hovered_resize_handle(srect, io.MousePos);
				if (h != ResizeHandle::None) {
					active_resize_handle = h;
					selection_state = SelectionState::Resizing;
					int x1 = selection_box.min_x();
					int y1 = selection_box.min_y();
					int x2 = selection_box.max_x();
					int y2 = selection_box.max_y();
					if (h == ResizeHandle::TopLeft) {
						selection_box.start_x = x2;
						selection_box.start_y = y2;
						selection_box.current_x = gx;
						selection_box.current_y = gy;
					} else if (h == ResizeHandle::TopRight) {
						selection_box.start_x = x1;
						selection_box.start_y = y2;
						selection_box.current_x = gx;
						selection_box.current_y = gy;
					} else if (h == ResizeHandle::BottomLeft) {
						selection_box.start_x = x2;
						selection_box.start_y = y1;
						selection_box.current_x = gx;
						selection_box.current_y = gy;
					} else if (h == ResizeHandle::BottomRight) {
						selection_box.start_x = x1;
						selection_box.start_y = y1;
						selection_box.current_x = gx;
						selection_box.current_y = gy;
					} else if (h == ResizeHandle::Top) {
						selection_box.start_x = x1;
						selection_box.current_x = x2;
						selection_box.start_y = y2;
						selection_box.current_y = gy;
					} else if (h == ResizeHandle::Bottom) {
						selection_box.start_x = x1;
						selection_box.current_x = x2;
						selection_box.start_y = y1;
						selection_box.current_y = gy;
					} else if (h == ResizeHandle::Left) {
						selection_box.start_y = y1;
						selection_box.current_y = y2;
						selection_box.start_x = x2;
						selection_box.current_x = gx;
					} else if (h == ResizeHandle::Right) {
						selection_box.start_y = y1;
						selection_box.current_y = y2;
						selection_box.start_x = x1;
						selection_box.current_x = gx;
					}
					return;
				}

				if (selection_box.contains(gx, gy)) {
					start_moving_selection(gx, gy);
					return;
				}
			}

			selection_box.start_x = gx;
			selection_box.start_y = gy;
			selection_box.current_x = gx;
			selection_box.current_y = gy;
			selection_state = SelectionState::Selecting;
		} else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && selection_state == SelectionState::Selecting) {
			selection_box.current_x = gx;
			selection_box.current_y = gy;
		} else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && selection_state == SelectionState::Selecting) {
			selection_box.current_x = gx;
			selection_box.current_y = gy;
			ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
			float dist = std::hypot(drag_delta.x, drag_delta.y);
			int dx = std::abs(selection_box.current_x - selection_box.start_x);
			int dy = std::abs(selection_box.current_y - selection_box.start_y);

			if (dist < 6.0f || (dx == 0 && dy == 0)) {
				deselect();
			} else {
				int min_x = selection_box.min_x();
				int min_y = selection_box.min_y();
				int max_x = selection_box.max_x();
				int max_y = selection_box.max_y();
				selection_box.start_x = min_x;
				selection_box.start_y = min_y;
				selection_box.current_x = max_x;
				selection_box.current_y = max_y;
				selection_state = SelectionState::Selected;
			}
		} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			deselect();
		}
		return;
	}

	if (current_tool == ToolMode::Attribute) {
		bool is_shift = io.KeyShift;
		bool is_alt = io.KeyAlt;
		bool left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
		bool left_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
		bool right_released = ImGui::IsMouseReleased(ImGuiMouseButton_Right);
		bool left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
		bool right_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);

		if (left_clicked || right_clicked) {
			UndoManager::set_pending_grid_snapshot();
			s_attr_stroke_id++;
			if (s_attr_stroke_id == 0) {
				std::fill(s_attr_cell_stroke.begin(), s_attr_cell_stroke.end(), 0);
				s_attr_stroke_id = 1;
			}
			if (is_shift && !is_alt) {
				ImGuiMouseButton btn = left_clicked ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
				s_line_brush_start_grid = screen_to_grid_pos(io.MouseClickedPos[btn]);
				s_line_brush_active = true;
			}
		}

		uint8_t target_attr = static_cast<uint8_t>(std::clamp(attr_tool_target_index, 0, 2));
		bool set_exact = (attr_tool_mode == 1);
		const auto& attrs = SetManager::get_current_metadata().attributes;
		float def_val = (target_attr < attrs.size()) ? static_cast<float>(attrs[target_attr].default_val) : 0.0f;

		if (is_shift && is_alt && (left_clicked || right_clicked)) {
			s_line_brush_active = false;
			uint32_t target_x = static_cast<uint32_t>(grid_pos.x);
			uint32_t target_y = static_cast<uint32_t>(grid_pos.y);
			float val = left_clicked ? attr_tool_value : (set_exact ? def_val : -attr_tool_value);
			flood_fill_attribute(target_x, target_y, target_attr, val, set_exact);
			UndoManager::commit_grid_snapshot_if_changed("Flood Fill Attribute");
		} else if (is_shift && !is_alt && (left_released || right_released)) {
			float val = left_released ? attr_tool_value : (set_exact ? def_val : -attr_tool_value);
			ImGuiMouseButton btn = left_released ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
			ImVec2 start_grid =
				s_line_brush_active ? s_line_brush_start_grid : screen_to_grid_pos(io.MouseClickedPos[btn]);
			s_attr_stroke_id++;
			if (s_attr_stroke_id == 0) {
				std::fill(s_attr_cell_stroke.begin(), s_attr_cell_stroke.end(), 0);
				s_attr_stroke_id = 1;
			}
			paint_attribute_line(start_grid, grid_pos, mouse_size, target_attr, val, set_exact, brush_shape);
			UndoManager::commit_grid_snapshot_if_changed("Draw Attribute Line");
			s_line_brush_active = false;
		} else if (!is_shift && !is_alt) {
			s_line_brush_active = false;
			if (left_down || right_down) {
				float val = left_down ? attr_tool_value : (set_exact ? def_val : -attr_tool_value);
				if (s_is_canvas_dragging) {
					if (s_prev_canvas_grid.x != grid_pos.x || s_prev_canvas_grid.y != grid_pos.y) {
						paint_attribute_line(s_prev_canvas_grid, grid_pos, mouse_size, target_attr, val, set_exact,
											 brush_shape);
					}
				} else {
					GridRect brush = calculate_brush_bounds(grid_pos, mouse_size);
					paint_attribute_brush_at(brush.x, brush.y, brush.w, target_attr, val, set_exact, brush_shape);
					s_is_canvas_dragging = true;
				}
				s_prev_canvas_grid = grid_pos;
				ImGui::ResetMouseDragDelta();
			} else {
				s_is_canvas_dragging = false;
			}

			if (left_released || right_released) {
				UndoManager::commit_grid_snapshot_if_changed("Paint Attribute");
			}
		}
		return;
	}

	bool is_shift = io.KeyShift;
	bool is_alt = io.KeyAlt;
	bool left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	bool right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
	bool left_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
	bool right_released = ImGui::IsMouseReleased(ImGuiMouseButton_Right);
	bool left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool right_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);

	if (left_clicked || right_clicked) {
		UndoManager::set_pending_grid_snapshot();
		if (is_shift && !is_alt) {
			ImGuiMouseButton btn = left_clicked ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
			s_line_brush_start_grid = screen_to_grid_pos(io.MouseClickedPos[btn]);
			s_line_brush_active = true;
		}
	}

	if (is_shift && is_alt && (left_clicked || right_clicked)) {
		s_line_brush_active = false;
		uint32_t target_x = static_cast<uint32_t>(grid_pos.x);
		uint32_t target_y = static_cast<uint32_t>(grid_pos.y);
		uint8_t fill_mat = left_clicked ? static_cast<uint8_t>(selected_id) : 0;
		flood_fill(target_x, target_y, fill_mat);
		UndoManager::commit_grid_snapshot_if_changed("Flood Fill");
	} else if (is_shift && !is_alt && (left_released || right_released)) {
		ImGuiMouseButton btn = left_released ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
		ImVec2 start_grid = s_line_brush_active ? s_line_brush_start_grid : screen_to_grid_pos(io.MouseClickedPos[btn]);
		uint8_t mat_id = left_released ? static_cast<uint8_t>(selected_id) : 0;
		paint_line(start_grid, grid_pos, mouse_size, mat_id, brush_shape);
		UndoManager::commit_grid_snapshot_if_changed("Draw Line");
		s_line_brush_active = false;
	} else if (!is_shift && !is_alt) {
		s_line_brush_active = false;
		if (left_down || right_down) {
			uint8_t mat_id = left_down ? static_cast<uint8_t>(selected_id) : 0;
			if (s_is_canvas_dragging) {
				if (s_prev_canvas_grid.x != grid_pos.x || s_prev_canvas_grid.y != grid_pos.y) {
					paint_line(s_prev_canvas_grid, grid_pos, mouse_size, mat_id, brush_shape);
				}
			} else {
				GridRect brush = calculate_brush_bounds(grid_pos, mouse_size);
				paint_brush_at(brush.x, brush.y, brush.w, mat_id, brush_shape);
				s_is_canvas_dragging = true;
			}
			s_prev_canvas_grid = grid_pos;
			ImGui::ResetMouseDragDelta();
		} else {
			s_is_canvas_dragging = false;
		}

		if (left_released || right_released) {
			UndoManager::commit_grid_snapshot_if_changed("Paint Brush");
		}
	}
}

void UI::handle_interaction() {
	ImGuiIO& io = ImGui::GetIO();
	handle_zoom_and_pan(io);
	handle_keyboard_shortcuts(io);
	handle_mouse_wheel_brush_size(io);
	handle_canvas_interaction();
}

void UI::on_step_completed() {
	if (!stepping_sequence_active) {
		UndoManager::push_snapshot("Step Simulation");
		stepping_sequence_active = true;
	}
}

void UI::render_header(ImGuiIO& io) {
	const auto& cfg = ConfigManager::get_config();
	bool first_stat = true;
	if (cfg.ui.show_fps) {
		ImGui::Text("FPS: %.1f", io.Framerate);
		first_stat = false;
	}

	if (cfg.ui.show_active_cells) {
		if (!first_stat)
			ImGui::SameLine(0.0f, 10.0f);
		ImGui::Text("Cells: %u", Grid::get_changed_cells());
		first_stat = false;
	}

	if (cfg.ui.show_simulation_status) {
		if (!first_stat)
			ImGui::SameLine(0.0f, 10.0f);
		if (update) {
			ImGui::TextColored(ImVec4(0.25f, 0.90f, 0.45f, 1.0f), "Status: UNPAUSED");
		} else {
			ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.35f, 1.0f), "Status: PAUSED");
		}
	}

	const auto& set_meta = SetManager::get_current_metadata();
	if (!set_meta.attributes.empty()) {
		int cur_hm = Grid::get_heatmap_mode();
		std::string preview = (cur_hm < 0 || cur_hm >= static_cast<int>(set_meta.attributes.size()))
								  ? "Normal Colors"
								  : ("Heatmap: " + set_meta.attributes[cur_hm].name);
		float avail_w = ImGui::GetContentRegionAvail().x;
		float combo_w = (cur_hm >= 0) ? std::min(130.0f, avail_w * 0.40f) : std::min(160.0f, avail_w * 0.50f);
		ImGui::SetNextItemWidth(combo_w);
		if (ImGui::BeginCombo("##heatmap_combo", preview.c_str())) {
			if (ImGui::Selectable("Normal Colors", cur_hm == -1)) {
				Grid::set_heatmap_mode(-1);
			}
			for (size_t a = 0; a < set_meta.attributes.size(); ++a) {
				std::string item_name = "Heatmap: " + set_meta.attributes[a].name;
				if (ImGui::Selectable(item_name.c_str(), cur_hm == static_cast<int>(a))) {
					Grid::set_heatmap_mode(static_cast<int>(a));
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Switch display mode (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::CycleDisplayMode).c_str());
		}
	}

	std::string info_str = "Material: ---";
	if (!io.WantCaptureMouse) {
		ImVec2 gpos = screen_to_grid_pos(io.MousePos);
		int gx = static_cast<int>(gpos.x);
		int gy = static_cast<int>(gpos.y);
		if (gx >= 0 && gx < static_cast<int>(Grid::get_width()) && gy >= 0 &&
			gy < static_cast<int>(Grid::get_height())) {
			const Cell& c = Grid::get_cell_full_const(gx, gy);
			const auto& mdef = MaterialManager::get_material(c.material);
			info_str = mdef.name;
			for (size_t a = 0; a < set_meta.attributes.size(); ++a) {
				float val = c.get_attr_f(a);
				if (set_meta.attributes[a].type == AttributeType::Int) {
					info_str +=
						fmt::format(" | {}: {}", set_meta.attributes[a].name, static_cast<int64_t>(std::round(val)));
				} else {
					info_str += fmt::format(" | {}: {:.2f}", set_meta.attributes[a].name, val);
				}
			}
		}
	}
	ImGui::TextDisabled("%s", info_str.c_str());
	ImGui::Separator();
}

void UI::render_sim_content() {
	const auto& cfg = ConfigManager::get_config();
	float avail_w = ImGui::GetContentRegionAvail().x;
	float spacing_x = ImGui::GetStyle().ItemSpacing.x;
	float sim_col_w = (avail_w - spacing_x * 2.0f) / 3.0f;
	float btn_sz = (cfg.ui.button_size > 0) ? static_cast<float>(cfg.ui.button_size) : 30.0f;
	ImVec2 sim_btn_sz(sim_col_w, btn_sz);

	ImGui::Spacing();
	if (update) {
		if (button_with_icon("##SimPause", IconManager::get(IconID::Pause), sim_btn_sz)) {
			update = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Pause simulation (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::ToggleSimulation).c_str());
		}
	} else {
		if (button_with_icon("##SimResume", IconManager::get(IconID::Play), sim_btn_sz)) {
			update = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Resume simulation (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::ToggleSimulation).c_str());
		}
	}

	ImGui::SameLine();
	if (update) {
		ImGui::BeginDisabled();
	}
	if (button_with_icon("##SimStep", IconManager::get(IconID::Step), sim_btn_sz)) {
		step_frame = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Step simulation by one frame (%s)",
						  ShortcutManager::get_key_string(ShortcutAction::StepFrame).c_str());
	}
	if (update) {
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	if (button_with_icon("##SimClear", IconManager::get(IconID::Clear), sim_btn_sz)) {
		Grid::clear();
		UndoManager::push_snapshot("Clear Grid");
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Clear / Reset grid (%s)",
						  ShortcutManager::get_key_string(ShortcutAction::ClearGrid).c_str());
	}

	ImGui::Separator();

	std::vector<MaterialDefinition>& materials = MaterialManager::get_materials();

	ImGui::Separator();
	ImGui::Text("Select Material:");
	ImGui::BeginChild("SimMaterialsList", ImVec2(0, -1), true);
	for (int i = 1; i < (int)materials.size(); ++i) {
		std::string label = materials[i].name;
		ImVec4 color =
			ImVec4(materials[i].color[0] / 255.f, materials[i].color[1] / 255.f, materials[i].color[2] / 255.f, 1.f);
		if (UI::selectable_with_color((label + "##sim_" + std::to_string(i)).c_str(), color,
									  selected_id == materials[i].id)) {
			selected_id = materials[i].id;
		}
		if (ImGui::IsItemHovered()) {
			std::string sc = get_effective_material_shortcut(materials[i].id);
			if (!sc.empty()) {
				ImGui::SetTooltip("Shortcut: %s", sc.c_str());
			}
		}
	}
	ImVec4 color =
		ImVec4(materials[0].color[0] / 255.f, materials[0].color[1] / 255.f, materials[0].color[2] / 255.f, 1.f);
	if (UI::selectable_with_color((materials[0].name + "##sim_0").c_str(), color, selected_id == 0)) {
		selected_id = 0;
	}
	if (ImGui::IsItemHovered()) {
		std::string sc = get_effective_material_shortcut(0);
		if (!sc.empty()) {
			ImGui::SetTooltip("Shortcut: %s", sc.c_str());
		}
	}
	ImGui::EndChild();
}

struct CopiedRuleCell {
	enum class Type { When, Then } type = Type::When;
	std::vector<uint8_t> when_val;
	std::vector<AttributeCondition> when_attrs;
	uint8_t then_val = 255;
	ThenCellAction then_action;
	int src_dx = 0;
	int src_dy = 0;
	bool has_copied = false;

	bool can_paste_as_then() const {
		if (!has_copied)
			return false;
		if (type == Type::Then)
			return true;
		return when_val.size() <= 1;
	}

	bool can_paste_as_when() const {
		if (!has_copied)
			return false;
		return true;
	}

	std::vector<uint8_t> get_as_when() const {
		if (type == Type::When) {
			return when_val;
		} else {
			if (then_val == 255)
				return {};
			return {then_val};
		}
	}

	uint8_t get_as_then() const {
		if (type == Type::Then) {
			return then_val;
		} else {
			if (when_val.empty())
				return 255;
			return when_val[0];
		}
	}
};

static CopiedRuleCell g_copied_rule_cell;
static bool s_is_dragging_paint = false;
static bool s_is_dragging_clear = false;

std::vector<UI::MaterialShortcutItem> UI::get_all_material_shortcuts() {
	auto raw = UIShortcuts::get_all_material_shortcuts();
	std::vector<UI::MaterialShortcutItem> res;
	res.reserve(raw.size());
	for (const auto& item : raw) {
		UI::MaterialShortcutItem m;
		m.name = item.name;
		m.id = item.id;
		m.custom_shortcut = item.custom_shortcut;
		m.effective_shortcut = item.effective_shortcut;
		m.is_custom = item.is_custom;
		res.push_back(std::move(m));
	}
	return res;
}

std::string UI::get_effective_material_shortcut(uint8_t id) { return UIShortcuts::get_effective_material_shortcut(id); }
struct ExprBadgeRef {
	int dx;
	int dy;
	bool is_relative;
	size_t start_pos;
	size_t length;
};

static std::vector<ExprBadgeRef> extract_cell_badges(const std::string& expr) {
	std::vector<ExprBadgeRef> badges;
	size_t i = 0;
	size_t n = expr.size();
	while (i < n) {
		char c = expr[i];
		if (c == '~' && i + 1 < n && expr[i + 1] == '[') {
			size_t close = expr.find(']', i + 1);
			if (close != std::string::npos) {
				std::string inner = expr.substr(i + 2, close - i - 2);
				size_t comma = inner.find(',');
				if (comma != std::string::npos) {
					try {
						int dx = std::stoi(inner.substr(0, comma));
						int dy = std::stoi(inner.substr(comma + 1));
						badges.push_back({dx, dy, true, i, close - i + 1});
						i = close + 1;
						continue;
					} catch (...) {}
				}
			}
		}
		if (c == '[') {
			size_t close = expr.find(']', i);
			if (close != std::string::npos) {
				std::string inner = expr.substr(i + 1, close - i - 1);
				size_t comma = inner.find(',');
				if (comma != std::string::npos) {
					try {
						int dx = std::stoi(inner.substr(0, comma));
						int dy = std::stoi(inner.substr(comma + 1));
						badges.push_back({dx, dy, false, i, close - i + 1});
						i = close + 1;
						continue;
					} catch (...) {}
				}
			}
		}
		if (c == '(') {
			size_t close = expr.find(')', i);
			if (close != std::string::npos) {
				std::string inner = expr.substr(i + 1, close - i - 1);
				size_t comma = inner.find(',');
				if (comma != std::string::npos) {
					bool only_nums = true;
					for (char ic : inner) {
						if (!std::isdigit(static_cast<unsigned char>(ic)) && ic != '+' && ic != '-' && ic != ' ' &&
							ic != '\t' && ic != ',') {
							only_nums = false;
							break;
						}
					}
					if (only_nums) {
						try {
							int dx = std::stoi(inner.substr(0, comma));
							int dy = std::stoi(inner.substr(comma + 1));
							badges.push_back({dx, dy, false, i, close - i + 1});
							i = close + 1;
							continue;
						} catch (...) {}
					}
				}
			}
		}
		i++;
	}
	return badges;
}

struct ExprSegment {
	bool is_badge;
	std::string text;
	ExprBadgeRef badge;
};

static std::vector<ExprSegment> split_expr_segments(const std::string& expr) {
	std::vector<ExprSegment> segs;
	auto badges = extract_cell_badges(expr);
	size_t cursor = 0;
	for (const auto& b : badges) {
		if (b.start_pos > cursor) {
			segs.push_back({false, expr.substr(cursor, b.start_pos - cursor), {}});
		}
		segs.push_back({true, expr.substr(b.start_pos, b.length), b});
		cursor = b.start_pos + b.length;
	}
	if (cursor < expr.size()) {
		segs.push_back({false, expr.substr(cursor), {}});
	}
	return segs;
}

static void toggle_badge_relative(std::string& expr, const ExprBadgeRef& badge, int cell_dx = 0, int cell_dy = 0) {
	if (badge.start_pos >= expr.size())
		return;
	std::string replacement;
	if (badge.is_relative) {
		int abs_x = cell_dx + badge.dx;
		int abs_y = cell_dy + badge.dy;
		replacement = fmt::format("[{},{}]", abs_x, abs_y);
	} else {
		int rel_x = badge.dx - cell_dx;
		int rel_y = badge.dy - cell_dy;
		replacement = fmt::format("~[{},{}]", rel_x, rel_y);
	}
	expr.replace(badge.start_pos, badge.length, replacement);
}

static void remove_badge(std::string& expr, const ExprBadgeRef& badge) {
	if (badge.start_pos >= expr.size())
		return;
	size_t start = badge.start_pos;
	size_t len = badge.length;

	size_t after = start + len;
	while (after < expr.size() && std::isspace(static_cast<unsigned char>(expr[after])))
		after++;
	if (after < expr.size() && (expr[after] == '+' || expr[after] == '-' || expr[after] == '*' || expr[after] == '/')) {
		after++;
		while (after < expr.size() && std::isspace(static_cast<unsigned char>(expr[after])))
			after++;
		len = after - start;
	} else {
		size_t before = start;
		while (before > 0 && std::isspace(static_cast<unsigned char>(expr[before - 1])))
			before--;
		if (before > 0 && (expr[before - 1] == '+' || expr[before - 1] == '-' || expr[before - 1] == '*' ||
						   expr[before - 1] == '/')) {
			before--;
			while (before > 0 && std::isspace(static_cast<unsigned char>(expr[before - 1])))
				before--;
			len = (start + len) - before;
			start = before;
		}
	}
	expr.erase(start, len);
}

static void append_cell_to_equation(std::string& expr, int dx, int dy) {
	std::string cell_str = fmt::format("[{},{}]", dx, dy);
	std::string trimmed = expr;
	while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))
		trimmed.pop_back();

	if (!trimmed.empty()) {
		char last_c = trimmed.back();
		if (last_c != '+' && last_c != '-' && last_c != '*' && last_c != '/' && last_c != '%' && last_c != '^' &&
			last_c != '(' && last_c != ',') {
			expr += " + ";
		} else if (last_c != '(' && last_c != ' ') {
			expr += " ";
		}
	}
	expr += cell_str;
}

void UI::render_material_editor() {
	std::vector<MaterialDefinition>& materials = MaterialManager::get_materials();
	auto& cfg = ConfigManager::get_config();
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::BeginTabItem("Materials")) {
		ImGui::Spacing();
		bool rebuild_needed = false;

		if (ImGui::CollapsingHeader("Materials List", ImGuiTreeNodeFlags_DefaultOpen)) {
			float list_h = static_cast<float>(cfg.ui.material_list_height > 0 ? cfg.ui.material_list_height : 150);
			ImGui::BeginChild("MaterialsListScroll", ImVec2(0, list_h), true);

			for (size_t i = 1; i < materials.size(); ++i) {
				std::string label = materials[i].name;

				ImVec4 color = ImVec4(materials[i].color[0] / 255.f, materials[i].color[1] / 255.f,
									  materials[i].color[2] / 255.f, 1.f);
				if (UI::selectable_with_color((label + "##" + std::to_string(i)).c_str(), color,
											  selected_id == materials[i].id)) {
					selected_id = materials[i].id;
				}
				if (ImGui::IsItemHovered()) {
					std::string sc = get_effective_material_shortcut(materials[i].id);
					if (!sc.empty()) {
						ImGui::SetTooltip("Has %zu rule%s\nShortcut: %s", materials[i].rules.size(),
										  (materials[i].rules.size() == 1 ? "" : "s"), sc.c_str());
					} else {
						ImGui::SetTooltip("Has %zu rule%s", materials[i].rules.size(),
										  (materials[i].rules.size() == 1 ? "" : "s"));
					}
				}
			}

			ImVec4 color = ImVec4(materials[0].color[0] / 255.f, materials[0].color[1] / 255.f,
								  materials[0].color[2] / 255.f, 1.f);
			if (UI::selectable_with_color((materials[0].name + "##0").c_str(), color, selected_id == 0)) {
				selected_id = 0;
			}
			if (ImGui::IsItemHovered()) {
				std::string sc = get_effective_material_shortcut(0);
				if (!sc.empty()) {
					ImGui::SetTooltip("Has %zu rule%s\nShortcut: %s", materials[0].rules.size(),
									  (materials[0].rules.size() == 1 ? "" : "s"), sc.c_str());
				} else {
					ImGui::SetTooltip("Has %zu rule%s", materials[0].rules.size(),
									  (materials[0].rules.size() == 1 ? "" : "s"));
				}
			}

			ImGui::EndChild();

			float bar_h = 8.0f;
			ImVec2 cur_pos = ImGui::GetCursorScreenPos();
			float avail_w = ImGui::GetContentRegionAvail().x;
			ImRect h_bar_bb(cur_pos, ImVec2(cur_pos.x + avail_w, cur_pos.y + bar_h));
			ImGui::InvisibleButton("##MatListResizeBar", ImVec2(-1, bar_h));
			bool h_hovered = ImGui::IsItemHovered();
			bool h_held = ImGui::IsItemActive();
			if (h_hovered || h_held) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
			}
			if (h_held && io.MouseDelta.y != 0.0f) {
				cfg.ui.material_list_height =
					std::clamp(cfg.ui.material_list_height + static_cast<int>(io.MouseDelta.y), 70, 600);
			}
			if (ImGui::IsItemDeactivated()) {
				ConfigManager::save();
			}
			if (h_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				cfg.ui.material_list_height = 150;
				ConfigManager::save();
			}
			if (h_hovered) {
				ImGui::SetTooltip("Drag to resize material list (Double-click to reset to 150px)");
			}

			ImDrawList* w_dl = ImGui::GetWindowDrawList();
			float mid_y = std::round((h_bar_bb.Min.y + h_bar_bb.Max.y) * 0.5f);
			ImU32 line_col = h_held		 ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
							 : h_hovered ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
										 : ImGui::GetColorU32(ImGuiCol_Separator);
			w_dl->AddLine(ImVec2(h_bar_bb.Min.x, mid_y), ImVec2(h_bar_bb.Max.x, mid_y), line_col, h_held ? 2.0f : 1.0f);
			float mid_x = std::round((h_bar_bb.Min.x + h_bar_bb.Max.x) * 0.5f);
			ImU32 handle_col = h_held	   ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
							   : h_hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
										   : ImGui::GetColorU32(ImGuiCol_TextDisabled);
			w_dl->AddRectFilled(ImVec2(mid_x - 18.0f, mid_y - 2.5f), ImVec2(mid_x + 18.0f, mid_y + 2.5f), handle_col,
								2.5f);

			ImGui::Spacing();
			const bool at_max = materials.size() == 255;
			if (at_max) {
				ImGui::BeginDisabled();
			}
			if (button_with_icon("New", IconManager::get(IconID::Add), ImVec2(80, 25))) {
				UndoManager::push_snapshot("New Material");
				MaterialDefinition new_mat;
				std::string base_name = "material_" + std::to_string(materials.size());
				std::string unique_name = base_name;
				int name_counter = 1;
				while (!MaterialManager::is_valid_name(unique_name)) {
					unique_name = "material_" + std::to_string(materials.size() + (name_counter++));
				}
				new_mat.name = unique_name;
				new_mat.color = {255, 255, 255};
				const auto& cur_set_meta = SetManager::get_current_metadata();
				for (size_t a = 0; a < 3 && a < cur_set_meta.attributes.size(); ++a) {
					new_mat.default_attributes[a] = cur_set_meta.attributes[a].default_val;
					float a_min = static_cast<float>(cur_set_meta.attributes[a].min_val);
					float a_max = static_cast<float>(cur_set_meta.attributes[a].max_val);
					new_mat.attr_color_modifiers[a].gradient = ColorGradient::create_default(a_min, a_max);
				}
				const uint8_t id = MaterialManager::add_material(new_mat);
				selected_id = id;
				unsaved_changes = true;
			}
			if (at_max) {
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
					ImGui::SetTooltip("Already at maximum material capacity.");
				}
				ImGui::EndDisabled();
			} else {
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Create a new material.");
				}
			}

			ImGui::SameLine();

			if (at_max) {
				ImGui::BeginDisabled();
			}
			if (button_with_icon("Copy", IconManager::get(IconID::Copy), ImVec2(80, 25))) {
				UndoManager::push_snapshot("Copy Material");
				MaterialDefinition duplicated_mat = MaterialManager::get_material(selected_id);
				std::string base_name = duplicated_mat.name + "_copy";
				std::string unique_name = base_name;
				int copy_counter = 1;
				while (!MaterialManager::is_valid_name(unique_name)) {
					unique_name = base_name + "_" + std::to_string(copy_counter++);
				}
				duplicated_mat.name = unique_name;
				const uint8_t id = MaterialManager::add_material(duplicated_mat);
				selected_id = id;
				unsaved_changes = true;
			}
			if (at_max) {
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
					ImGui::SetTooltip("Already at maximum material capacity.");
				}
				ImGui::EndDisabled();
			} else {
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Duplicate the selected material.");
				}
			}

			ImGui::SameLine();
			bool disabled = selected_id == 0;
			if (disabled) {
				ImGui::BeginDisabled();
			}
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			if (button_with_icon("Delete", IconManager::get(IconID::Clear), ImVec2(80, 25))) {
				UndoManager::push_snapshot("Delete Material");
				MaterialManager::remove_material(selected_id);
				selected_id = 0;
				unsaved_changes = true;
			}
			ImGui::PopStyleColor(3);
			if (disabled) {
				ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
					ImGui::SetTooltip("The default material cannot be deleted.");
				}
			} else if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Remove the selected material.");
			}
		}

		ImGui::Separator();

		if (MaterialManager::get_material(selected_id).id != selected_id) {
			selected_id = 0;
		}
		auto& mat = const_cast<MaterialDefinition&>(MaterialManager::get_material(selected_id));

		if (ImGui::CollapsingHeader("Basic Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
			char name_buf[128];
			strncpy(name_buf, mat.name.c_str(), sizeof(name_buf));
			name_buf[sizeof(name_buf) - 1] = '\0';
			if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
				std::string new_name = name_buf;
				new_name = sanitize_name(new_name);
				if (MaterialManager::is_valid_name(new_name) && new_name != mat.name) {
					UndoManager::push_snapshot("Rename Material");
					MaterialManager::update_material_name(selected_id, new_name);
					unsaved_changes = true;
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Rename the material configuration.");
			}

			float col[3] = {static_cast<float>(mat.color[0]) / 255.f, static_cast<float>(mat.color[1]) / 255.f,
							static_cast<float>(mat.color[2]) / 255.f};
			if (ImGui::ColorEdit3("Color", col)) {
				UndoManager::push_snapshot("Edit Material Color");
				mat.color[0] = static_cast<uint8_t>(col[0] * 255.f);
				mat.color[1] = static_cast<uint8_t>(col[1] * 255.f);
				mat.color[2] = static_cast<uint8_t>(col[2] * 255.f);
				unsaved_changes = true;
				MaterialManager::update_material_color(selected_id, mat);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Pick color for this material in the simulator.");
			}

			std::string current_parent =
				(mat.inherits_from == 255) ? "None" : MaterialManager::get_material(mat.inherits_from).name;
			if (ImGui::BeginCombo("Inherit From", current_parent.c_str())) {
				if (ImGui::Selectable("None", mat.inherits_from == 255)) {
					UndoManager::push_snapshot("Set Inheritance");
					MaterialManager::set_material_inheritance(selected_id, 255);
					rebuild_needed = true;
					unsaved_changes = true;
				}
				for (const auto& other_mat : materials) {
					if (other_mat.id != selected_id && other_mat.name != mat.name) {
						bool is_selected = (mat.inherits_from == other_mat.id);
						ImVec4 o_col(other_mat.color[0] / 255.f, other_mat.color[1] / 255.f, other_mat.color[2] / 255.f,
									 1.0f);
						if (UI::selectable_with_color(other_mat.name.c_str(), o_col, is_selected)) {
							if (mat.inherits_from != other_mat.id) {
								bool target_has_rules = false;
								uint8_t curr = other_mat.id;
								while (curr != 255) {
									const auto& p = MaterialManager::get_material(curr);
									if (p.id != curr)
										break;
									if (!p.rules.empty()) {
										target_has_rules = true;
										break;
									}
									curr = p.inherits_from;
								}
								if (selected_id == 0 && target_has_rules) {
									UIModals::open_empty_rule_warning(other_mat.id);
								} else {
									UndoManager::push_snapshot("Set Inheritance");
									MaterialManager::set_material_inheritance(selected_id, other_mat.id);
									rebuild_needed = true;
									unsaved_changes = true;
								}
							}
						}
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Dynamically inherits rules from the selected parent material.");
			}

			const auto& cur_set_meta = SetManager::get_current_metadata();
			if (!cur_set_meta.attributes.empty()) {
				ImGui::Spacing();
				ImGui::Text("Default Attributes:");
				for (size_t a_idx = 0; a_idx < cur_set_meta.attributes.size(); ++a_idx) {
					const auto& attr = cur_set_meta.attributes[a_idx];
					std::string label = attr.name + "##def_attr_" + std::to_string(a_idx);
					if (attr.type == AttributeType::Float) {
						float val = static_cast<float>(mat.default_attributes[a_idx]);
						if (ImGui::InputFloat(label.c_str(), &val, 1.0f, 10.0f, "%.2f")) {
							UndoManager::push_snapshot("Edit Default Attribute");
							val = std::clamp(val, static_cast<float>(attr.min_val), static_cast<float>(attr.max_val));
							mat.default_attributes[a_idx] = val;
							unsaved_changes = true;
						}
					} else {
						int val = static_cast<int>(mat.default_attributes[a_idx]);
						if (ImGui::InputInt(label.c_str(), &val, 1, 10)) {
							UndoManager::push_snapshot("Edit Default Attribute");
							val = std::clamp(val, static_cast<int>(attr.min_val), static_cast<int>(attr.max_val));
							mat.default_attributes[a_idx] = val;
							unsaved_changes = true;
						}
					}
					if (ImGui::IsItemHovered()) {
						std::string tt = fmt::format("Default {} for newly placed {} cells ({}..{})", attr.name,
													 mat.name, attr.min_val, attr.max_val);
						ImGui::SetTooltip("%s", tt.c_str());
					}
				}
			}
		}

		const auto& cur_set_meta = SetManager::get_current_metadata();
		if (!cur_set_meta.attributes.empty()) {
			ImGui::Separator();

			if (ImGui::CollapsingHeader("Attribute Color Modifiers", ImGuiTreeNodeFlags_DefaultOpen)) {
				for (size_t a_idx = 0; a_idx < cur_set_meta.attributes.size(); ++a_idx) {
					const auto& attr = cur_set_meta.attributes[a_idx];
					auto& cmod = mat.attr_color_modifiers[a_idx];
					ImGui::PushID(static_cast<int>(a_idx + 8800));
					bool enabled = cmod.enabled;
					ImGui::Text("%s", attr.name.c_str());
					ImGui::SameLine(0.0f, get_spacing(0.0f));
					if (custom_checkbox(("##cmod_en_" + attr.name).c_str(), &enabled, IconID::ColorWheel,
										"Enable continuous color modification based on this attribute")) {
						UndoManager::push_snapshot("Toggle Color Modifier");
						cmod.enabled = enabled;
						rebuild_needed = true;
						unsaved_changes = true;
					}
					if (enabled) {
						ImGui::Indent(18.0f);
						float a_min = static_cast<float>(attr.min_val);
						float a_max = static_cast<float>(attr.max_val);
						if (gradient_editor(("##cmod_grad_" + attr.name).c_str(), cmod.gradient, a_min, a_max)) {
							UndoManager::push_snapshot("Edit Color Modifier Gradient");
							rebuild_needed = true;
							unsaved_changes = true;
						}
						ImGui::Unindent(18.0f);
					}
					ImGui::PopID();
				}
			}
		}

		ImGui::Separator();
		ImGui::Spacing();
		ImGui::Text("Rules:");
		ImGui::SameLine();

		mat.sync_rule_order();

		if (button_with_icon("Add Rule", IconManager::get(IconID::Add))) {
			if (selected_id == 0 && mat.rules.empty()) {
				UIModals::open_empty_rule_warning(255);
			} else {
				UndoManager::push_snapshot("Add Rule");
				RuleDefinition new_rule;
				new_rule.init_default(selected_id);
				mat.rules.push_back(new_rule);
				mat.rule_order.push_back({false, mat.rules.size() - 1});
				rebuild_needed = true;
				unsaved_changes = true;
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Creates a rule.");
		}

		ImGui::BeginChild("RulesScroll", ImVec2(0, 0), true);
		for (int r_id = 0; r_id < (int)mat.rule_order.size(); ++r_id) {
			RuleReference reference = mat.rule_order[r_id];
			RuleDefinition rule = mat.get_effective_rule(r_id);
			ImGui::PushID(r_id);

			int grid_w = rule.width();
			int grid_h = rule.height();
			int max_dim = std::max(grid_w, grid_h);
			float avail_w = ImGui::GetContentRegionAvail().x;
			ImVec2 tiny_sz(20.0f, 20.0f);
			float nav_btns_w = tiny_sz.x * 2.0f;
			float total_margins = (nav_btns_w * 2.0f) + 50.0f;
			float max_grid_w = std::max(100.0f, (avail_w - total_margins) * 0.5f);
			float cell_sz = std::clamp(std::floor((max_grid_w - (max_dim - 1) * 1.0f) / max_dim), 24.0f, 56.0f);
			float when_grid_px_w = grid_w * cell_sz + (grid_w - 1) * 1.0f;
			float when_grid_px_h = grid_h * cell_sz + (grid_h - 1) * 1.0f;

			auto try_expand = [&](ExpandSide side) {
				int new_w = grid_w;
				int new_h = grid_h;
				if (side == ExpandSide::Top || side == ExpandSide::Bottom)
					new_h++;
				else
					new_w++;
				int new_total = new_w * new_h;
				if (new_total > 100 && !ConfigManager::get_config().allow_large_neighborhoods) {
					UIModals::open_large_neighborhood_warning(
						reference.is_inherited ? -1 : static_cast<int>(reference.index), selected_id,
						static_cast<int>(side));
					return;
				}
				UndoManager::push_snapshot("Expand Rule Neighborhood");
				if (side == ExpandSide::Top)
					rule.expand_top();
				else if (side == ExpandSide::Bottom)
					rule.expand_bottom();
				else if (side == ExpandSide::Left)
					rule.expand_left();
				else if (side == ExpandSide::Right)
					rule.expand_right();
				rebuild_needed = true;
				unsaved_changes = true;
			};

			auto do_shrink = [&](ExpandSide side) {
				UndoManager::push_snapshot("Shrink Rule Neighborhood");
				if (side == ExpandSide::Top)
					rule.shrink_top();
				else if (side == ExpandSide::Bottom)
					rule.shrink_bottom();
				else if (side == ExpandSide::Left)
					rule.shrink_left();
				else if (side == ExpandSide::Right)
					rule.shrink_right();
				rebuild_needed = true;
				unsaved_changes = true;
			};

			ImGui::BeginChild(("##rule_card_" + std::to_string(r_id)).c_str(), ImVec2(0, 0),
							  ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);

			float btn_size = ImGui::GetFrameHeight();

			ImGui::BeginGroup();

			if (custom_checkbox("##enable_rule", &rule.enabled, rule.enabled ? IconID::Accept : IconID::Pause,
								"Toggle rule.", ImVec2(btn_size, btn_size))) {
				UndoManager::push_snapshot("Toggle Enable Rule");
				rebuild_needed = true;
				unsaved_changes = true;
			}

			ImGui::SameLine(0.0f, get_spacing(0.0f));

			if (r_id > 0) {
				if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
					UndoManager::push_snapshot("Reorder Rules");
					std::swap(mat.rule_order[r_id], mat.rule_order[r_id - 1]);
					rebuild_needed = true;
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Reorder up. Higher priority rules execute first.");
				}
			} else {
				ImGui::Dummy(ImVec2(btn_size, btn_size));
			}
			ImGui::SameLine();

			if (r_id < (int)mat.rule_order.size() - 1) {
				if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
					UndoManager::push_snapshot("Reorder Rules");
					std::swap(mat.rule_order[r_id], mat.rule_order[r_id + 1]);
					rebuild_needed = true;
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Reorder down. Lower priority rules execute later.");
				}
			} else {
				ImGui::Dummy(ImVec2(btn_size, btn_size));
			}
			ImGui::SameLine(0.0f, get_spacing(0.0f));

			if (button_with_icon("##CopyRule", IconManager::get(IconID::Copy), ImVec2(btn_size, btn_size))) {
				UndoManager::push_snapshot("Copy Rule");
				const RuleDefinition& duplicated_rule = rule;
				mat.rules.push_back(duplicated_rule);
				RuleReference new_ref{false, mat.rules.size() - 1};
				mat.rule_order.insert(mat.rule_order.begin() + r_id + 1, new_ref);
				rebuild_needed = true;
				unsaved_changes = true;
				ImGui::EndGroup();
				ImGui::EndChild();
				ImGui::PopID();
				break;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Duplicate this rule.");
			}
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			bool delete_rule_clicked =
				button_with_icon("##DeleteRule", IconManager::get(IconID::Cross), ImVec2(btn_size, btn_size));
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete this rule.");
			}
			if (delete_rule_clicked) {
				UndoManager::push_snapshot("Delete Rule");
				if (!reference.is_inherited) {
					size_t target_idx = reference.index;
					if (target_idx < mat.rules.size()) {
						mat.rules.erase(mat.rules.begin() + target_idx);
						mat.rule_order.erase(mat.rule_order.begin() + r_id);
						for (auto& o_ref : mat.rule_order) {
							if (!o_ref.is_inherited && o_ref.index > target_idx) {
								o_ref.index--;
							}
						}
					}
				} else {
					mat.rule_order.erase(mat.rule_order.begin() + r_id);
				}
				rebuild_needed = true;
				unsaved_changes = true;
				ImGui::EndGroup();
				ImGui::EndChild();
				ImGui::PopID();
				break;
			}

			if (reference.is_inherited) {
				ImGui::BeginDisabled(true);
			}

			ImGui::SameLine(0.0f, get_spacing(0.0f));
			ImGui::SetNextItemWidth(140.0f);
			if (ImGui::InputFloat("##chance_input", &rule.chance, 1.0f, 10.0f, "%.03f%%")) {
				UndoManager::push_snapshot("Edit Rule Chance");
				if (rule.chance < 0.001f) {
					rule.chance = 0.001f;
				} else if (rule.chance > 100.0f) {
					rule.chance = 100.0f;
				}
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Controls rule probability.");
			}

			ImGui::SameLine(0.0f, get_spacing(0.0f));

			if (custom_checkbox("Keep##continue_next", &rule.continue_to_next, IconID::Branch,
								"Continue evaluating next rules on match.", ImVec2(-1.0f, btn_size))) {
				UndoManager::push_snapshot("Toggle Continue Rule");
				rebuild_needed = true;
				unsaved_changes = true;
			}

			ImGui::SameLine(0.0f, get_spacing(0.0f));

			if (custom_checkbox("Multi##multi_apply", &rule.multi_apply, IconID::MultiApply,
								"Allows matching all symmetrical variants of this rule to run in the same step.\n"
								"Useful for spreading in multiple directions at the same time.",
								ImVec2(-1.0f, btn_size))) {
				UndoManager::push_snapshot("Toggle Multi-Apply Rule");
				rebuild_needed = true;
				unsaved_changes = true;
			}

			ImGui::Spacing();

			if (custom_checkbox("X Symmetry##x_sym", &rule.symmetry.flip_x, IconID::FlipHorizontal,
								"Allows the rule to match its left-right mirror image.", ImVec2(-1.0f, btn_size))) {
				UndoManager::push_snapshot("Edit Rule Symmetry");
				rebuild_needed = true;
				unsaved_changes = true;
			}
			ImGui::SameLine(0.0f, get_spacing(0.0f));
			if (custom_checkbox("Y Symmetry##y_sym", &rule.symmetry.flip_y, IconID::FlipVertical,
								"Allows the rule to match its up-down mirror image.", ImVec2(-1.0f, btn_size))) {
				UndoManager::push_snapshot("Edit Rule Symmetry");
				rebuild_needed = true;
				unsaved_changes = true;
			}
			ImGui::SameLine(0.0f, get_spacing(0.0f));
			if (custom_checkbox("Rotational Symmetry##rot_sym", &rule.symmetry.rotate, IconID::RotateCW,
								"Allows the rule to match with 90 degree rotations of itself.",
								ImVec2(-1.0f, btn_size))) {
				UndoManager::push_snapshot("Edit Rule Symmetry");
				rebuild_needed = true;
				unsaved_changes = true;
			}

			ImGui::EndGroup();

			float left_group_right = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + ImGui::GetScrollX();
			float manip_grid_w = btn_size * 2.0f + 4.0f;
			float content_max_x = ImGui::GetWindowContentRegionMax().x;
			float target_x = content_max_x - manip_grid_w;
			if (target_x > left_group_right + 8.0f) {
				ImGui::SameLine(target_x, 0.0f);
			} else {
				ImGui::SameLine(0.0f, 8.0f);
			}

			ImGui::BeginGroup();

			if (button_with_icon("##rcw", IconManager::get(IconID::RotateCW), ImVec2(btn_size, btn_size))) {
				UndoManager::push_snapshot("Rotate Rule Grid");
				rule.rotate_cw();
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Rotate grid 90 degrees clockwise");
			}

			ImGui::SameLine(0.0f, 4.0f);
			if (button_with_icon("##rccw", IconManager::get(IconID::RotateCCW), ImVec2(btn_size, btn_size))) {
				UndoManager::push_snapshot("Rotate Rule Grid");
				rule.rotate_ccw();
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Rotate grid 90 degrees counter-clockwise");
			}

			ImGui::Spacing();
			if (button_with_icon("##fx", IconManager::get(IconID::FlipHorizontal), ImVec2(btn_size, btn_size))) {
				UndoManager::push_snapshot("Flip Rule Grid");
				rule.flip_x();
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Flip grid horizontally (left-right)");
			}

			ImGui::SameLine(0.0f, 4.0f);
			if (button_with_icon("##fy", IconManager::get(IconID::FlipVertical), ImVec2(btn_size, btn_size))) {
				UndoManager::push_snapshot("Flip Rule Grid");
				rule.flip_y();
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Flip grid vertically (top-bottom)");
			}
			ImGui::EndGroup();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImDrawList* dl = ImGui::GetWindowDrawList();

			bool eq_click_consumed = false;
			if (s_active_eq_cell_id >= 0 && s_active_eq_rule_id == r_id &&
				ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !s_active_popup_rect.Contains(io.MousePos)) {
				bool hit = false;
				int hit_dx = 0, hit_dy = 0;
				int cols = rule.max_dx - rule.min_dx + 1;
				int rows = rule.max_dy - rule.min_dy + 1;
				auto wit = s_rule_when_rect.find(r_id);
				auto tit = s_rule_then_rect.find(r_id);
				if (wit != s_rule_when_rect.end() && wit->second.Contains(io.MousePos)) {
					float rx = io.MousePos.x - wit->second.Min.x;
					float ry = io.MousePos.y - wit->second.Min.y;
					int col = std::clamp(static_cast<int>(rx / (cell_sz + 1.0f)), 0, cols - 1);
					int row = std::clamp(static_cast<int>(ry / (cell_sz + 1.0f)), 0, rows - 1);
					hit_dx = rule.min_dx + col;
					hit_dy = rule.min_dy + row;
					hit = true;
				} else if (tit != s_rule_then_rect.end() && tit->second.Contains(io.MousePos)) {
					float rx = io.MousePos.x - tit->second.Min.x;
					float ry = io.MousePos.y - tit->second.Min.y;
					int col = std::clamp(static_cast<int>(rx / (cell_sz + 1.0f)), 0, cols - 1);
					int row = std::clamp(static_cast<int>(ry / (cell_sz + 1.0f)), 0, rows - 1);
					hit_dx = rule.min_dx + col;
					hit_dy = rule.min_dy + row;
					hit = true;
				}
				if (hit) {
					eq_click_consumed = true;
					s_keep_eq_popup_open = true;
					if (s_active_eq_cell_id < static_cast<int>(rule.then_actions.size()) && s_active_eq_attr_idx >= 0 &&
						s_active_eq_attr_idx < 3) {
						auto& cur_ta = rule.then_actions[s_active_eq_cell_id];
						UndoManager::push_snapshot("Add Cell to Equation");
						append_cell_to_equation(cur_ta.attr_exprs[s_active_eq_attr_idx], hit_dx, hit_dy);
						rebuild_needed = true;
						unsaved_changes = true;
					}
				}
			}

			ImGui::BeginGroup();
			ImGui::Text("When");

			float btn_pair_w = tiny_sz.x * 2.0f + 2.0f;
			float left_col_btn_w = tiny_sz.x + 2.0f;
			float top_pad_x = left_col_btn_w + std::max(0.0f, (when_grid_px_w - btn_pair_w) * 0.5f);
			ImGui::Dummy(ImVec2(top_pad_x, 0.0f));
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));
			if (ImGui::Button("+##top", tiny_sz)) {
				try_expand(ExpandSide::Top);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Add row to top");
			ImGui::SameLine(0.0f, 2.0f);
			bool top_shrink_disabled = (rule.min_dy >= 0 || grid_h <= 1);
			if (top_shrink_disabled)
				ImGui::BeginDisabled();
			if (ImGui::Button("-##top", tiny_sz)) {
				do_shrink(ExpandSide::Top);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Remove row from top");
			if (top_shrink_disabled)
				ImGui::EndDisabled();

			ImGui::BeginGroup();
			float left_v_pad = std::max(0.0f, (when_grid_px_h - (tiny_sz.y * 2.0f + 2.0f)) * 0.5f);
			if (left_v_pad > 0.0f)
				ImGui::Dummy(ImVec2(0.0f, left_v_pad));
			if (ImGui::Button("+##left", tiny_sz)) {
				try_expand(ExpandSide::Left);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Add column to left");
			bool left_shrink_disabled = (rule.min_dx >= 0 || grid_w <= 1);
			if (left_shrink_disabled)
				ImGui::BeginDisabled();
			if (ImGui::Button("-##left", tiny_sz)) {
				do_shrink(ExpandSide::Left);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Remove column from left");
			if (left_shrink_disabled)
				ImGui::EndDisabled();
			ImGui::EndGroup();

			ImGui::SameLine(0.0f, 2.0f);

			ImGui::BeginGroup();
			ImVec2 when_cells_start = ImGui::GetCursorScreenPos();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));
			for (int dy = rule.min_dy; dy <= rule.max_dy; ++dy) {
				for (int dx = rule.min_dx; dx <= rule.max_dx; ++dx) {
					int c_id = rule.get_cell_idx(dx, dy);
					ImGui::PushID(c_id);

					ImGui::InvisibleButton("##wc", ImVec2(cell_sz, cell_sz));
					bool is_hovered = ImGui::IsItemHovered();
					bool btn_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
					bool is_right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
					bool is_middle_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Middle);
					ImVec2 c_min = ImGui::GetItemRectMin();
					ImVec2 c_max = ImGui::GetItemRectMax();
					bool in_cell = (io.MousePos.x >= c_min.x && io.MousePos.x < c_max.x && io.MousePos.y >= c_min.y &&
									io.MousePos.y < c_max.y);

					if (dx == 0 && dy == 0) {
						ImU32 col = IM_COL32(mat.color[0], mat.color[1], mat.color[2], 255);
						dl->AddRectFilled(c_min, c_max, col, 3.0f);
						float lum = 0.299f * mat.color[0] + 0.587f * mat.color[1] + 0.114f * mat.color[2];
						ImU32 dot_col = (lum > 130.0f) ? IM_COL32(0, 0, 0, 180) : IM_COL32(255, 255, 255, 220);
						dl->AddCircleFilled(ImVec2((c_min.x + c_max.x) * 0.5f, (c_min.y + c_max.y) * 0.5f),
											std::max(1.5f, cell_sz * 0.14f), dot_col);
					} else {
						const auto& when_ids = (c_id >= 0 && c_id < static_cast<int>(rule.when.size()))
												   ? rule.when[c_id]
												   : std::vector<uint8_t>{};
						if (when_ids.empty()) {
							dl->AddRectFilled(c_min, c_max, IM_COL32(26, 28, 33, 255), 3.0f);
							dl->AddCircleFilled(ImVec2((c_min.x + c_max.x) * 0.5f, (c_min.y + c_max.y) * 0.5f), 1.3f,
												IM_COL32(85, 92, 105, 160));
						} else if (when_ids.size() == 1) {
							const auto& ref_mat = MaterialManager::get_material(when_ids[0]);
							dl->AddRectFilled(c_min, c_max,
											  IM_COL32(ref_mat.color[0], ref_mat.color[1], ref_mat.color[2], 255),
											  3.0f);
						} else {
							size_t num_mats = when_ids.size();
							float stripe_w = (c_max.x - c_min.x) / static_cast<float>(num_mats);
							for (size_t s = 0; s < num_mats; ++s) {
								const auto& ref_mat = MaterialManager::get_material(when_ids[s]);
								dl->AddRectFilled(ImVec2(c_min.x + s * stripe_w, c_min.y),
												  ImVec2(c_min.x + (s + 1) * stripe_w, c_max.y),
												  IM_COL32(ref_mat.color[0], ref_mat.color[1], ref_mat.color[2], 255),
												  (s == 0 || s == num_mats - 1) ? 3.0f : 0.0f);
							}
						}
					}

					std::string popup_id = "select_material_when_" + std::to_string(r_id) + "_" + std::to_string(c_id);

					bool is_eq_picking =
						(s_active_eq_popup_imgui_id != 0 && s_active_eq_rule_id == r_id && s_active_eq_cell_id >= 0 &&
						 s_active_eq_cell_id < static_cast<int>(rule.then_actions.size()) &&
						 !s_active_popup_rect.Contains(io.MousePos));

					if (is_eq_picking && in_cell) {
						dl->AddRect(c_min, c_max, IM_COL32(56, 189, 248, 240), 3.0f, 0, 2.0f);
						ImGui::SetTooltip("Click to add [%d, %d] to equation", dx, dy);
					} else if (!eq_click_consumed && !reference.is_inherited && c_id >= 0 &&
							   c_id < static_cast<int>(rule.when.size())) {
						if (is_middle_clicked) {
							if (dx == 0 && dy == 0) {
								g_copied_rule_cell.type = CopiedRuleCell::Type::When;
								g_copied_rule_cell.when_val = {mat.id};
								g_copied_rule_cell.when_attrs =
									(c_id >= 0 && c_id < static_cast<int>(rule.when_attrs.size()))
										? rule.when_attrs[c_id]
										: std::vector<AttributeCondition>{};
								g_copied_rule_cell.src_dx = dx;
								g_copied_rule_cell.src_dy = dy;
								g_copied_rule_cell.has_copied = true;
							} else {
								g_copied_rule_cell.type = CopiedRuleCell::Type::When;
								g_copied_rule_cell.when_val = rule.when[c_id];
								g_copied_rule_cell.when_attrs =
									(c_id >= 0 && c_id < static_cast<int>(rule.when_attrs.size()))
										? rule.when_attrs[c_id]
										: std::vector<AttributeCondition>{};
								g_copied_rule_cell.src_dx = dx;
								g_copied_rule_cell.src_dy = dy;
								g_copied_rule_cell.has_copied = true;
							}
						} else if (is_right_clicked || (in_cell && ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
							if (dx != 0 || dy != 0) {
								if (!rule.when[c_id].empty() || (c_id < static_cast<int>(rule.when_attrs.size()) &&
																 !rule.when_attrs[c_id].empty())) {
									if (!s_is_dragging_clear) {
										UndoManager::push_snapshot("Clear Rule Cell");
										s_is_dragging_clear = true;
									}
									rule.when[c_id].clear();
									if (c_id < static_cast<int>(rule.when_attrs.size())) {
										rule.when_attrs[c_id].clear();
									}
									rebuild_needed = true;
									unsaved_changes = true;
								}
							} else {
								if (c_id >= 0 && c_id < static_cast<int>(rule.when_attrs.size()) &&
									!rule.when_attrs[c_id].empty()) {
									if (!s_is_dragging_clear) {
										UndoManager::push_snapshot("Clear Center Conditions");
										s_is_dragging_clear = true;
									}
									rule.when_attrs[c_id].clear();
									rebuild_needed = true;
									unsaved_changes = true;
								}
							}
						} else if (io.KeyShift &&
								   (btn_clicked || (in_cell && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
							if (dx != 0 || dy != 0) {
								if (g_copied_rule_cell.can_paste_as_when()) {
									auto target_when = g_copied_rule_cell.get_as_when();
									if (rule.when[c_id] != target_when ||
										(g_copied_rule_cell.type == CopiedRuleCell::Type::When &&
										 (c_id >= static_cast<int>(rule.when_attrs.size())
											  ? !g_copied_rule_cell.when_attrs.empty()
											  : rule.when_attrs[c_id] != g_copied_rule_cell.when_attrs))) {
										if (!s_is_dragging_paint) {
											UndoManager::push_snapshot("Paint Rule Cell");
											s_is_dragging_paint = true;
										}
										rule.when[c_id] = target_when;
										if (g_copied_rule_cell.type == CopiedRuleCell::Type::When) {
											if (c_id >= static_cast<int>(rule.when_attrs.size())) {
												rule.when_attrs.resize(rule.total_cells());
											}
											rule.when_attrs[c_id] = g_copied_rule_cell.when_attrs;
										}
										rebuild_needed = true;
										unsaved_changes = true;
									}
								}
							}
						} else if (btn_clicked && !io.KeyShift) {
							ImGui::OpenPopup(popup_id.c_str());
						}
					}

					if (ImGui::BeginPopup(popup_id.c_str())) {
						if (dx == 0 && dy == 0) {
							ImGui::TextDisabled("Center Material: %s", mat.name.c_str());
						} else {
							for (const auto& m : materials) {
								bool checked = (c_id >= 0 && c_id < static_cast<int>(rule.when.size())) &&
											   (std::find(rule.when[c_id].begin(), rule.when[c_id].end(), m.id) !=
												rule.when[c_id].end());
								ImVec4 m_col = ImVec4(m.color[0] / 255.f, m.color[1] / 255.f, m.color[2] / 255.f, 1.0f);
								if (UI::selectable_with_color((m.name + "##wcb_" + std::to_string(r_id) + "_" +
															   std::to_string(c_id) + "_" + std::to_string(m.id))
																  .c_str(),
															  m_col, checked, ImGuiSelectableFlags_DontClosePopups,
															  ImVec2(0, 0), true)) {
									UndoManager::push_snapshot("Toggle Rule Checkbox");
									if (!checked) {
										if (std::find(rule.when[c_id].begin(), rule.when[c_id].end(), m.id) ==
											rule.when[c_id].end()) {
											rule.when[c_id].push_back(m.id);
										}
									} else {
										std::erase(rule.when[c_id], m.id);
									}
									g_copied_rule_cell.type = CopiedRuleCell::Type::When;
									g_copied_rule_cell.when_val = rule.when[c_id];
									g_copied_rule_cell.when_attrs =
										(c_id >= 0 && c_id < static_cast<int>(rule.when_attrs.size()))
											? rule.when_attrs[c_id]
											: std::vector<AttributeCondition>{};
									g_copied_rule_cell.src_dx = dx;
									g_copied_rule_cell.src_dy = dy;
									g_copied_rule_cell.has_copied = true;
									rebuild_needed = true;
									unsaved_changes = true;
								}
							}
						}

						const auto& meta = SetManager::get_current_metadata();
						if (!meta.attributes.empty()) {
							ImGui::Separator();
							ImGui::Text("Attribute Conditions:");
							if (c_id >= static_cast<int>(rule.when_attrs.size())) {
								rule.when_attrs.resize(rule.total_cells());
							}
							auto& cell_conds = rule.when_attrs[c_id];
							for (size_t cond_i = 0; cond_i < cell_conds.size(); ++cond_i) {
								ImGui::PushID(static_cast<int>(cond_i + 3000));
								auto& ac = cell_conds[cond_i];

								std::string cur_attr_name = (ac.attr_index < meta.attributes.size())
																? meta.attributes[ac.attr_index].name
																: ("attr" + std::to_string(ac.attr_index));
								ImGui::SetNextItemWidth(85.0f);
								if (ImGui::BeginCombo("##attr_sel", cur_attr_name.c_str())) {
									for (size_t a_idx = 0; a_idx < meta.attributes.size(); ++a_idx) {
										if (ImGui::Selectable(meta.attributes[a_idx].name.c_str(),
															  ac.attr_index == a_idx)) {
											UndoManager::push_snapshot("Edit Attribute Condition");
											ac.attr_index = static_cast<uint8_t>(a_idx);
											rebuild_needed = true;
											unsaved_changes = true;
										}
									}
									ImGui::EndCombo();
								}

								ImGui::SameLine();
								const char* ops[] = {"==", "!=", "<", "<=", ">", ">="};
								CompareOp op_enums[] = {CompareOp::Equal,	CompareOp::NotEqual,
														CompareOp::Less,	CompareOp::LessOrEqual,
														CompareOp::Greater, CompareOp::GreaterOrEqual};
								int cur_op_idx = 2;
								for (int oi = 0; oi < 6; ++oi) {
									if (ac.op == op_enums[oi]) {
										cur_op_idx = oi;
										break;
									}
								}
								ImGui::SetNextItemWidth(55.0f);
								if (ImGui::BeginCombo("##op_sel", ops[cur_op_idx])) {
									for (int oi = 0; oi < 6; ++oi) {
										if (ImGui::Selectable(ops[oi], cur_op_idx == oi)) {
											UndoManager::push_snapshot("Edit Attribute Condition");
											ac.op = op_enums[oi];
											rebuild_needed = true;
											unsaved_changes = true;
										}
									}
									ImGui::EndCombo();
								}

								ImGui::SameLine();
								if (dx == 0 && dy == 0) {
									ac.target = CompareTarget::Constant;
									ImGui::TextDisabled("Constant");
								} else {
									const char* targets[] = {"Center", "Constant"};
									int cur_tgt = (ac.target == CompareTarget::Constant) ? 1 : 0;
									ImGui::SetNextItemWidth(75.0f);
									if (ImGui::BeginCombo("##tgt_sel", targets[cur_tgt])) {
										if (ImGui::Selectable("Center", cur_tgt == 0)) {
											UndoManager::push_snapshot("Edit Attribute Condition");
											ac.target = CompareTarget::CenterCell;
											rebuild_needed = true;
											unsaved_changes = true;
										}
										if (ImGui::Selectable("Constant", cur_tgt == 1)) {
											UndoManager::push_snapshot("Edit Attribute Condition");
											ac.target = CompareTarget::Constant;
											rebuild_needed = true;
											unsaved_changes = true;
										}
										ImGui::EndCombo();
									}
								}

								if (ac.target == CompareTarget::Constant) {
									ImGui::SameLine();
									bool is_float = (ac.attr_index < meta.attributes.size()) &&
													(meta.attributes[ac.attr_index].type == AttributeType::Float);
									double min_bound = (ac.attr_index < meta.attributes.size())
														   ? meta.attributes[ac.attr_index].min_val
														   : -1e9;
									double max_bound = (ac.attr_index < meta.attributes.size())
														   ? meta.attributes[ac.attr_index].max_val
														   : 1e9;
									ImGui::SetNextItemWidth(70.0f);
									if (is_float) {
										float fcval = static_cast<float>(ac.const_value);
										if (ImGui::InputFloat("##cval", &fcval, 0.0f, 0.0f, "%.2f")) {
											UndoManager::push_snapshot("Edit Attribute Condition");
											fcval = std::clamp(fcval, static_cast<float>(min_bound),
															   static_cast<float>(max_bound));
											ac.const_value = fcval;
											rebuild_needed = true;
											unsaved_changes = true;
										}
									} else {
										int cval = static_cast<int>(ac.const_value);
										if (ImGui::InputInt("##cval", &cval, 0, 0)) {
											UndoManager::push_snapshot("Edit Attribute Condition");
											cval = std::clamp(cval, static_cast<int>(min_bound),
															  static_cast<int>(max_bound));
											ac.const_value = cval;
											rebuild_needed = true;
											unsaved_changes = true;
										}
									}
								}

								ImGui::SameLine();
								ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
								if (button_with_icon("##DelCond", IconManager::get(IconID::Cross),
													 ImVec2(20.0f, 20.0f))) {
									UndoManager::push_snapshot("Delete Attribute Condition");
									cell_conds.erase(cell_conds.begin() + cond_i);
									rebuild_needed = true;
									unsaved_changes = true;
									ImGui::PopStyleColor();
									ImGui::PopID();
									break;
								}
								ImGui::PopStyleColor();
								ImGui::PopID();
							}

							if (button_with_icon("Add Condition", IconManager::get(IconID::Add),
												 ImVec2(120.0f, 22.0f))) {
								UndoManager::push_snapshot("Add Attribute Condition");
								AttributeCondition new_ac;
								new_ac.attr_index = 0;
								new_ac.op = CompareOp::Less;
								new_ac.target =
									(dx == 0 && dy == 0) ? CompareTarget::Constant : CompareTarget::CenterCell;
								new_ac.const_value = 0;
								cell_conds.push_back(new_ac);
								rebuild_needed = true;
								unsaved_changes = true;
							}
						}

						ImGui::EndPopup();
					}

					if (dx == 0 && dy == 0) {
						dl->AddRect(c_min, c_max, IM_COL32(245, 185, 45, 230), 3.0f, 0, 1.6f);
					} else {
						dl->AddRect(c_min, c_max, IM_COL32(50, 54, 64, 220), 3.0f, 0, 1.0f);
					}

					bool has_when_attrs_badge = (c_id >= 0 && c_id < static_cast<int>(rule.when_attrs.size()) &&
												 !rule.when_attrs[c_id].empty());
					if (has_when_attrs_badge) {
						float bsz = std::min(cell_sz * 0.42f, 13.0f);
						ImVec2 badge_min(c_min.x + 1.0f, c_min.y + 1.0f);
						ImVec2 badge_max(c_min.x + bsz, c_min.y + bsz);
						dl->AddRectFilled(badge_min, badge_max, IM_COL32(30, 144, 255, 240), 2.5f);
						ImVec2 txt_sz = ImGui::CalcTextSize("*");
						ImVec2 txt_pos(badge_min.x + (bsz - txt_sz.x) * 0.5f, badge_min.y + (bsz - txt_sz.y) * 0.5f);
						dl->AddText(txt_pos, IM_COL32(255, 255, 255, 255), "*");
					}

					if (is_hovered) {
						dl->AddRect(c_min, c_max, IM_COL32(255, 255, 255, 240), 3.0f, 0, 1.5f);
						std::string tip;
						if (dx == 0 && dy == 0) {
							tip = mat.name;
						} else if (c_id >= 0 && c_id < static_cast<int>(rule.when.size()) && rule.when[c_id].empty()) {
							tip = "Any (*)";
						} else if (c_id >= 0 && c_id < static_cast<int>(rule.when.size())) {
							for (size_t i = 0; i < rule.when[c_id].size(); ++i) {
								if (i > 0)
									tip += ", ";
								tip += MaterialManager::get_material(rule.when[c_id][i]).name;
							}
						}
						if (has_when_attrs_badge) {
							const auto& meta = SetManager::get_current_metadata();
							for (const auto& ac : rule.when_attrs[c_id]) {
								std::string aname = (ac.attr_index < meta.attributes.size())
														? meta.attributes[ac.attr_index].name
														: ("attr" + std::to_string(ac.attr_index));
								std::string op_str = (ac.op == CompareOp::Less)				? "<"
													 : (ac.op == CompareOp::LessOrEqual)	? "<="
													 : (ac.op == CompareOp::Greater)		? ">"
													 : (ac.op == CompareOp::GreaterOrEqual) ? ">="
													 : (ac.op == CompareOp::Equal)			? "=="
													 : (ac.op == CompareOp::NotEqual)		? "!="
																							: "?";
								std::string tgt_str = (ac.target == CompareTarget::CenterCell)
														  ? "Center"
														  : std::to_string(ac.const_value);
								tip += fmt::format("\n• {} {} {}", aname, op_str, tgt_str);
							}
						}
						ImGui::SetTooltip("%s", tip.c_str());
					}

					if (dx < rule.max_dx) {
						ImGui::SameLine();
					}
					ImGui::PopID();
				}
			}
			ImGui::PopStyleVar();
			ImGui::EndGroup();

			ImGui::SameLine(0.0f, 2.0f);

			ImGui::BeginGroup();
			float right_v_pad = std::max(0.0f, (when_grid_px_h - (tiny_sz.y * 2.0f + 2.0f)) * 0.5f);
			if (right_v_pad > 0.0f)
				ImGui::Dummy(ImVec2(0.0f, right_v_pad));
			if (ImGui::Button("+##right", tiny_sz)) {
				try_expand(ExpandSide::Right);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Add column to right");
			bool right_shrink_disabled = (rule.max_dx <= 0 || grid_w <= 1);
			if (right_shrink_disabled)
				ImGui::BeginDisabled();
			if (ImGui::Button("-##right", tiny_sz)) {
				do_shrink(ExpandSide::Right);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Remove column from right");
			if (right_shrink_disabled)
				ImGui::EndDisabled();
			ImGui::EndGroup();

			float btm_pad_x = left_col_btn_w + std::max(0.0f, (when_grid_px_w - btn_pair_w) * 0.5f);
			ImGui::Dummy(ImVec2(btm_pad_x, 0.0f));
			ImGui::SameLine(0.0f, 0.0f);
			if (ImGui::Button("+##bottom", tiny_sz)) {
				try_expand(ExpandSide::Bottom);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Add row to bottom");
			ImGui::SameLine(0.0f, 2.0f);
			bool btm_shrink_disabled = (rule.max_dy <= 0 || grid_h <= 1);
			if (btm_shrink_disabled)
				ImGui::BeginDisabled();
			if (ImGui::Button("-##bottom", tiny_sz)) {
				do_shrink(ExpandSide::Bottom);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Remove row from bottom");
			if (btm_shrink_disabled)
				ImGui::EndDisabled();

			ImGui::PopStyleVar(2);
			ImGui::EndGroup();

			int grid_cols = rule.max_dx - rule.min_dx + 1;
			int grid_rows = rule.max_dy - rule.min_dy + 1;
			s_rule_when_rect[r_id] =
				ImRect(when_cells_start, ImVec2(when_cells_start.x + grid_cols * cell_sz + (grid_cols - 1) * 1.0f,
												when_cells_start.y + grid_rows * cell_sz + (grid_rows - 1) * 1.0f));

			ImGui::SameLine(0.0f, get_spacing(0.0f));
			ImGui::BeginGroup();
			float arrow_y_offset = (tiny_sz.y + 2.0f) + std::max(0.0f, (when_grid_px_h - 14.0f) * 0.5f);
			ImGui::Dummy(ImVec2(0.0f, arrow_y_offset));
			ImGui::TextDisabled("->");
			ImGui::EndGroup();

			ImGui::SameLine(0.0f, get_spacing(0.0f));

			ImGui::BeginGroup();
			ImGui::Text("Then");
			ImGui::Dummy(ImVec2(0.0f, tiny_sz.y + 2.0f));

			ImVec2 then_cells_start = ImGui::GetCursorScreenPos();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));
			for (int dy = rule.min_dy; dy <= rule.max_dy; ++dy) {
				for (int dx = rule.min_dx; dx <= rule.max_dx; ++dx) {
					int c_id = rule.get_cell_idx(dx, dy);
					ImGui::PushID(c_id + 1000);

					ImGui::InvisibleButton("##tc", ImVec2(cell_sz, cell_sz));
					bool is_hovered = ImGui::IsItemHovered();
					bool btn_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
					bool is_right_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);
					bool is_middle_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Middle);
					ImVec2 c_min = ImGui::GetItemRectMin();
					ImVec2 c_max = ImGui::GetItemRectMax();
					bool in_cell = (io.MousePos.x >= c_min.x && io.MousePos.x < c_max.x && io.MousePos.y >= c_min.y &&
									io.MousePos.y < c_max.y);

					uint8_t then_id = (c_id >= 0 && c_id < static_cast<int>(rule.then.size())) ? rule.then[c_id] : 255;

					if (then_id != 255) {
						const auto& ref_mat = MaterialManager::get_material(then_id);
						dl->AddRectFilled(c_min, c_max,
										  IM_COL32(ref_mat.color[0], ref_mat.color[1], ref_mat.color[2], 255), 3.0f);
					} else {
						if (dx == 0 && dy == 0) {
							dl->AddRectFilled(
								c_min, c_max,
								IM_COL32(mat.color[0] * 0.35f, mat.color[1] * 0.35f, mat.color[2] * 0.35f, 255), 3.0f);
						} else if (c_id >= 0 && c_id < static_cast<int>(rule.when.size()) &&
								   rule.when[c_id].size() == 1) {
							const auto& ref_mat = MaterialManager::get_material(rule.when[c_id][0]);
							dl->AddRectFilled(c_min, c_max,
											  IM_COL32(ref_mat.color[0] * 0.35f, ref_mat.color[1] * 0.35f,
													   ref_mat.color[2] * 0.35f, 255),
											  3.0f);
						} else if (c_id >= 0 && c_id < static_cast<int>(rule.when.size()) &&
								   rule.when[c_id].size() > 1) {
							size_t num_mats = rule.when[c_id].size();
							float stripe_w = (c_max.x - c_min.x) / static_cast<float>(num_mats);
							for (size_t s = 0; s < num_mats; ++s) {
								const auto& ref_mat = MaterialManager::get_material(rule.when[c_id][s]);
								dl->AddRectFilled(ImVec2(c_min.x + s * stripe_w, c_min.y),
												  ImVec2(c_min.x + (s + 1) * stripe_w, c_max.y),
												  IM_COL32(ref_mat.color[0] * 0.35f, ref_mat.color[1] * 0.35f,
														   ref_mat.color[2] * 0.35f, 255),
												  (s == 0 || s == num_mats - 1) ? 3.0f : 0.0f);
							}
						} else {
							dl->AddRectFilled(c_min, c_max, IM_COL32(24, 26, 30, 255), 3.0f);
						}
						float cx = (c_min.x + c_max.x) * 0.5f;
						float cy = (c_min.y + c_max.y) * 0.5f;
						float half_w = cell_sz * 0.20f;
						dl->AddLine(ImVec2(cx - half_w, cy), ImVec2(cx + half_w, cy), IM_COL32(215, 220, 230, 160),
									1.5f);
					}

					if (dx == 0 && dy == 0) {
						float cx = (c_min.x + c_max.x) * 0.5f;
						float cy = (c_min.y + c_max.y) * 0.5f;
						ImU32 pip_col = (then_id == 255) ? IM_COL32(245, 185, 45, 220) : IM_COL32(255, 255, 255, 200);
						dl->AddCircleFilled(ImVec2(cx, cy), std::max(1.3f, cell_sz * 0.12f), pip_col);
					}

					const std::string popup_id =
						"select_material_then_" + std::to_string(r_id) + "_" + std::to_string(c_id);

					bool is_eq_picking =
						(s_active_eq_popup_imgui_id != 0 && s_active_eq_rule_id == r_id && s_active_eq_cell_id >= 0 &&
						 s_active_eq_cell_id < static_cast<int>(rule.then_actions.size()) &&
						 !s_active_popup_rect.Contains(io.MousePos));

					if (is_eq_picking && in_cell) {
						dl->AddRect(c_min, c_max, IM_COL32(56, 189, 248, 240), 3.0f, 0, 2.0f);
						ImGui::SetTooltip("Click to add [%d, %d] to equation", dx, dy);
					} else if (!eq_click_consumed && !reference.is_inherited && c_id >= 0 &&
							   c_id < static_cast<int>(rule.then.size())) {
						if (is_middle_clicked) {
							g_copied_rule_cell.type = CopiedRuleCell::Type::Then;
							g_copied_rule_cell.then_val = rule.then[c_id];
							g_copied_rule_cell.then_action =
								(c_id >= 0 && c_id < static_cast<int>(rule.then_actions.size()))
									? rule.then_actions[c_id]
									: ThenCellAction{};
							g_copied_rule_cell.src_dx = dx;
							g_copied_rule_cell.src_dy = dy;
							g_copied_rule_cell.has_copied = true;
						} else if (is_right_clicked || (in_cell && ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
							bool has_custom_then = (rule.then[c_id] != 255);
							if (c_id < static_cast<int>(rule.then_actions.size())) {
								const auto& ta = rule.then_actions[c_id];
								if (!ta.attr_exprs[0].empty() || !ta.attr_exprs[1].empty() ||
									!ta.attr_exprs[2].empty()) {
									has_custom_then = true;
								}
							}
							if (has_custom_then) {
								if (!s_is_dragging_clear) {
									UndoManager::push_snapshot("Clear Then Cell");
									s_is_dragging_clear = true;
								}
								rule.then[c_id] = 255;
								if (c_id < static_cast<int>(rule.then_actions.size())) {
									rule.then_actions[c_id] = ThenCellAction{};
								}
								rebuild_needed = true;
								unsaved_changes = true;
							}
						} else if (io.KeyShift &&
								   (btn_clicked || (in_cell && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
							if (g_copied_rule_cell.can_paste_as_then()) {
								uint8_t target_then = g_copied_rule_cell.get_as_then();
								ThenCellAction shifted_action;
								if (g_copied_rule_cell.type == CopiedRuleCell::Type::Then) {
									shifted_action = g_copied_rule_cell.then_action;
								}
								bool paste_needed = (rule.then[c_id] != target_then);
								if (g_copied_rule_cell.type == CopiedRuleCell::Type::Then &&
									c_id < static_cast<int>(rule.then_actions.size()) &&
									rule.then_actions[c_id] != shifted_action) {
									paste_needed = true;
								}
								if (paste_needed) {
									if (!s_is_dragging_paint) {
										UndoManager::push_snapshot("Paint Then Cell");
										s_is_dragging_paint = true;
									}
									rule.then[c_id] = target_then;
									if (c_id >= static_cast<int>(rule.then_actions.size())) {
										rule.then_actions.resize(rule.total_cells());
									}
									if (g_copied_rule_cell.type == CopiedRuleCell::Type::Then) {
										rule.then_actions[c_id] = shifted_action;
									}
									rebuild_needed = true;
									unsaved_changes = true;
								}
							}
						} else if (btn_clicked && !io.KeyShift) {
							ImGui::OpenPopup(popup_id.c_str());
						}
					}

					if (s_keep_eq_popup_open && s_active_eq_cell_id == c_id && s_active_eq_rule_id == r_id) {
						ImGui::OpenPopup(popup_id.c_str());
						s_keep_eq_popup_open = false;
					}

					if (ImGui::BeginPopup(popup_id.c_str())) {
						s_active_eq_popup_imgui_id = ImGui::GetID(popup_id.c_str());
						s_active_eq_popup_id = popup_id;
						s_active_eq_rule_id = r_id;
						s_active_eq_cell_id = c_id;
						ImVec2 wpos = ImGui::GetWindowPos();
						ImVec2 wsz = ImGui::GetWindowSize();
						s_active_popup_rect = ImRect(wpos, ImVec2(wpos.x + wsz.x, wpos.y + wsz.y));
						ImDrawList* popup_dl = ImGui::GetWindowDrawList();

						ImVec4 neutral_col = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
						if (UI::selectable_with_color("Unchanged (-)", neutral_col, then_id == 255)) {
							UndoManager::push_snapshot("Set Then Unchanged");
							rule.then[c_id] = 255;
							g_copied_rule_cell.type = CopiedRuleCell::Type::Then;
							g_copied_rule_cell.then_val = 255;
							g_copied_rule_cell.then_action = (c_id < static_cast<int>(rule.then_actions.size()))
																 ? rule.then_actions[c_id]
																 : ThenCellAction{};
							g_copied_rule_cell.src_dx = dx;
							g_copied_rule_cell.src_dy = dy;
							g_copied_rule_cell.has_copied = true;
							rebuild_needed = true;
							unsaved_changes = true;
						}
						for (const auto& m : materials) {
							ImVec4 m_col = ImVec4(m.color[0] / 255.f, m.color[1] / 255.f, m.color[2] / 255.f, 1.0f);
							if (UI::selectable_with_color((m.name + "##tcb_" + std::to_string(r_id) + "_" +
														   std::to_string(c_id) + "_" + std::to_string(m.id))
															  .c_str(),
														  m_col, then_id == m.id)) {
								UndoManager::push_snapshot("Select Then Material");
								rule.then[c_id] = m.id;
								g_copied_rule_cell.type = CopiedRuleCell::Type::Then;
								g_copied_rule_cell.then_val = m.id;
								g_copied_rule_cell.then_action = (c_id < static_cast<int>(rule.then_actions.size()))
																	 ? rule.then_actions[c_id]
																	 : ThenCellAction{};
								g_copied_rule_cell.src_dx = dx;
								g_copied_rule_cell.src_dy = dy;
								g_copied_rule_cell.has_copied = true;
								rebuild_needed = true;
								unsaved_changes = true;
							}
						}

						if (c_id >= static_cast<int>(rule.then_actions.size())) {
							rule.then_actions.resize(rule.total_cells());
						}
						auto& cur_ta = rule.then_actions[c_id];

						ImGui::Separator();
						if (custom_checkbox(
								"Update Cell##_then_cell_upd", &cur_ta.updated, IconID::Update,
								"Controls whether this cell is marked updated when this rule applies.\nDefault is ON. "
								"Turning OFF is useful for rules that always run (e.g. heat conduction) so they don't "
								"block other rules on this cell in the same frame.")) {
							UndoManager::push_snapshot("Toggle Then Cell Updated");
							rebuild_needed = true;
							unsaved_changes = true;
						}

						const auto& meta = SetManager::get_current_metadata();
						if (!meta.attributes.empty()) {
							ImGui::Separator();
							ImGui::Text("Attribute Functions:");
							for (size_t a_idx = 0; a_idx < meta.attributes.size(); ++a_idx) {
								const auto& attr = meta.attributes[a_idx];
								ImGui::PushID(static_cast<int>(a_idx + 4000));

								bool is_active_attr = (s_active_eq_attr_idx == static_cast<int>(a_idx));
								if (is_active_attr) {
									ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.0f, 1.0f), "%s =", attr.name.c_str());
								} else {
									ImGui::Text("%s =", attr.name.c_str());
								}
								if (ImGui::IsItemClicked()) {
									s_active_eq_attr_idx = static_cast<int>(a_idx);
								}
								ImGui::SameLine();

								bool is_raw_editing = (s_active_raw_edit_attr == static_cast<int>(a_idx));
								char buf[256];
								std::strncpy(buf, cur_ta.attr_exprs[a_idx].c_str(), sizeof(buf));
								buf[sizeof(buf) - 1] = '\0';

								if (is_raw_editing) {
									ImGui::SetNextItemWidth(180.0f);
									if (ImGui::InputTextWithHint("##expr_raw", "Click cells or enter formula", buf,
																 sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
										UndoManager::push_snapshot("Edit Attribute Function");
										cur_ta.attr_exprs[a_idx] = buf;
										s_active_raw_edit_attr = -1;
										rebuild_needed = true;
										unsaved_changes = true;
									}
									if (cur_ta.attr_exprs[a_idx] != buf) {
										UndoManager::push_snapshot("Edit Attribute Function");
										cur_ta.attr_exprs[a_idx] = buf;
										rebuild_needed = true;
										unsaved_changes = true;
									}
									ImGui::SameLine();
									if (ImGui::SmallButton("Done")) {
										s_active_raw_edit_attr = -1;
									}
									if (ImGui::IsItemHovered()) {
										ImGui::SetTooltip("Finish editing formula text");
									}
								} else {
									float frame_w = 180.0f;
									float frame_h = ImGui::GetFrameHeight();
									ImVec2 frame_min = ImGui::GetCursorScreenPos();
									ImVec2 frame_max = ImVec2(frame_min.x + frame_w, frame_min.y + frame_h);

									ImGui::InvisibleButton("##expr_box", ImVec2(frame_w, frame_h));
									bool box_hovered = ImGui::IsItemHovered();
									bool box_double_clicked =
										box_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

									if (box_double_clicked) {
										s_active_eq_attr_idx = static_cast<int>(a_idx);
										s_active_raw_edit_attr = static_cast<int>(a_idx);
									}

									ImU32 frame_bg =
										ImGui::GetColorU32(box_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
									ImU32 border_col = is_active_attr ? IM_COL32(56, 189, 248, 180)
																	  : ImGui::GetColorU32(ImGuiCol_Border);
									popup_dl->AddRectFilled(frame_min, frame_max, frame_bg, 4.0f);
									popup_dl->AddRect(frame_min, frame_max, border_col, 4.0f);

									popup_dl->PushClipRect(ImVec2(frame_min.x + 2.0f, frame_min.y + 1.0f),
														   ImVec2(frame_max.x - 2.0f, frame_max.y - 1.0f), true);

									if (cur_ta.attr_exprs[a_idx].empty()) {
										float text_y = frame_min.y + (frame_h - ImGui::GetTextLineHeight()) * 0.5f;
										popup_dl->AddText(ImVec2(frame_min.x + 6.0f, text_y),
														  ImGui::GetColorU32(ImGuiCol_TextDisabled),
														  "— (Click cells or Edit)");
									} else {
										auto segs = split_expr_segments(cur_ta.attr_exprs[a_idx]);
										float cur_x = frame_min.x + 6.0f;
										float text_y = frame_min.y + (frame_h - ImGui::GetTextLineHeight()) * 0.5f;

										for (const auto& seg : segs) {
											if (!seg.is_badge) {
												if (!seg.text.empty()) {
													ImVec2 t_sz = ImGui::CalcTextSize(seg.text.c_str());
													popup_dl->AddText(ImVec2(cur_x, text_y),
																	  ImGui::GetColorU32(ImGuiCol_Text),
																	  seg.text.c_str());
													cur_x += t_sz.x;
												}
											} else {
												const auto& b = seg.badge;
												std::string badge_label = b.is_relative
																			  ? fmt::format("~[{},{}]", b.dx, b.dy)
																			  : fmt::format("[{},{}]", b.dx, b.dy);
												ImVec2 b_sz = ImGui::CalcTextSize(badge_label.c_str());
												float pill_w = b_sz.x + 8.0f;
												float pill_h = frame_h - 6.0f;
												ImVec2 pill_min = ImVec2(cur_x, frame_min.y + 3.0f);
												ImVec2 pill_max = ImVec2(cur_x + pill_w, pill_min.y + pill_h);

												bool pill_hovered =
													(io.MousePos.x >= pill_min.x && io.MousePos.x < pill_max.x &&
													 io.MousePos.y >= pill_min.y && io.MousePos.y < pill_max.y &&
													 box_hovered);

												if (pill_hovered) {
													if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
														UndoManager::push_snapshot("Toggle Relative Cell Badge");
														toggle_badge_relative(cur_ta.attr_exprs[a_idx], b, dx, dy);
														rebuild_needed = true;
														unsaved_changes = true;
													} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
														if (io.KeyShift) {
															UndoManager::push_snapshot("Remove Cell Badge");
															remove_badge(cur_ta.attr_exprs[a_idx], b);
															rebuild_needed = true;
															unsaved_changes = true;
														}
													}

													if (b.is_relative) {
														int abs_x = dx + b.dx;
														int abs_y = dy + b.dy;
														ImGui::SetTooltip(
															"Relative cell ~[%d, %d] (targets [%d, %d])\nRight-click: "
															"Make Absolute [%d, %d]\nShift+Click: Remove",
															b.dx, b.dy, abs_x, abs_y, abs_x, abs_y);
													} else {
														int rel_x = b.dx - dx;
														int rel_y = b.dy - dy;
														ImGui::SetTooltip("Absolute cell [%d, %d]\nRight-click: Make "
																		  "Relative ~[%d, %d]\nShift+Click: Remove",
																		  b.dx, b.dy, rel_x, rel_y);
													}
												}

												ImU32 pill_bg = b.is_relative ? IM_COL32(18, 64, 76, 230)
																			  : IM_COL32(45, 52, 65, 230);
												if (pill_hovered) {
													pill_bg = b.is_relative ? IM_COL32(28, 92, 108, 255)
																			: IM_COL32(65, 75, 92, 255);
												}
												ImU32 pill_border = b.is_relative ? IM_COL32(56, 189, 248, 200)
																				  : IM_COL32(100, 116, 139, 180);
												ImU32 pill_text_col = b.is_relative ? IM_COL32(103, 232, 249, 255)
																					: IM_COL32(241, 245, 249, 255);

												popup_dl->AddRectFilled(pill_min, pill_max, pill_bg, 3.0f);
												popup_dl->AddRect(pill_min, pill_max, pill_border, 3.0f);
												popup_dl->AddText(
													ImVec2(pill_min.x + 4.0f, pill_min.y + (pill_h - b_sz.y) * 0.5f),
													pill_text_col, badge_label.c_str());

												cur_x += pill_w + 2.0f;
											}
										}
									}
									popup_dl->PopClipRect();

									ImGui::SameLine();
									if (ImGui::SmallButton("Edit")) {
										s_active_eq_attr_idx = static_cast<int>(a_idx);
										s_active_raw_edit_attr = static_cast<int>(a_idx);
									}
									if (ImGui::IsItemHovered()) {
										ImGui::SetTooltip("Edit formula text directly (or double-click the box)");
									}
								}

								if (!cur_ta.attr_exprs[a_idx].empty()) {
									std::vector<ExprOp> test_ops;
									std::string err_msg;
									bool is_valid = parse_and_compile_expr(
										cur_ta.attr_exprs[a_idx], static_cast<uint8_t>(a_idx), test_ops, &err_msg);
									ImGui::SameLine();
									if (is_valid) {
										SDL_Texture* ok_icon = IconManager::get(IconID::Accept);
										if (ok_icon) {
											ImGui::Image(reinterpret_cast<ImTextureID>(ok_icon), ImVec2(15.0f, 15.0f));
										}
										if (ImGui::IsItemHovered()) {
											ImGui::SetTooltip("Valid expression (%zu op%s)", test_ops.size(),
															  test_ops.size() == 1 ? "" : "s");
										}
									} else {
										SDL_Texture* err_icon = IconManager::get(IconID::Cross);
										if (err_icon) {
											ImGui::Image(reinterpret_cast<ImTextureID>(err_icon), ImVec2(15.0f, 15.0f));
										}
										if (ImGui::IsItemHovered()) {
											ImGui::SetTooltip("Error: %s", err_msg.c_str());
										}
									}

									ImGui::SameLine();
									if (ImGui::SmallButton("Clear")) {
										UndoManager::push_snapshot("Clear Attribute Function");
										cur_ta.attr_exprs[a_idx].clear();
										s_active_raw_edit_attr = -1;
										rebuild_needed = true;
										unsaved_changes = true;
									}
								}

								ImGui::PopID();
							}

							ImGui::Spacing();
							ImGui::TextDisabled("Click cells in grid to insert. Right-click badge for relative.");
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 3.0f));
							const char* math_ops[] = {"+", "-", "*", "/", "(", ")", ",", "min", "max", "clamp"};
							for (const char* op : math_ops) {
								if (ImGui::SmallButton(op)) {
									if (s_active_eq_attr_idx >= 0 &&
										s_active_eq_attr_idx < static_cast<int>(meta.attributes.size())) {
										UndoManager::push_snapshot("Insert Math Operator");
										auto& expr = cur_ta.attr_exprs[s_active_eq_attr_idx];
										if (!expr.empty() && expr.back() != ' ' && expr.back() != '(' &&
											std::string(op) != ")") {
											expr += " ";
										}
										expr += op;
										if (std::string(op) == "min" || std::string(op) == "max" ||
											std::string(op) == "clamp") {
											expr += "(";
										} else if (std::string(op) != ")" && std::string(op) != ",") {
											expr += " ";
										}
										rebuild_needed = true;
										unsaved_changes = true;
									}
								}
								ImGui::SameLine();
							}
							ImGui::NewLine();
							ImGui::PopStyleVar();
						}

						ImGui::EndPopup();
					} else {
						if (s_active_eq_popup_id == popup_id && !eq_click_consumed && !s_keep_eq_popup_open) {
							s_active_eq_popup_imgui_id = 0;
							s_active_eq_popup_id.clear();
							s_active_eq_rule_id = -1;
							s_active_eq_cell_id = -1;
							s_active_popup_rect = ImRect();
						}
					}

					if (dx == 0 && dy == 0) {
						dl->AddRect(c_min, c_max, IM_COL32(245, 185, 45, 230), 3.0f, 0, 1.6f);
					} else {
						dl->AddRect(c_min, c_max, IM_COL32(50, 54, 64, 220), 3.0f, 0, 1.0f);
					}

					const ThenCellAction& cell_ta = (c_id >= 0 && c_id < static_cast<int>(rule.then_actions.size()))
														? rule.then_actions[c_id]
														: ThenCellAction{};
					bool has_attr_badge = (!cell_ta.attr_exprs[0].empty() || !cell_ta.attr_exprs[1].empty() ||
										   !cell_ta.attr_exprs[2].empty());

					float bsz = std::min(cell_sz * 0.42f, 13.0f);

					if (has_attr_badge) {
						ImVec2 dmin(c_max.x - bsz - 1.0f, c_max.y - bsz - 1.0f);
						ImVec2 dmax(c_max.x - 1.0f, c_max.y - 1.0f);
						ImU32 badge_bg = IM_COL32(142, 68, 173, 245);
						const char* badge_glyph = "f";
						dl->AddRectFilled(dmin, dmax, badge_bg, 2.5f);
						ImVec2 txt_sz = ImGui::CalcTextSize(badge_glyph);
						ImVec2 txt_pos(dmin.x + (bsz - txt_sz.x) * 0.5f, dmin.y + (bsz - txt_sz.y) * 0.5f);
						dl->AddText(txt_pos, IM_COL32(255, 255, 255, 255), badge_glyph);
					}

					if (!cell_ta.updated) {
						ImVec2 dmin(c_min.x + 1.0f, c_min.y + 1.0f);
						ImVec2 dmax(c_min.x + bsz + 1.0f, c_min.y + bsz + 1.0f);
						ImU32 badge_bg = IM_COL32(230, 126, 34, 245);
						const char* badge_glyph = "~";
						dl->AddRectFilled(dmin, dmax, badge_bg, 2.5f);
						ImVec2 txt_sz = ImGui::CalcTextSize(badge_glyph);
						ImVec2 txt_pos(dmin.x + (bsz - txt_sz.x) * 0.5f, dmin.y + (bsz - txt_sz.y) * 0.5f);
						dl->AddText(txt_pos, IM_COL32(255, 255, 255, 255), badge_glyph);
					}

					if (is_hovered) {
						dl->AddRect(c_min, c_max, IM_COL32(255, 255, 255, 240), 3.0f, 0, 1.5f);
						std::string tip =
							(then_id == 255) ? "Unchanged (-)" : MaterialManager::get_material(then_id).name;
						if (has_attr_badge) {
							const auto& meta = SetManager::get_current_metadata();
							for (size_t a = 0; a < meta.attributes.size(); ++a) {
								if (!cell_ta.attr_exprs[a].empty()) {
									tip += fmt::format("\n• {} = {}", meta.attributes[a].name, cell_ta.attr_exprs[a]);
								}
							}
						}
						if (!cell_ta.updated) {
							tip += "\n• Update: Disabled (never blocks updates)";
						}
						ImGui::SetTooltip("%s", tip.c_str());
					}

					if (dx < rule.max_dx) {
						ImGui::SameLine();
					}
					ImGui::PopID();
				}
			}
			ImGui::PopStyleVar();
			ImGui::EndGroup();

			s_rule_then_rect[r_id] =
				ImRect(then_cells_start, ImVec2(then_cells_start.x + grid_cols * cell_sz + (grid_cols - 1) * 1.0f,
												then_cells_start.y + grid_rows * cell_sz + (grid_rows - 1) * 1.0f));

			if (reference.is_inherited) {
				ImGui::EndDisabled();
			}

			if (!reference.is_inherited) {
				mat.rules[reference.index] = rule;
			}

			ImGui::EndChild();
			ImGui::PopID();
		}
		ImGui::EndChild();

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			s_is_dragging_paint = false;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			s_is_dragging_clear = false;
		}

		if (rebuild_needed) {
			MaterialManager::update_material_rules(selected_id, mat);
			Grid::draw_material(selected_id);
		}

		ImGui::EndTabItem();
	}
}

static std::vector<std::string> get_forked_sets_for(const std::string& set_name) {
	std::vector<std::string> forked_sets;
	if (set_name.empty())
		return forked_sets;

	SetMetadata orig_meta = SetManager::load_set_metadata(set_name);
	std::vector<std::string> all_sets = SetManager::get_sets();

	for (const auto& s : all_sets) {
		if (s == set_name)
			continue;
		SetMetadata s_meta = SetManager::load_set_metadata(s);
		bool is_forked = false;
		if (!s_meta.forked_from_id.empty()) {
			if (s_meta.forked_from_id == set_name ||
				(!orig_meta.workshop_id.empty() && s_meta.forked_from_id == orig_meta.workshop_id) ||
				(!orig_meta.forked_from_id.empty() && s_meta.forked_from_id == orig_meta.forked_from_id)) {
				is_forked = true;
			}
		}
		if (!is_forked && s.rfind(set_name + "_fork", 0) == 0) {
			is_forked = true;
		}
		if (is_forked) {
			forked_sets.push_back(s);
		}
	}
	return forked_sets;
}

void UI::render_manage_sets() {
	const std::string& current_set = SetManager::get_current_set();
	SetMetadata meta = SetManager::get_current_metadata();
	Config& cfg = ConfigManager::get_config();
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::BeginTabItem("Sets")) {
		ImGui::Spacing();

		ImGui::Text("Available Sets:");
		float list_h = static_cast<float>(cfg.ui.set_list_height > 0 ? cfg.ui.set_list_height : 200);
		ImGui::BeginChild("SetsListScroll", ImVec2(0, list_h), true);
		float avail_w = ImGui::GetContentRegionAvail().x;
		const float btn_sz = ImGui::GetTextLineHeightWithSpacing();
		const float btn_gap = 4.0f;

		bool is_logged_in = WorkshopClient::is_logged_in();
		std::string cur_user = is_logged_in ? WorkshopClient::get_logged_in_username() : "";

		for (const auto& s : SetManager::get_sets()) {
			ImGui::PushID(("set_row_" + s).c_str());
			SetMetadata s_meta = SetManager::load_set_metadata(s);
			std::string display_name = s;
			bool is_online = s_meta.is_online || SetManager::is_set_online(s);
			bool is_transient = WorkshopCache::is_transient(SETS_DIRECTORY + s);
			bool is_owner = is_logged_in && !s_meta.author.empty() && (s_meta.author == cur_user);

			if (is_online) {
				if (!s_meta.workshop_id.empty()) {
					UIWorkshop::check_item_existence(s_meta.workshop_id, "set", s, current_set, false);
				}
				if (is_transient) {
					display_name += " [Cached]";
				} else {
					display_name += " [Online]";
				}
			}

			bool show_share = !is_online && is_logged_in;
			bool show_update = is_online && is_owner;
			bool show_download = is_online && is_transient;

			int num_buttons = (show_share ? 1 : 0) + (show_update ? 1 : 0) + (show_download ? 1 : 0);
			float total_btn_w = (num_buttons > 0) ? (num_buttons * btn_sz + num_buttons * btn_gap) : 0.0f;
			float selectable_w = std::max(20.0f, avail_w - total_btn_w);

			if (ImGui::Selectable((display_name + "##selectable_set_" + s).c_str(), s == current_set, 0,
								  ImVec2(selectable_w, 0.0f))) {
				if (s != current_set) {
					if (unsaved_changes) {
						UIModals::open_unsaved_switch(s, "");
					} else {
						SetManager::set_current_set(s);
						selected_id = 0;
					}
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Author: %s\nDescription: %s", s_meta.author.empty() ? "None" : s_meta.author.c_str(),
								  s_meta.description.empty() ? "No description" : s_meta.description.c_str());
			}

			const ImVec2 sel_min = ImGui::GetItemRectMin();
			const float sel_h = ImGui::GetItemRectSize().y;

			if (show_share) {
				ImGui::SameLine(0, btn_gap);
				ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
				if (UI::button_with_icon("##share_set", IconManager::get(IconID::Transmit), ImVec2(sel_h, sel_h))) {
					UIModals::open_workshop_share_set(s);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Share set '%s' to Community Workshop", s.c_str());
				}
			}

			if (show_update) {
				ImGui::SameLine(0, btn_gap);
				ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
				if (UI::button_with_icon("##update_set", IconManager::get(IconID::Update), ImVec2(sel_h, sel_h))) {
					if (!s_meta.workshop_id.empty()) {
						std::string wid = s_meta.workshop_id;
						std::string wset = s;
						WorkshopClient::check_item_exists(wid, [wset, wid, s_meta](bool exists, int http_status) {
							if (!exists && http_status == 404) {
								SetManager::clear_workshop_info(wset);
								ToastManager::warning(fmt::format("Set '{}' was deleted from the workshop.", wset));
							} else {
								WorkshopItemClient it;
								it.id = wid;
								it.title = s_meta.name.empty() ? wset : s_meta.name;
								it.description = s_meta.description;
								it.type = "set";
								it.version = s_meta.version;
								it.author = s_meta.author;
								WorkshopItemEditor::open_update_modal(it, []() { refresh_workshop_items(); });
							}
						});
					} else {
						UIModals::open_workshop_share_set(s);
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Update set '%s' to Community Workshop", s.c_str());
				}
			}

			if (show_download) {
				ImGui::SameLine(0, btn_gap);
				ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
				if (UI::button_with_icon("##download_set", IconManager::get(IconID::Save), ImVec2(sel_h, sel_h))) {
					std::string wid = s_meta.workshop_id;
					std::string wset = s;
					WorkshopClient::check_item_exists(wid, [wset, wid, s_meta](bool exists, int http_status) {
						if (!exists && http_status == 404) {
							SetManager::clear_workshop_info(wset);
							ToastManager::warning(
								fmt::format("Set '{}' was deleted from the workshop. Converted to local.", wset));
						} else {
							bool ok = WorkshopCache::promote_to_local(
								s_meta.workshop_id, "set", s_meta.name.empty() ? wset : s_meta.name,
								SetManager::get_current_set(), s_meta.workshop_hash);
							if (ok) {
								SetManager::set_workshop_info(s_meta.name.empty() ? wset : s_meta.name,
															  s_meta.workshop_id, s_meta.workshop_hash, s_meta.version,
															  s_meta.author);
								ToastManager::success("Saved to local storage!");
							}
						}
					});
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Download '%s' to your computer", s.c_str());
				}
			}

			ImGui::PopID();
		}
		ImGui::EndChild();

		float bar_h = 8.0f;
		ImVec2 cur_pos = ImGui::GetCursorScreenPos();
		ImRect h_bar_bb(cur_pos, ImVec2(cur_pos.x + avail_w, cur_pos.y + bar_h));
		ImGui::InvisibleButton("##SetListResizeBar", ImVec2(-1, bar_h));
		bool h_hovered = ImGui::IsItemHovered();
		bool h_held = ImGui::IsItemActive();
		if (h_hovered || h_held) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		}
		if (h_held && io.MouseDelta.y != 0.0f) {
			cfg.ui.set_list_height = std::clamp(cfg.ui.set_list_height + static_cast<int>(io.MouseDelta.y), 70, 600);
		}
		if (ImGui::IsItemDeactivated()) {
			ConfigManager::save();
		}
		if (h_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
			cfg.ui.set_list_height = 200;
			ConfigManager::save();
		}
		if (h_hovered) {
			ImGui::SetTooltip("Drag to resize set list (Double-click to reset to 200px)");
		}

		ImDrawList* w_dl = ImGui::GetWindowDrawList();
		float mid_y = std::round((h_bar_bb.Min.y + h_bar_bb.Max.y) * 0.5f);
		ImU32 line_col = h_held		 ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
						 : h_hovered ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
									 : ImGui::GetColorU32(ImGuiCol_Separator);
		w_dl->AddLine(ImVec2(h_bar_bb.Min.x, mid_y), ImVec2(h_bar_bb.Max.x, mid_y), line_col, h_held ? 2.0f : 1.0f);
		float mid_x = std::round((h_bar_bb.Min.x + h_bar_bb.Max.x) * 0.5f);
		ImU32 handle_col = h_held	   ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
						   : h_hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
									   : ImGui::GetColorU32(ImGuiCol_TextDisabled);
		w_dl->AddRectFilled(ImVec2(mid_x - 18.0f, mid_y - 2.5f), ImVec2(mid_x + 18.0f, mid_y + 2.5f), handle_col, 2.5f);

		ImGui::Spacing();
		if (button_with_icon("New", IconManager::get(IconID::Add), ImVec2(80, 25))) {
			UIModals::open_create_set(false);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create a new empty material set.");
		}

		ImGui::SameLine();
		if (button_with_icon("Copy", IconManager::get(IconID::Copy), ImVec2(80, 25))) {
			UIModals::open_create_set(true);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create a duplicate copy of the current set.");
		}

		if (is_logged_in) {
			ImGui::SameLine();
			if (button_with_icon("Fork", IconManager::get(IconID::Branch), ImVec2(80, 25))) {
				UIModals::open_fork_set();
				std::string def_name = current_set + "_fork";
				std::strncpy(save_as_buf, def_name.c_str(), sizeof(save_as_buf));
				save_as_buf[sizeof(save_as_buf) - 1] = '\0';
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Fork this set into a new set that you can modify and publish separately.");
			}
		}

		ImGui::SameLine();
		if (SetManager::get_sets().size() > 1) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			if (button_with_icon("Delete", IconManager::get(IconID::Clear), ImVec2(80, 25))) {
				UIModals::open_delete_set();
			}
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete the current set.");
			}
		} else {
			ImGui::BeginDisabled();
			button_with_icon("Delete", IconManager::get(IconID::Clear), ImVec2(80, 25));
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("Cannot delete the only set.");
			}
			ImGui::EndDisabled();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Set Settings:");

		char set_rename_buf[64];
		strncpy(set_rename_buf, current_set.c_str(), sizeof(set_rename_buf));
		set_rename_buf[sizeof(set_rename_buf) - 1] = '\0';
		if (ImGui::InputText("Name##set_rename_input", set_rename_buf, sizeof(set_rename_buf))) {
			std::string new_name = set_rename_buf;
			new_name = sanitize_name(new_name);
			if (!new_name.empty() && new_name != current_set) {
				if (SetManager::rename_set(current_set, new_name)) {
					unsaved_changes = false;
				}
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Name of this set.");
		}

		char s_author[128];
		strncpy(s_author, meta.author.c_str(), sizeof(s_author));
		s_author[sizeof(s_author) - 1] = '\0';
		if (ImGui::InputText("Author", s_author, sizeof(s_author))) {
			meta.author = s_author;
			SetManager::update_current_metadata(meta);
			unsaved_changes = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Name of the author of this set.");
		}

		auto filterNewline = [](ImGuiInputTextCallbackData* data) -> int {
			if (data->EventChar == '\r' || data->EventChar == '\n') {
				return true;
			}
			return false;
		};

		char s_desc[512];
		strncpy(s_desc, meta.description.c_str(), sizeof(s_desc));
		s_desc[sizeof(s_desc) - 1] = '\0';
		ImGui::Text("Description");
		if (ImGui::InputTextMultiline("##set_desc", s_desc, sizeof(s_desc), ImVec2(-1.0f, 150.0f),
									  ImGuiInputTextFlags_WordWrap | ImGuiInputTextFlags_CallbackCharFilter,
									  filterNewline)) {
			meta.description = s_desc;
			SetManager::update_current_metadata(meta);
			unsaved_changes = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Description of this set.");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		ImGui::Text("Cell Attributes (%zu / 3):", meta.attributes.size());
		if (meta.attributes.empty()) {
			ImGui::TextDisabled(
				"No attributes defined. Attributes enable continuous physics (density, temperature, life).");
		} else {
			for (size_t a_idx = 0; a_idx < meta.attributes.size(); ++a_idx) {
				ImGui::PushID(static_cast<int>(a_idx + 5000));
				auto& attr = meta.attributes[a_idx];
				ImGui::Text("Slot %zu:", a_idx);
				ImGui::SameLine();

				char a_name[32];
				std::strncpy(a_name, attr.name.c_str(), sizeof(a_name));
				a_name[sizeof(a_name) - 1] = '\0';
				ImGui::SetNextItemWidth(90.0f);
				if (ImGui::InputText("##attr_name", a_name, sizeof(a_name))) {
					attr.name = a_name;
					SetManager::update_current_metadata(meta);
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Attribute name (e.g. density, temperature, life)");

				ImGui::SameLine();
				const char* type_names[] = {"Int", "Float"};
				int type_idx = (attr.type == AttributeType::Float) ? 1 : 0;
				ImGui::SetNextItemWidth(65.0f);
				if (ImGui::Combo("##attr_type", &type_idx, type_names, IM_ARRAYSIZE(type_names))) {
					attr.type = (type_idx == 1) ? AttributeType::Float : AttributeType::Int;
					SetManager::update_current_metadata(meta);
					unsaved_changes = true;
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Attribute data type: Integer or Floating-point");

				ImGui::SameLine();
				ImGui::Text("Min:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(65.0f);
				if (attr.type == AttributeType::Float) {
					auto f_min = static_cast<float>(attr.min_val);
					if (ImGui::InputFloat("##attr_min", &f_min, 0.0f, 0.0f, "%.1f")) {
						attr.min_val = f_min;
						SetManager::update_current_metadata(meta);
						unsaved_changes = true;
					}
				} else {
					double d_min = attr.min_val;
					if (ImGui::InputDouble("##attr_min", &d_min, 0.0, 0.0, "%.0f")) {
						attr.min_val = d_min;
						SetManager::update_current_metadata(meta);
						unsaved_changes = true;
					}
				}

				ImGui::SameLine();
				ImGui::Text("Max:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(65.0f);
				if (attr.type == AttributeType::Float) {
					auto f_max = static_cast<float>(attr.max_val);
					if (ImGui::InputFloat("##attr_max", &f_max, 0.0f, 0.0f, "%.1f")) {
						attr.max_val = f_max;
						SetManager::update_current_metadata(meta);
						unsaved_changes = true;
					}
				} else {
					double d_max = attr.max_val;
					if (ImGui::InputDouble("##attr_max", &d_max, 0.0, 0.0, "%.0f")) {
						attr.max_val = d_max;
						SetManager::update_current_metadata(meta);
						unsaved_changes = true;
					}
				}

				ImGui::SameLine();
				ImGui::Text("Def:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(65.0f);
				if (attr.type == AttributeType::Float) {
					auto f_def = static_cast<float>(attr.default_val);
					if (ImGui::InputFloat("##attr_def", &f_def, 0.0f, 0.0f, "%.1f")) {
						attr.default_val = f_def;
						SetManager::update_current_metadata(meta);
						unsaved_changes = true;
					}
				} else {
					double d_def = attr.default_val;
					if (ImGui::InputDouble("##attr_def", &d_def, 0.0, 0.0, "%.0f")) {
						attr.default_val = d_def;
						SetManager::update_current_metadata(meta);
						unsaved_changes = true;
					}
				}

				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
				if (button_with_icon("##DelAttr", IconManager::get(IconID::Cross), ImVec2(22.0f, 22.0f))) {
					meta.attributes.erase(meta.attributes.begin() + a_idx);
					SetManager::update_current_metadata(meta);
					unsaved_changes = true;
					ImGui::PopStyleColor();
					ImGui::PopID();
					break;
				}
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Delete this attribute");

				ImGui::PopID();
			}
		}

		if (meta.attributes.size() < 3) {
			if (button_with_icon("Add Attribute", IconManager::get(IconID::Add), ImVec2(120.0f, 24.0f))) {
				AttributeDefinition new_attr;
				new_attr.name = "attr" + std::to_string(meta.attributes.size());
				new_attr.min_val = 0;
				new_attr.max_val = 255;
				new_attr.default_val = 0;
				meta.attributes.push_back(new_attr);
				SetManager::update_current_metadata(meta);
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Add a new cell attribute to this set (max 3)");
		}

		if (!meta.attributes.empty()) {
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::Text("Heatmap Gradient Palette:");
			ImGui::SameLine();
			float reset_btn_sz = ImGui::GetFrameHeight();
			float avail_w = ImGui::GetContentRegionAvail().x;
			if (avail_w > reset_btn_sz) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_w - reset_btn_sz);
			}
			auto hm_min = static_cast<float>(meta.attributes[0].min_val);
			auto hm_max = static_cast<float>(meta.attributes[0].max_val);
			if (button_with_icon("##reset_heatmap_grad", IconManager::get(IconID::ColorWheel),
								 ImVec2(reset_btn_sz, reset_btn_sz))) {
				meta.heatmap_gradient = ColorGradient::default_heatmap(hm_min, hm_max);
				SetManager::update_current_metadata(meta);
				Vulkan::update_rules();
				Grid::draw();
				Grid::refresh_gpu_display();
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Reset to default heatmap colors");
			}

			if (gradient_editor("##cfg_heatmap_grad", meta.heatmap_gradient, hm_min, hm_max)) {
				SetManager::update_current_metadata(meta);
				Vulkan::update_rules();
				Grid::draw();
				Grid::refresh_gpu_display();
				unsaved_changes = true;
			}
		}

		ImGui::Spacing();
		if (button_with_icon("Save Current Set", IconManager::get(IconID::Save), ImVec2(-1, 30))) {
			SetManager::update_current_metadata(meta);
			MaterialManager::save_all_materials(SETS_DIRECTORY + current_set);
			unsaved_changes = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Saves all materials and set configuration.");
		}

		ImGui::EndTabItem();
	}
}

void UI::render_save_load() {
	const std::string& current_set = SetManager::get_current_set();
	Config& cfg = ConfigManager::get_config();
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::BeginTabItem("Saves and Stamps")) {
		ImGui::Spacing();

		if (ImGui::BeginTabBar("SavesSubTabBar")) {
			if (ImGui::BeginTabItem("Saves")) {
				ImGui::Spacing();
				ImGui::Text("Save Current Simulation:");
				ImGui::InputText("Save Name##save_name", save_file_name_buf, sizeof(save_file_name_buf));
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Input a name for the save file.");
				}

				const char* comp_quality_items[] = {"Low (Fast, uncompressed attributes)", "Medium (Balanced)",
													"High (Best, compresses attributes)"};
				int comp_idx = std::clamp(static_cast<int>(cfg.compression_quality), 0, 2);
				if (ImGui::Combo("Compression##save_comp", &comp_idx, comp_quality_items,
								 IM_ARRAYSIZE(comp_quality_items))) {
					cfg.compression_quality = comp_idx;
					ConfigManager::save();
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Compression quality for saves. High compression compresses attribute values "
									  "too, ideal for online sharing.");
				}

				static size_t cached_save_orig_size = 0;
				static size_t cached_save_final_size = 0;
				static double last_save_est_time = -2.0;
				static int last_save_comp_quality = -1;
				static bool last_save_was_focused = false;

				bool ui_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
				double cur_time = ImGui::GetTime();
				if (ui_focused) {
					bool comp_changed = (comp_idx != last_save_comp_quality);
					bool just_focused = (!last_save_was_focused);
					bool timer_expired = (cur_time - last_save_est_time) > 2.0;
					if (cached_save_orig_size == 0 || comp_changed || just_focused || timer_expired) {
						SaveManager::estimate_save_size(Grid::get_width(), Grid::get_height(),
														static_cast<CompressionQuality>(comp_idx), cached_save_orig_size,
														cached_save_final_size);
						last_save_est_time = cur_time;
						last_save_comp_quality = comp_idx;
					}
				}
				last_save_was_focused = ui_focused;

				float save_reduction = cached_save_orig_size > 0
										   ? (100.0f * (1.0f - static_cast<float>(cached_save_final_size) /
																   static_cast<float>(cached_save_orig_size)))
										   : 0.0f;
				ImGui::TextDisabled("Estimated Size: %s -> %s (-%.1f%%)", format_bytes(cached_save_orig_size).c_str(),
									format_bytes(cached_save_final_size).c_str(), save_reduction);

				if (button_with_icon("Save Simulation", IconManager::get(IconID::Save), ImVec2(-1, 30))) {
					std::string s_name = save_file_name_buf;
					if (!s_name.empty()) {
						SaveManager::save_to_file_async(s_name, current_set,
														static_cast<CompressionQuality>(cfg.compression_quality),
														[](bool success) {
															if (success) {
																save_file_name_buf[0] = '\0';
															}
														});
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Saves grid state to sets/<set>/saves/*.save");
				}

				ImGui::Separator();
				ImGui::Text("Available Saves in sets/%s/saves/:", current_set.c_str());

				auto save_files = SaveManager::get_save_files(current_set);

				float list_h = static_cast<float>(cfg.ui.save_list_height > 0 ? cfg.ui.save_list_height : 180);
				ImGui::BeginChild("SavesListScroll", ImVec2(0, list_h), true);
				float avail_w = ImGui::GetContentRegionAvail().x;
				const float btn_sz = ImGui::GetTextLineHeightWithSpacing();
				const float btn_gap = 4.0f;

				bool is_logged_in = WorkshopClient::is_logged_in();
				std::string cur_user = is_logged_in ? WorkshopClient::get_logged_in_username() : "";

				for (uint32_t i = 0; i < static_cast<uint32_t>(save_files.size()); ++i) {
					ImGui::PushID(static_cast<int>(i));
					bool is_selected = (selected_save_id == static_cast<int>(i));
					const auto& sfile = save_files[i];
					std::string save_full_path = SaveManager::get_saves_directory(current_set) + sfile.filename;

					bool is_online = sfile.is_online;
					bool is_transient = WorkshopCache::is_transient(save_full_path);
					bool is_owner = is_logged_in && !sfile.author.empty() && (sfile.author == cur_user);

					std::string tag;
					if (is_online) {
						if (!sfile.workshop_id.empty()) {
							UIWorkshop::check_item_existence(sfile.workshop_id, "save", sfile.filename, current_set,
															 false);
						}
						tag = is_transient ? " [Cached]" : " [Online]";
					} else if (sfile.dimensions_differ) {
						tag = " [Diff Size]";
					}

					std::string label = fmt::format("{} ({}x{}){}", sfile.name, sfile.width, sfile.height, tag);

					std::vector<std::string> forked_sets = get_forked_sets_for(current_set);
					bool show_share = !is_online && is_logged_in;
					bool show_update = is_online && is_owner;
					bool show_download = is_online && is_transient;
					bool show_migrate = !forked_sets.empty();

					int num_buttons =
						(show_share ? 1 : 0) + (show_update ? 1 : 0) + (show_download ? 1 : 0) + (show_migrate ? 1 : 0);
					float total_btn_w = (num_buttons > 0) ? (num_buttons * btn_sz + num_buttons * btn_gap) : 0.0f;
					float selectable_w = std::max(20.0f, avail_w - total_btn_w);

					if (ImGui::Selectable(label.c_str(), is_selected, 0, ImVec2(selectable_w, 0.0f))) {
						selected_save_id = static_cast<int>(i);
					}
					if (is_selected && ImGui::IsMouseDoubleClicked(0)) {
						if (sfile.dimensions_differ) {
							UIModals::open_diff_size_save(sfile);
						} else {
							if (unsaved_changes) {
								UIModals::open_unsaved_switch("", sfile.filename);
							} else {
								SaveManager::load_from_file_async(sfile.filename, current_set, LoadPlacement::Center,
																  [](bool success, const std::string&) {
																	  if (success) {
																		  selected_id = 0;
																		  unsaved_changes = false;
																	  }
																  });
							}
						}
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Double click to load.");
					}

					const ImVec2 sel_min = ImGui::GetItemRectMin();
					const float sel_h = ImGui::GetItemRectSize().y;

					if (show_share) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##share_save", IconManager::get(IconID::Transmit),
												 ImVec2(sel_h, sel_h))) {
							UIModals::open_workshop_share_save(sfile.filename, sfile.name, current_set, sfile.width,
															   sfile.height);
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Share save '%s' to Community Workshop", sfile.name.c_str());
						}
					}

					if (show_update) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##update_save", IconManager::get(IconID::Update),
												 ImVec2(sel_h, sel_h))) {
							if (!sfile.workshop_id.empty()) {
								std::string wid = sfile.workshop_id;
								std::string wfn = sfile.filename;
								std::string wset = current_set;
								std::string wname = sfile.name;
								uint32_t wver = sfile.version;
								std::string wauth = sfile.author;
								WorkshopClient::check_item_exists(wid, [wset, wfn, wname, wid, wver,
																		wauth](bool exists, int http_status) {
									if (!exists && http_status == 404) {
										SaveManager::clear_save_workshop_info(wfn, wset);
										ToastManager::warning(fmt::format(
											"Save '{}' was deleted from the workshop. Converted to local.", wname));
									} else {
										WorkshopItemClient it;
										it.id = wid;
										it.title = wname;
										it.type = "save";
										it.version = wver;
										it.author = wauth;
										WorkshopItemEditor::open_update_modal(it, []() { refresh_workshop_items(); });
									}
								});
							} else {
								UIModals::open_workshop_share_save(sfile.filename, sfile.name, current_set, sfile.width,
																   sfile.height);
							}
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Update save '%s' to Community Workshop", sfile.name.c_str());
						}
					}

					if (show_download) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##download_save", IconManager::get(IconID::Save),
												 ImVec2(sel_h, sel_h))) {
							std::string wid = sfile.workshop_id;
							std::string wfn = sfile.filename;
							std::string wset = current_set;
							std::string wname = sfile.name;
							WorkshopClient::check_item_exists(wid, [wset, wfn, wname, wid, sfile](bool exists,
																								  int http_status) {
								if (!exists && http_status == 404) {
									SaveManager::clear_save_workshop_info(wfn, wset);
									ToastManager::warning(fmt::format(
										"Save '{}' was deleted from the workshop. Converted to local.", wname));
								} else {
									bool ok =
										WorkshopCache::promote_to_local(sfile.workshop_id, "save", sfile.name, wset);
									if (ok) {
										SaveManager::set_save_workshop_info(sfile.name, wset, sfile.workshop_id, "",
																			sfile.author, sfile.version);
										ToastManager::success("Saved save to local storage!");
									}
								}
							});
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Download '%s' to local storage", sfile.name.c_str());
						}
					}

					if (show_migrate) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##migrate_save", IconManager::get(IconID::Branch),
												 ImVec2(sel_h, sel_h))) {
							UIModals::open_migrate(true, sfile.filename, sfile.name, current_set, forked_sets);
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Migrate save '%s' to a forked version of '%s'", sfile.name.c_str(),
											  current_set.c_str());
						}
					}

					ImGui::PopID();
				}
				ImGui::EndChild();

				float bar_h = 8.0f;
				ImVec2 cur_pos = ImGui::GetCursorScreenPos();
				ImRect h_bar_bb(cur_pos, ImVec2(cur_pos.x + avail_w, cur_pos.y + bar_h));
				ImGui::InvisibleButton("##SaveListResizeBar", ImVec2(-1, bar_h));
				bool h_hovered = ImGui::IsItemHovered();
				bool h_held = ImGui::IsItemActive();
				if (h_hovered || h_held) {
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				}
				if (h_held && io.MouseDelta.y != 0.0f) {
					cfg.ui.save_list_height =
						std::clamp(cfg.ui.save_list_height + static_cast<int>(io.MouseDelta.y), 70, 600);
				}
				if (ImGui::IsItemDeactivated()) {
					ConfigManager::save();
				}
				if (h_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					cfg.ui.save_list_height = 180;
					ConfigManager::save();
				}
				if (h_hovered) {
					ImGui::SetTooltip("Drag to resize save list (Double-click to reset to 180px)");
				}

				ImDrawList* w_dl = ImGui::GetWindowDrawList();
				float mid_y = std::round((h_bar_bb.Min.y + h_bar_bb.Max.y) * 0.5f);
				ImU32 line_col = h_held		 ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
								 : h_hovered ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
											 : ImGui::GetColorU32(ImGuiCol_Separator);
				w_dl->AddLine(ImVec2(h_bar_bb.Min.x, mid_y), ImVec2(h_bar_bb.Max.x, mid_y), line_col,
							  h_held ? 2.0f : 1.0f);
				float mid_x = std::round((h_bar_bb.Min.x + h_bar_bb.Max.x) * 0.5f);
				ImU32 handle_col = h_held	   ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
								   : h_hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
											   : ImGui::GetColorU32(ImGuiCol_TextDisabled);
				w_dl->AddRectFilled(ImVec2(mid_x - 18.0f, mid_y - 2.5f), ImVec2(mid_x + 18.0f, mid_y + 2.5f),
									handle_col, 2.5f);

				ImGui::Spacing();
				if (selected_save_id >= 0 && selected_save_id < static_cast<int>(save_files.size())) {
					if (button_with_icon("Load", IconManager::get(IconID::Folder), ImVec2(80, 25))) {
						if (save_files[selected_save_id].dimensions_differ) {
							UIModals::open_diff_size_save(save_files[selected_save_id]);
						} else {
							if (unsaved_changes) {
								UIModals::open_unsaved_switch("", save_files[selected_save_id].filename);
							} else {
								SaveManager::load_from_file_async(save_files[selected_save_id].filename, current_set,
																  LoadPlacement::Center,
																  [](bool success, const std::string&) {
																	  if (success) {
																		  selected_id = 0;
																		  unsaved_changes = false;
																	  }
																  });
							}
						}
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Load the selected save file.");
					}

					ImGui::SameLine();
					if (button_with_icon("Copy", IconManager::get(IconID::Copy), ImVec2(80, 25))) {
						std::string new_name = save_files[selected_save_id].name + "_copy";
						SaveManager::duplicate_save_file(save_files[selected_save_id].filename, new_name, current_set);
						selected_save_id = -1;
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Duplicate the selected save file.");
					}

					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
					if (button_with_icon("Delete", IconManager::get(IconID::Clear), ImVec2(80, 25))) {
						SaveManager::delete_save_file(save_files[selected_save_id].filename, current_set);
						selected_save_id = -1;
					}
					ImGui::PopStyleColor(3);
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Delete the selected save file.");
					}
				} else {
					ImGui::TextDisabled("Select a save file to load/copy/delete");
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Stamps")) {
				ImGui::Spacing();
				bool has_selection = (selection_state == SelectionState::Selected);
				if (has_selection) {
					ImGui::Text("Save Active Selection as Stamp Prefab:");
					static char stamp_tab_name[64] = "";
					ImGui::InputText("Stamp Name##tab_stamp", stamp_tab_name, sizeof(stamp_tab_name));

					const char* comp_quality_items[] = {"Low (Fast, uncompressed attributes)", "Medium (Balanced)",
														"High (Best, compresses attributes)"};
					int stamp_comp_idx = std::clamp(static_cast<int>(cfg.compression_quality), 0, 2);
					if (ImGui::Combo("Compression##stamp_comp", &stamp_comp_idx, comp_quality_items,
									 IM_ARRAYSIZE(comp_quality_items))) {
						cfg.compression_quality = stamp_comp_idx;
						ConfigManager::save();
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Compression quality for stamps. High compression compresses attribute "
										  "values too, ideal for online sharing.");
					}

					static size_t cached_stamp_orig_size = 0;
					static size_t cached_stamp_final_size = 0;
					static double last_stamp_est_time = -2.0;
					static int last_stamp_comp_quality = -1;
					static int last_sel_w = 0;
					static int last_sel_h = 0;

					int sw = selection_box.width();
					int sh = selection_box.height();
					int sx = selection_box.min_x();
					int sy = selection_box.min_y();

					static bool last_stamp_was_focused = false;
					bool stamp_ui_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
					double cur_stamp_time = ImGui::GetTime();
					if (stamp_ui_focused) {
						bool stamp_comp_changed = (stamp_comp_idx != last_stamp_comp_quality);
						bool stamp_just_focused = (!last_stamp_was_focused);
						bool stamp_dim_changed = (sw != last_sel_w || sh != last_sel_h);
						bool stamp_timer_expired = (cur_stamp_time - last_stamp_est_time) > 2.0;
						if (cached_stamp_orig_size == 0 || stamp_comp_changed || stamp_just_focused ||
							stamp_dim_changed || stamp_timer_expired) {
							if (Grid::get_processing_mode() == ProcessingMode::GPU) {
								Grid::sync_from_gpu();
							}
							std::vector<Cell> est_cells(sw * sh);
							for (int y = 0; y < sh; ++y) {
								for (int x = 0; x < sw; ++x) {
									est_cells[y * sw + x] = Grid::get_cell_full_const(sx + x, sy + y);
								}
							}
							SaveManager::estimate_stamp_size(est_cells, sw, sh,
															 static_cast<CompressionQuality>(stamp_comp_idx),
															 cached_stamp_orig_size, cached_stamp_final_size);
							last_stamp_est_time = cur_stamp_time;
							last_stamp_comp_quality = stamp_comp_idx;
							last_sel_w = sw;
							last_sel_h = sh;
						}
					}
					last_stamp_was_focused = stamp_ui_focused;

					float stamp_reduction = cached_stamp_orig_size > 0
												? (100.0f * (1.0f - static_cast<float>(cached_stamp_final_size) /
																		static_cast<float>(cached_stamp_orig_size)))
												: 0.0f;
					ImGui::TextDisabled("Estimated Size: %s -> %s (-%.1f%%)",
										format_bytes(cached_stamp_orig_size).c_str(),
										format_bytes(cached_stamp_final_size).c_str(), stamp_reduction);

					if (button_with_icon("Save Selection as Stamp", IconManager::get(IconID::Save), ImVec2(-1, 28))) {
						std::string sname = stamp_tab_name;
						if (!sname.empty()) {
							if (Grid::get_processing_mode() == ProcessingMode::GPU) {
								Grid::sync_from_gpu();
							}
							std::vector<Cell> stamp_cells(sw * sh);
							for (int y = 0; y < sh; ++y) {
								for (int x = 0; x < sw; ++x) {
									stamp_cells[y * sw + x] = Grid::get_cell_full_const(sx + x, sy + y);
								}
							}
							SaveManager::save_stamp_to_file_async(
								sname, current_set, stamp_cells, sw, sh,
								static_cast<CompressionQuality>(cfg.compression_quality), [](bool success) {
									if (success) {
										stamp_tab_name[0] = '\0';
									}
								});
						}
					}
					ImGui::Separator();
				}

				ImGui::Text("Available Stamps in sets/%s/saves/:", current_set.c_str());
				auto stamp_files = SaveManager::get_stamp_files(current_set);
				float list_h = static_cast<float>(cfg.ui.stamp_list_height > 0 ? cfg.ui.stamp_list_height : 180);
				ImGui::BeginChild("StampsListScroll", ImVec2(0, list_h), true);
				float avail_w = ImGui::GetContentRegionAvail().x;
				const float btn_sz = ImGui::GetTextLineHeightWithSpacing();
				const float btn_gap = 4.0f;

				bool is_logged_in = WorkshopClient::is_logged_in();
				std::string cur_user = is_logged_in ? WorkshopClient::get_logged_in_username() : "";

				for (uint32_t i = 0; i < static_cast<uint32_t>(stamp_files.size()); ++i) {
					ImGui::PushID(static_cast<int>(i));
					bool is_selected = (selected_stamp_id == static_cast<int>(i));
					const auto& stfile = stamp_files[i];
					std::string stamp_full_path = SaveManager::get_saves_directory(current_set) + stfile.filename;

					bool is_online = stfile.is_online;
					bool is_transient = WorkshopCache::is_transient(stamp_full_path);
					bool is_owner = is_logged_in && !stfile.author.empty() && (stfile.author == cur_user);

					std::string tag;
					if (is_online) {
						if (!stfile.workshop_id.empty()) {
							UIWorkshop::check_item_existence(stfile.workshop_id, "stamp", stfile.filename, current_set,
															 false);
						}
						tag = is_transient ? " [Cached]" : " [Online]";
					}

					std::string label = fmt::format("{} ({}x{}){}", stfile.name, stfile.width, stfile.height, tag);

					std::vector<std::string> forked_sets = get_forked_sets_for(current_set);
					bool show_share = !is_online && is_logged_in;
					bool show_update = is_online && is_owner;
					bool show_download = is_online && is_transient;
					bool show_migrate = !forked_sets.empty();

					int num_buttons =
						(show_share ? 1 : 0) + (show_update ? 1 : 0) + (show_download ? 1 : 0) + (show_migrate ? 1 : 0);
					float total_btn_w = (num_buttons > 0) ? (num_buttons * btn_sz + num_buttons * btn_gap) : 0.0f;
					float selectable_w = std::max(20.0f, avail_w - total_btn_w);

					if (ImGui::Selectable(label.c_str(), is_selected, 0, ImVec2(selectable_w, 0.0f))) {
						selected_stamp_id = static_cast<int>(i);
					}
					if (is_selected && ImGui::IsMouseDoubleClicked(0)) {
						SaveManager::load_stamp_from_file_async(
							stfile.filename, current_set,
							[](bool success, const std::vector<Cell>& cells, uint32_t sw, uint32_t sh) {
								if (success) {
									load_stamp(cells, sw, sh);
								}
							});
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Double click to load.");
					}

					const ImVec2 sel_min = ImGui::GetItemRectMin();
					const float sel_h = ImGui::GetItemRectSize().y;

					if (show_share) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##share_stamp", IconManager::get(IconID::Transmit),
												 ImVec2(sel_h, sel_h))) {
							UIModals::open_workshop_share_stamp(stfile.filename, stfile.name, current_set, stfile.width,
																stfile.height);
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Share stamp '%s' to Community Workshop", stfile.name.c_str());
						}
					}

					if (show_update) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##update_stamp", IconManager::get(IconID::Update),
												 ImVec2(sel_h, sel_h))) {
							if (!stfile.workshop_id.empty()) {
								std::string wid = stfile.workshop_id;
								std::string wfn = stfile.filename;
								std::string wset = current_set;
								std::string wname = stfile.name;
								uint32_t wver = stfile.version;
								std::string wauth = stfile.author;
								WorkshopClient::check_item_exists(wid, [wset, wfn, wname, wid, wver,
																		wauth](bool exists, int http_status) {
									if (!exists && http_status == 404) {
										SaveManager::clear_stamp_workshop_info(wfn, wset);
										ToastManager::warning(fmt::format(
											"Stamp '{}' was deleted from the workshop. Converted to local.", wname));
									} else {
										WorkshopItemClient it;
										it.id = wid;
										it.title = wname;
										it.type = "stamp";
										it.version = wver;
										it.author = wauth;
										WorkshopItemEditor::open_update_modal(it, []() { refresh_workshop_items(); });
									}
								});
							} else {
								UIModals::open_workshop_share_stamp(stfile.filename, stfile.name, current_set,
																	stfile.width, stfile.height);
							}
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Update stamp '%s' to Community Workshop", stfile.name.c_str());
						}
					}

					if (show_download) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##download_stamp", IconManager::get(IconID::Save),
												 ImVec2(sel_h, sel_h))) {
							std::string wid = stfile.workshop_id;
							std::string wfn = stfile.filename;
							std::string wset = current_set;
							std::string wname = stfile.name;
							WorkshopClient::check_item_exists(wid, [wset, wfn, wname, wid, stfile](bool exists,
																								   int http_status) {
								if (!exists && http_status == 404) {
									SaveManager::clear_stamp_workshop_info(wfn, wset);
									ToastManager::warning(fmt::format(
										"Stamp '{}' was deleted from the workshop. Converted to local.", wname));
								} else {
									bool ok =
										WorkshopCache::promote_to_local(stfile.workshop_id, "stamp", stfile.name, wset);
									if (ok) {
										SaveManager::set_stamp_workshop_info(stfile.name, wset, stfile.workshop_id, "",
																			 stfile.author, stfile.version);
										ToastManager::success("Saved stamp to local storage!");
									}
								}
							});
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Download '%s' to local storage", stfile.name.c_str());
						}
					}

					if (show_migrate) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, sel_min.y));
						if (UI::button_with_icon("##migrate_stamp", IconManager::get(IconID::Branch),
												 ImVec2(sel_h, sel_h))) {
							UIModals::open_migrate(false, stfile.filename, stfile.name, current_set, forked_sets);
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Migrate stamp '%s' to a forked version of '%s'", stfile.name.c_str(),
											  current_set.c_str());
						}
					}

					ImGui::PopID();
				}
				ImGui::EndChild();

				float bar_h = 8.0f;
				ImVec2 cur_pos = ImGui::GetCursorScreenPos();
				ImRect h_bar_bb(cur_pos, ImVec2(cur_pos.x + avail_w, cur_pos.y + bar_h));
				ImGui::InvisibleButton("##StampListResizeBar", ImVec2(-1, bar_h));
				bool h_hovered = ImGui::IsItemHovered();
				bool h_held = ImGui::IsItemActive();
				if (h_hovered || h_held) {
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				}
				if (h_held && io.MouseDelta.y != 0.0f) {
					cfg.ui.stamp_list_height =
						std::clamp(cfg.ui.stamp_list_height + static_cast<int>(io.MouseDelta.y), 70, 600);
				}
				if (ImGui::IsItemDeactivated()) {
					ConfigManager::save();
				}
				if (h_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					cfg.ui.stamp_list_height = 180;
					ConfigManager::save();
				}
				if (h_hovered) {
					ImGui::SetTooltip("Drag to resize stamp list (Double-click to reset to 180px)");
				}

				ImDrawList* w_dl = ImGui::GetWindowDrawList();
				float mid_y = std::round((h_bar_bb.Min.y + h_bar_bb.Max.y) * 0.5f);
				ImU32 line_col = h_held		 ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
								 : h_hovered ? ImGui::GetColorU32(ImGuiCol_SeparatorActive)
											 : ImGui::GetColorU32(ImGuiCol_Separator);
				w_dl->AddLine(ImVec2(h_bar_bb.Min.x, mid_y), ImVec2(h_bar_bb.Max.x, mid_y), line_col,
							  h_held ? 2.0f : 1.0f);
				float mid_x = std::round((h_bar_bb.Min.x + h_bar_bb.Max.x) * 0.5f);
				ImU32 handle_col = h_held	   ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
								   : h_hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
											   : ImGui::GetColorU32(ImGuiCol_TextDisabled);
				w_dl->AddRectFilled(ImVec2(mid_x - 18.0f, mid_y - 2.5f), ImVec2(mid_x + 18.0f, mid_y + 2.5f),
									handle_col, 2.5f);

				ImGui::Spacing();
				if (selected_stamp_id >= 0 && selected_stamp_id < static_cast<int>(stamp_files.size())) {
					if (button_with_icon("Stamp", IconManager::get(IconID::Paste), ImVec2(100, 25))) {
						SaveManager::load_stamp_from_file_async(
							stamp_files[selected_stamp_id].filename, current_set,
							[](bool success, const std::vector<Cell>& cells, uint32_t sw, uint32_t sh) {
								if (success) {
									load_stamp(cells, sw, sh);
								}
							});
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("Load into clipboard and start pasting.");
					}

					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
					if (button_with_icon("Delete##StampDel", IconManager::get(IconID::Clear), ImVec2(80, 25))) {
						SaveManager::delete_stamp_file(stamp_files[selected_stamp_id].filename, current_set);
						selected_stamp_id = -1;
					}
					ImGui::PopStyleColor(3);
				} else {
					ImGui::TextDisabled("Select a stamp to paste or delete");
				}

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::EndTabItem();
	}
}

void UI::refresh_workshop_items() { UIWorkshop::refresh_workshop_items(); }

void UI::remove_workshop_item(const std::string& item_id) { UIWorkshop::remove_workshop_item(item_id); }

void UI::validate_online_items(bool force) { UIWorkshop::validate_online_items(force); }

void UI::handle_uri(const std::string& uri) { UIWorkshop::handle_uri(uri); }

std::string UI::generate_thumbnail_base64(int type_idx, const std::string& target_set, const std::string& target_file) {
	return UIThumbnails::generate_thumbnail_base64(type_idx, target_set, target_file);
}