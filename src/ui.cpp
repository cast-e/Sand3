#include "ui.hpp"

#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

#include "config_manager.hpp"
#include "const.hpp"
#include "grid.hpp"
#include "icon_manager.hpp"
#include "material_manager.hpp"
#include "resources/roboto_ttf.h"
#include "sanitize.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "shortcut_manager.hpp"
#include "toast_manager.hpp"
#include "undo_manager.hpp"
#include "vulkan.hpp"
#include "window.hpp"
#include "workshop_cache.hpp"
#include "workshop_client.hpp"
#include "workshop_item_editor.hpp"
#include "zip_util.hpp"

static ImVec4 default_colors[ImGuiCol_COUNT];
static bool default_colors_initialized = false;

static void init_style() {
	const auto& cfg = ConfigManager::get_config();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = cfg.ui.window_rounding;
	style.ChildRounding = cfg.ui.frame_rounding;
	style.FrameRounding = cfg.ui.frame_rounding;
	style.PopupRounding = cfg.ui.frame_rounding;
	style.GrabRounding = cfg.ui.frame_rounding;
	style.TabRounding = cfg.ui.frame_rounding;

	style.ItemSpacing = ImVec2(8, 6);
	style.ItemInnerSpacing = ImVec2(6, 6);
	style.WindowPadding = ImVec2(12, 12);
	style.FramePadding = ImVec2(8, 5);

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.13f, 0.95f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.10f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.16f, 0.98f);
	colors[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.19f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.23f, 0.26f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.12f, 0.14f, 0.75f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.39f, 0.42f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.48f, 0.49f, 0.52f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.35f, 0.58f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.54f, 0.94f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.64f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.22f, 0.23f, 0.26f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.50f, 0.85f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.18f, 0.19f, 0.21f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.26f, 0.29f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.33f, 0.37f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.28f, 0.29f, 0.32f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.38f, 0.39f, 0.42f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.17f, 0.19f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.54f, 0.94f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.45f, 0.78f, 1.00f);
	colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.20f, 0.50f, 0.85f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.11f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.15f, 0.40f, 0.70f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.60f);

	if (!default_colors_initialized) {
		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			default_colors[i] = colors[i];
		}
		default_colors_initialized = true;
	}

	for (const auto& [name, val] : ConfigManager::get_color_overrides()) {
		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			if (name == ImGui::GetStyleColorName(i)) {
				ImVec4 col;
				if (ConfigManager::parse_color_string(val, col)) {
					colors[i] = col;
				}
				break;
			}
		}
	}
}

const ImVec4* UI::get_default_colors() { return default_colors; }

void UI::reset_theme_colors() {
	ImGuiStyle& style = ImGui::GetStyle();
	for (int i = 0; i < ImGuiCol_COUNT; ++i) {
		style.Colors[i] = default_colors[i];
	}
	auto& cfg = ConfigManager::get_config();
	const auto& def_cfg = ConfigManager::get_default_config();
	cfg.ui.background_color = def_cfg.ui.background_color;
	cfg.ui.selection_box_color = def_cfg.ui.selection_box_color;
	cfg.ui.selection_box_fill = def_cfg.ui.selection_box_fill;
	Window::set_background_color(cfg.ui.background_color);

	ConfigManager::get_color_overrides().clear();
	ConfigManager::save();
}

struct GridRect {
	int x, y, w, h;
};

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
		int steps = static_cast<int>(std::ceil(dist * 2.0f));
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

		// Left and Right edges
		segs.push_back({x0, y, x0, y + 1});
		segs.push_back({x1, y, x1, y + 1});

		// Top edges
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

		// Bottom edges
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
				Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
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
					Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
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
			Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
		}
	}
}

static void flood_fill(uint32_t start_x, uint32_t start_y, uint8_t fill_mat) {
	if (start_x >= Grid::get_width() || start_y >= Grid::get_height())
		return;
	uint8_t target_mat = Grid::get_cell(start_x, start_y);
	if (target_mat == fill_mat)
		return;

	std::vector<std::pair<uint32_t, uint32_t>> queue;
	queue.reserve(4096);
	queue.push_back({start_x, start_y});
	Grid::set_cell(start_x, start_y, fill_mat);

	size_t head = 0;
	while (head < queue.size()) {
		auto [x, y] = queue[head++];

		const int dx[4] = {-1, 1, 0, 0};
		const int dy[4] = {0, 0, -1, 1};

		for (int i = 0; i < 4; ++i) {
			int nx = static_cast<int>(x) + dx[i];
			int ny = static_cast<int>(y) + dy[i];

			if (nx >= 0 && nx < static_cast<int>(Grid::get_width()) && ny >= 0 &&
				ny < static_cast<int>(Grid::get_height())) {
				uint32_t unx = static_cast<uint32_t>(nx);
				uint32_t uny = static_cast<uint32_t>(ny);
				if (Grid::get_cell(unx, uny) == target_mat) {
					Grid::set_cell(unx, uny, fill_mat);
					queue.push_back({unx, uny});
				}
			}
		}
	}
}

char UI::save_file_name_buf[128] = "";
char UI::save_as_buf[64] = "";
char UI::new_set_name_buf[64] = "";

int UI::selected_save_id = -1;
int UI::selected_stamp_id = -1;
bool UI::open_diff_size_save_popup = false;
SaveFileInfo UI::pending_diff_save{};
uint8_t UI::selected_id = 0;
int UI::mouse_size = 5;
BrushShape UI::brush_shape = BrushShape::Square;

bool UI::open_switch_popup = false;
bool UI::open_create_set_popup = false;
bool UI::open_delete_set_popup = false;
bool UI::open_empty_rule_warning_popup = false;

bool UI::duplicate_set_checkbox = false;
bool UI::exit_save_as_new_set = false;

bool UI::update = false;
bool UI::step_frame = false;

bool UI::show_exit_popup = false;

bool UI::unsaved_changes = false;
std::string UI::pending_set_switch = "";
std::string UI::pending_save_load = "";
bool UI::ui_compact = false;

float UI::zoom = 1.0f;
float UI::pan_x = 0.0f;
float UI::pan_y = 0.0f;
float UI::target_zoom = 1.0f;
float UI::target_pan_x = 0.0f;
float UI::target_pan_y = 0.0f;

ToolMode UI::current_tool = ToolMode::Brush;
SelectionState UI::selection_state = SelectionState::None;
ResizeHandle UI::active_resize_handle = ResizeHandle::None;
SelectionBox UI::selection_box = {};
ClipboardData UI::clipboard = {};
std::vector<uint8_t> UI::floating_cells = {};
std::vector<uint8_t> UI::move_initial_cells = {};
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

	init_style();

	if (cfg.ui.ui_scale != 1.0f && cfg.ui.ui_scale >= 0.5f && cfg.ui.ui_scale <= 3.0f) {
		ImGui::GetStyle().ScaleAllSizes(cfg.ui.ui_scale);
	}

	ImGui_ImplSDL3_InitForSDLRenderer(Window::get_window(), Window::get_renderer());
	ImGui_ImplSDLRenderer3_Init(Window::get_renderer());

	UndoManager::init();
	IconManager::init();
	WorkshopCache::init();
	WorkshopClient::init();
}

void UI::shutdown() {
	ConfigManager::save();
	IconManager::shutdown();
	WorkshopClient::shutdown();
	WorkshopCache::purge_transient_cache();
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

		ImGui::SetNextWindowPos(ImVec2(sidebar_x, sidebar_y), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(cfg.ui.sidebar_width), target_h), ImGuiCond_Always);
		ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, target_h), ImVec2(800.0f, target_h));
		if (ImGui::Begin("Sand3 - Simulation Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz = ImGui::GetWindowSize();
			cfg.ui.sidebar_x = static_cast<int>(pos.x);
			cfg.ui.sidebar_y = static_cast<int>(pos.y);
			cfg.ui.sidebar_width = static_cast<int>(sz.x);
			render_header(io);

			if (ImGui::BeginTabBar("SidebarTabs")) {
				render_material_editor();
				render_manage_sets();
				render_save_load();
				render_workshop();
				render_shortcuts();
				render_theme_editor();
				render_advanced_options();
				ImGui::EndTabBar();
			}

			ImGui::End();
		}

		float bar_x = sidebar_x + static_cast<float>(cfg.ui.sidebar_width);
		float hit_w = 10.0f;

		ImGui::SetNextWindowPos(ImVec2(bar_x - hit_w * 0.5f, sidebar_y));
		ImGui::SetNextWindowSize(ImVec2(hit_w, target_h));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		constexpr ImGuiWindowFlags kSplitterFlags =
			ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoTitleBar;
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
				cfg.ui.sidebar_width = 380;
				ConfigManager::save();
			}

			ImDrawList* fg_dl = ImGui::GetForegroundDrawList();
			if (bar_hovered || bar_held) {
				ImU32 glow_col =
					bar_held ? ImGui::GetColorU32(ImGuiCol_ButtonActive) : ImGui::GetColorU32(ImGuiCol_SeparatorActive);
				fg_dl->AddLine(ImVec2(bar_x, sidebar_y), ImVec2(bar_x, sidebar_y + target_h), glow_col,
							   bar_held ? 3.0f : 2.0f);
				float mid_y = sidebar_y + target_h * 0.5f;
				fg_dl->AddRectFilled(ImVec2(bar_x - 3.0f, mid_y - 20.0f), ImVec2(bar_x + 3.0f, mid_y + 20.0f), glow_col,
									 3.0f);
			} else {
				fg_dl->AddLine(ImVec2(bar_x, sidebar_y), ImVec2(bar_x, sidebar_y + target_h),
							   ImGui::GetColorU32(ImGuiCol_Separator), 1.0f);
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	render_mouse_overlay();

	render_modals();

	ImGuiContext& g = *GImGui;
	if (g.HoveredIdIsDisabled) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
	} else if (g.HoveredId != 0 && ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
	}
}

void UI::set_tool_mode(ToolMode mode) {
	if (current_tool == mode)
		return;
	if (current_tool == ToolMode::Select) {
		deselect();
	}
	active_resize_handle = ResizeHandle::None;
	selection_state = SelectionState::None;
	s_line_brush_active = false;
	current_tool = mode;
	UndoManager::push_snapshot("Switched to " + std::string(current_tool == ToolMode::Brush ? "Brush" : "Select"));
}

void UI::deselect() {
	if (selection_state == SelectionState::Moving) {
		for (int y = 0; y < move_initial_height; ++y) {
			for (int x = 0; x < move_initial_width; ++x) {
				int gx = move_origin_x + x;
				int gy = move_origin_y + y;
				if (gx >= 0 && gx < static_cast<int>(Grid::get_width()) && gy >= 0 &&
					gy < static_cast<int>(Grid::get_height())) {
					Grid::set_cell(gx, gy, move_initial_cells[y * move_initial_width + x]);
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
	int min_x = selection_box.min_x();
	int min_y = selection_box.min_y();
	int bw = selection_box.width();
	int bh = selection_box.height();
	clipboard.width = bw;
	clipboard.height = bh;
	clipboard.cells.resize(bw * bh);
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			clipboard.cells[y * bw + x] = Grid::get_cell(min_x + x, min_y + y);
		}
	}
}

void UI::cut_selection() {
	if (selection_state != SelectionState::Selected)
		return;
	copy_selection();
	int min_x = selection_box.min_x();
	int min_y = selection_box.min_y();
	int bw = selection_box.width();
	int bh = selection_box.height();
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			Grid::set_cell(min_x + x, min_y + y, 0);
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
			Grid::set_cell(min_x + x, min_y + y, id);
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

		std::vector<uint8_t> old_cells(w * h);
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				old_cells[y * w + x] = Grid::get_cell(min_x + x, min_y + y);
			}
		}

		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				if (!transparent_mode || old_cells[y * w + x] != 0) {
					Grid::set_cell(min_x + x, min_y + y, 0);
				}
			}
		}

		int new_w = h;
		int new_h = w;
		std::vector<uint8_t> new_cells(new_w * new_h);
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
				uint8_t c = new_cells[ny * new_w + nx];
				if (!transparent_mode || c != 0) {
					Grid::set_cell(new_min_x + nx, new_min_y + ny, c);
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
		std::vector<uint8_t> new_cells(new_w * new_h);
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
		std::vector<uint8_t> new_cells(new_w * new_h);
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

void UI::load_stamp(const std::vector<uint8_t>& cells, uint32_t w, uint32_t h) {
	if (cells.empty() || w == 0 || h == 0)
		return;
	clipboard.cells = cells;
	clipboard.width = static_cast<int>(w);
	clipboard.height = static_cast<int>(h);
	paste_clipboard();
}

void UI::start_moving_selection(int gx, int gy) {
	if (selection_state != SelectionState::Selected)
		return;
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
	for (int y = 0; y < bh; ++y) {
		for (int x = 0; x < bw; ++x) {
			uint8_t c = Grid::get_cell(min_x + x, min_y + y);
			floating_cells[y * bw + x] = c;
			move_initial_cells[y * bw + x] = c;
			if (!transparent_mode || c != 0) {
				Grid::set_cell(min_x + x, min_y + y, 0);
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
				uint8_t cell = floating_cells[y * bw + x];
				if (!transparent_mode || cell != 0) {
					Grid::set_cell(fgx, fgy, cell);
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
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;
	const auto& cfg = ConfigManager::get_config();
	const ImGuiID id = window->GetID(label);
	const char* label_end = ImGui::FindRenderedTextEnd(label);
	const ImVec2 text_size = ImGui::CalcTextSize(label, label_end, false);

	float default_btn_h = (cfg.ui.button_size > 0) ? static_cast<float>(cfg.ui.button_size) : 30.0f;
	float req_h = (size_arg.y > 0.0f) ? size_arg.y : default_btn_h;
	float base_icon_sz = (cfg.ui.icon_size > 0) ? static_cast<float>(cfg.ui.icon_size) : 16.0f;
	float icon_sz = icon ? std::min(base_icon_sz, std::max(8.0f, req_h - 4.0f)) : 0.0f;
	const float spacing = (icon && text_size.x > 0.0f) ? 6.0f : 0.0f;
	const float total_content_w = (icon ? icon_sz : 0.0f) + spacing + text_size.x;
	const float total_content_h = std::max(icon_sz, text_size.y);

	ImVec2 pos = window->DC.CursorPos;
	float req_w = (size_arg.x > 0.0f) ? size_arg.x
									  : (text_size.x > 0.0f ? (total_content_w + style.FramePadding.x * 2.0f) : req_h);
	ImVec2 size(req_w, req_h);

	const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
	ImGui::ItemSize(size, style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, id))
		return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);

	const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive
										 : hovered		   ? ImGuiCol_ButtonHovered
														   : ImGuiCol_Button);
	ImGui::RenderNavCursor(bb, id);
	ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

	float content_start_x = std::round(bb.Min.x + (bb.GetWidth() - total_content_w) * 0.5f);
	if (content_start_x < bb.Min.x + style.FramePadding.x)
		content_start_x = bb.Min.x + style.FramePadding.x;

	if (icon) {
		float icon_x = (text_size.x > 0.0f) ? content_start_x : std::round(bb.Min.x + (bb.GetWidth() - icon_sz) * 0.5f);
		float icon_y = std::round(bb.Min.y + (bb.GetHeight() - icon_sz) * 0.5f);
		ImVec2 icon_min(icon_x, icon_y);
		ImVec2 icon_max(icon_x + icon_sz, icon_y + icon_sz);
		bool is_disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;

		ImU32 icon_tint = is_disabled ? IM_COL32(180, 180, 180, 130) : IM_COL32(255, 255, 255, 255);
		window->DrawList->AddImage((ImTextureID)(intptr_t)icon, icon_min, icon_max, ImVec2(0, 0), ImVec2(1, 1),
								   icon_tint);
	}

	if (text_size.x > 0.0f) {
		float text_x = std::round(content_start_x + (icon ? (icon_sz + spacing) : 0.0f));
		float text_y = std::round(bb.Min.y + (bb.GetHeight() - text_size.y) * 0.5f);
		ImVec2 text_min(text_x, text_y);
		ImVec2 text_max(bb.Max.x - style.FramePadding.x, bb.Max.y);
		ImGui::RenderTextClipped(text_min, text_max, label, label_end, &text_size, ImVec2(0.0f, 0.5f), &bb);
	}

	return pressed;
}

void UI::render_selection_controls() {
	const auto& cfg = ConfigManager::get_config();
	bool is_brush = (current_tool == ToolMode::Brush);
	bool is_select = (current_tool == ToolMode::Select);

	float avail_w = ImGui::GetContentRegionAvail().x;
	float spacing_x = ImGui::GetStyle().ItemSpacing.x;
	float btn = (cfg.ui.button_size > 0) ? static_cast<float>(cfg.ui.button_size) : 28.0f;

	const ImVec4& btn_active_col = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
	ImVec4 btn_active_hover =
		ImVec4(std::min(1.0f, btn_active_col.x * 1.15f + 0.05f), std::min(1.0f, btn_active_col.y * 1.15f + 0.05f),
			   std::min(1.0f, btn_active_col.z * 1.15f + 0.05f), btn_active_col.w);

	if (is_brush) {
		ImGui::PushStyleColor(ImGuiCol_Button, btn_active_col);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_active_hover);
	}
	if (button_with_icon("##BrushTool", IconManager::get(IconID::Brush), ImVec2(btn, btn))) {
		set_tool_mode(ToolMode::Brush);
	}
	if (is_brush) {
		ImGui::PopStyleColor(2);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Drawing brush tool (%s)\nDraw cells on canvas",
						  ShortcutManager::get_key_string(ShortcutAction::ToolBrush).c_str());

	ImGui::SameLine();
	if (is_select) {
		ImGui::PushStyleColor(ImGuiCol_Button, btn_active_col);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btn_active_hover);
	}
	if (button_with_icon("##SelectTool", IconManager::get(IconID::Select), ImVec2(btn, btn))) {
		set_tool_mode(ToolMode::Select);
	}
	if (is_select) {
		ImGui::PopStyleColor(2);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Box selection and transform tool (%s / %s)\nSelect rectangular regions to move, copy, cut, or rotate",
			ShortcutManager::get_key_string(ShortcutAction::ToolSelect).c_str(),
			ShortcutManager::get_key_string(ShortcutAction::Copy).c_str());

	if (current_tool == ToolMode::Brush) {
		ImGui::Spacing();
		ImGui::Text("Brush Settings:");
		ImGui::SliderInt("Brush Size", &mouse_size, 1, static_cast<int>(Grid::get_width() * 0.5f));
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Adjust brush width (Scroll wheel)");
		}

		const char* shape_names[] = {"Square", "Circle"};
		int current_shape = static_cast<int>(brush_shape);
		if (ImGui::Combo("Brush Shape", &current_shape, shape_names, IM_ARRAYSIZE(shape_names))) {
			brush_shape = static_cast<BrushShape>(current_shape);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Select brush shape (Square or Circle, toggle with %s)",
							  ShortcutManager::get_key_string(ShortcutAction::BrushShape).c_str());
		}
	} else if (current_tool == ToolMode::Select) {
		ImGui::Spacing();
		bool has_selection = (selection_state == SelectionState::Selected);
		bool has_clipboard = !clipboard.empty();
		bool can_rotate = (has_selection || selection_state == SelectionState::Moving ||
						   (selection_state == SelectionState::Pasting && !clipboard.empty()));

		float col_w = (avail_w - spacing_x * 3.0f) / 4.0f;
		ImVec2 btn_sz(col_w, btn);

		if (!has_selection)
			ImGui::BeginDisabled();
		if (button_with_icon("##CopySel", IconManager::get(IconID::Copy), btn_sz)) {
			copy_selection();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Copy selection (%s)", ShortcutManager::get_key_string(ShortcutAction::Copy).c_str());

		ImGui::SameLine();
		if (!has_selection)
			ImGui::BeginDisabled();
		if (button_with_icon("##CutSel", IconManager::get(IconID::Cut), btn_sz)) {
			cut_selection();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Cut selected cells to clipboard (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::Cut).c_str());

		ImGui::SameLine();
		if (!has_clipboard)
			ImGui::BeginDisabled();
		if (button_with_icon("##PasteSel", IconManager::get(IconID::Paste), btn_sz)) {
			paste_clipboard();
		}
		if (!has_clipboard)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Paste clipboard at cursor (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::Paste).c_str());

		ImGui::SameLine();
		if (!has_selection)
			ImGui::BeginDisabled();
		if (button_with_icon("##DeleteSel", IconManager::get(IconID::Delete), btn_sz)) {
			fill_selection(0);
			deselect();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Delete selected cells (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::Delete).c_str());

		if (!has_selection)
			ImGui::BeginDisabled();
		if (button_with_icon("##FillSel", IconManager::get(IconID::Fill), btn_sz)) {
			fill_selection(selected_id);
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Fill selected cells with current material (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::Fill).c_str());

		ImGui::SameLine();
		if (!has_selection)
			ImGui::BeginDisabled();
		if (button_with_icon("##DeselectSel", IconManager::get(IconID::Cross), btn_sz)) {
			deselect();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Clear active selection (%s / Right Click)",
							  ShortcutManager::get_key_string(ShortcutAction::CancelOrQuit).c_str());

		ImGui::SameLine();
		if (!can_rotate)
			ImGui::BeginDisabled();
		if (button_with_icon("##RotateCWSel", IconManager::get(IconID::RotateCW), btn_sz)) {
			rotate_selection(true);
		}
		if (!can_rotate)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Rotate selection 90° clockwise (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::RotateCW).c_str());

		ImGui::SameLine();
		if (!can_rotate)
			ImGui::BeginDisabled();
		if (button_with_icon("##RotateCCWSel", IconManager::get(IconID::RotateCCW), btn_sz)) {
			rotate_selection(false);
		}
		if (!can_rotate)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Rotate selection 90° counter-clockwise (%s)",
							  ShortcutManager::get_key_string(ShortcutAction::RotateCCW).c_str());

		ImGui::Spacing();
		ImGui::Checkbox("Transparent", &transparent_mode);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"When enabled, air/empty cells in clipboard or moved selection will not overwrite existing cells.");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Stamp Prefabs:");
		float stamp_col_w = (avail_w - spacing_x) * 0.5f;
		ImVec2 stamp_btn_sz(stamp_col_w, btn);

		bool can_save_stamp = has_selection;
		if (!can_save_stamp)
			ImGui::BeginDisabled();
		if (button_with_icon("Save Stamp##Sel", IconManager::get(IconID::Save), stamp_btn_sz)) {
			ImGui::OpenPopup("SaveStampPopup");
		}
		if (!can_save_stamp)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("Save current selection as a reusable stamp prefab");
		}

		ImGui::SameLine();
		if (button_with_icon("Load Stamp##Sel", IconManager::get(IconID::Folder), stamp_btn_sz)) {
			ImGui::OpenPopup("LoadStampQuickPicker");
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Pick a stamp prefab to paste");
		}

		if (ImGui::BeginPopup("SaveStampPopup")) {
			ImGui::Text("Save Stamp Name:");
			static char stamp_name_buf[64] = "";
			ImGui::InputText("##stamp_name", stamp_name_buf, sizeof(stamp_name_buf));
			if (ImGui::Button("Save##StampBtn", ImVec2(80, 24))) {
				std::string sname = stamp_name_buf;
				if (!sname.empty()) {
					int sx = selection_box.min_x();
					int sy = selection_box.min_y();
					int sw = selection_box.width();
					int sh = selection_box.height();
					std::vector<uint8_t> stamp_cells(sw * sh);
					for (int y = 0; y < sh; ++y) {
						for (int x = 0; x < sw; ++x) {
							stamp_cells[y * sw + x] = Grid::get_cell(sx + x, sy + y);
						}
					}
					SaveManager::save_stamp_to_file_async(sname, SetManager::get_current_set(), stamp_cells, sw, sh,
														  [](bool success) {
															  if (success) {
																  stamp_name_buf[0] = '\0';
															  }
														  });
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel##StampBtn", ImVec2(80, 24))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("LoadStampQuickPicker")) {
			ImGui::Text("Select Stamp Prefab:");
			ImGui::Separator();
			auto stamps = SaveManager::get_stamp_files(SetManager::get_current_set());
			if (stamps.empty()) {
				ImGui::TextDisabled("No stamps found in current set.");
			} else {
				for (const auto& s : stamps) {
					std::string label = fmt::format("{} ({}x{})", s.name, s.width, s.height);
					if (ImGui::Selectable(label.c_str())) {
						SaveManager::load_stamp_from_file_async(
							s.filename, SetManager::get_current_set(),
							[](bool success, const std::vector<uint8_t>& cells, uint32_t sw, uint32_t sh) {
								if (success) {
									load_stamp(cells, sw, sh);
								}
							});
						ImGui::CloseCurrentPopup();
					}
				}
			}
			ImGui::EndPopup();
		}

		ImGui::Spacing();
		if (has_selection) {
			ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Selected: %dx%d (%d cells)", selection_box.width(),
							   selection_box.height(), selection_box.width() * selection_box.height());
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Drag inside: Move\nDrag handles: Resize\nArrows: Nudge\nQ / E: Rotate");
			}
		} else if (selection_state == SelectionState::Pasting) {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Pasting: %dx%d", clipboard.width, clipboard.height);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Click canvas: Stamp\nQ / E: Rotate\nEsc: Cancel");
			}
		}
	}
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
					uint8_t m = floating_cells[y * bw + x];
					if (m == 0 && transparent_mode) {
						x++;
						continue;
					}
					int start_x = x;
					while (x < bw && floating_cells[y * bw + x] == m) {
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
					uint8_t m = clipboard.cells[y * bw + x];
					if (m == 0 && transparent_mode) {
						x++;
						continue;
					}
					int start_x = x;
					while (x < bw && clipboard.cells[y * bw + x] == m) {
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
	} else if (is_shift_down) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
	} else if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
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

	ImU32 fill_color = IM_COL32(r, g, b, 70);
	ImU32 line_color = IM_COL32(cr, cg, cb, 255);
	ImU32 inner_line_color = IM_COL32(r, g, b, 240);

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
		if (selection_state == SelectionState::Moving) {
			deselect();
		} else if (selection_state == SelectionState::Pasting) {
			selection_state = SelectionState::None;
		} else if (selection_state == SelectionState::Selected || selection_state == SelectionState::Selecting ||
				   selection_state == SelectionState::Resizing) {
			deselect();
		} else {
			show_exit_popup = true;
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
	if (io.WantCaptureMouse && selection_state != SelectionState::Moving &&
		selection_state != SelectionState::Resizing) {
		finalize_canvas_drag();
		s_line_brush_active = false;
		return;
	}

	ImVec2 grid_pos = screen_to_grid_pos(io.MousePos);

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
		ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
		if (drag_delta.x * drag_delta.x + drag_delta.y * drag_delta.y < 25.0f) {
			uint32_t x_cell = static_cast<uint32_t>(grid_pos.x);
			uint32_t y_cell = static_cast<uint32_t>(grid_pos.y);
			if (x_cell < Grid::get_width() && y_cell < Grid::get_height()) {
				uint8_t cell = Grid::get_cell(x_cell, y_cell);
				selected_id = MaterialManager::get_material(cell).id;
			}
		}
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
							uint8_t cell = clipboard.cells[y * clipboard.width + x];
							if (!transparent_mode || cell != 0) {
								Grid::set_cell(gx, gy, cell);
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
				paint_line(s_prev_canvas_grid, grid_pos, mouse_size, mat_id, brush_shape);
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

void UI::render_header(ImGuiIO& io) {
	const auto& cfg = ConfigManager::get_config();
	if (cfg.ui.show_fps) {
		ImGui::Text("FPS: %.1f (%.3f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
	}
	if (cfg.ui.show_active_cells) {
		ImGui::Text("Active cells: %u", Grid::get_changed_cells());
	}
	if (cfg.ui.show_simulation_status) {
		if (update) {
			ImGui::TextColored(ImVec4(0.25f, 0.90f, 0.45f, 1.0f), "Status: UNPAUSED");
		} else {
			ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.35f, 1.0f), "Status: PAUSED");
		}
	}
	ImGui::Separator();
	render_selection_controls();
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
		UndoManager::push_snapshot("Step Simulation");
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
	render_selection_controls();
	ImGui::Separator();

	std::vector<MaterialDefinition>& materials = MaterialManager::get_materials();

	ImGui::Separator();
	ImGui::Text("Select Material:");
	ImGui::BeginChild("SimMaterialsList", ImVec2(0, -1), true);
	for (int i = 1; i < (int)materials.size(); ++i) {
		std::string label = materials[i].name;
		ImVec4 color =
			ImVec4(materials[i].color[0] / 255.f, materials[i].color[1] / 255.f, materials[i].color[2] / 255.f, 1.f);
		ImGui::ColorButton(("##sim_color_" + std::to_string(i)).c_str(), color, ImGuiColorEditFlags_NoTooltip,
						   ImVec2(15, 15));
		ImGui::SameLine();
		if (ImGui::Selectable((label + "##sim_" + std::to_string(i)).c_str(), selected_id == materials[i].id)) {
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
	ImGui::ColorButton("##sim_color_0", color, ImGuiColorEditFlags_NoTooltip, ImVec2(15, 15));
	ImGui::SameLine();
	if (ImGui::Selectable((materials[0].name + "##sim_0").c_str(), selected_id == 0)) {
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
	uint8_t then_val = 255;
	bool has_copied = false;

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

std::vector<UI::MaterialShortcutItem> UI::get_all_material_shortcuts() {
	const auto& mats = MaterialManager::get_materials();
	if (mats.empty())
		return {};

	const auto& meta = SetManager::get_current_metadata();

	std::vector<size_t> order;
	order.reserve(mats.size());
	for (size_t i = 1; i < mats.size(); ++i) {
		order.push_back(i);
	}
	order.push_back(0);

	std::vector<MaterialShortcutItem> result;
	result.reserve(order.size());

	std::unordered_set<std::string> used_shortcuts;
	for (size_t idx : order) {
		auto it = meta.shortcuts.find(mats[idx].name);
		if (it != meta.shortcuts.end() && !it->second.empty()) {
			used_shortcuts.insert(it->second);
		}
	}

	int next_order_num = 1;

	auto find_next_available_digit = [&](int start_num) -> int {
		for (int n = start_num; n <= 9; ++n) {
			if (!used_shortcuts.count(std::to_string(n))) {
				return n;
			}
		}
		for (int n = 1; n < start_num && n <= 9; ++n) {
			if (!used_shortcuts.count(std::to_string(n))) {
				return n;
			}
		}
		return 0;
	};

	for (size_t idx : order) {
		MaterialShortcutItem item;
		item.name = mats[idx].name;
		item.id = mats[idx].id;

		auto it = meta.shortcuts.find(mats[idx].name);
		if (it != meta.shortcuts.end() && !it->second.empty()) {
			item.custom_shortcut = it->second;
			item.effective_shortcut = it->second;
			item.is_custom = true;

			if (item.custom_shortcut.length() == 1 && item.custom_shortcut[0] >= '1' &&
				item.custom_shortcut[0] <= '9') {
				next_order_num = (item.custom_shortcut[0] - '0') + 1;
			}
		} else {
			item.is_custom = false;

			int chosen_digit = find_next_available_digit(next_order_num);
			if (chosen_digit >= 1 && chosen_digit <= 9) {
				std::string num_str = std::to_string(chosen_digit);
				item.effective_shortcut = num_str;
				used_shortcuts.insert(num_str);
				next_order_num = chosen_digit + 1;
			} else {
				item.effective_shortcut = "";
			}
		}
		result.push_back(item);
	}

	return result;
}

std::string UI::get_effective_material_shortcut(uint8_t id) {
	auto all = get_all_material_shortcuts();
	for (const auto& item : all) {
		if (item.id == id) {
			return item.effective_shortcut;
		}
	}
	return "";
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
				ImGui::ColorButton(("##color_" + std::to_string(i)).c_str(), color, ImGuiColorEditFlags_NoTooltip,
								   ImVec2(15, 15));
				ImGui::SameLine();

				if (ImGui::Selectable((label + "##" + std::to_string(i)).c_str(), selected_id == materials[i].id)) {
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
			ImGui::ColorButton("##color_0", color, ImGuiColorEditFlags_NoTooltip, ImVec2(15, 15));
			ImGui::SameLine();

			if (ImGui::Selectable((materials[0].name + "##0").c_str(), selected_id == 0)) {
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

		ImGui::Text("Editing: %s", mat.name.c_str());

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

		float col[3] = {mat.color[0] / 255.f, mat.color[1] / 255.f, mat.color[2] / 255.f};
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
					if (ImGui::Selectable(other_mat.name.c_str(), is_selected)) {
						UndoManager::push_snapshot("Set Inheritance");
						MaterialManager::set_material_inheritance(selected_id, other_mat.id);
						rebuild_needed = true;
						unsaved_changes = true;
					}
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Dynamically inherits rules from the selected parent material.");
		}

		ImGui::Separator();

		ImGui::Spacing();
		ImGui::Text("Rules:");
		ImGui::SameLine();

		mat.sync_rule_order();

		if (button_with_icon("Add Rule", IconManager::get(IconID::Add))) {
			if (selected_id == 0) {
				open_empty_rule_warning_popup = true;
			} else {
				UndoManager::push_snapshot("Add Rule");
				RuleDefinition new_rule;
				new_rule.when[12] = {selected_id};
				new_rule.then.fill(255);
				new_rule.chance = 100.0f;
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

			ImGui::BeginChild(("##rule_card_" + std::to_string(r_id)).c_str(), ImVec2(0, 200), true,
							  ImGuiWindowFlags_NoScrollbar);

			ImGui::BeginGroup();
			float btn_size = ImGui::GetFrameHeight();
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
			ImGui::SameLine(0.0f, 10.0f);

			if (button_with_icon("##CopyRule", IconManager::get(IconID::Copy), ImVec2(btn_size, btn_size))) {
				UndoManager::push_snapshot("Copy Rule");
				RuleDefinition duplicated_rule = rule;
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

			ImGui::SetNextItemWidth(150.0f);
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

			if (ImGui::Checkbox("X Symmetry", &rule.symmetry.flip_x)) {
				UndoManager::push_snapshot("Edit Rule Symmetry");
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Allows the rule to match its left-right mirror image.");
			}
			if (ImGui::Checkbox("Y Symmetry", &rule.symmetry.flip_y)) {
				UndoManager::push_snapshot("Edit Rule Symmetry");
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Allows the rule to match its up-down mirror image.");
			}
			if (ImGui::Checkbox("Rotational Symmetry", &rule.symmetry.rotate)) {
				UndoManager::push_snapshot("Edit Rule Symmetry");
				rebuild_needed = true;
				unsaved_changes = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Allows the rule to match with 90 degree rotations of itself.");
			}

			ImGui::EndGroup();

			ImGui::SameLine(0.0f, 5.0f);

			ImGui::BeginGroup();
			ImGui::Text("When");
			ImGuiIO& io = ImGui::GetIO();

			for (int y = 0; y < NEIGHBOR_SIZE; ++y) {
				for (int x = 0; x < NEIGHBOR_SIZE; ++x) {
					int c_id = y * NEIGHBOR_SIZE + x;
					ImGui::PushID(c_id);

					std::string label = "*";
					ImVec4 btn_col = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

					if (c_id == 12) {
						label = mat.name.substr(0, 1);
						btn_col = ImVec4(mat.color[0] / 255.f, mat.color[1] / 255.f, mat.color[2] / 255.f, 1.0f);
						ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
						ImGui::Button(label.c_str(), ImVec2(25, 25));
						ImGui::PopStyleColor();
					} else {
						const auto& when_ids = rule.when[c_id];

						if (when_ids.size() == 1) {
							const auto& reference_mat = MaterialManager::get_material(when_ids[0]);
							label = reference_mat.name.substr(0, 1);
							btn_col = ImVec4(reference_mat.color[0] / 255.f, reference_mat.color[1] / 255.f,
											 reference_mat.color[2] / 255.f, 1.0f);
						} else if (when_ids.size() > 1) {
							label = std::to_string(when_ids.size());
							float r_sum = 0, g_sum = 0, b_sum = 0;
							for (uint8_t id : when_ids) {
								const auto& reference_mat = MaterialManager::get_material(id);
								r_sum += reference_mat.color[0] / 255.f;
								g_sum += reference_mat.color[1] / 255.f;
								b_sum += reference_mat.color[2] / 255.f;
							}
							float n = static_cast<float>(when_ids.size());
							btn_col = ImVec4(r_sum / n, g_sum / n, b_sum / n, 1.0f);
						}

						std::string popup_id =
							"select_material_when_" + std::to_string(r_id) + "_" + std::to_string(c_id);

						ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
						bool btn_clicked = ImGui::Button(label.c_str(), ImVec2(25, 25));
						bool is_hovered = ImGui::IsItemHovered();
						bool is_right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right) && is_hovered;
						bool is_middle_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && is_hovered;
						ImGui::PopStyleColor();

						if (!reference.is_inherited) {
							if (is_middle_clicked) {
								g_copied_rule_cell.type = CopiedRuleCell::Type::When;
								g_copied_rule_cell.when_val = rule.when[c_id];
								g_copied_rule_cell.has_copied = true;
							} else if (is_right_clicked || (is_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
								if (!rule.when[c_id].empty()) {
									UndoManager::push_snapshot("Clear Rule Cell");
									rule.when[c_id].clear();
									rebuild_needed = true;
									unsaved_changes = true;
								}
							} else if (io.KeyShift &&
									   (btn_clicked || (is_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
								if (g_copied_rule_cell.has_copied) {
									auto target_when = g_copied_rule_cell.get_as_when();
									if (rule.when[c_id] != target_when) {
										UndoManager::push_snapshot("Paste Rule Cell");
										rule.when[c_id] = target_when;
										rebuild_needed = true;
										unsaved_changes = true;
									}
								}
							} else if (btn_clicked && !io.KeyShift) {
								ImGui::OpenPopup(popup_id.c_str());
							}
						}

						if (is_hovered) {
							std::string tip;
							if (when_ids.empty()) {
								tip = "Wildcard (*)";
							} else {
								for (size_t i = 0; i < when_ids.size(); ++i) {
									if (i > 0)
										tip += ", ";
									tip += MaterialManager::get_material(when_ids[i]).name;
								}
							}
							tip += "\n(Left-click menu, Shift-click/drag paint, Middle-click copy, Right-click clear)";
							ImGui::SetTooltip("%s", tip.c_str());
						}

						if (ImGui::BeginPopup(popup_id.c_str())) {
							for (const auto& m : materials) {
								bool checked = std::find(rule.when[c_id].begin(), rule.when[c_id].end(), m.id) !=
											   rule.when[c_id].end();
								ImVec4 m_col = ImVec4(m.color[0] / 255.f, m.color[1] / 255.f, m.color[2] / 255.f, 1.0f);
								ImGui::ColorButton(("##wcb_" + std::to_string(r_id) + "_" + std::to_string(c_id) + "_" +
													std::to_string(m.id))
													   .c_str(),
												   m_col, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
								ImGui::SameLine();
								ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.4f, 0.4f));
								if (ImGui::Checkbox(
										(m.name + "##cb_" + std::to_string(r_id) + "_" + std::to_string(c_id)).c_str(),
										&checked)) {
									UndoManager::push_snapshot("Toggle Rule Checkbox");
									if (checked) {
										if (std::find(rule.when[c_id].begin(), rule.when[c_id].end(), m.id) ==
											rule.when[c_id].end()) {
											rule.when[c_id].push_back(m.id);
										}
									} else {
										std::erase(rule.when[c_id], m.id);
									}
									g_copied_rule_cell.type = CopiedRuleCell::Type::When;
									g_copied_rule_cell.when_val = rule.when[c_id];
									g_copied_rule_cell.has_copied = true;
									rebuild_needed = true;
									unsaved_changes = true;
								}
								ImGui::PopStyleVar();
							}

							ImGui::EndPopup();
						}
					}

					if (x < 4)
						ImGui::SameLine();
					ImGui::PopID();
				}
			}
			ImGui::EndGroup();

			ImGui::SameLine(0.0f, 15.0f);

			ImGui::BeginGroup();
			ImGui::Text("Then");

			for (uint32_t y = 0; y < NEIGHBOR_SIZE; ++y) {
				for (uint32_t x = 0; x < NEIGHBOR_SIZE; ++x) {
					const uint32_t c_id = y * NEIGHBOR_SIZE + x;
					ImGui::PushID(c_id + 100);

					std::string label = "-";
					ImVec4 btn_col = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

					uint8_t then_id = rule.then[c_id];

					if (then_id != 255) {
						const auto& reference_mat = MaterialManager::get_material(then_id);
						label = reference_mat.name.substr(0, 1);
						btn_col = ImVec4(reference_mat.color[0] / 255.f, reference_mat.color[1] / 255.f,
										 reference_mat.color[2] / 255.f, 1.0f);
					} else {
						if (rule.when[c_id].size() == 1) {
							const auto& reference_mat = MaterialManager::get_material(rule.when[c_id][0]);
							label = reference_mat.name.substr(0, 1);
							btn_col =
								ImVec4(reference_mat.color[0] / 255.f * 0.4f, reference_mat.color[1] / 255.f * 0.4f,
									   reference_mat.color[2] / 255.f * 0.4f, 1.0f);
						} else if (rule.when[c_id].size() > 1) {
							label = std::to_string(rule.when[c_id].size());
							float r_sum = 0, g_sum = 0, b_sum = 0;
							for (uint8_t id : rule.when[c_id]) {
								const auto& reference_mat = MaterialManager::get_material(id);
								r_sum += reference_mat.color[0] / 255.f;
								g_sum += reference_mat.color[1] / 255.f;
								b_sum += reference_mat.color[2] / 255.f;
							}
							float n = static_cast<float>(rule.when[c_id].size());
							btn_col = ImVec4(r_sum / n * 0.4f, g_sum / n * 0.4f, b_sum / n * 0.4f, 1.0f);
						}
					}

					const std::string popup_id =
						"select_material_then_" + std::to_string(r_id) + "_" + std::to_string(c_id);

					ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
					bool btn_clicked = ImGui::Button(label.c_str(), ImVec2(25, 25));
					bool is_hovered = ImGui::IsItemHovered();
					bool is_right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right) && is_hovered;
					bool is_middle_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Middle) && is_hovered;
					ImGui::PopStyleColor();

					if (!reference.is_inherited) {
						if (is_middle_clicked) {
							g_copied_rule_cell.type = CopiedRuleCell::Type::Then;
							g_copied_rule_cell.then_val = rule.then[c_id];
							g_copied_rule_cell.has_copied = true;
						} else if (is_right_clicked || (is_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
							if (rule.then[c_id] != 255) {
								UndoManager::push_snapshot("Clear Then Cell");
								rule.then[c_id] = 255;
								rebuild_needed = true;
								unsaved_changes = true;
							}
						} else if (io.KeyShift &&
								   (btn_clicked || (is_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
							if (g_copied_rule_cell.has_copied) {
								uint8_t target_then = g_copied_rule_cell.get_as_then();
								if (rule.then[c_id] != target_then) {
									UndoManager::push_snapshot("Paste Then Cell");
									rule.then[c_id] = target_then;
									rebuild_needed = true;
									unsaved_changes = true;
								}
							}
						} else if (btn_clicked && !io.KeyShift) {
							ImGui::OpenPopup(popup_id.c_str());
						}
					}

					if (is_hovered) {
						std::string tip =
							(then_id == 255) ? "Unchanged (-)" : MaterialManager::get_material(rule.then[c_id]).name;
						tip += "\n(Left-click menu, Shift-click/drag paint, Middle-click copy, Right-click clear)";
						ImGui::SetTooltip("%s", tip.c_str());
					}

					if (ImGui::BeginPopup(popup_id.c_str())) {
						if (ImGui::Selectable("Unchanged", then_id == 255)) {
							UndoManager::push_snapshot("Set Then Unchanged");
							rule.then[c_id] = 255;
							g_copied_rule_cell.type = CopiedRuleCell::Type::Then;
							g_copied_rule_cell.then_val = 255;
							g_copied_rule_cell.has_copied = true;
							rebuild_needed = true;
							unsaved_changes = true;
						}
						for (const auto& m : materials) {
							ImVec4 m_col = ImVec4(m.color[0] / 255.f, m.color[1] / 255.f, m.color[2] / 255.f, 1.0f);
							ImGui::ColorButton(("##tcb_" + std::to_string(r_id) + "_" + std::to_string(c_id) + "_" +
												std::to_string(m.id))
												   .c_str(),
											   m_col, ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
							ImGui::SameLine();
							if (ImGui::Selectable(m.name.c_str(), then_id == m.id)) {
								UndoManager::push_snapshot("Select Then Material");
								rule.then[c_id] = m.id;
								g_copied_rule_cell.type = CopiedRuleCell::Type::Then;
								g_copied_rule_cell.then_val = m.id;
								g_copied_rule_cell.has_copied = true;
								rebuild_needed = true;
								unsaved_changes = true;
							}
						}
						ImGui::EndPopup();
					}

					if (x < 4)
						ImGui::SameLine();
					ImGui::PopID();
				}
			}
			ImGui::EndGroup();

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

		if (rebuild_needed) {
			MaterialManager::update_material_rules(selected_id, mat);
		}

		ImGui::EndTabItem();
	}
}

namespace {
	bool s_show_publish_modal = false;
	int s_publish_type_idx = 0;	 // 0=Save, 1=Set, 2=Stamp, 3=Theme
	char s_publish_title[128] = "";
	char s_publish_author[64] = "";
	char s_publish_desc[512] = "";
	bool s_publishing = false;
	std::string s_publish_status = "";
	bool s_pub_bundle_set = true;
	std::string s_publish_target_set = "";
	std::string s_publish_target_save = "";
	std::string s_publish_target_stamp = "";
	int s_publish_target_w = 0;
	int s_publish_target_h = 0;

	void open_share_for_set(const std::string& set_name) {
		s_publish_target_set = set_name;
		s_publish_target_save = "";
		s_publish_target_stamp = "";
		s_publish_type_idx = 1;	 // Set
		s_show_publish_modal = true;
		s_publish_status = "";
		std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", set_name.c_str());
		SetMetadata sm = SetManager::load_set_metadata(set_name);
		std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username()
															   : (sm.author.empty() ? "Player" : sm.author);
		std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
		std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", sm.description.c_str());
	}

	void open_share_for_save(const std::string& filename, const std::string& name, const std::string& parent_set, int w,
							 int h) {
		s_publish_target_set = parent_set;
		s_publish_target_save = filename;
		s_publish_target_stamp = "";
		s_publish_target_w = w;
		s_publish_target_h = h;
		s_publish_type_idx = 0;	 // Save
		s_show_publish_modal = true;
		s_publish_status = "";
		std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", name.c_str());
		SetMetadata sm = SetManager::load_set_metadata(parent_set);
		std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username()
															   : (sm.author.empty() ? "Player" : sm.author);
		std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
		std::string desc = "Save from set '" + parent_set + "'";
		std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", desc.c_str());
	}

	void open_share_for_stamp(const std::string& filename, const std::string& name, const std::string& parent_set,
							  int w, int h) {
		s_publish_target_set = parent_set;
		s_publish_target_save = "";
		s_publish_target_stamp = filename;
		s_publish_target_w = w;
		s_publish_target_h = h;
		s_publish_type_idx = 2;	 // Stamp
		s_show_publish_modal = true;
		s_publish_status = "";
		std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", name.c_str());
		SetMetadata sm = SetManager::load_set_metadata(parent_set);
		std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username()
															   : (sm.author.empty() ? "Player" : sm.author);
		std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
		std::string desc = "Stamp from set '" + parent_set + "'";
		std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", desc.c_str());
	}

	void open_share_for_theme() {
		s_publish_target_set = "";
		s_publish_target_save = "";
		s_publish_target_stamp = "";
		s_publish_type_idx = 3;	 // Theme
		s_show_publish_modal = true;
		s_publish_status = "";
		std::snprintf(s_publish_title, sizeof(s_publish_title), "My Custom Theme");
		std::string auth_user = WorkshopClient::is_logged_in() ? WorkshopClient::get_logged_in_username() : "Player";
		std::snprintf(s_publish_author, sizeof(s_publish_author), "%s", auth_user.c_str());
		std::snprintf(s_publish_desc, sizeof(s_publish_desc), "Custom UI theme for Sand3");
	}

	void check_item_existence(const std::string& item_id, const std::string& type, const std::string& name_or_file,
							  const std::string& set_context = "", bool force = false);
	void validate_all_online_and_cached_items(bool force = false);
}  // namespace

void UI::render_manage_sets() {
	const std::string& current_set = SetManager::get_current_set();
	SetMetadata meta = SetManager::get_current_metadata();

	if (ImGui::BeginTabItem("Sets")) {
		ImGui::Spacing();

		ImGui::Text("Available Sets:");
		ImGui::BeginChild("SetsListScroll", ImVec2(0, 150), true);
		float avail_w = ImGui::GetContentRegionAvail().x;
		const float row_h = 22.0f;
		const float btn_sz = 22.0f;
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
					check_item_existence(s_meta.workshop_id, "set", s, current_set, false);
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

			float start_y = ImGui::GetCursorPosY();

			if (ImGui::Selectable((display_name + "##selectable_set_" + s).c_str(), s == current_set, 0,
								  ImVec2(selectable_w, row_h))) {
				if (s != current_set) {
					if (unsaved_changes) {
						pending_set_switch = s;
						open_switch_popup = true;
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

			if (show_share) {
				ImGui::SameLine(0, btn_gap);
				ImGui::SetCursorPosY(start_y);
				if (UI::button_with_icon("##share_set", IconManager::get(IconID::Transmit), ImVec2(btn_sz, row_h))) {
					open_share_for_set(s);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Share set '%s' to Community Workshop", s.c_str());
				}
			}

			if (show_update) {
				ImGui::SameLine(0, btn_gap);
				ImGui::SetCursorPosY(start_y);
				if (UI::button_with_icon("##update_set", IconManager::get(IconID::Update), ImVec2(btn_sz, row_h))) {
					if (!s_meta.workshop_id.empty()) {
						std::string wid = s_meta.workshop_id;
						std::string wset = s;
						WorkshopClient::check_item_exists(wid, [wset, wid, s_meta](bool exists, int http_status) {
							if (!exists && http_status == 404) {
								SetManager::clear_workshop_info(wset);
								ToastManager::warning(
									fmt::format("Set '{}' was deleted from the workshop. Converted to local.", wset));
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
						open_share_for_set(s);
					}
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Update set '%s' to Community Workshop", s.c_str());
				}
			}

			if (show_download) {
				ImGui::SameLine(0, btn_gap);
				ImGui::SetCursorPosY(start_y);
				if (UI::button_with_icon("##download_set", IconManager::get(IconID::Save), ImVec2(btn_sz, row_h))) {
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

		ImGui::Spacing();
		if (button_with_icon("New", IconManager::get(IconID::Add), ImVec2(80, 25))) {
			open_create_set_popup = true;
			duplicate_set_checkbox = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create a new empty material set.");
		}

		ImGui::SameLine();
		if (button_with_icon("Copy", IconManager::get(IconID::Copy), ImVec2(80, 25))) {
			open_create_set_popup = true;
			duplicate_set_checkbox = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create a duplicate copy of the current set.");
		}

		ImGui::SameLine();
		if (SetManager::get_sets().size() > 1) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			if (button_with_icon("Delete", IconManager::get(IconID::Clear), ImVec2(80, 25))) {
				open_delete_set_popup = true;
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
		if (ImGui::InputText("Name##set_rename_input", set_rename_buf, sizeof(set_rename_buf),
							 ImGuiInputTextFlags_EnterReturnsTrue)) {
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

		char s_desc[256];
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

	if (ImGui::BeginTabItem("Saves")) {
		ImGui::Spacing();

		if (ImGui::BeginTabBar("SavesSubTabBar")) {
			if (ImGui::BeginTabItem("World Saves")) {
				ImGui::Spacing();
				ImGui::Text("Save Current Simulation:");
				ImGui::InputText("Save Name##save_name", save_file_name_buf, sizeof(save_file_name_buf));
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Input a name for the save file.");
				}

				if (button_with_icon("Save Simulation", IconManager::get(IconID::Save), ImVec2(-1, 30))) {
					std::string s_name = save_file_name_buf;
					if (!s_name.empty()) {
						SaveManager::save_to_file_async(s_name, current_set, [](bool success) {
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

				ImGui::BeginChild("SavesListScroll", ImVec2(0, 180), true);
				float avail_w = ImGui::GetContentRegionAvail().x;
				const float row_h = 22.0f;
				const float btn_sz = 22.0f;
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

					std::string tag = "";
					if (is_online) {
						if (!sfile.workshop_id.empty()) {
							check_item_existence(sfile.workshop_id, "save", sfile.filename, current_set, false);
						}
						tag = is_transient ? " [Cached]" : " [Online]";
					} else if (sfile.dimensions_differ) {
						tag = " [Diff Size]";
					}

					std::string label = fmt::format("{} ({}x{}){}", sfile.name, sfile.width, sfile.height, tag);

					bool show_share = !is_online && is_logged_in;
					bool show_update = is_online && is_owner;
					bool show_download = is_online && is_transient;

					int num_buttons = (show_share ? 1 : 0) + (show_update ? 1 : 0) + (show_download ? 1 : 0);
					float total_btn_w = (num_buttons > 0) ? (num_buttons * btn_sz + num_buttons * btn_gap) : 0.0f;
					float selectable_w = std::max(20.0f, avail_w - total_btn_w);

					float start_y = ImGui::GetCursorPosY();

					if (ImGui::Selectable(label.c_str(), is_selected, 0, ImVec2(selectable_w, row_h))) {
						selected_save_id = static_cast<int>(i);
					}
					if (is_selected && ImGui::IsMouseDoubleClicked(0)) {
						if (sfile.dimensions_differ) {
							pending_diff_save = sfile;
							open_diff_size_save_popup = true;
						} else {
							if (unsaved_changes) {
								pending_save_load = sfile.filename;
								open_switch_popup = true;
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

					if (show_share) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorPosY(start_y);
						if (UI::button_with_icon("##share_save", IconManager::get(IconID::Transmit),
												 ImVec2(btn_sz, row_h))) {
							open_share_for_save(sfile.filename, sfile.name, current_set, sfile.width, sfile.height);
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Share save '%s' to Community Workshop", sfile.name.c_str());
						}
					}

					if (show_update) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorPosY(start_y);
						if (UI::button_with_icon("##update_save", IconManager::get(IconID::Update),
												 ImVec2(btn_sz, row_h))) {
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
								open_share_for_save(sfile.filename, sfile.name, current_set, sfile.width, sfile.height);
							}
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Update save '%s' to Community Workshop", sfile.name.c_str());
						}
					}

					if (show_download) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorPosY(start_y);
						if (UI::button_with_icon("##download_save", IconManager::get(IconID::Save),
												 ImVec2(btn_sz, row_h))) {
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

					ImGui::PopID();
				}
				ImGui::EndChild();
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Double click to load. Handles different canvas dimensions.");
				}

				ImGui::Spacing();
				if (selected_save_id >= 0 && selected_save_id < static_cast<int>(save_files.size())) {
					if (button_with_icon("Load", IconManager::get(IconID::Folder), ImVec2(80, 25))) {
						if (save_files[selected_save_id].dimensions_differ) {
							pending_diff_save = save_files[selected_save_id];
							open_diff_size_save_popup = true;
						} else {
							if (unsaved_changes) {
								pending_save_load = save_files[selected_save_id].filename;
								open_switch_popup = true;
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
					if (button_with_icon("Save Selection as Stamp", IconManager::get(IconID::Save), ImVec2(-1, 28))) {
						std::string sname = stamp_tab_name;
						if (!sname.empty()) {
							int sx = selection_box.min_x();
							int sy = selection_box.min_y();
							int sw = selection_box.width();
							int sh = selection_box.height();
							std::vector<uint8_t> stamp_cells(sw * sh);
							for (int y = 0; y < sh; ++y) {
								for (int x = 0; x < sw; ++x) {
									stamp_cells[y * sw + x] = Grid::get_cell(sx + x, sy + y);
								}
							}
							SaveManager::save_stamp_to_file_async(sname, current_set, stamp_cells, sw, sh,
																  [](bool success) {
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
				ImGui::BeginChild("StampsListScroll", ImVec2(0, 180), true);
				float avail_w = ImGui::GetContentRegionAvail().x;
				const float row_h = 22.0f;
				const float btn_sz = 22.0f;
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

					std::string tag = "";
					if (is_online) {
						if (!stfile.workshop_id.empty()) {
							check_item_existence(stfile.workshop_id, "stamp", stfile.filename, current_set, false);
						}
						tag = is_transient ? " [Cached]" : " [Online]";
					}

					std::string label = fmt::format("{} ({}x{}){}", stfile.name, stfile.width, stfile.height, tag);

					bool show_share = !is_online && is_logged_in;
					bool show_update = is_online && is_owner;
					bool show_download = is_online && is_transient;

					int num_buttons = (show_share ? 1 : 0) + (show_update ? 1 : 0) + (show_download ? 1 : 0);
					float total_btn_w = (num_buttons > 0) ? (num_buttons * btn_sz + num_buttons * btn_gap) : 0.0f;
					float selectable_w = std::max(20.0f, avail_w - total_btn_w);

					float start_y = ImGui::GetCursorPosY();

					if (ImGui::Selectable(label.c_str(), is_selected, 0, ImVec2(selectable_w, row_h))) {
						selected_stamp_id = static_cast<int>(i);
					}
					if (is_selected && ImGui::IsMouseDoubleClicked(0)) {
						SaveManager::load_stamp_from_file_async(
							stfile.filename, current_set,
							[](bool success, const std::vector<uint8_t>& cells, uint32_t sw, uint32_t sh) {
								if (success) {
									load_stamp(cells, sw, sh);
								}
							});
					}

					if (show_share) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorPosY(start_y);
						if (UI::button_with_icon("##share_stamp", IconManager::get(IconID::Transmit),
												 ImVec2(btn_sz, row_h))) {
							open_share_for_stamp(stfile.filename, stfile.name, current_set, stfile.width,
												 stfile.height);
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Share stamp '%s' to Community Workshop", stfile.name.c_str());
						}
					}

					if (show_update) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorPosY(start_y);
						if (UI::button_with_icon("##update_stamp", IconManager::get(IconID::Update),
												 ImVec2(btn_sz, row_h))) {
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
								open_share_for_stamp(stfile.filename, stfile.name, current_set, stfile.width,
													 stfile.height);
							}
						}
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Update stamp '%s' to Community Workshop", stfile.name.c_str());
						}
					}

					if (show_download) {
						ImGui::SameLine(0, btn_gap);
						ImGui::SetCursorPosY(start_y);
						if (UI::button_with_icon("##download_stamp", IconManager::get(IconID::Save),
												 ImVec2(btn_sz, row_h))) {
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

					ImGui::PopID();
				}
				ImGui::EndChild();
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Double click to stamp/paste onto canvas.");
				}

				ImGui::Spacing();
				if (selected_stamp_id >= 0 && selected_stamp_id < static_cast<int>(stamp_files.size())) {
					if (button_with_icon("Stamp / Paste", IconManager::get(IconID::Paste), ImVec2(100, 25))) {
						SaveManager::load_stamp_from_file_async(
							stamp_files[selected_stamp_id].filename, current_set,
							[](bool success, const std::vector<uint8_t>& cells, uint32_t sw, uint32_t sh) {
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

		if (open_diff_size_save_popup) {
			ImGui::OpenPopup("Differently Sized Save Detected");
			open_diff_size_save_popup = false;
		}

		if (ImGui::BeginPopupModal("Differently Sized Save Detected", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Save file: '%s'", pending_diff_save.name.c_str());
			ImGui::Text("Save Dimensions: %u x %u", pending_diff_save.width, pending_diff_save.height);
			ImGui::Text("Current Grid: %u x %u", Grid::get_width(), Grid::get_height());
			ImGui::Spacing();
			ImGui::Text("Choose how to load this save into the simulation:");
			ImGui::Spacing();

			if (ImGui::Button("Place in Center", ImVec2(180, 32))) {
				SaveManager::load_from_file_async(pending_diff_save.filename, current_set, LoadPlacement::Center,
												  [](bool success, const std::string&) {
													  if (success) {
														  selected_id = 0;
														  unsaved_changes = false;
													  }
												  });
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Places the saved content centered in the current canvas.");
			}

			ImGui::SameLine();
			if (ImGui::Button("Crop / Top-Left", ImVec2(180, 32))) {
				SaveManager::load_from_file_async(pending_diff_save.filename, current_set, LoadPlacement::TopLeft,
												  [](bool success, const std::string&) {
													  if (success) {
														  selected_id = 0;
														  unsaved_changes = false;
													  }
												  });
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Places at (0,0), cropping if larger or leaving air if smaller.");
			}

			ImGui::Spacing();
			if (ImGui::Button("Resize Grid to Match Save", ImVec2(368, 32))) {
				SaveManager::load_from_file_async(pending_diff_save.filename, current_set, LoadPlacement::ResizeGrid,
												  [](bool success, const std::string&) {
													  if (success) {
														  selected_id = 0;
														  unsaved_changes = false;
													  }
												  });
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Resizes canvas to %u x %u.", pending_diff_save.width, pending_diff_save.height);
			}
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Warning: Resizing grid resets undo/redo history!");

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Cancel", ImVec2(100, 25))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::EndTabItem();
	}
}

namespace {
	std::vector<WorkshopItemClient> s_workshop_items;
	bool s_workshop_loading = false;
	bool s_workshop_loaded_once = false;
	std::string s_workshop_error = "";
	bool s_workshop_my_items = false;

	char s_workshop_search[128] = "";
	int s_workshop_type_filter = 0;	 // 0=All, 1=Sets, 2=Saves, 3=Stamps, 4=Themes
	int s_workshop_sort_idx = 0;	 // 0=Popular, 1=Liked, 2=Favorites, 3=Newest, 4=Downloads
	bool s_force_workshop_tab_selected = false;

	bool s_show_auth_modal = false;
	char s_auth_username[64] = "";
	char s_auth_password[64] = "";
	std::string s_auth_status = "";
	bool s_auth_is_register = false;

	std::unordered_map<std::string, SDL_Texture*> s_workshop_textures;
	std::unordered_set<std::string> s_thumbnails_downloading;

	bool s_show_report_modal = false;
	std::string s_report_item_id = "";
	std::string s_report_item_title = "";
	int s_report_reason_idx = 0;
	char s_report_details[256] = "";
	std::string s_report_status = "";

	static std::unordered_map<std::string, std::chrono::steady_clock::time_point> s_item_last_checked;
	static std::unordered_set<std::string> s_item_checking_in_flight;

	void check_item_existence(const std::string& item_id, const std::string& type, const std::string& name_or_file,
							  const std::string& set_context, bool force) {
		if (item_id.empty()) {
			return;
		}

		auto now = std::chrono::steady_clock::now();
		if (!force) {
			auto it = s_item_last_checked.find(item_id);
			if (it != s_item_last_checked.end()) {
				auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
				if (elapsed < 10) {
					return;
				}
			}
		}

		if (s_item_checking_in_flight.find(item_id) != s_item_checking_in_flight.end()) {
			return;
		}

		s_item_checking_in_flight.insert(item_id);
		s_item_last_checked[item_id] = now;

		WorkshopClient::check_item_exists(
			item_id, [item_id, type, name_or_file, set_context](bool exists, int http_status) {
				s_item_checking_in_flight.erase(item_id);
				if (!exists && http_status == 404) {
					if (type == "set") {
						SetManager::clear_workshop_info(name_or_file);
						ToastManager::warning(
							fmt::format("Set '{}' was deleted from the workshop. Converted to local.", name_or_file));
					} else if (type == "save") {
						SaveManager::clear_save_workshop_info(name_or_file, set_context);
						ToastManager::warning(
							fmt::format("Save '{}' was deleted from the workshop. Converted to local.", name_or_file));
					} else if (type == "stamp") {
						SaveManager::clear_stamp_workshop_info(name_or_file, set_context);
						ToastManager::warning(
							fmt::format("Stamp '{}' was deleted from the workshop. Converted to local.", name_or_file));
					}
					WorkshopCache::remove_transient_by_id(item_id);

					for (auto it = s_workshop_items.begin(); it != s_workshop_items.end();) {
						if (it->id == item_id) {
							it = s_workshop_items.erase(it);
						} else {
							++it;
						}
					}
				}
			});
	}

	void validate_all_online_and_cached_items(bool force) {
		std::string cur_set = SetManager::get_current_set();

		for (const auto& s : SetManager::get_sets()) {
			SetMetadata sm = SetManager::load_set_metadata(s);
			if ((sm.is_online || SetManager::is_set_online(s)) && !sm.workshop_id.empty()) {
				check_item_existence(sm.workshop_id, "set", s, cur_set, force);
			}
		}

		auto saves = SaveManager::get_save_files(cur_set);
		for (const auto& sf : saves) {
			if (sf.is_online && !sf.workshop_id.empty()) {
				check_item_existence(sf.workshop_id, "save", sf.filename, cur_set, force);
			}
		}

		auto stamps = SaveManager::get_stamp_files(cur_set);
		for (const auto& st : stamps) {
			if (st.is_online && !st.workshop_id.empty()) {
				check_item_existence(st.workshop_id, "stamp", st.filename, cur_set, force);
			}
		}

		for (const auto& path : WorkshopCache::get_transient_paths()) {
			size_t slash = path.find_last_of("/\\");
			std::string fname = (slash != std::string::npos) ? path.substr(slash + 1) : path;
			size_t underscore = fname.find('_');
			if (underscore != std::string::npos) {
				std::string tid = fname.substr(0, underscore);
				if (tid.length() >= 8) {
					check_item_existence(tid, "cache", fname, cur_set, force);
				}
			}
		}
	}

	void refresh_workshop_items() {
		validate_all_online_and_cached_items(true);
		s_workshop_loading = true;
		s_workshop_error = "";

		std::string type_str = "all";
		if (s_workshop_type_filter == 1)
			type_str = "set";
		else if (s_workshop_type_filter == 2)
			type_str = "save";
		else if (s_workshop_type_filter == 3)
			type_str = "stamp";
		else if (s_workshop_type_filter == 4)
			type_str = "theme";

		std::string sort_str = "popular";
		if (s_workshop_sort_idx == 1)
			sort_str = "liked";
		else if (s_workshop_sort_idx == 2)
			sort_str = "favorites";
		else if (s_workshop_sort_idx == 3)
			sort_str = "newest";
		else if (s_workshop_sort_idx == 4)
			sort_str = "downloads";

		std::string author_str = "";
		if (s_workshop_my_items && WorkshopClient::is_logged_in()) {
			author_str = WorkshopClient::get_logged_in_username();
		}

		WorkshopClient::fetch_items(
			type_str, sort_str, s_workshop_search, author_str,
			[](bool success, const std::vector<WorkshopItemClient>& items, int total) {
				s_workshop_loading = false;
				s_workshop_loaded_once = true;
				if (success) {
					s_workshop_items = items;
					s_workshop_error = "";
				} else {
					s_workshop_error =
						"Could not reach workshop server. Please ensure you are connected to the internet.";
				}
			});
	}

	SDL_Texture* get_or_load_thumbnail(const WorkshopItemClient& item) {
		if (item.thumbnail_path.empty())
			return nullptr;
		auto it = s_workshop_textures.find(item.id);
		if (it != s_workshop_textures.end())
			return it->second;

		if (s_thumbnails_downloading.find(item.id) == s_thumbnails_downloading.end()) {
			s_thumbnails_downloading.insert(item.id);
			std::string cache_dir = WorkshopCache::get_cache_root() + "thumbnails/";
			std::filesystem::create_directories(cache_dir);
			std::string dest_path = cache_dir + item.id + ".png";

			WorkshopClient::download_thumbnail(
				item.id, dest_path, [item_id = item.id](bool success, const std::string& path) {
					s_thumbnails_downloading.erase(item_id);
					if (success && std::filesystem::exists(path)) {
						SDL_IOStream* io = SDL_IOFromFile(path.c_str(), "rb");
						if (io) {
							SDL_Surface* surf = SDL_LoadPNG_IO(io, true);
							if (surf) {
								SDL_Texture* tex = SDL_CreateTextureFromSurface(Window::get_renderer(), surf);
								SDL_DestroySurface(surf);
								if (tex) {
									s_workshop_textures[item_id] = tex;
								}
							}
						}
					}
				});
		}
		return nullptr;
	}

	struct SimpleMaterialInfo {
		std::string name;
		uint8_t id = 0;
		std::array<uint8_t, 3> color = {255, 255, 255};
	};

	static std::vector<SimpleMaterialInfo> get_materials_for_set(const std::string& set_name) {
		std::vector<SimpleMaterialInfo> result;
		if (set_name.empty() || set_name == SetManager::get_current_set()) {
			for (const auto& m : MaterialManager::get_materials()) {
				result.push_back({m.name, m.id, m.color});
			}
			return result;
		}

		std::string set_dir = std::string(SETS_DIRECTORY) + set_name;
		if (!std::filesystem::exists(set_dir) || !std::filesystem::is_directory(set_dir)) {
			return result;
		}

		SimpleMaterialInfo empty_mat{"empty", 0, {64, 64, 64}};
		std::vector<SimpleMaterialInfo> loaded;

		for (const auto& entry : std::filesystem::directory_iterator(set_dir)) {
			if (entry.is_regular_file() && entry.path().extension().string() == ".mat") {
				std::ifstream file(entry.path());
				if (!file.is_open())
					continue;
				try {
					nlohmann::json j = nlohmann::json::parse(file);
					SimpleMaterialInfo info;
					info.name = j.value("name", entry.path().stem().string());
					info.id = j.value("id", (uint8_t)0);
					if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
						info.color[0] = j["color"][0].get<uint8_t>();
						info.color[1] = j["color"][1].get<uint8_t>();
						info.color[2] = j["color"][2].get<uint8_t>();
					}
					if (info.id == 0) {
						empty_mat = info;
					} else {
						loaded.push_back(info);
					}
				} catch (...) {}
			}
		}

		std::sort(loaded.begin(), loaded.end(),
				  [](const SimpleMaterialInfo& a, const SimpleMaterialInfo& b) { return a.id < b.id; });

		result.push_back(empty_mat);
		result.insert(result.end(), loaded.begin(), loaded.end());
		return result;
	}

	std::string generate_thumbnail_base64(int type_idx, const std::string& target_set,
										  const std::string& target_file = "") {
		SDL_Surface* surf = SDL_CreateSurface(128, 128, SDL_PIXELFORMAT_RGBA32);
		if (!surf)
			return "";

		uint32_t* pixels = static_cast<uint32_t*>(surf->pixels);
		uint32_t bg_col = SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, 13, 17, 23, 255);
		for (int i = 0; i < 128 * 128; ++i) {
			pixels[i] = bg_col;
		}

		auto mats = get_materials_for_set(target_set);
		std::array<std::array<uint8_t, 3>, 256> id_to_color{};
		for (int i = 0; i < 256; ++i) {
			id_to_color[i] = {64, 64, 64};
		}
		for (const auto& m : mats) {
			id_to_color[m.id] = m.color;
		}

		if (type_idx == 0) {
			std::vector<uint8_t> grid_cells;
			uint32_t gw = 0, gh = 0;

			if (!target_file.empty()) {
				std::string fpath = SaveManager::get_saves_directory(target_set) + target_file;
				std::ifstream fin(fpath, std::ios::binary);
				if (fin.is_open()) {
					std::string set_str;
					std::getline(fin, set_str);
					fin.read(reinterpret_cast<char*>(&gw), sizeof(gw));
					fin.read(reinterpret_cast<char*>(&gh), sizeof(gh));
					uint32_t num_blocks = 0;
					fin.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
					if (gw > 0 && gh > 0 && gw <= 4096 && gh <= 4096) {
						grid_cells.resize(gw * gh);
						if (!SaveManager::read_chunked_data(fin, num_blocks, grid_cells.size(), grid_cells.data())) {
							grid_cells.clear();
						}
					}
				}
			}

			if (grid_cells.empty() && target_set == SetManager::get_current_set()) {
				gw = Grid::get_width();
				gh = Grid::get_height();
				if (gw > 0 && gh > 0) {
					grid_cells.resize(gw * gh);
					for (uint32_t y = 0; y < gh; ++y) {
						for (uint32_t x = 0; x < gw; ++x) {
							grid_cells[y * gw + x] = Grid::get_cell(x, y);
						}
					}
				}
			}

			if (!grid_cells.empty() && gw > 0 && gh > 0) {
				for (int ty = 0; ty < 128; ++ty) {
					int gy = (ty * gh) / 128;
					for (int tx = 0; tx < 128; ++tx) {
						int gx = (tx * gw) / 128;
						uint8_t mid = grid_cells[gy * gw + gx];
						if (mid != 0) {
							pixels[ty * 128 + tx] =
								SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, id_to_color[mid][0],
											id_to_color[mid][1], id_to_color[mid][2], 255);
						}
					}
				}
			}
		} else if (type_idx == 1) {
			std::vector<SimpleMaterialInfo> valid_mats;
			for (const auto& m : mats) {
				if (m.id != 0) {
					valid_mats.push_back(m);
				}
			}
			int count = static_cast<int>(valid_mats.size());
			if (count > 0) {
				int cols = std::clamp(static_cast<int>(std::ceil(std::sqrt(count))), 2, 8);
				int rows = (count + cols - 1) / cols;
				int cell_w = 128 / cols;
				int cell_h = 128 / rows;
				for (int idx = 0; idx < count; ++idx) {
					const auto& mat = valid_mats[idx];
					uint32_t c = SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, mat.color[0],
											 mat.color[1], mat.color[2], 255);
					int cx = (idx % cols) * cell_w;
					int cy = (idx / cols) * cell_h;
					for (int y = cy + 2; y < cy + cell_h - 2 && y < 128; ++y) {
						for (int x = cx + 2; x < cx + cell_w - 2 && x < 128; ++x) {
							pixels[y * 128 + x] = c;
						}
					}
				}
			}
		} else if (type_idx == 2) {
			std::vector<uint8_t> stamp_cells;
			uint32_t sw = 0, sh = 0;
			if (!target_file.empty()) {
				SaveManager::load_stamp_from_file(target_file, target_set, stamp_cells, sw, sh);
			}
			if (stamp_cells.empty()) {
				const auto& clip = UI::get_clipboard();
				if (!clip.empty() && clip.width > 0 && clip.height > 0) {
					stamp_cells = clip.cells;
					sw = clip.width;
					sh = clip.height;
				}
			}

			if (!stamp_cells.empty() && sw > 0 && sh > 0) {
				for (int ty = 0; ty < 128; ++ty) {
					int sy = (ty * sh) / 128;
					for (int tx = 0; tx < 128; ++tx) {
						int sx = (tx * sw) / 128;
						uint8_t mid = stamp_cells[sy * sw + sx];
						if (mid != 0) {
							pixels[ty * 128 + tx] =
								SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, id_to_color[mid][0],
											id_to_color[mid][1], id_to_color[mid][2], 255);
						}
					}
				}
			}
		} else if (type_idx == 3) {
			const auto& style = ImGui::GetStyle();
			const auto& cfg = ConfigManager::get_config();
			ImVec4 bg_col = cfg.ui.background_color;
			ImVec4 win_col = style.Colors[ImGuiCol_WindowBg];
			ImVec4 btn_col = style.Colors[ImGuiCol_Button];
			ImVec4 act_col = style.Colors[ImGuiCol_ButtonActive];
			ImVec4 text_col = style.Colors[ImGuiCol_Text];

			for (int ty = 0; ty < 128; ++ty) {
				for (int tx = 0; tx < 128; ++tx) {
					ImVec4 col = (ty < 54)	 ? win_col
								 : (ty < 64) ? bg_col
								 : (tx < 42) ? btn_col
								 : (tx < 85) ? act_col
											 : text_col;
					pixels[ty * 128 + tx] =
						SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr,
									static_cast<uint8_t>(std::clamp(col.x, 0.0f, 1.0f) * 255.0f),
									static_cast<uint8_t>(std::clamp(col.y, 0.0f, 1.0f) * 255.0f),
									static_cast<uint8_t>(std::clamp(col.z, 0.0f, 1.0f) * 255.0f), 255);
				}
			}
		}

		std::string temp_png =
			"/tmp/sand3_thumb_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".png";
		SDL_SavePNG(surf, temp_png.c_str());
		SDL_DestroySurface(surf);

		std::string b64 = "";
		std::ifstream fin(temp_png, std::ios::binary);
		if (fin.is_open()) {
			std::vector<uint8_t> png_bytes((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
			fin.close();
			std::filesystem::remove(temp_png);
			if (!png_bytes.empty()) {
				static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				std::string enc;
				enc.reserve(((png_bytes.size() + 2) / 3) * 4);
				for (size_t i = 0; i < png_bytes.size(); i += 3) {
					uint32_t val = (static_cast<uint32_t>(png_bytes[i]) << 16);
					if (i + 1 < png_bytes.size())
						val |= (static_cast<uint32_t>(png_bytes[i + 1]) << 8);
					if (i + 2 < png_bytes.size())
						val |= static_cast<uint32_t>(png_bytes[i + 2]);
					enc.push_back(b64_chars[(val >> 18) & 0x3F]);
					enc.push_back(b64_chars[(val >> 12) & 0x3F]);
					enc.push_back((i + 1 < png_bytes.size()) ? b64_chars[(val >> 6) & 0x3F] : '=');
					enc.push_back((i + 2 < png_bytes.size()) ? b64_chars[val & 0x3F] : '=');
				}
				b64 = "data:image/png;base64," + enc;
			}
		}
		return b64;
	}

	static bool is_item_present_locally(const WorkshopItemClient& item) {
		if (item.type == "set") {
			std::string dir = std::string(SETS_DIRECTORY) + item.title;
			return std::filesystem::exists(dir) && std::filesystem::is_directory(dir) &&
				   !WorkshopCache::is_transient(dir);
		} else if (item.type == "save") {
			std::string parent_name =
				!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
			std::string path = std::string(SETS_DIRECTORY) + parent_name + "/saves/" + item.title + ".save";
			return std::filesystem::exists(path) && !WorkshopCache::is_transient(path);
		} else if (item.type == "stamp") {
			std::string parent_name =
				!item.parent_set_title.empty() ? item.parent_set_title : SetManager::get_current_set();
			std::string path = std::string(SETS_DIRECTORY) + parent_name + "/stamps/" + item.title + ".stamp";
			return std::filesystem::exists(path) && !WorkshopCache::is_transient(path);
		} else if (item.type == "theme") {
			return true;  // Not actually locally present but doesn't really matter anyway
		}
		return false;
	}
}  // namespace

void UI::refresh_workshop_items() { ::refresh_workshop_items(); }
void UI::validate_online_items(bool force) { ::validate_all_online_and_cached_items(force); }

void UI::handle_uri(const std::string& uri) {
	std::string item_id = "";
	std::string item_type = "";

	std::string s = uri;
	while (!s.empty() && (s.front() == '"' || s.front() == '\'' || s.front() == ' '))
		s.erase(0, 1);
	while (!s.empty() &&
		   (s.back() == '"' || s.back() == '\'' || s.back() == ' ' || s.back() == '\n' || s.back() == '\r'))
		s.pop_back();

	// Parse query parameters if present (?type=...&id=...)
	size_t qmark = s.find('?');
	if (qmark != std::string::npos) {
		std::string query = s.substr(qmark + 1);
		size_t pos = 0;
		while (pos < query.size()) {
			size_t next_amp = query.find('&', pos);
			std::string param = (next_amp == std::string::npos) ? query.substr(pos) : query.substr(pos, next_amp - pos);
			size_t eq = param.find('=');
			if (eq != std::string::npos) {
				std::string key = param.substr(0, eq);
				std::string val = param.substr(eq + 1);
				if (key == "id") {
					item_id = val;
				} else if (key == "type") {
					item_type = val;
				}
			}
			if (next_amp == std::string::npos)
				break;
			pos = next_amp + 1;
		}
	}

	if (item_id.empty()) {
		std::string path_part = (qmark != std::string::npos) ? s.substr(0, qmark) : s;
		if (path_part.rfind("sand3://", 0) == 0) {
			path_part = path_part.substr(8);
		}
		if (path_part.rfind("open/", 0) == 0)
			path_part = path_part.substr(5);
		else if (path_part == "open")
			path_part = "";
		else if (path_part.rfind("items/", 0) == 0)
			path_part = path_part.substr(6);
		else if (path_part.rfind("item/", 0) == 0)
			path_part = path_part.substr(5);

		while (!path_part.empty() && path_part.back() == '/')
			path_part.pop_back();
		while (!path_part.empty() && path_part.front() == '/')
			path_part.erase(0, 1);

		if (!path_part.empty()) {
			item_id = path_part;
		}
	}

	if (!item_id.empty()) {
		// 1. Force the workshop tab to be selected
		s_force_workshop_tab_selected = true;

		// 2. Set search filter to exact item ID
		std::memset(s_workshop_search, 0, sizeof(s_workshop_search));
		std::strncpy(s_workshop_search, item_id.c_str(), sizeof(s_workshop_search) - 1);

		// 3. Set type filter if provided, or reset to All Types
		if (item_type == "set") {
			s_workshop_type_filter = 1;
		} else if (item_type == "save") {
			s_workshop_type_filter = 2;
		} else if (item_type == "stamp") {
			s_workshop_type_filter = 3;
		} else if (item_type == "theme") {
			s_workshop_type_filter = 4;
		} else {
			s_workshop_type_filter = 0;	 // All Types
		}

		s_workshop_my_items = false;
		s_workshop_sort_idx = 0;

		// 4. Trigger refresh to fetch matching item by ID
		refresh_workshop_items();

		// 5. Raise and focus window
		if (Window::get_window()) {
			SDL_ShowWindow(Window::get_window());
			SDL_RaiseWindow(Window::get_window());
		}
	}
}

std::string UI::generate_thumbnail_base64(int type_idx, const std::string& target_set, const std::string& target_file) {
	return ::generate_thumbnail_base64(type_idx, target_set, target_file);
}

namespace {
	static bool s_show_set_override_warning = false;
	static WorkshopItemClient s_pending_override_item;
	static uint32_t s_pending_local_version = 1;
	static bool s_pending_is_update = false;
	static std::function<void()> s_pending_override_confirm = nullptr;

	void render_workshop_item_card(const WorkshopItemClient& item) {
		ImGui::PushID(item.id.c_str());

		ImVec4 border_col = ImVec4(0.2f, 0.25f, 0.32f, 0.8f);
		ImGui::PushStyleColor(ImGuiCol_Border, border_col);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

		ImGui::BeginChild(("CardChild_" + item.id).c_str(), ImVec2(0, 155.0f), true,
						  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		ImVec2 thumb_sz(110, 95);
		SDL_Texture* tex = get_or_load_thumbnail(item);
		if (tex) {
			ImGui::Image((ImTextureID)tex, thumb_sz);
		} else {
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetCursorScreenPos();
			ImVec2 p_max(p_min.x + thumb_sz.x, p_min.y + thumb_sz.y);
			dl->AddRectFilled(p_min, p_max, IM_COL32(16, 22, 30, 255), 4.0f);
			dl->AddRect(p_min, p_max, IM_COL32(48, 54, 61, 255), 4.0f);

			IconID ph_icon = IconID::Save;
			if (item.type == "set")
				ph_icon = IconID::Package;
			else if (item.type == "stamp")
				ph_icon = IconID::Select;
			SDL_Texture* ph_tex = IconManager::get(ph_icon);
			if (ph_tex) {
				dl->AddImage((ImTextureID)ph_tex,
							 ImVec2(p_min.x + thumb_sz.x * 0.5f - 12, p_min.y + thumb_sz.y * 0.5f - 12),
							 ImVec2(p_min.x + thumb_sz.x * 0.5f + 12, p_min.y + thumb_sz.y * 0.5f + 12));
			}
			ImGui::Dummy(thumb_sz);
		}

		ImGui::SameLine(0, 14);

		ImGui::BeginGroup();

		IconID type_icon = IconID::Save;
		ImVec4 type_col = ImVec4(0.3f, 0.8f, 0.4f, 1.0f);
		const char* type_str = "SAVE";
		if (item.type == "set") {
			type_icon = IconID::Package;
			type_col = ImVec4(0.4f, 0.65f, 1.0f, 1.0f);
			type_str = "SET";
		} else if (item.type == "stamp") {
			type_icon = IconID::Select;
			type_col = ImVec4(0.9f, 0.75f, 0.2f, 1.0f);
			type_str = "STAMP";
		} else if (item.type == "theme") {
			type_icon = IconID::Fill;
			type_col = ImVec4(0.8f, 0.45f, 0.95f, 1.0f);
			type_str = "THEME";
		}

		SDL_Texture* t_tex = IconManager::get(type_icon);
		if (t_tex) {
			ImGui::Image((ImTextureID)t_tex, ImVec2(14, 14));
			ImGui::SameLine(0, 4);
		}
		ImGui::TextColored(type_col, "[%s]", type_str);
		ImGui::SameLine();
		if (item.version > 1) {
			ImGui::TextColored(ImVec4(0.6f, 0.65f, 0.75f, 1.0f), "v%d", item.version);
			ImGui::SameLine();
		}
		ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", item.title.c_str());
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "by %s", item.author.c_str());

		bool is_owner = WorkshopClient::is_logged_in() && !item.author.empty() &&
						(WorkshopClient::get_logged_in_username() == item.author);
		if (is_owner) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[Owner]");
		}

		if (!item.parent_set_title.empty()) {
			ImGui::SameLine(0, 12);
			SDL_Texture* tag_tex = IconManager::get(IconID::Tag);
			if (tag_tex) {
				ImGui::Image((ImTextureID)tag_tex, ImVec2(12, 12));
				ImGui::SameLine(0, 4);
			}
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "Uses Set: %s", item.parent_set_title.c_str());
		}

		if (!item.description.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
			ImGui::TextWrapped("%s", item.description.c_str());
			ImGui::PopStyleColor();
		} else {
			ImGui::TextDisabled("No description provided.");
		}

		if (item.meta.contains("changelog") && item.meta["changelog"].is_array() && !item.meta["changelog"].empty()) {
			const auto& cl = item.meta["changelog"];
			if (cl[0].contains("notes") && cl[0]["notes"].is_string()) {
				std::string latest_notes = cl[0]["notes"].get<std::string>();
				if (!latest_notes.empty()) {
					int ver = cl[0].value("version", item.version);
					ImGui::TextColored(ImVec4(0.5f, 0.7f, 0.9f, 1.0f), "v%d changes: %s", ver, latest_notes.c_str());
				}
			}
		}

		ImGui::Spacing();
		if (item.type == "set") {
			int mat_count = 0;
			if (item.meta.contains("materials_count") && item.meta["materials_count"].is_number()) {
				mat_count = item.meta["materials_count"].get<int>();
			}
			if (mat_count > 0) {
				ImGui::TextColored(ImVec4(0.55f, 0.8f, 0.55f, 1.0f), "%d materials", mat_count);
				ImGui::SameLine(0, 10);
			}
		} else {
			int w = 0, h = 0;
			if (item.meta.contains("width") && item.meta["width"].is_number()) {
				w = item.meta["width"].get<int>();
			}
			if (item.meta.contains("height") && item.meta["height"].is_number()) {
				h = item.meta["height"].get<int>();
			}
			if (w > 0 && h > 0) {
				ImGui::TextColored(ImVec4(0.5f, 0.75f, 1.0f, 1.0f), "%d×%d px", w, h);
				ImGui::SameLine(0, 10);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool is_local = is_item_present_locally(item);
		uint32_t local_version = 1;
		bool is_update = false;
		if (item.type == "set" && is_local) {
			SetMetadata local_m = SetManager::load_set_metadata(item.title);
			local_version = local_m.version;
			if (item.version > static_cast<int>(local_version)) {
				is_update = true;
			}
		}

		std::string try_label = (item.type == "set")
									? (is_local ? (is_update ? "Update Set" : "Switch to Set") : "Switch to Set")
								: (item.type == "stamp") ? "Load Stamp"
								: (item.type == "theme") ? "Apply Theme"
														 : "Play / Load";
		if (UI::button_with_icon(try_label.c_str(), IconManager::get(IconID::Play), ImVec2(0, 22))) {
			if (item.type == "set" && is_local) {
				if (is_update) {
					s_pending_override_item = item;
					s_pending_local_version = local_version;
					s_pending_is_update = true;
					s_pending_override_confirm = [item]() {
						std::string zip_path = WorkshopCache::get_cache_path(item.id, "set", "set.zip");
						WorkshopClient::download_item(item.id, zip_path, [item](bool success, const std::string& p) {
							if (success) {
								std::string dest_dir = std::string(SETS_DIRECTORY) + item.title;
								ZipUtil::extract_zip(p, dest_dir);
								SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version);
								SetManager::mark_set_online(item.title, true);
								SetManager::set_current_set(item.title);
								ToastManager::success("Set '" + item.title + "' updated to v" +
													  std::to_string(item.version) + "!");
							} else {
								ToastManager::error("Failed to download set update.");
							}
						});
					};
					s_show_set_override_warning = true;
					return;
				} else {
					if (SetManager::get_current_set() != item.title) {
						SetManager::set_current_set(item.title);
					}
					ToastManager::info("Switched to local set '" + item.title + "'!");
					return;
				}
			}

			if (item.type == "set") {
				std::string target_set_dir = std::string(SETS_DIRECTORY) + item.title;
				if (std::filesystem::exists(target_set_dir) && !WorkshopCache::is_transient(target_set_dir)) {
					SetMetadata local_m = SetManager::load_set_metadata(item.title);
					s_pending_override_item = item;
					s_pending_local_version = local_m.version;
					s_pending_is_update = false;
					s_pending_override_confirm = [item, target_set_dir]() {
						std::string zip_path = WorkshopCache::get_cache_path(item.id, "set", "set.zip");
						WorkshopClient::download_item(
							item.id, zip_path, [item, target_set_dir](bool success, const std::string& p) {
								if (success) {
									ZipUtil::extract_zip(p, target_set_dir);
									SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version);
									SetManager::mark_set_online(item.title, true);
									SetManager::set_current_set(item.title);
									ToastManager::success("Set '" + item.title + "' overwritten & activated!");
								}
							});
					};
					s_show_set_override_warning = true;
					return;
				}
			}

			std::string ext = (item.type == "save")	   ? ".save"
							  : (item.type == "stamp") ? ".stamp"
							  : (item.type == "theme") ? ".theme"
													   : ".zip";
			std::string cache_path = WorkshopCache::get_cache_path(item.id, item.type, "item" + ext);

			WorkshopClient::download_item(
				item.id, cache_path, [item, cache_path](bool success, const std::string& path) {
					if (success) {
						WorkshopCache::register_transient(path);

						auto load_content = [item, path]() {
							if (item.type == "save") {
								SaveManager::load_from_file_async(
									path, SetManager::get_current_set(), LoadPlacement::Center,
									[](bool s_ok, const std::string&) {
										if (s_ok) {
											ToastManager::info("Save loaded into sandbox! (Temporary cache, deleted on "
															   "exit unless kept)");
										}
									});
							} else if (item.type == "stamp") {
								SaveManager::load_stamp_from_file_async(
									path, SetManager::get_current_set(),
									[](bool s_ok, const std::vector<uint8_t>& cells, uint32_t w, uint32_t h) {
										if (s_ok) {
											UI::load_stamp(cells, w, h);
											ToastManager::info(
												"Stamp loaded into brush! (Cached temporarily until exit)");
										}
									});
							} else if (item.type == "set") {
								std::string target_set_dir = std::string(SETS_DIRECTORY) + item.title;
								ZipUtil::extract_zip(path, target_set_dir);
								WorkshopCache::register_transient(target_set_dir);
								SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version,
															  item.author);
								SetManager::mark_set_online(item.title, true);
								SetManager::set_current_set(item.title);
								ToastManager::info("Set '" + item.title +
												   "' activated! (Cached temporarily until exit)");
							} else if (item.type == "theme") {
								std::ifstream fin(path);
								if (fin.is_open()) {
									try {
										nlohmann::json tj = nlohmann::json::parse(fin);
										auto& cfg = ConfigManager::get_config();
										auto& style = ImGui::GetStyle();
										if (tj.contains("window_rounding")) {
											cfg.ui.window_rounding = tj["window_rounding"].get<float>();
											style.WindowRounding = cfg.ui.window_rounding;
										}
										if (tj.contains("frame_rounding")) {
											cfg.ui.frame_rounding = tj["frame_rounding"].get<float>();
											style.FrameRounding = cfg.ui.frame_rounding;
											style.ChildRounding = cfg.ui.frame_rounding;
											style.PopupRounding = cfg.ui.frame_rounding;
											style.GrabRounding = cfg.ui.frame_rounding;
											style.TabRounding = cfg.ui.frame_rounding;
										}
										if (tj.contains("button_size"))
											cfg.ui.button_size = tj["button_size"].get<int>();
										if (tj.contains("icon_size"))
											cfg.ui.icon_size = tj["icon_size"].get<int>();
										if (tj.contains("sidebar_width"))
											cfg.ui.sidebar_width = tj["sidebar_width"].get<int>();
										if (tj.contains("material_list_height"))
											cfg.ui.material_list_height = tj["material_list_height"].get<int>();
										if (tj.contains("background_color") && tj["background_color"].is_array() &&
											tj["background_color"].size() >= 4) {
											cfg.ui.background_color =
												ImVec4(tj["background_color"][0], tj["background_color"][1],
													   tj["background_color"][2], tj["background_color"][3]);
											Window::set_background_color(cfg.ui.background_color);
										}
										if (tj.contains("selection_box_color") &&
											tj["selection_box_color"].is_array() &&
											tj["selection_box_color"].size() >= 4) {
											cfg.ui.selection_box_color =
												ImVec4(tj["selection_box_color"][0], tj["selection_box_color"][1],
													   tj["selection_box_color"][2], tj["selection_box_color"][3]);
										}
										if (tj.contains("selection_box_fill") && tj["selection_box_fill"].is_array() &&
											tj["selection_box_fill"].size() >= 4) {
											cfg.ui.selection_box_fill =
												ImVec4(tj["selection_box_fill"][0], tj["selection_box_fill"][1],
													   tj["selection_box_fill"][2], tj["selection_box_fill"][3]);
										}
										if (tj.contains("colors") && tj["colors"].is_object()) {
											for (auto& [cname, val] : tj["colors"].items()) {
												if (val.is_string()) {
													std::string hex = val.get<std::string>();
													ConfigManager::get_color_overrides()[cname] = hex;
													for (int ci = 0; ci < ImGuiCol_COUNT; ++ci) {
														if (cname == ImGui::GetStyleColorName(ci)) {
															ImVec4 parsed;
															if (ConfigManager::parse_color_string(hex, parsed)) {
																style.Colors[ci] = parsed;
															}
															break;
														}
													}
												}
											}
										}
										ConfigManager::save();
										ToastManager::success("Theme '" + item.title + "' applied!");
									} catch (...) {
										ToastManager::error("Failed to parse theme file.");
									}
								}
							}
						};

						if ((item.type == "save" || item.type == "stamp") && !item.parent_set_id.empty() &&
							!item.parent_set_title.empty()) {
							std::string parent_name = item.parent_set_title;
							std::string parent_dir = std::string(SETS_DIRECTORY) + parent_name;
							if (!std::filesystem::exists(parent_dir) ||
								!std::filesystem::exists(parent_dir + "/set_config.ini")) {
								std::string parent_zip =
									WorkshopCache::get_cache_path(item.parent_set_id, "set", "set.zip");
								WorkshopClient::download_item(
									item.parent_set_id, parent_zip,
									[item, parent_name, path, load_content](bool set_ok, const std::string& zpath) {
										if (set_ok) {
											std::string target_dir = std::string(SETS_DIRECTORY) + parent_name;
											ZipUtil::extract_zip(zpath, target_dir);
											WorkshopCache::register_transient(target_dir);
											SetManager::set_workshop_info(parent_name, item.parent_set_id,
																		  item.set_hash, 1);
											SetManager::mark_set_online(parent_name, true);
											SetManager::set_current_set(parent_name);
										}
										load_content();
									});
								return;
							} else {
								if (SetManager::get_current_set() != parent_name) {
									SetManager::set_current_set(parent_name);
								}
							}
						}

						load_content();
					} else {
						ToastManager::error("Failed to download item from workshop server.");
					}
				});
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Loads item into game temporarily.\nDeleted on exit unless 'Download' is clicked.");
		}

		if (!is_local) {
			ImGui::SameLine();
			if (UI::button_with_icon("Download", IconManager::get(IconID::Save), ImVec2(0, 22))) {
				bool ok = WorkshopCache::promote_to_local(item.id, item.type, item.title, SetManager::get_current_set(),
														  item.set_hash);
				if (ok) {
					if (item.type == "set") {
						SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version, item.author);
					}
					ToastManager::success("Saved to local storage!");
				} else {
					if (item.type == "save") {
						std::string dest_dir = std::string(SETS_DIRECTORY) + SetManager::get_current_set() + "/saves/";
						std::filesystem::create_directories(dest_dir);
						std::string dest_path = dest_dir + item.title + ".save";
						WorkshopClient::download_item(item.id, dest_path, [item](bool success, const std::string&) {
							if (success) {
								SaveManager::set_save_workshop_info(item.title, SetManager::get_current_set(), item.id,
																	item.set_hash, item.author, item.version);
								ToastManager::success("Saved to local saves folder!");
							}
						});
					} else if (item.type == "stamp") {
						std::string dest_dir = std::string(SETS_DIRECTORY) + SetManager::get_current_set() + "/stamps/";
						std::filesystem::create_directories(dest_dir);
						std::string dest_path = dest_dir + item.title + ".stamp";
						WorkshopClient::download_item(item.id, dest_path, [item](bool success, const std::string&) {
							if (success) {
								SaveManager::set_stamp_workshop_info(item.title, SetManager::get_current_set(), item.id,
																	 item.set_hash, item.author, item.version);
								ToastManager::success("Saved to local stamps folder!");
							}
						});
					} else if (item.type == "set") {
						std::string zip_path = WorkshopCache::get_cache_path(item.id, "set", "set.zip");
						WorkshopClient::download_item(item.id, zip_path, [item](bool success, const std::string& p) {
							if (success) {
								std::string dest_dir = std::string(SETS_DIRECTORY) + item.title;
								ZipUtil::extract_zip(p, dest_dir);
								SetManager::set_workshop_info(item.title, item.id, item.set_hash, item.version,
															  item.author);
								SetManager::mark_set_online(item.title, true);
								ToastManager::success("Saved set to local sets folder!");
							}
						});
					}
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Saves this item permanently to disk so it persists across sessions.");
			}
		}

		ImGui::SameLine();
		char like_btn_lbl[32];
		std::snprintf(like_btn_lbl, sizeof(like_btn_lbl), "%d##like_%s", item.likes_count, item.id.c_str());
		if (item.is_liked) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
		}
		if (UI::button_with_icon(like_btn_lbl, IconManager::get(IconID::Heart), ImVec2(0, 22))) {
			std::string item_id = item.id;
			WorkshopClient::toggle_like(item_id, [item_id](bool success, bool is_liked, int count) {
				if (success) {
					for (auto& it : s_workshop_items) {
						if (it.id == item_id) {
							it.is_liked = is_liked;
							it.likes_count = count;
							break;
						}
					}
				}
			});
		}
		if (item.is_liked) {
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();
		char fav_btn_lbl[32];
		std::snprintf(fav_btn_lbl, sizeof(fav_btn_lbl), "%d##fav_%s", item.favorites_count, item.id.c_str());
		if (item.is_favorited) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));
		}
		if (UI::button_with_icon(fav_btn_lbl, IconManager::get(IconID::Star), ImVec2(0, 22))) {
			std::string item_id = item.id;
			WorkshopClient::toggle_favorite(item_id, [item_id](bool success, bool is_fav, int count) {
				if (success) {
					for (auto& it : s_workshop_items) {
						if (it.id == item_id) {
							it.is_favorited = is_fav;
							it.favorites_count = count;
							break;
						}
					}
				}
			});
		}
		if (item.is_favorited) {
			ImGui::PopStyleColor();
		}

		ImGui::SameLine();
		if (UI::button_with_icon("##rep", IconManager::get(IconID::Report), ImVec2(22, 22))) {
			s_show_report_modal = true;
			s_report_item_id = item.id;
			s_report_item_title = item.title;
			s_report_status = "";
			s_report_details[0] = '\0';
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Report this item");
		}

		if (is_owner) {
			ImGui::SameLine();
			if (UI::button_with_icon(("##edit_" + item.id).c_str(), IconManager::get(IconID::Edit), ImVec2(22, 22))) {
				WorkshopItemEditor::open_edit_modal(item, []() { ::refresh_workshop_items(); });
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Edit title and description");
			}

			ImGui::SameLine();
			if (UI::button_with_icon(("##update_" + item.id).c_str(), IconManager::get(IconID::Update),
									 ImVec2(22, 22))) {
				WorkshopItemEditor::open_update_modal(item, []() { ::refresh_workshop_items(); });
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Publish a new version (with changelog)");
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			if (UI::button_with_icon(("##del_" + item.id).c_str(), IconManager::get(IconID::Delete), ImVec2(22, 22))) {
				WorkshopItemEditor::open_delete_modal(item, []() { ::refresh_workshop_items(); });
			}
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete this item from Workshop");
			}
		}

		ImGui::EndGroup();

		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
		ImGui::PopID();
	}
}  // namespace

void UI::render_workshop() {
	ImGuiTabItemFlags tab_flags = 0;
	if (s_force_workshop_tab_selected) {
		tab_flags |= ImGuiTabItemFlags_SetSelected;
		s_force_workshop_tab_selected = false;
	}
	if (ImGui::BeginTabItem("Workshop", nullptr, tab_flags)) {
		ImGuiIO& io = ImGui::GetIO();

		if (!s_workshop_loaded_once && !s_workshop_loading) {
			refresh_workshop_items();
		}

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.0f, 1.0f), "Community Workshop");
		ImGui::TextDisabled("Browse, load, and share community items.");

		if (WorkshopClient::is_logged_in()) {
			SDL_Texture* u_icon = IconManager::get(IconID::User);
			if (u_icon) {
				ImGui::Image((ImTextureID)u_icon, ImVec2(14, 14));
				ImGui::SameLine(0, 4);
			}
			ImGui::Text("User: %s", WorkshopClient::get_logged_in_username().c_str());
			ImGui::SameLine(0, 8);
			if (ImGui::SmallButton("Log out")) {
				WorkshopClient::logout();
				ConfigManager::get_config().workshop.token = "";
				ConfigManager::get_config().workshop.username = "";
				ConfigManager::save();
				s_workshop_my_items = false;
				refresh_workshop_items();
			}
			ImGui::SameLine(0, 10);
			if (ImGui::Checkbox("My items Only", &s_workshop_my_items)) {
				refresh_workshop_items();
			}
		} else {
			if (ImGui::SmallButton("Sign in / Register")) {
				s_show_auth_modal = true;
				s_auth_status = "";
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float avail_w = ImGui::GetContentRegionAvail().x;
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputTextWithHint("##ws_sidebar_search", "Search items...", s_workshop_search,
									 sizeof(s_workshop_search), ImGuiInputTextFlags_EnterReturnsTrue)) {
			refresh_workshop_items();
		}

		const char* type_labels[] = {"All Types", "Sets", "Saves", "Stamps", "Themes"};
		ImGui::SetNextItemWidth(avail_w * 0.48f);
		if (ImGui::Combo("##ws_type_filter", &s_workshop_type_filter, type_labels, IM_ARRAYSIZE(type_labels))) {
			refresh_workshop_items();
		}
		ImGui::SameLine();
		const char* sort_labels[] = {"Popular", "Most Liked", "Favorites", "Newest", "Downloads"};
		ImGui::SetNextItemWidth(-1);
		if (ImGui::Combo("##ws_sort_filter", &s_workshop_sort_idx, sort_labels, IM_ARRAYSIZE(sort_labels))) {
			refresh_workshop_items();
		}

		ImGui::Spacing();
		ImGui::BeginChild("WorkshopSidebarScroll", ImVec2(0, -25), true);
		if (s_workshop_loading) {
			ImGui::TextDisabled("Loading items...");
		} else if (!s_workshop_error.empty()) {
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "%s", s_workshop_error.c_str());
			if (ImGui::Button("Retry")) {
				refresh_workshop_items();
			}
		} else if (s_workshop_items.empty()) {
			ImGui::TextDisabled("No items found.");
		} else {
			for (const auto& it : s_workshop_items) {
				render_workshop_item_card(it);
				ImGui::Spacing();
			}
		}
		ImGui::EndChild();

		ImGui::EndTabItem();
	}
}

void UI::render_workshop_auth_modal() {
	if (!s_show_auth_modal)
		return;

	ImGui::OpenPopup("Workshop Authentication##Modal");
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(380, 280), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal("Workshop Authentication##Modal", &s_show_auth_modal,
							   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::TextColored(ImVec4(0.35f, 0.65f, 1.0f, 1.0f),
						   s_auth_is_register ? "Create Workshop Account" : "Sign In to Workshop");
		ImGui::Separator();
		ImGui::Spacing();

		if (!s_auth_status.empty()) {
			ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "%s", s_auth_status.c_str());
			ImGui::Spacing();
		}

		ImGui::Text("Username:");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##auth_user", s_auth_username, sizeof(s_auth_username));

		ImGui::Spacing();
		ImGui::Text("Password:");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##auth_pass", s_auth_password, sizeof(s_auth_password), ImGuiInputTextFlags_Password);

		ImGui::Spacing();
		if (s_auth_is_register) {
			if (ImGui::SmallButton("Already have an account? Sign in instead")) {
				s_auth_is_register = false;
				s_auth_status = "";
			}
		} else {
			if (ImGui::SmallButton("Don't have an account? Register new account")) {
				s_auth_is_register = true;
				s_auth_status = "";
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		std::string submit_label = s_auth_is_register ? "Create Account" : "Sign In";
		if (ImGui::Button(submit_label.c_str(), ImVec2(140, 28))) {
			std::string u = s_auth_username;
			std::string p = s_auth_password;
			if (u.empty() || p.empty()) {
				s_auth_status = "Please provide both username and password.";
			} else {
				if (s_auth_is_register) {
					WorkshopClient::register_user(u, p, [u](bool success, const std::string& err) {
						if (success) {
							s_show_auth_modal = false;
							s_auth_password[0] = '\0';
							ConfigManager::get_config().workshop.token = WorkshopClient::get_auth_token();
							ConfigManager::get_config().workshop.username = u;
							ConfigManager::save();
							ToastManager::success("Welcome, " + u + "! Registered and logged in.");
						} else {
							s_auth_status = err;
						}
					});
				} else {
					WorkshopClient::login(u, p, [u](bool success, const std::string& err) {
						if (success) {
							s_show_auth_modal = false;
							s_auth_password[0] = '\0';
							ConfigManager::get_config().workshop.token = WorkshopClient::get_auth_token();
							ConfigManager::get_config().workshop.username = u;
							ConfigManager::save();
							ToastManager::success("Welcome back, " + u + "!");
						} else {
							s_auth_status = err;
						}
					});
				}
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 28))) {
			s_show_auth_modal = false;
			s_auth_password[0] = '\0';
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

namespace {
	void check_and_render_report_modal() {
		if (s_show_report_modal) {
			ImGui::OpenPopup("Report Item##ModalDialog");
		}
		if (ImGui::BeginPopupModal("Report Item##ModalDialog", &s_show_report_modal,
								   ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Report item: \"%s\"", s_report_item_title.c_str());
			ImGui::Separator();
			ImGui::Spacing();

			if (!s_report_status.empty()) {
				ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.5f, 1.0f), "%s", s_report_status.c_str());
				ImGui::Spacing();
				if (ImGui::Button("Close", ImVec2(100, 24))) {
					s_show_report_modal = false;
					ImGui::CloseCurrentPopup();
				}
			} else {
				ImGui::Text("Reason:");
				const char* reasons[] = {"Broken simulation / Game crashes", "Inappropriate or offensive content",
										 "Spam or duplicate", "Other"};
				ImGui::Combo("##report_reason", &s_report_reason_idx, reasons, IM_ARRAYSIZE(reasons));

				ImGui::Spacing();
				ImGui::Text("Details (optional):");
				ImGui::InputTextMultiline("##report_details", s_report_details, sizeof(s_report_details),
										  ImVec2(350, 70));

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				if (UI::button_with_icon("Submit Report", IconManager::get(IconID::Report), ImVec2(130, 26))) {
					const char* reason_keys[] = {"broken", "offensive", "spam", "other"};
					WorkshopClient::submit_report(
						s_report_item_id, reason_keys[s_report_reason_idx], s_report_details,
						[](bool success, const std::string& msg) {
							if (success) {
								s_report_status = "Thank you! Report received.";
							} else {
								s_report_status = "Failed to submit report. Please check internet connection.";
							}
						});
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(90, 24))) {
					s_show_report_modal = false;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}
	}

	void check_and_render_publish_modal() {
		if (s_show_publish_modal) {
			ImGui::OpenPopup("Share to Workshop##ModalDialog");
		}
		if (ImGui::BeginPopupModal("Share to Workshop##ModalDialog", &s_show_publish_modal,
								   ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::TextColored(ImVec4(0.35f, 0.7f, 1.0f, 1.0f), "Publish Item to Community Workshop");
			ImGui::Separator();
			ImGui::Spacing();

			if (!s_publish_status.empty()) {
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "%s", s_publish_status.c_str());
				ImGui::Spacing();
				if (ImGui::Button("Done", ImVec2(100, 24))) {
					s_show_publish_modal = false;
					ImGui::CloseCurrentPopup();
					refresh_workshop_items();
				}
			} else {
				ImGui::Text("What to share:");
				const char* pub_types[] = {"Save File", "Material Set", "Stamp Prefab", "Custom Theme"};
				ImGui::Combo("##pub_type", &s_publish_type_idx, pub_types, IM_ARRAYSIZE(pub_types));

				if (s_publish_type_idx == 0) {
					if (!s_publish_target_save.empty()) {
						ImGui::TextDisabled("Target Save: %s (Set: %s)", s_publish_target_save.c_str(),
											s_publish_target_set.c_str());
					} else {
						ImGui::TextDisabled("Target Save: [Current Simulation Grid] (Set: %s)",
											(s_publish_target_set.empty() ? SetManager::get_current_set().c_str()
																		  : s_publish_target_set.c_str()));
					}
				} else if (s_publish_type_idx == 1) {
					auto all_sets = SetManager::get_sets();
					if (s_publish_target_set.empty() && !all_sets.empty()) {
						s_publish_target_set = SetManager::get_current_set();
					}
					int current_set_idx = 0;
					std::vector<const char*> set_cstrs;
					for (size_t i = 0; i < all_sets.size(); ++i) {
						set_cstrs.push_back(all_sets[i].c_str());
						if (all_sets[i] == s_publish_target_set) {
							current_set_idx = static_cast<int>(i);
						}
					}
					ImGui::Text("Target Set:");
					ImGui::SameLine();
					if (ImGui::Combo("##pub_target_set", &current_set_idx, set_cstrs.data(),
									 static_cast<int>(set_cstrs.size()))) {
						s_publish_target_set = all_sets[current_set_idx];
						std::snprintf(s_publish_title, sizeof(s_publish_title), "%s", s_publish_target_set.c_str());
						SetMetadata sm = SetManager::load_set_metadata(s_publish_target_set);
						std::snprintf(s_publish_desc, sizeof(s_publish_desc), "%s", sm.description.c_str());
					}
				} else if (s_publish_type_idx == 2) {
					if (!s_publish_target_stamp.empty()) {
						ImGui::TextDisabled("Target Stamp: %s (Set: %s)", s_publish_target_stamp.c_str(),
											s_publish_target_set.c_str());
					} else {
						ImGui::TextDisabled("Target Stamp: [Current Selection / Clipboard] (Set: %s)",
											(s_publish_target_set.empty() ? SetManager::get_current_set().c_str()
																		  : s_publish_target_set.c_str()));
					}
				} else if (s_publish_type_idx == 3) {
					ImGui::TextDisabled("Includes your current UI colors, window/frame rounding, button sizes, etc.");
				}

				ImGui::Spacing();
				ImGui::Text("Item Title *");
				ImGui::InputText("##pub_title", s_publish_title, sizeof(s_publish_title));

				ImGui::Spacing();
				ImGui::Text("Author / Creator *");
				ImGui::InputText("##pub_author", s_publish_author, sizeof(s_publish_author));

				ImGui::Spacing();
				ImGui::Text("Description:");
				ImGui::InputTextMultiline("##pub_desc", s_publish_desc, sizeof(s_publish_desc), ImVec2(360, 60));

				ImGui::Spacing();

				std::string cur_set =
					s_publish_target_set.empty() ? SetManager::get_current_set() : s_publish_target_set;
				SetMetadata cur_m = SetManager::load_set_metadata(cur_set);
				if (s_publish_type_idx == 0 || s_publish_type_idx == 2) {
					if (!cur_m.workshop_id.empty()) {
						ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f),
										   "Attached to Online Set: '%s' (Hash: %s...)", cur_set.c_str(),
										   cur_m.workshop_hash.substr(0, 8).c_str());
					} else {
						ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
										   "Set '%s' is local. Will be published as independent item.",
										   cur_set.c_str());
					}
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				if (s_publishing) {
					ImGui::Text("Uploading and publishing item...");
				} else {
					if (UI::button_with_icon("Upload & Publish", IconManager::get(IconID::Transmit), ImVec2(150, 26))) {
						s_publishing = true;
						std::string cur_s =
							s_publish_target_set.empty() ? SetManager::get_current_set() : s_publish_target_set;
						SetMetadata set_meta = SetManager::load_set_metadata(cur_s);

						if (s_publish_type_idx == 0) {
							std::vector<uint8_t> bytes;
							uint32_t sw = Grid::get_width();
							uint32_t sh = Grid::get_height();
							if (s_publish_target_w > 0)
								sw = s_publish_target_w;
							if (s_publish_target_h > 0)
								sh = s_publish_target_h;

							if (!s_publish_target_save.empty()) {
								std::string fpath = SaveManager::get_saves_directory(cur_s) + s_publish_target_save;
								std::ifstream fin(fpath, std::ios::binary);
								if (fin.is_open()) {
									bytes.assign((std::istreambuf_iterator<char>(fin)),
												 std::istreambuf_iterator<char>());
									fin.close();
								}
							}
							if (bytes.empty()) {
								SaveManager::save_to_file("__ws_temp_pub", cur_s);
								std::string fpath = SaveManager::get_saves_directory(cur_s) + "__ws_temp_pub.save";
								std::ifstream fin(fpath, std::ios::binary);
								if (fin.is_open()) {
									bytes.assign((std::istreambuf_iterator<char>(fin)),
												 std::istreambuf_iterator<char>());
									fin.close();
								}
								std::filesystem::remove(fpath);
							}

							std::string parent_id = "";
							std::string set_hash = "";
							if (!set_meta.workshop_id.empty()) {
								parent_id = set_meta.workshop_id;
								set_hash = SetManager::compute_set_hash(cur_s);
							}

							std::string thumb_b64 = generate_thumbnail_base64(0, cur_s, s_publish_target_save);
							nlohmann::json meta;
							meta["width"] = sw;
							meta["height"] = sh;
							meta["set_name"] = cur_s;

							WorkshopClient::publish_item(
								"save", s_publish_title, s_publish_desc, s_publish_author, parent_id, bytes, ".save",
								meta.dump(), set_hash, thumb_b64,
								[cur_s, s_target_save = std::string(s_publish_target_save),
								 s_title = std::string(s_publish_title), set_hash,
								 s_author = std::string(s_publish_author)](bool success, const std::string& created_id,
																		   const std::string& err) {
									s_publishing = false;
									if (success) {
										std::string save_name = s_target_save.empty() ? s_title : s_target_save;
										SaveManager::set_save_workshop_info(save_name, cur_s, created_id, set_hash,
																			s_author, 1);
										s_publish_status = "Published successfully! Available in workshop.";
										ToastManager::success("Save published to Workshop!");
									} else {
										s_publish_status = "Upload failed: " + err;
									}
								});
						} else if (s_publish_type_idx == 1) {
							if (cur_s == SetManager::get_current_set()) {
								MaterialManager::save_all_materials(std::string(SETS_DIRECTORY) + cur_s);
							}
							std::string set_dir = std::string(SETS_DIRECTORY) + cur_s;
							std::vector<std::pair<std::string, std::vector<uint8_t>>> set_files;
							for (const auto& entry : std::filesystem::directory_iterator(set_dir)) {
								if (entry.is_regular_file()) {
									std::string fname = entry.path().filename().string();
									std::string ext = entry.path().extension().string();
									if (ext == ".mat" || fname == "set_config.ini") {
										std::ifstream in(entry.path().string(), std::ios::binary);
										if (in.is_open()) {
											std::vector<uint8_t> fbytes((std::istreambuf_iterator<char>(in)),
																		std::istreambuf_iterator<char>());
											set_files.emplace_back(fname, fbytes);
										}
									}
								}
							}
							std::vector<uint8_t> zip_bytes = ZipUtil::create_zip(set_files);
							std::string set_hash = SetManager::compute_set_hash(cur_s);
							std::string thumb_b64 = generate_thumbnail_base64(1, cur_s);

							auto set_mats = get_materials_for_set(cur_s);
							int non_empty_count = 0;
							for (const auto& m : set_mats) {
								if (m.id != 0) {
									non_empty_count++;
								}
							}

							nlohmann::json meta;
							meta["materials_count"] = non_empty_count;
							meta["set_name"] = cur_s;

							WorkshopClient::publish_item(
								"set", s_publish_title, s_publish_desc, s_publish_author, "", zip_bytes, ".zip",
								meta.dump(), set_hash, thumb_b64,
								[cur_s, set_hash, s_author = std::string(s_publish_author)](
									bool success, const std::string& created_id, const std::string& err) {
									s_publishing = false;
									if (success) {
										SetManager::set_workshop_info(cur_s, created_id, set_hash, 1, s_author);
										SetManager::mark_set_online(cur_s, true);
										s_publish_status = "Set published successfully and linked to workshop!";
										ToastManager::success("Set published to Workshop!");
									} else {
										s_publish_status = "Upload failed: " + err;
									}
								});
						} else if (s_publish_type_idx == 2) {
							std::vector<uint8_t> stamp_bytes;
							uint32_t sw = s_publish_target_w > 0 ? s_publish_target_w : 64;
							uint32_t sh = s_publish_target_h > 0 ? s_publish_target_h : 64;

							if (!s_publish_target_stamp.empty()) {
								std::string fpath =
									std::string(SETS_DIRECTORY) + cur_s + "/stamps/" + s_publish_target_stamp;
								std::ifstream fin(fpath, std::ios::binary);
								if (fin.is_open()) {
									stamp_bytes.assign((std::istreambuf_iterator<char>(fin)),
													   std::istreambuf_iterator<char>());
									fin.close();
								}
							}
							if (stamp_bytes.empty()) {
								const auto& clip = UI::get_clipboard();
								if (!clip.empty() && clip.width > 0 && clip.height > 0) {
									sw = clip.width;
									sh = clip.height;
									SaveManager::save_stamp_to_file("__ws_temp_stamp", cur_s, clip.cells, sw, sh);
									std::string fpath =
										std::string(SETS_DIRECTORY) + cur_s + "/stamps/__ws_temp_stamp.stamp";
									std::ifstream fin(fpath, std::ios::binary);
									if (fin.is_open()) {
										stamp_bytes.assign((std::istreambuf_iterator<char>(fin)),
														   std::istreambuf_iterator<char>());
										fin.close();
									}
									std::filesystem::remove(fpath);
								} else {
									std::vector<uint8_t> single_cell = {UI::get_selected_id()};
									SaveManager::save_stamp_to_file("__ws_temp_stamp", cur_s, single_cell, 1, 1);
									std::string fpath =
										std::string(SETS_DIRECTORY) + cur_s + "/stamps/__ws_temp_stamp.stamp";
									std::ifstream fin(fpath, std::ios::binary);
									if (fin.is_open()) {
										stamp_bytes.assign((std::istreambuf_iterator<char>(fin)),
														   std::istreambuf_iterator<char>());
										fin.close();
									}
									std::filesystem::remove(fpath);
									sw = 1;
									sh = 1;
								}
							}

							std::string parent_id = "";
							std::string set_hash = "";
							if (!set_meta.workshop_id.empty()) {
								parent_id = set_meta.workshop_id;
								set_hash = SetManager::compute_set_hash(cur_s);
							}

							std::string thumb_b64 = generate_thumbnail_base64(2, cur_s, s_publish_target_stamp);
							nlohmann::json meta;
							meta["width"] = sw;
							meta["height"] = sh;
							meta["set_name"] = cur_s;

							WorkshopClient::publish_item(
								"stamp", s_publish_title, s_publish_desc, s_publish_author, parent_id, stamp_bytes,
								".stamp", meta.dump(), set_hash, thumb_b64,
								[cur_s, s_target_stamp = std::string(s_publish_target_stamp),
								 s_title = std::string(s_publish_title), set_hash,
								 s_author = std::string(s_publish_author)](bool success, const std::string& created_id,
																		   const std::string& err) {
									s_publishing = false;
									if (success) {
										std::string stamp_name = s_target_stamp.empty() ? s_title : s_target_stamp;
										SaveManager::set_stamp_workshop_info(stamp_name, cur_s, created_id, set_hash,
																			 s_author, 1);
										s_publish_status = "Stamp published successfully!";
										ToastManager::success("Stamp published to Workshop!");
									} else {
										s_publish_status = "Upload failed: " + err;
									}
								});
						} else if (s_publish_type_idx == 3) {
							auto& cfg = ConfigManager::get_config();
							nlohmann::json tj;
							tj["window_rounding"] = cfg.ui.window_rounding;
							tj["frame_rounding"] = cfg.ui.frame_rounding;
							tj["button_size"] = cfg.ui.button_size;
							tj["icon_size"] = cfg.ui.icon_size;
							tj["sidebar_width"] = cfg.ui.sidebar_width;
							tj["material_list_height"] = cfg.ui.material_list_height;
							tj["background_color"] = {cfg.ui.background_color.x, cfg.ui.background_color.y,
													  cfg.ui.background_color.z, cfg.ui.background_color.w};
							tj["selection_box_color"] = {cfg.ui.selection_box_color.x, cfg.ui.selection_box_color.y,
														 cfg.ui.selection_box_color.z, cfg.ui.selection_box_color.w};
							tj["selection_box_fill"] = {cfg.ui.selection_box_fill.x, cfg.ui.selection_box_fill.y,
														cfg.ui.selection_box_fill.z, cfg.ui.selection_box_fill.w};

							nlohmann::json col_map = nlohmann::json::object();
							for (const auto& [cname, hex] : ConfigManager::get_color_overrides()) {
								col_map[cname] = hex;
							}
							tj["colors"] = col_map;

							std::string dump_str = tj.dump(2);
							std::vector<uint8_t> theme_bytes(dump_str.begin(), dump_str.end());
							std::string thumb_b64 = generate_thumbnail_base64(3, "");

							nlohmann::json meta;
							meta["colors_count"] = col_map.size();

							WorkshopClient::publish_item(
								"theme", s_publish_title, s_publish_desc, s_publish_author, "", theme_bytes, ".theme",
								meta.dump(), "", thumb_b64,
								[](bool success, const std::string&, const std::string& err) {
									s_publishing = false;
									if (success) {
										s_publish_status = "Custom theme published successfully!";
										ToastManager::success("Custom theme published to Workshop!");
									} else {
										s_publish_status = "Upload failed: " + err;
									}
								});
						}
					}

					ImGui::SameLine();
					if (ImGui::Button("Cancel", ImVec2(90, 26))) {
						s_show_publish_modal = false;
						ImGui::CloseCurrentPopup();
					}
				}
			}
			ImGui::EndPopup();
		}
	}
}  // namespace

void UI::render_advanced_options() {
	if (ImGui::BeginTabItem("Advanced")) {
		ImGui::Spacing();
		ImGui::Text("Advanced Options");
		ImGui::Separator();
		ImGui::Spacing();

		bool vsync_enabled = Window::get_vsync();
		if (ImGui::Checkbox("Enable VSync", &vsync_enabled)) {
			Window::set_vsync(vsync_enabled);
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Synchronizes the frame rate with the monitor's refresh rate.");
		}

		if (vsync_enabled) {
			ImGui::BeginDisabled();
		}
		int target_fps = Window::get_target_fps();
		if (ImGui::SliderInt("Target FPS", &target_fps, 10, 500, target_fps < 500 ? "%d FPS" : "Unlimited")) {
			Window::set_target_fps(target_fps);
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Sets the target frame rate.");
		}
		if (vsync_enabled) {
			ImGui::EndDisabled();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Simulation Engine");
		ProcessingMode q = Grid::get_processing_mode();

		if (ImGui::RadioButton("CPU (Multithreaded)", q == ProcessingMode::CPU)) {
			finalize_canvas_drag();
			Grid::set_processing_mode(ProcessingMode::CPU);
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Multithreaded CPU simulation across configurable worker threads.");
		}

		bool gpu_avail = Vulkan::is_available();
		if (!gpu_avail) {
			ImGui::BeginDisabled();
		}
		if (ImGui::RadioButton("GPU (Vulkan)", q == ProcessingMode::GPU)) {
			finalize_canvas_drag();
			Grid::set_processing_mode(ProcessingMode::GPU);
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Hardware-accelerated simulation using Vulkan compute shaders.");
		}
		if (!gpu_avail) {
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"GPU is not available. Please ensure you have a Vulkan-compatible GPU and drivers installed.");
			}
			ImGui::EndDisabled();
		}

		if (q == ProcessingMode::CPU) {
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			int thread_count = static_cast<int>(Grid::get_thread_count());
			if (ImGui::SliderInt("Active Threads", &thread_count, 1, Grid::get_num_strips_y() / 2)) {
				Grid::configure_threads(static_cast<uint32_t>(thread_count));
				ConfigManager::save();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Sets the size of the worker thread pool. Automatically set to the maximum number of\n"
					"parallelizable strips.");
			}
		}

		if (q == ProcessingMode::GPU && gpu_avail) {
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			bool prevent_downclock = Vulkan::is_prevent_downclock_enabled();
			if (ImGui::Checkbox("Stop Downclocking", &prevent_downclock)) {
				Vulkan::set_prevent_downclock(prevent_downclock);
				ConfigManager::save();
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Stops the GPU from slowing down to save power when idle. Makes unpausing instant "
								  "after a few seconds of being paused. Increases idle power usage.");
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Simulation Canvas Size");
		uint32_t current_w = Grid::get_width();
		uint32_t current_h = Grid::get_height();
		uint64_t current_cells = static_cast<uint64_t>(current_w) * current_h;
		ImGui::TextDisabled("Current Size: %u x %u (%lu cells)", current_w, current_h, current_cells);

		static int target_sim_w = static_cast<int>(current_w);
		static int target_sim_h = static_cast<int>(current_h);
		static bool resize_preserve_content = true;
		static bool open_resize_confirm_popup = false;

		static uint32_t last_known_w = current_w;
		static uint32_t last_known_h = current_h;
		if (last_known_w != current_w || last_known_h != current_h) {
			target_sim_w = static_cast<int>(current_w);
			target_sim_h = static_cast<int>(current_h);
			last_known_w = current_w;
			last_known_h = current_h;
		}

		ImGui::Text("Presets:");
		if (ImGui::Button("256x256")) {
			target_sim_w = 256;
			target_sim_h = 256;
		}
		ImGui::SameLine();
		if (ImGui::Button("512x512")) {
			target_sim_w = 512;
			target_sim_h = 512;
		}
		ImGui::SameLine();
		if (ImGui::Button("1024x1024")) {
			target_sim_w = 1024;
			target_sim_h = 1024;
		}
		ImGui::SameLine();
		if (ImGui::Button("1536x1536")) {
			target_sim_w = 1536;
			target_sim_h = 1536;
		}
		ImGui::SameLine();
		if (ImGui::Button("2048x2048")) {
			target_sim_w = 2048;
			target_sim_h = 2048;
		}

		ImGui::InputInt("Width##SimW", &target_sim_w, 16, 64);
		ImGui::InputInt("Height##SimH", &target_sim_h, 16, 64);
		if (target_sim_w < 128)
			target_sim_w = 128;
		if (target_sim_w > 4096)
			target_sim_w = 4096;
		if (target_sim_h < 128)
			target_sim_h = 128;
		if (target_sim_h > 4096)
			target_sim_h = 4096;
		target_sim_w = ((target_sim_w + 15) / 16) * 16;
		target_sim_h = ((target_sim_h + 15) / 16) * 16;

		uint64_t target_cells = static_cast<uint64_t>(target_sim_w) * target_sim_h;
		if (target_cells > 1024 * 1024) {
			ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
							   "Warning: Large canvas (>1024x1024 / >1M cells) can significantly reduce performance!");
		}

		ImGui::Checkbox("Preserve existing canvas cells (crop / pad)", &resize_preserve_content);

		bool size_differs =
			(target_sim_w != static_cast<int>(current_w) || target_sim_h != static_cast<int>(current_h));
		if (!size_differs)
			ImGui::BeginDisabled();
		if (ImGui::Button("Apply Simulation Size", ImVec2(-1, 30))) {
			open_resize_confirm_popup = true;
		}
		if (!size_differs)
			ImGui::EndDisabled();

		if (open_resize_confirm_popup) {
			ImGui::OpenPopup("Confirm Canvas Resize");
			open_resize_confirm_popup = false;
		}

		if (ImGui::BeginPopupModal("Confirm Canvas Resize", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("Are you sure you want to resize the simulation canvas?");
			ImGui::Text("Target size: %d x %d (%lu cells)", target_sim_w, target_sim_h, target_cells);
			ImGui::Spacing();
			if (target_cells > 1024 * 1024) {
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.1f, 1.0f),
								   "Performance Warning: Canvas exceeds 1,048,576 cells.\nFrame rates on CPU and GPU "
								   "may drop.");
				ImGui::Spacing();
			}
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
							   "Warning: Resizing the simulation will reset undo/redo history!");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Confirm Resize", ImVec2(130, 30))) {
				deselect();
				Grid::resize(static_cast<uint32_t>(target_sim_w), static_cast<uint32_t>(target_sim_h),
							 resize_preserve_content);
				UndoManager::clear();
				UndoManager::init();
				last_known_w = target_sim_w;
				last_known_h = target_sim_h;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 30))) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		ImGui::EndTabItem();
	}
}

static std::string trim_string(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

struct MouseShortcutEntry {
	std::string action;
	std::string control;
	std::string description;
};

static const MouseShortcutEntry MOUSE_SHORTCUTS[] = {
	{"Paint Material", "Left Mouse Button (Click / Drag)", "Draw with active material on the simulation grid"},
	{"Erase Cells", "Right Mouse Button (Click / Drag)", "Erase cells (draw empty material) under brush"},
	{"Pan Camera", "Middle Mouse Button (Drag)", "Pan the camera viewport across the grid"},
	{"Eyedropper", "Middle Mouse Button (Click)", "Sample material from the grid cell under cursor"},
	{"Brush Size", "Mouse Scroll Wheel", "Adjust simulation brush size"},
	{"Fast Brush Size", "Ctrl + Mouse Scroll Wheel", "Rapidly adjust brush size (4x step size)"},
	{"Zoom Viewport", "Shift + Mouse Scroll Wheel", "Smoothly zoom canvas in or out"},
	{"Draw Straight Line", "Shift + Left / Right Drag", "Draw or erase straight lines between points"},
	{"Flood Fill Area", "Shift + Alt + Left / Right Click", "Fill or clear connected region of matching cells"},
	{"Fast Camera Pan", "Shift + W / A / S / D", "Pan camera at 2.5x speed"},
	{"Move Selection", "Left Click Drag inside box", "Reposition floating selection area"},
	{"Resize Selection Box", "Drag Handles on box border", "Resize active selection bounding box"},
	{"Nudge Selection", "Arrow Keys", "Nudge selection box 1 cell (Shift for 10 cells)"},
	{"Alternative Redo", "Ctrl + Shift + Z", "Secondary shortcut to redo undone action"},
	{"Alternative Clear", "Ctrl + Shift + Delete", "Secondary shortcut to clear entire grid"},
	{"Alternative Zoom", "Keypad + / Keypad -", "Secondary shortcuts to zoom camera in or out"},
};

enum CustomThemeColor : int {
	CustomCol_None = -100,
	CustomCol_CanvasBackground = -1,
	CustomCol_SelectionBoxBorder = -2,
	CustomCol_SelectionBoxFill = -3,
};

struct CuratedColor {
	int col;
	const char* category;
	const char* label;
};

static const CuratedColor CURATED_THEME_COLORS[] = {
	// Window Title Bar
	{ImGuiCol_TitleBg, "Window Title Bar", "Title Bar Normal"},
	{ImGuiCol_TitleBgActive, "Window Title Bar", "Title Bar Active (Focused)"},
	{ImGuiCol_TitleBgCollapsed, "Window Title Bar", "Title Bar Collapsed"},

	// Windows & Backgrounds
	{CustomCol_CanvasBackground, "Windows & Backgrounds", "Canvas / Simulation Background"},
	{ImGuiCol_WindowBg, "Windows & Backgrounds", "Window Background"},
	{ImGuiCol_ChildBg, "Windows & Backgrounds", "Panel / Child Window Background"},
	{ImGuiCol_PopupBg, "Windows & Backgrounds", "Popup & Tooltip Background"},
	{ImGuiCol_Border, "Windows & Backgrounds", "Border Color"},
	{ImGuiCol_BorderShadow, "Windows & Backgrounds", "Border Shadow"},
	{ImGuiCol_MenuBarBg, "Windows & Backgrounds", "Menu Bar Background"},

	// Checkboxes, Inputs & Sliders
	{ImGuiCol_FrameBg, "Checkboxes, Inputs & Sliders", "Frame Background (Unchecked Checkbox / Input)"},
	{ImGuiCol_FrameBgHovered, "Checkboxes, Inputs & Sliders", "Frame Background Hovered"},
	{ImGuiCol_FrameBgActive, "Checkboxes, Inputs & Sliders", "Frame Background Active"},
	{ImGuiCol_CheckboxSelectedBg, "Checkboxes, Inputs & Sliders", "Checkbox Background (Checked)"},
	{ImGuiCol_CheckMark, "Checkboxes, Inputs & Sliders", "Checkmark Tick Color"},
	{ImGuiCol_SliderGrab, "Checkboxes, Inputs & Sliders", "Slider Grabber"},
	{ImGuiCol_SliderGrabActive, "Checkboxes, Inputs & Sliders", "Slider Grabber Active"},
	{ImGuiCol_InputTextCursor, "Checkboxes, Inputs & Sliders", "Input Text Caret / Cursor"},

	// Buttons & Accent
	{ImGuiCol_Button, "Buttons & Accent", "Button Normal"},
	{ImGuiCol_ButtonHovered, "Buttons & Accent", "Button Hovered"},
	{ImGuiCol_ButtonActive, "Buttons & Accent", "Button Active / Pressed"},
	{ImGuiCol_Header, "Buttons & Accent", "Header / List Selected"},
	{ImGuiCol_HeaderHovered, "Buttons & Accent", "Header Hovered"},
	{ImGuiCol_HeaderActive, "Buttons & Accent", "Header Active"},

	// Text & Selection
	{ImGuiCol_Text, "Text & Selection", "Main Text"},
	{ImGuiCol_TextDisabled, "Text & Selection", "Disabled Text"},
	{ImGuiCol_TextSelectedBg, "Text & Selection", "Text Selection Background"},
	{ImGuiCol_TextLink, "Text & Selection", "Hyperlink Color"},
	{CustomCol_SelectionBoxBorder, "Text & Selection", "Selection Box Border"},
	{CustomCol_SelectionBoxFill, "Text & Selection", "Selection Box Background / Fill"},

	// Tabs
	{ImGuiCol_Tab, "Tabs", "Tab Inactive"},
	{ImGuiCol_TabHovered, "Tabs", "Tab Hovered"},
	{ImGuiCol_TabSelected, "Tabs", "Tab Active / Selected"},
	{ImGuiCol_TabSelectedOverline, "Tabs", "Tab Active Overline"},
	{ImGuiCol_TabDimmed, "Tabs", "Tab Unfocused Inactive"},
	{ImGuiCol_TabDimmedSelected, "Tabs", "Tab Unfocused Selected"},
	{ImGuiCol_TabDimmedSelectedOverline, "Tabs", "Tab Unfocused Overline"},

	// Dividers & Scrollbars
	{ImGuiCol_Separator, "Dividers & Scrollbars", "Separator Line"},
	{ImGuiCol_SeparatorHovered, "Dividers & Scrollbars", "Separator Line Hovered"},
	{ImGuiCol_SeparatorActive, "Dividers & Scrollbars", "Separator Line Active"},
	{ImGuiCol_ScrollbarBg, "Dividers & Scrollbars", "Scrollbar Track"},
	{ImGuiCol_ScrollbarGrab, "Dividers & Scrollbars", "Scrollbar Thumb"},
	{ImGuiCol_ScrollbarGrabHovered, "Dividers & Scrollbars", "Scrollbar Thumb Hovered"},
	{ImGuiCol_ScrollbarGrabActive, "Dividers & Scrollbars", "Scrollbar Thumb Active"},
	{ImGuiCol_ResizeGrip, "Dividers & Scrollbars", "Window Resize Grip"},
	{ImGuiCol_ResizeGripHovered, "Dividers & Scrollbars", "Window Resize Grip Hovered"},
	{ImGuiCol_ResizeGripActive, "Dividers & Scrollbars", "Window Resize Grip Active"},

	// Tables
	{ImGuiCol_TableHeaderBg, "Tables", "Table Header Background"},
	{ImGuiCol_TableBorderStrong, "Tables", "Table Border Strong"},
	{ImGuiCol_TableBorderLight, "Tables", "Table Border Light"},
	{ImGuiCol_TableRowBg, "Tables", "Table Row Background"},
	{ImGuiCol_TableRowBgAlt, "Tables", "Table Row Alt Background"},

	// Modals & Highlights
	{ImGuiCol_ModalWindowDimBg, "Modals & Highlights", "Modal Background Dim"},
	{ImGuiCol_NavCursor, "Modals & Highlights", "Navigation Focus Border"},
	{ImGuiCol_NavWindowingHighlight, "Modals & Highlights", "Windowing (Ctrl+Tab) Highlight"},
	{ImGuiCol_NavWindowingDimBg, "Modals & Highlights", "Windowing Dim Background"},
	{ImGuiCol_DragDropTarget, "Modals & Highlights", "Drag & Drop Target Border"},
	{ImGuiCol_DragDropTargetBg, "Modals & Highlights", "Drag & Drop Target Background"},
	{ImGuiCol_UnsavedMarker, "Modals & Highlights", "Unsaved Marker"},
	{ImGuiCol_PlotLines, "Modals & Highlights", "Plot Lines"},
	{ImGuiCol_PlotLinesHovered, "Modals & Highlights", "Plot Lines Hovered"},
	{ImGuiCol_PlotHistogram, "Modals & Highlights", "Plot Histogram"},
	{ImGuiCol_PlotHistogramHovered, "Modals & Highlights", "Plot Histogram Hovered"},
	{ImGuiCol_TreeLines, "Modals & Highlights", "Tree Lines"},
};

void UI::render_shortcuts() {
	if (ImGui::BeginTabItem("Shortcuts")) {
		ImGui::Spacing();
		if (ImGui::Button("Reset All Shortcuts to Default", ImVec2(240, 26))) {
			ShortcutManager::reset_all_to_defaults();
			ConfigManager::save();
		}
		ImGui::SameLine();
		static char shortcut_filter_buf[64] = "";
		ImGui::SetNextItemWidth(180);
		ImGui::InputTextWithHint("##shortcut_filter", "Search shortcuts...", shortcut_filter_buf,
								 sizeof(shortcut_filter_buf));

		ImGui::Spacing();
		ImGui::BeginChild("ShortcutsEditorScroll", ImVec2(0, -1), true);

		std::string filter = shortcut_filter_buf;
		std::transform(filter.begin(), filter.end(), filter.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		std::string last_cat = "";
		for (const auto& s : ShortcutManager::get_all()) {
			if (s.action >= ShortcutAction::QuickSelect1 && s.action <= ShortcutAction::QuickSelect9) {
				continue;
			}
			std::string name_lower = s.display_name;
			std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string cat_lower = s.category;
			std::transform(cat_lower.begin(), cat_lower.end(), cat_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string key_lower = s.current_key;
			std::transform(key_lower.begin(), key_lower.end(), key_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string def_lower = s.default_key;
			std::transform(def_lower.begin(), def_lower.end(), def_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && name_lower.find(filter) == std::string::npos &&
				cat_lower.find(filter) == std::string::npos && key_lower.find(filter) == std::string::npos &&
				def_lower.find(filter) == std::string::npos) {
				continue;
			}

			if (s.category != last_cat) {
				if (!last_cat.empty())
					ImGui::Spacing();
				ImGui::Text("%s", s.category.c_str());
				ImGui::Separator();
				last_cat = s.category;
			}

			ImGui::PushID(s.id.c_str());
			bool mod = ShortcutManager::is_modified(s.action);
			if (mod) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "*");
				ImGui::SameLine();
			}
			ImGui::Text("%s:", s.display_name.c_str());
			ImGui::SameLine(240);

			char key_buf[32];
			std::snprintf(key_buf, sizeof(key_buf), "%s", s.current_key.c_str());
			ImGui::SetNextItemWidth(120);
			if (ImGui::InputText("##key", key_buf, sizeof(key_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
				if (ShortcutManager::set_key_string(s.action, key_buf)) {
					ConfigManager::save();
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Key combo (e.g. Space, F, Ctrl+Z, Shift+A, PageUp). Press Enter to apply.\nDefault: %s",
					s.default_key.c_str());
			}

			if (mod) {
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##key")) {
					ShortcutManager::reset_to_default(s.action);
					ConfigManager::save();
				}
			}
			ImGui::PopID();
		}

		bool mouse_header_shown = false;
		for (const auto& entry : MOUSE_SHORTCUTS) {
			std::string a_lower = entry.action;
			std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string c_lower = entry.control;
			std::transform(c_lower.begin(), c_lower.end(), c_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string d_lower = entry.description;
			std::transform(d_lower.begin(), d_lower.end(), d_lower.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && a_lower.find(filter) == std::string::npos &&
				c_lower.find(filter) == std::string::npos && d_lower.find(filter) == std::string::npos &&
				filter != "mouse") {
				continue;
			}

			if (!mouse_header_shown) {
				ImGui::Spacing();
				ImGui::Text("Canvas & Mouse Controls");
				ImGui::Separator();
				mouse_header_shown = true;
			}

			ImGui::Text("%s:", entry.action.c_str());
			ImGui::SameLine(240);
			ImGui::TextColored(ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "%s", entry.control.c_str());
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("%s", entry.description.c_str());
			}
		}

		ImGui::Spacing();
		ImGui::Text("Material Shortcuts (Set: %s)", SetManager::get_current_set().c_str());
		ImGui::Separator();
		ImGui::TextDisabled("Stored in set_config.ini under [Shortcuts]. If a material is assigned a custom shortcut, "
							"next materials follow the order.");
		if (ImGui::SmallButton("Reset Set Material Shortcuts to Default")) {
			SetManager::clear_current_material_shortcuts();
		}
		ImGui::Spacing();

		auto mat_items = get_all_material_shortcuts();
		for (const auto& item : mat_items) {
			std::string name_l = item.name;
			std::transform(name_l.begin(), name_l.end(), name_l.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string eff_l = item.effective_shortcut;
			std::transform(eff_l.begin(), eff_l.end(), eff_l.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && name_l.find(filter) == std::string::npos &&
				eff_l.find(filter) == std::string::npos && filter != "material" && filter != "materials") {
				continue;
			}

			ImGui::PushID(("mat_sc_" + item.name + "_" + std::to_string(item.id)).c_str());

			const auto& mat = MaterialManager::get_material(item.id);
			ImVec4 col(mat.color[0] / 255.f, mat.color[1] / 255.f, mat.color[2] / 255.f, 1.0f);
			ImGui::ColorButton("##swatch", col, ImGuiColorEditFlags_NoTooltip, ImVec2(15, 15));
			ImGui::SameLine();

			if (item.is_custom) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "*");
				ImGui::SameLine();
			}
			ImGui::Text("%s:", item.name.c_str());
			ImGui::SameLine(180);

			if (item.is_custom) {
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[Custom]");
			} else {
				ImGui::TextDisabled("[Auto]");
			}
			ImGui::SameLine(240);

			char mat_buf[32];
			std::snprintf(mat_buf, sizeof(mat_buf), "%s", item.effective_shortcut.c_str());
			ImGui::SetNextItemWidth(120);
			if (ImGui::InputTextWithHint("##mat_input", "-", mat_buf, sizeof(mat_buf),
										 ImGuiInputTextFlags_EnterReturnsTrue)) {
				std::string new_str = trim_string(mat_buf);
				if (new_str.empty() || new_str == "-") {
					SetManager::remove_current_material_shortcut(item.name);
				} else {
					ImGuiKey k;
					bool c, s, a;
					if (ShortcutManager::parse_key_combo(new_str, k, c, s, a)) {
						std::string formatted = ShortcutManager::format_key_combo(k, c, s, a);
						SetManager::set_current_material_shortcut(item.name, formatted);
					}
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Custom shortcut for '%s' (e.g. 1, 2, G, Shift+1). Press Enter to apply.\nStored in "
								  "set_config.ini under [Shortcuts].",
								  item.name.c_str());
			}

			if (item.is_custom) {
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##mat_reset")) {
					SetManager::remove_current_material_shortcut(item.name);
				}
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
		ImGui::EndTabItem();
	}
}

void UI::render_theme_editor() {
	if (ImGui::BeginTabItem("Theme Editor")) {
		ImGui::Spacing();

		auto& cfg = ConfigManager::get_config();
		const auto& def_cfg = ConfigManager::get_default_config();
		ImGuiStyle& style = ImGui::GetStyle();

		ImGui::Spacing();
		if (button_with_icon("Share Custom Theme to Workshop", IconManager::get(IconID::Transmit), ImVec2(-1, 28))) {
			open_share_for_theme();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Publish your current UI styling and color palette to the Community Workshop.");
		}
		ImGui::Spacing();

		if (ImGui::Button("Reset Style to Defaults", ImVec2(220, 26))) {
			cfg.ui.window_rounding = def_cfg.ui.window_rounding;
			cfg.ui.frame_rounding = def_cfg.ui.frame_rounding;
			cfg.ui.button_size = def_cfg.ui.button_size;
			cfg.ui.icon_size = def_cfg.ui.icon_size;
			cfg.ui.material_list_height = def_cfg.ui.material_list_height;
			cfg.ui.sidebar_width = def_cfg.ui.sidebar_width;
			cfg.ui.ui_scale = def_cfg.ui.ui_scale;
			cfg.ui.font_size = def_cfg.ui.font_size;
			cfg.ui.show_fps = def_cfg.ui.show_fps;
			cfg.ui.show_active_cells = def_cfg.ui.show_active_cells;
			cfg.ui.show_simulation_status = def_cfg.ui.show_simulation_status;
			cfg.ui.background_color = def_cfg.ui.background_color;

			style.WindowRounding = cfg.ui.window_rounding;
			style.FrameRounding = cfg.ui.frame_rounding;
			style.ChildRounding = cfg.ui.frame_rounding;
			style.PopupRounding = cfg.ui.frame_rounding;
			style.GrabRounding = cfg.ui.frame_rounding;
			style.TabRounding = cfg.ui.frame_rounding;

			ConfigManager::save();
			Window::set_background_color(cfg.ui.background_color);
		}
		ImGui::Spacing();

		float win_round = cfg.ui.window_rounding;
		if (ImGui::SliderFloat("Window Rounding", &win_round, 0.0f, 20.0f, "%.1f px")) {
			cfg.ui.window_rounding = win_round;
			style.WindowRounding = win_round;
			ConfigManager::save();
		}

		float frame_round = cfg.ui.frame_rounding;
		if (ImGui::SliderFloat("Frame Rounding", &frame_round, 0.0f, 16.0f, "%.1f px")) {
			cfg.ui.frame_rounding = frame_round;
			style.FrameRounding = frame_round;
			style.ChildRounding = frame_round;
			style.PopupRounding = frame_round;
			style.GrabRounding = frame_round;
			style.TabRounding = frame_round;
			ConfigManager::save();
		}

		int btn_size = cfg.ui.button_size;
		if (ImGui::SliderInt("Button Size", &btn_size, 20, 50, "%d px")) {
			cfg.ui.button_size = btn_size;
			ConfigManager::save();
		}

		int icon_sz = cfg.ui.icon_size;
		if (ImGui::SliderInt("Icon Size", &icon_sz, 10, 36, "%d px")) {
			cfg.ui.icon_size = icon_sz;
			ConfigManager::save();
		}

		int sb_w = cfg.ui.sidebar_width;
		if (ImGui::SliderInt("Sidebar Width", &sb_w, 260, 800, "%d px")) {
			cfg.ui.sidebar_width = sb_w;
			ConfigManager::save();
		}

		int mat_h = cfg.ui.material_list_height;
		if (ImGui::SliderInt("Material List Height", &mat_h, 70, 600, "%d px")) {
			cfg.ui.material_list_height = mat_h;
			ConfigManager::save();
		}

		float scale = cfg.ui.ui_scale;
		if (ImGui::SliderFloat("UI Scale*", &scale, 0.5f, 2.5f, "%.2fx")) {
			cfg.ui.ui_scale = scale;
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("*Requires application restart to apply.");
		}

		float f_sz = cfg.ui.font_size;
		if (ImGui::SliderFloat("Font Size*", &f_sz, 10.0f, 32.0f, "%.1f px")) {
			cfg.ui.font_size = f_sz;
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("*Requires application restart to reload font atlas.");
		}

		bool show_fps = cfg.ui.show_fps;
		if (ImGui::Checkbox("Show FPS Counter in Header", &show_fps)) {
			cfg.ui.show_fps = show_fps;
			ConfigManager::save();
		}
		ImGui::SameLine();
		bool show_cells = cfg.ui.show_active_cells;
		if (ImGui::Checkbox("Show Active Cells in Header", &show_cells)) {
			cfg.ui.show_active_cells = show_cells;
			ConfigManager::save();
		}
		bool show_status = cfg.ui.show_simulation_status;
		if (ImGui::Checkbox("Show Simulation Status (PAUSED / UNPAUSED) in Header", &show_status)) {
			cfg.ui.show_simulation_status = show_status;
			ConfigManager::save();
		}

		ImGui::Spacing();

		if (ImGui::Button("Reset All Colors to Default", ImVec2(220, 26))) {
			UI::reset_theme_colors();
		}
		ImGui::SameLine();
		static char color_filter_buf[64] = "";
		ImGui::SetNextItemWidth(180);
		ImGui::InputTextWithHint("##col_filter", "Filter colors...", color_filter_buf, sizeof(color_filter_buf));

		ImGui::Spacing();
		ImGui::BeginChild("ColorEditorScroll", ImVec2(0, -1), true);
		const ImVec4* defs = UI::get_default_colors();
		std::string filter = color_filter_buf;
		std::transform(filter.begin(), filter.end(), filter.begin(),
					   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		std::string last_col_cat = "";
		for (const auto& item : CURATED_THEME_COLORS) {
			const char* col_name = "";
			ImVec4* col_ptr = nullptr;
			const ImVec4* def_ptr = nullptr;

			if (item.col >= 0) {
				col_name = ImGui::GetStyleColorName(item.col);
				col_ptr = &style.Colors[item.col];
				if (defs)
					def_ptr = &defs[item.col];
			} else if (item.col == CustomCol_CanvasBackground) {
				col_name = "CanvasBackground";
				col_ptr = &cfg.ui.background_color;
				def_ptr = &ConfigManager::get_default_config().ui.background_color;
			} else if (item.col == CustomCol_SelectionBoxBorder) {
				col_name = "SelectionBoxBorder";
				col_ptr = &cfg.ui.selection_box_color;
				def_ptr = &ConfigManager::get_default_config().ui.selection_box_color;
			} else if (item.col == CustomCol_SelectionBoxFill) {
				col_name = "SelectionBoxFill";
				col_ptr = &cfg.ui.selection_box_fill;
				def_ptr = &ConfigManager::get_default_config().ui.selection_box_fill;
			}

			if (!col_ptr)
				continue;

			std::string lname = col_name;
			std::transform(lname.begin(), lname.end(), lname.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string llabel = item.label;
			std::transform(llabel.begin(), llabel.end(), llabel.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::string lcat = item.category;
			std::transform(lcat.begin(), lcat.end(), lcat.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			if (!filter.empty() && lname.find(filter) == std::string::npos &&
				llabel.find(filter) == std::string::npos && lcat.find(filter) == std::string::npos) {
				continue;
			}

			if (item.category != last_col_cat) {
				if (!last_col_cat.empty())
					ImGui::Spacing();
				ImGui::Text("%s", item.category);
				ImGui::Separator();
				last_col_cat = item.category;
			}

			ImGui::PushID(item.col);
			bool modified = def_ptr && ConfigManager::color_differs(*col_ptr, *def_ptr);
			if (modified) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "*");
				ImGui::SameLine();
			}
			ImGuiColorEditFlags flags = (item.col == CustomCol_CanvasBackground)
											? ImGuiColorEditFlags_NoAlpha
											: (ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
			if (ImGui::ColorEdit4(item.label, (float*)col_ptr, flags)) {
				if (item.col >= 0) {
					ConfigManager::get_color_overrides()[col_name] = ConfigManager::color_to_hex(*col_ptr);
				} else if (item.col == CustomCol_CanvasBackground) {
					Window::set_background_color(cfg.ui.background_color);
				}
				ConfigManager::save();
			}
			if (modified) {
				ImGui::SameLine();
				if (ImGui::SmallButton("Reset##col")) {
					*col_ptr = *def_ptr;
					if (item.col >= 0) {
						ConfigManager::get_color_overrides().erase(col_name);
					} else if (item.col == CustomCol_CanvasBackground) {
						Window::set_background_color(cfg.ui.background_color);
					}
					ConfigManager::save();
				}
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::EndTabItem();
	}
}

void UI::render_modals() {
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	const std::string& current_set = SetManager::get_current_set();

	if (open_empty_rule_warning_popup) {
		ImGui::OpenPopup("Performance Warning##EmptyRule");
		open_empty_rule_warning_popup = false;
	}

	if (open_switch_popup) {
		ImGui::OpenPopup("Unsaved Changes Switch");
		open_switch_popup = false;
	}

	if (open_create_set_popup) {
		ImGui::OpenPopup("Create Set...");
		save_as_buf[0] = '\0';
		open_create_set_popup = false;
	}

	if (open_delete_set_popup) {
		ImGui::OpenPopup("Delete Set Confirmation");
		open_delete_set_popup = false;
	}

	if (s_show_set_override_warning) {
		ImGui::OpenPopup("Set Warning##OverrideModal");
		s_show_set_override_warning = false;
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Set Warning##OverrideModal", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		if (s_pending_is_update) {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "SET UPDATE AVAILABLE");
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextWrapped("An updated version of set '%s' is available on the Workshop.",
							   s_pending_override_item.title.c_str());
			ImGui::Spacing();
			ImGui::BulletText("Installed Local Version: v%u", s_pending_local_version);
			ImGui::BulletText("Workshop Online Version: v%d", s_pending_override_item.version);
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "WARNING: OVERWRITING LOCAL SET");
			ImGui::Separator();
			ImGui::Spacing();
			ImGui::TextWrapped("A local set named '%s' (v%u) already exists on disk.",
							   s_pending_override_item.title.c_str(), s_pending_local_version);
			ImGui::Spacing();
			ImGui::BulletText("Workshop Online Version: v%d", s_pending_override_item.version);
		}
		ImGui::Spacing();
		ImGui::TextWrapped(
			"Proceeding will replace your local set files with the online version.\n"
			"Any local modifications made to this set's materials or configuration will be overwritten.");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Overwrite & Update", ImVec2(160, 30))) {
			if (s_pending_override_confirm) {
				s_pending_override_confirm();
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100, 30))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Performance Warning##EmptyRule", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.70f, 0.20f, 1.00f), "PERFORMANCE WARNING");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped("Adding rules to the 'empty' material means every empty cell on the grid will be "
						   "evaluated every frame.");
		ImGui::TextWrapped(
			"This can significantly lower simulation performance when large areas of the grid are empty.");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Add Rule Anyway", ImVec2(150, 30))) {
			auto& mats = MaterialManager::get_materials();
			for (auto& m : mats) {
				if (m.id == 0) {
					RuleDefinition new_rule;
					new_rule.when[12] = {m.id};
					new_rule.then.fill(255);
					new_rule.chance = 100.0f;
					m.rules.push_back(new_rule);
					MaterialManager::rebuild_compiled_rules();
					unsaved_changes = true;
					break;
				}
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Create Set...", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		if (duplicate_set_checkbox) {
			ImGui::Text("DUPLICATE CURRENT SET");
		} else {
			ImGui::Text("CREATE NEW EMPTY SET");
		}
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Enter name for the new set:");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##new_set_name_input", save_as_buf, sizeof(save_as_buf),
							 ImGuiInputTextFlags_EnterReturnsTrue)) {
			std::string name = save_as_buf;
			name = sanitize_name(name);
			if (!name.empty()) {
				if (duplicate_set_checkbox) {
					SetManager::copy_set(current_set, name);
				} else {
					SetManager::create_new_empty_set(name);
					SetManager::set_current_set(name);
				}
				unsaved_changes = false;
				ImGui::CloseCurrentPopup();
			}
		}

		if (ImGui::IsWindowAppearing()) {
			ImGui::SetKeyboardFocusHere();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Confirm", ImVec2(120, 30))) {
			std::string name = save_as_buf;
			if (!name.empty()) {
				if (duplicate_set_checkbox) {
					SetManager::copy_set(current_set, name);
				} else {
					SetManager::create_new_empty_set(name);
					SetManager::set_current_set(name);
				}
				unsaved_changes = false;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Delete Set Confirmation", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.00f), "DELETE MATERIAL SET");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped("Are you sure you want to delete the set '%s'?", current_set.c_str());
		ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.00f), "This will delete all its materials, rules, and saves.");
		ImGui::Text("This action cannot be undone.");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
		if (ImGui::Button("Delete Permanently", ImVec2(180, 30))) {
			SetManager::delete_set(current_set);
			unsaved_changes = false;
			selected_id = 0;
			ImGui::PopStyleColor(3);
			ImGui::CloseCurrentPopup();
		} else {
			ImGui::PopStyleColor(3);
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (show_exit_popup) {
		if (unsaved_changes) {
			ImGui::OpenPopup("Exit Confirmation");
			exit_save_as_new_set = false;
			new_set_name_buf[0] = '\0';
		} else {
			ConfigManager::save();
			UI::shutdown();
			Grid::shutdown();
			Window::shutdown();
			exit(0);
		}
		show_exit_popup = false;
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Exit Confirmation", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.40f, 0.40f, 1.00f), "WARNING: UNSAVED CHANGES");
		ImGui::Separator();
		ImGui::Spacing();

		if (!exit_save_as_new_set) {
			ImGui::TextWrapped("You have unsaved changes in %s. What would you like to do?", current_set.c_str());
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			std::string save_btn_lbl = "Save & Exit (" + current_set + ")";
			if (ImGui::Button(save_btn_lbl.c_str(), ImVec2(190, 30))) {
				MaterialManager::save_all_materials(SETS_DIRECTORY + current_set);
				ConfigManager::save();
				UI::shutdown();
				Grid::shutdown();
				Window::shutdown();
				exit(0);
			}
			ImGui::SameLine();
			if (ImGui::Button("Save as New Set...", ImVec2(190, 30))) {
				exit_save_as_new_set = true;
			}

			if (ImGui::Button("Exit Without Saving", ImVec2(190, 30))) {
				ConfigManager::save();
				UI::shutdown();
				Grid::shutdown();
				Window::shutdown();
				exit(0);
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(190, 30))) {
				ImGui::CloseCurrentPopup();
			}
		} else {
			ImGui::Text("Enter name for the new set:");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##exit_new_set_name", new_set_name_buf, sizeof(new_set_name_buf),
								 ImGuiInputTextFlags_EnterReturnsTrue)) {
				std::string new_set_name = new_set_name_buf;
				new_set_name = sanitize_name(new_set_name);
				if (!new_set_name.empty()) {
					SetManager::copy_set(current_set, new_set_name);
					ConfigManager::save();
					UI::shutdown();
					Grid::shutdown();
					Window::shutdown();
					exit(0);
				}
			}

			if (ImGui::IsWindowAppearing()) {
				ImGui::SetKeyboardFocusHere();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Save & Exit", ImVec2(120, 30))) {
				std::string new_set_name = new_set_name_buf;
				new_set_name = sanitize_name(new_set_name);
				if (!new_set_name.empty()) {
					SetManager::copy_set(current_set, new_set_name);
					ConfigManager::save();
					UI::shutdown();
					Grid::shutdown();
					Window::shutdown();
					exit(0);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Back", ImVec2(120, 30))) {
				exit_save_as_new_set = false;
			}
		}

		ImGui::EndPopup();
	}

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	bool should_proceed_switch = false;
	if (ImGui::BeginPopupModal("Unsaved Changes Switch", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.70f, 0.20f, 1.00f), "WARNING: UNSAVED CHANGES");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped("You have unsaved changes in the current set. Do you "
						   "want to save them before switching?");
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Save & Switch", ImVec2(120, 30))) {
			MaterialManager::save_all_materials(SETS_DIRECTORY + current_set);
			should_proceed_switch = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard & Switch", ImVec2(120, 30))) {
			should_proceed_switch = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 30))) {
			pending_set_switch = "";
			pending_save_load = "";
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (should_proceed_switch) {
		if (!pending_set_switch.empty()) {
			SetManager::set_current_set(pending_set_switch);
			selected_id = 0;
			unsaved_changes = false;
			pending_set_switch = "";
		} else if (!pending_save_load.empty()) {
			SaveManager::load_from_file_async(pending_save_load, current_set, LoadPlacement::Center,
											  [](bool success, const std::string&) {
												  if (success) {
													  selected_id = 0;
													  unsaved_changes = false;
												  }
											  });
			pending_save_load = "";
		}
	}

	if (s_show_auth_modal) {
		render_workshop_auth_modal();
	}
	check_and_render_report_modal();
	check_and_render_publish_modal();
	WorkshopItemEditor::render();
	ToastManager::render();
}