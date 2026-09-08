#include "ui.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "config_manager.hpp"
#include "grid.hpp"
#include "material_manager.hpp"
#include "resources/roboto_ttf.h"
#include "sanitize.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "undo_manager.hpp"
#include "vulkan.hpp"
#include "window.hpp"

namespace fs = std::filesystem;

static void init_style() {
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 6.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.GrabRounding = 4.0f;
	style.TabRounding = 4.0f;

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
	colors[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.39f, 0.42f, 1.00f);
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
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.11f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
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
	return ImVec2(norm_x * SIM_WIDTH, norm_y * SIM_HEIGHT);
}

static std::pair<float, float> screen_to_grid(float sx, float sy) {
	ImVec2 g = screen_to_grid_pos(ImVec2(sx, sy));
	return {g.x, g.y};
}

static ScreenRect grid_to_screen_rect(int start_x, int start_y, int size) {
	SDL_FRect dst_rect = Window::get_dst_rect();
	float sx1 = dst_rect.x + (static_cast<float>(start_x) / SIM_WIDTH) * dst_rect.w;
	float sy1 = dst_rect.y + (static_cast<float>(start_y) / SIM_HEIGHT) * dst_rect.h;
	float sx2 = dst_rect.x + (static_cast<float>(start_x + size) / SIM_WIDTH) * dst_rect.w;
	float sy2 = dst_rect.y + (static_cast<float>(start_y + size) / SIM_HEIGHT) * dst_rect.h;
	return {sx1, sy1, sx2, sy2};
}

static ScreenRect grid_to_screen_rect_wh(int start_x, int start_y, int w, int h) {
	SDL_FRect dst_rect = Window::get_dst_rect();
	float sx1 = dst_rect.x + (static_cast<float>(start_x) / SIM_WIDTH) * dst_rect.w;
	float sy1 = dst_rect.y + (static_cast<float>(start_y) / SIM_HEIGHT) * dst_rect.h;
	float sx2 = dst_rect.x + (static_cast<float>(start_x + w) / SIM_WIDTH) * dst_rect.w;
	float sy2 = dst_rect.y + (static_cast<float>(start_y + h) / SIM_HEIGHT) * dst_rect.h;
	return {sx1, sy1, sx2, sy2};
}

static GridRect calculate_brush_bounds(ImVec2 grid_pos, int brush_size) {
	int x_start = static_cast<int>(grid_pos.x - static_cast<float>(brush_size) / 2.0f + 0.5f);
	int y_start = static_cast<int>(grid_pos.y - static_cast<float>(brush_size) / 2.0f + 0.5f);
	return {x_start, y_start, brush_size, brush_size};
}

static ResizeHandle get_hovered_resize_handle(const ScreenRect& srect, ImVec2 mouse_pos) {
	ImVec2 handle_pts[8] = {
		ImVec2(srect.x1, srect.y1),						 // TopLeft
		ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y1),	 // Top
		ImVec2(srect.x2, srect.y1),						 // TopRight
		ImVec2(srect.x2, (srect.y1 + srect.y2) * 0.5f),	 // Right
		ImVec2(srect.x2, srect.y2),						 // BottomRight
		ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y2),	 // Bottom
		ImVec2(srect.x1, srect.y2),						 // BottomLeft
		ImVec2(srect.x1, (srect.y1 + srect.y2) * 0.5f)	 // Left
	};

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

static float cross_2d(ImVec2 a, ImVec2 b, ImVec2 c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }

static std::vector<ImVec2> compute_swept_brush_hull(ScreenRect s_start, ScreenRect s_end) {
	ImVec2 pts[8] = {{s_start.x1, s_start.y1}, {s_start.x2, s_start.y1}, {s_start.x2, s_start.y2},
					 {s_start.x1, s_start.y2}, {s_end.x1, s_end.y1},	 {s_end.x2, s_end.y1},
					 {s_end.x2, s_end.y2},	   {s_end.x1, s_end.y2}};

	std::sort(pts, pts + 8, [](ImVec2 a, ImVec2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });

	ImVec2 hull[16];
	int k = 0;

	for (int i = 0; i < 8; ++i) {
		while (k >= 2 && cross_2d(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) {
			k--;
		}
		hull[k++] = pts[i];
	}

	for (int i = 6, t = k + 1; i >= 0; i--) {
		while (k >= t && cross_2d(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) {
			k--;
		}
		hull[k++] = pts[i];
	}

	return std::vector<ImVec2>(hull, hull + std::max(0, k - 1));
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
	int min_x = std::clamp(start_x, 0, static_cast<int>(SIM_WIDTH));
	int max_x = std::clamp(start_x + brush_size, 0, static_cast<int>(SIM_WIDTH));
	int min_y = std::clamp(start_y, 0, static_cast<int>(SIM_HEIGHT));
	int max_y = std::clamp(start_y + brush_size, 0, static_cast<int>(SIM_HEIGHT));

	if (min_x >= max_x || min_y >= max_y)
		return;

	if (shape == BrushShape::Square) {
		parallel_for_rows(min_y, max_y, [min_x, max_x, mat_id](int y) {
			for (int x = min_x; x < max_x; ++x) {
				Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
			}
		});
	} else {
		float cx = static_cast<float>(start_x) + static_cast<float>(brush_size) / 2.0f;
		float cy = static_cast<float>(start_y) + static_cast<float>(brush_size) / 2.0f;
		float r = static_cast<float>(brush_size) / 2.0f;
		float r_sq = r * r;

		parallel_for_rows(min_y, max_y, [min_x, max_x, cx, cy, r_sq, mat_id](int y) {
			float dy = (static_cast<float>(y) + 0.5f) - cy;
			float dy_sq = dy * dy;
			if (dy_sq <= r_sq) {
				float dx_half = std::sqrt(r_sq - dy_sq);
				int rx_min = std::clamp(static_cast<int>(std::floor(cx - dx_half)), min_x, max_x);
				int rx_max = std::clamp(static_cast<int>(std::ceil(cx + dx_half)), min_x, max_x);
				for (int x = rx_min; x < rx_max; ++x) {
					Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
				}
			}
		});
	}
}

static void paint_line(ImVec2 start_grid, ImVec2 end_grid, int brush_size, uint8_t mat_id, BrushShape shape) {
	float half_brush = static_cast<float>(brush_size) / 2.0f;

	if (shape == BrushShape::Square) {
		int x0_start = static_cast<int>(start_grid.x - half_brush + 0.5f);
		int y0_start = static_cast<int>(start_grid.y - half_brush + 0.5f);
		int x1_start = static_cast<int>(end_grid.x - half_brush + 0.5f);
		int y1_start = static_cast<int>(end_grid.y - half_brush + 0.5f);

		int min_y = std::clamp(std::min(y0_start, y1_start), 0, static_cast<int>(SIM_HEIGHT));
		int max_y = std::clamp(std::max(y0_start, y1_start) + brush_size, 0, static_cast<int>(SIM_HEIGHT));

		if (min_y >= max_y)
			return;

		float dy = end_grid.y - start_grid.y;
		float dx = end_grid.x - start_grid.x;

		parallel_for_rows(min_y, max_y, [&](int y) {
			int row_min_x = static_cast<int>(SIM_WIDTH);
			int row_max_x = -1;

			if (std::abs(dy) < 0.0001f) {
				row_min_x = std::min(x0_start, x1_start);
				row_max_x = std::max(x0_start, x1_start) + brush_size;
			} else {
				float t_a = (static_cast<float>(y) - start_grid.y + half_brush - 0.5f) / dy;
				float t_b = (static_cast<float>(y + 1 - brush_size) - start_grid.y + half_brush - 0.5f) / dy;

				float t_min = std::clamp(std::min(t_a, t_b), 0.0f, 1.0f);
				float t_max = std::clamp(std::max(t_a, t_b), 0.0f, 1.0f);

				float gx_min = start_grid.x + t_min * dx;
				float gx_max = start_grid.x + t_max * dx;

				int x_a = static_cast<int>(gx_min - half_brush + 0.5f);
				int x_b = static_cast<int>(gx_max - half_brush + 0.5f);

				row_min_x = std::min(x_a, x_b);
				row_max_x = std::max(x_a, x_b) + brush_size;
			}

			row_min_x = std::clamp(row_min_x, 0, static_cast<int>(SIM_WIDTH));
			row_max_x = std::clamp(row_max_x, 0, static_cast<int>(SIM_WIDTH));

			for (int x = row_min_x; x < row_max_x; ++x) {
				Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
			}
		});
	} else {
		GridRect b0 = calculate_brush_bounds(start_grid, brush_size);
		GridRect b1 = calculate_brush_bounds(end_grid, brush_size);

		float r = static_cast<float>(brush_size) / 2.0f;
		float r_sq = r * r;

		float p0x = static_cast<float>(b0.x) + r;
		float p0y = static_cast<float>(b0.y) + r;
		float p1x = static_cast<float>(b1.x) + r;
		float p1y = static_cast<float>(b1.y) + r;

		int min_y = std::clamp(static_cast<int>(std::floor(std::min(p0y, p1y) - r)), 0, static_cast<int>(SIM_HEIGHT));
		int max_y = std::clamp(static_cast<int>(std::ceil(std::max(p0y, p1y) + r)), 0, static_cast<int>(SIM_HEIGHT));

		if (min_y >= max_y)
			return;

		float dx = p1x - p0x;
		float dy = p1y - p0y;
		float len_sq = dx * dx + dy * dy;

		parallel_for_rows(min_y, max_y, [&](int y) {
			float cy_row = static_cast<float>(y) + 0.5f;

			float row_left = static_cast<float>(SIM_WIDTH);
			float row_right = -1.0f;
			bool has_span = false;

			float dy0 = cy_row - p0y;
			if (dy0 * dy0 <= r_sq) {
				float dx0 = std::sqrt(r_sq - dy0 * dy0);
				row_left = std::min(row_left, p0x - dx0);
				row_right = std::max(row_right, p0x + dx0);
				has_span = true;
			}

			float dy1 = cy_row - p1y;
			if (dy1 * dy1 <= r_sq) {
				float dx1 = std::sqrt(r_sq - dy1 * dy1);
				row_left = std::min(row_left, p1x - dx1);
				row_right = std::max(row_right, p1x + dx1);
				has_span = true;
			}

			if (len_sq > 0.0001f) {
				if (std::abs(dy) > 0.0001f) {
					float H = r * std::sqrt(len_sq) / std::abs(dy);
					float x_line_mid = p0x + (cy_row - p0y) * dx / dy;
					float x_line_left = x_line_mid - H;
					float x_line_right = x_line_mid + H;

					if (std::abs(dx) > 0.0001f) {
						float x_t0 = p0x - (cy_row - p0y) * dy / dx;
						float x_t1 = p0x + (len_sq - (cy_row - p0y) * dy) / dx;

						float x_body_left = std::max(x_line_left, std::min(x_t0, x_t1));
						float x_body_right = std::min(x_line_right, std::max(x_t0, x_t1));

						if (x_body_left < x_body_right) {
							row_left = std::min(row_left, x_body_left);
							row_right = std::max(row_right, x_body_right);
							has_span = true;
						}
					} else {
						float min_p_y = std::min(p0y, p1y);
						float max_p_y = std::max(p0y, p1y);
						if (cy_row >= min_p_y && cy_row <= max_p_y) {
							row_left = std::min(row_left, p0x - r);
							row_right = std::max(row_right, p0x + r);
							has_span = true;
						}
					}
				}
			}

			if (has_span) {
				int rx_min = std::clamp(static_cast<int>(std::floor(row_left)), 0, static_cast<int>(SIM_WIDTH));
				int rx_max = std::clamp(static_cast<int>(std::ceil(row_right)), 0, static_cast<int>(SIM_WIDTH));

				for (int x = rx_min; x < rx_max; ++x) {
					Grid::set_cell(static_cast<uint32_t>(x), static_cast<uint32_t>(y), mat_id);
				}
			}
		});
	}
}

static void flood_fill(uint32_t start_x, uint32_t start_y, uint8_t fill_mat) {
	if (start_x >= SIM_WIDTH || start_y >= SIM_HEIGHT)
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

			if (nx >= 0 && nx < static_cast<int>(SIM_WIDTH) && ny >= 0 && ny < static_cast<int>(SIM_HEIGHT)) {
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
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigInputTrickleEventQueue = false;
	io.ConfigWindowsMoveFromTitleBarOnly = true;

	ImFontConfig font_cfg;
	font_cfg.FontDataOwnedByAtlas = false;
	io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(roboto_ttf), roboto_ttf_len, 16.0f, &font_cfg);

	init_style();

	ImGui_ImplSDL3_InitForSDLRenderer(Window::get_window(), Window::get_renderer());
	ImGui_ImplSDLRenderer3_Init(Window::get_renderer());

	UndoManager::init();
}

void UI::shutdown() {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
}

void UI::render() {
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	if (ui_compact) {
		ImGui::SetNextWindowPos(ImVec2(2.0f, 2.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 400.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Simulation Controls")) {
			render_sim_content();
		}
		ImGui::End();
	} else {
		ImGui::SetNextWindowPos(ImVec2(2.0f, 2.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(570.0f, Window::get_size().second - 4.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Simulation Editor", nullptr)) {
			render_header(io);

			if (ImGui::BeginTabBar("SidebarTabs")) {
				render_material_editor();
				render_manage_sets();
				render_save_load();
				render_shortcuts();
				render_advanced_options();
				ImGui::EndTabBar();
			}

			ImGui::End();
		}
	}

	render_mouse_overlay();

	render_modals();

	// Cursor icons for the UI
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
	current_tool = mode;
	UndoManager::push_snapshot("Switched to " + std::string(current_tool == ToolMode::Brush ? "Brush" : "Select"));
}

void UI::deselect() {
	if (selection_state == SelectionState::Moving) {
		int bw = selection_box.width();
		int bh = selection_box.height();
		for (int y = 0; y < bh; ++y) {
			for (int x = 0; x < bw; ++x) {
				int gx = move_origin_x + x;
				int gy = move_origin_y + y;
				if (gx >= 0 && gx < static_cast<int>(SIM_WIDTH) && gy >= 0 && gy < static_cast<int>(SIM_HEIGHT)) {
					Grid::set_cell(gx, gy, floating_cells[y * bw + x]);
				}
			}
		}
		floating_cells.clear();
	}
	active_resize_handle = ResizeHandle::None;
	selection_state = SelectionState::None;
}

void UI::restore_selection_state(ToolMode mode, SelectionState state, const SelectionBox& box) {
	if (selection_state == SelectionState::Moving) {
		floating_cells.clear();
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
				Grid::set_cell(min_x + x, min_y + y, 0);
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
		int new_min_x = std::clamp(cx - new_w / 2, 0, std::max(0, static_cast<int>(SIM_WIDTH) - new_w));
		int new_min_y = std::clamp(cy - new_h / 2, 0, std::max(0, static_cast<int>(SIM_HEIGHT) - new_h));

		for (int ny = 0; ny < new_h; ++ny) {
			for (int nx = 0; nx < new_w; ++nx) {
				Grid::set_cell(new_min_x + nx, new_min_y + ny, new_cells[ny * new_w + nx]);
			}
		}

		selection_box.start_x = new_min_x;
		selection_box.start_y = new_min_y;
		selection_box.current_x = new_min_x + new_w - 1;
		selection_box.current_y = new_min_y + new_h - 1;

		UndoManager::push_snapshot(clockwise ? "Rotate Selection CW" : "Rotate Selection CCW");
	} else if (selection_state == SelectionState::Moving) {
		int w = selection_box.width();
		int h = selection_box.height();
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

		int old_gx = move_grab_offset_x;
		int old_gy = move_grab_offset_y;
		if (clockwise) {
			move_grab_offset_x = (h - 1) - old_gy;
			move_grab_offset_y = old_gx;
		} else {
			move_grab_offset_x = old_gy;
			move_grab_offset_y = (w - 1) - old_gx;
		}

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
	}
}

void UI::paste_clipboard() {
	if (clipboard.empty())
		return;
	current_tool = ToolMode::Select;
	selection_state = SelectionState::Pasting;
}

void UI::render_selection_controls() {
	bool is_brush = (current_tool == ToolMode::Brush);
	bool is_select = (current_tool == ToolMode::Select);

	float avail_w = ImGui::GetContentRegionAvail().x;
	float spacing_x = ImGui::GetStyle().ItemSpacing.x;
	float tool_w = (avail_w - spacing_x) * 0.5f;

	if (is_brush) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.85f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.95f, 1.0f));
	}
	if (ImGui::Button("Brush (B)", ImVec2(tool_w, 28.0f))) {
		set_tool_mode(ToolMode::Brush);
	}
	if (is_brush) {
		ImGui::PopStyleColor(2);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Drawing brush tool (B)");

	ImGui::SameLine();
	if (is_select) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.85f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.95f, 1.0f));
	}
	if (ImGui::Button("Select (C)", ImVec2(tool_w, 28.0f))) {
		set_tool_mode(ToolMode::Select);
	}
	if (is_select) {
		ImGui::PopStyleColor(2);
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Box selection and move/copy/paste tool (Ctrl+C)");

	if (current_tool == ToolMode::Brush) {
		ImGui::Spacing();
		ImGui::Text("Brush Settings:");
		ImGui::SliderInt("Brush Size", &mouse_size, 1, 512);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Adjust brush width (Scroll wheel).");
		}

		const char* shape_names[] = {"Square", "Circle"};
		int current_shape = static_cast<int>(brush_shape);
		if (ImGui::Combo("Brush Shape", &current_shape, shape_names, IM_ARRAYSIZE(shape_names))) {
			brush_shape = static_cast<BrushShape>(current_shape);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Select brush shape (Square or Circle, toggle with T).");
		}
	} else if (current_tool == ToolMode::Select) {
		ImGui::Spacing();
		bool has_selection = (selection_state == SelectionState::Selected);
		bool has_clipboard = !clipboard.empty();

		float col_w = (avail_w - spacing_x) * 0.5f;
		ImVec2 btn_sz(col_w, 28.0f);

		// Row 1: Copy and Cut
		if (!has_selection)
			ImGui::BeginDisabled();
		if (ImGui::Button("Copy (Ctrl+C)##Sel", btn_sz)) {
			copy_selection();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Copy selected cells to clipboard (Ctrl+C)");

		ImGui::SameLine();
		if (!has_selection)
			ImGui::BeginDisabled();
		if (ImGui::Button("Cut (Ctrl+X)##Sel", btn_sz)) {
			cut_selection();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Cut selected cells to clipboard (Ctrl+X)");

		// Row 2: Paste and Delete
		if (!has_clipboard)
			ImGui::BeginDisabled();
		if (ImGui::Button("Paste (Ctrl+V)##Sel", btn_sz)) {
			paste_clipboard();
		}
		if (!has_clipboard)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Paste clipboard at cursor (Ctrl+V)");

		ImGui::SameLine();
		if (!has_selection)
			ImGui::BeginDisabled();
		if (ImGui::Button("Delete (Del)##Sel", btn_sz)) {
			fill_selection(0);
			deselect();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Clear selected cells on grid (Delete)");

		// Row 3: Fill and Deselect
		if (!has_selection)
			ImGui::BeginDisabled();
		if (ImGui::Button("Fill (Ctrl+F)##Sel", btn_sz)) {
			fill_selection(selected_id);
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Fill selected cells with current material (F or Ctrl+F)");

		ImGui::SameLine();
		if (!has_selection)
			ImGui::BeginDisabled();
		if (ImGui::Button("Deselect (Esc)##Sel", btn_sz)) {
			deselect();
		}
		if (!has_selection)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Clear active selection (Esc)");

		// Row 4: Rotate CW and Rotate CCW
		bool can_rotate = (has_selection || selection_state == SelectionState::Moving ||
						   (selection_state == SelectionState::Pasting && !clipboard.empty()));
		if (!can_rotate)
			ImGui::BeginDisabled();
		if (ImGui::Button("Rotate CW (Q)##Sel", btn_sz)) {
			rotate_selection(true);
		}
		if (!can_rotate)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Rotate selection 90° clockwise (Q)");

		ImGui::SameLine();
		if (!can_rotate)
			ImGui::BeginDisabled();
		if (ImGui::Button("Rotate CCW (E)##Sel", btn_sz)) {
			rotate_selection(false);
		}
		if (!can_rotate)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("Rotate selection 90° counter-clockwise (E)");

		// Row 5: Transparent Checkbox
		ImGui::Spacing();
		ImGui::Checkbox("Transparent", &transparent_mode);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"When enabled, air/empty cells in clipboard or moved selection will not overwrite existing cells.");
		}

		ImGui::Spacing();
		if (has_selection) {
			ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Selected: %dx%d (%d cells)", selection_box.width(),
							   selection_box.height(), selection_box.width() * selection_box.height());
			ImGui::TextDisabled("Drag inside: Move | Drag handles: Resize | Arrows: Nudge | Q/E: Rotate");
		} else if (selection_state == SelectionState::Pasting) {
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f),
							   "Pasting: %dx%d (Click canvas to stamp, Q/E to rotate, Esc to cancel)", clipboard.width,
							   clipboard.height);
		} else {
			ImGui::TextDisabled("Click & drag to select area | Click canvas to deselect");
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

			// Semi-transparent fill
			draw_list->AddRectFilled(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2),
									 IM_COL32(50, 150, 255, 30));

			// Dual-tone border
			draw_list->AddRect(ImVec2(srect.x1 - 1.0f, srect.y1 - 1.0f), ImVec2(srect.x2 + 1.0f, srect.y2 + 1.0f),
							   IM_COL32(0, 0, 0, 220), 0.0f, 0, 2.0f);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), IM_COL32(80, 200, 255, 255),
							   0.0f, 0, 1.5f);

			if (selection_state == SelectionState::Selected || selection_state == SelectionState::Resizing) {
				float handle_size = 5.0f;
				ImVec2 handle_pts[8] = {
					ImVec2(srect.x1, srect.y1),						 // TopLeft
					ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y1),	 // Top
					ImVec2(srect.x2, srect.y1),						 // TopRight
					ImVec2(srect.x2, (srect.y1 + srect.y2) * 0.5f),	 // Right
					ImVec2(srect.x2, srect.y2),						 // BottomRight
					ImVec2((srect.x1 + srect.x2) * 0.5f, srect.y2),	 // Bottom
					ImVec2(srect.x1, srect.y2),						 // BottomLeft
					ImVec2(srect.x1, (srect.y1 + srect.y2) * 0.5f)	 // Left
				};
				ResizeHandle handle_enums[8] = {
					ResizeHandle::TopLeft,	   ResizeHandle::Top,	 ResizeHandle::TopRight,   ResizeHandle::Right,
					ResizeHandle::BottomRight, ResizeHandle::Bottom, ResizeHandle::BottomLeft, ResizeHandle::Left};
				ResizeHandle hovered_handle = (selection_state == SelectionState::Resizing)
												  ? active_resize_handle
												  : get_hovered_resize_handle(srect, io.MousePos);

				for (int i = 0; i < 8; ++i) {
					bool is_active = (hovered_handle == handle_enums[i]);
					float sz = is_active ? (handle_size + 1.5f) : handle_size;
					ImU32 fill_col = is_active ? IM_COL32(100, 220, 255, 255) : IM_COL32(255, 255, 255, 255);
					ImU32 border_col = is_active ? IM_COL32(0, 50, 100, 255) : IM_COL32(0, 0, 0, 255);
					draw_list->AddRectFilled(ImVec2(handle_pts[i].x - sz, handle_pts[i].y - sz),
											 ImVec2(handle_pts[i].x + sz, handle_pts[i].y + sz), fill_col);
					draw_list->AddRect(ImVec2(handle_pts[i].x - sz, handle_pts[i].y - sz),
									   ImVec2(handle_pts[i].x + sz, handle_pts[i].y + sz), border_col, 0.0f, 0,
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
			int bw = selection_box.width();
			int bh = selection_box.height();
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
		return;
	}

	if (io.WantCaptureMouse)
		return;

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
		int cx = static_cast<int>(grid_pos.x);
		int cy = static_cast<int>(grid_pos.y);
		ScreenRect scell = grid_to_screen_rect(cx, cy, 1);
		draw_list->AddRect(ImVec2(scell.x1, scell.y1), ImVec2(scell.x2, scell.y2), line_color, 0.0f, 0, 3.0f);
		draw_list->AddRect(ImVec2(scell.x1, scell.y1), ImVec2(scell.x2, scell.y2), inner_line_color, 0.0f, 0, 1.5f);
	} else if (is_shift_down && (is_left_down || is_right_down)) {
		ImGuiMouseButton btn = is_left_down ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
		ImVec2 start_mouse = io.MouseClickedPos[btn];

		ImVec2 start_grid = screen_to_grid_pos(start_mouse);
		ImVec2 end_grid = screen_to_grid_pos(cur_mouse);

		GridRect start_brush = calculate_brush_bounds(start_grid, mouse_size);
		GridRect end_brush = calculate_brush_bounds(end_grid, mouse_size);

		ScreenRect s_start = grid_to_screen_rect(start_brush.x, start_brush.y, start_brush.w);
		ScreenRect s_end = grid_to_screen_rect(end_brush.x, end_brush.y, end_brush.w);

		if (brush_shape == BrushShape::Square) {
			std::vector<ImVec2> hull = compute_swept_brush_hull(s_start, s_end);
			if (!hull.empty()) {
				draw_list->AddConvexPolyFilled(hull.data(), static_cast<int>(hull.size()), fill_color);
				draw_list->AddPolyline(hull.data(), static_cast<int>(hull.size()), line_color, ImDrawFlags_Closed,
									   3.0f);
				draw_list->AddPolyline(hull.data(), static_cast<int>(hull.size()), inner_line_color, ImDrawFlags_Closed,
									   1.5f);
			}
		} else {
			ImVec2 c_start((s_start.x1 + s_start.x2) * 0.5f, (s_start.y1 + s_start.y2) * 0.5f);
			ImVec2 c_end((s_end.x1 + s_end.x2) * 0.5f, (s_end.y1 + s_end.y2) * 0.5f);
			float r = (s_start.x2 - s_start.x1) * 0.5f;

			float dx = c_end.x - c_start.x;
			float dy = c_end.y - c_start.y;
			float len = std::sqrt(dx * dx + dy * dy);

			if (len > 0.5f) {
				float angle = std::atan2(dy, dx);
				float half_pi = 1.57079632679f;

				draw_list->PathClear();
				draw_list->PathArcTo(c_start, r, angle + half_pi, angle + 3.0f * half_pi, 16);
				draw_list->PathArcTo(c_end, r, angle - half_pi, angle + half_pi, 16);
				draw_list->PathFillConvex(fill_color);

				draw_list->PathClear();
				draw_list->PathArcTo(c_start, r, angle + half_pi, angle + 3.0f * half_pi, 16);
				draw_list->PathArcTo(c_end, r, angle - half_pi, angle + half_pi, 16);
				draw_list->PathStroke(line_color, ImDrawFlags_Closed, 3.0f);

				draw_list->PathClear();
				draw_list->PathArcTo(c_start, r, angle + half_pi, angle + 3.0f * half_pi, 16);
				draw_list->PathArcTo(c_end, r, angle - half_pi, angle + half_pi, 16);
				draw_list->PathStroke(inner_line_color, ImDrawFlags_Closed, 1.5f);
			} else {
				draw_list->AddCircleFilled(c_start, r, fill_color);
				draw_list->AddCircle(c_start, r, line_color, 0, 3.0f);
				draw_list->AddCircle(c_start, r, inner_line_color, 0, 1.5f);
			}
		}
	} else {
		ImVec2 grid_pos = screen_to_grid_pos(cur_mouse);
		GridRect brush = calculate_brush_bounds(grid_pos, mouse_size);
		ScreenRect srect = grid_to_screen_rect(brush.x, brush.y, brush.w);

		if (brush_shape == BrushShape::Square) {
			draw_list->AddRectFilled(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), fill_color, 0.0f, 0);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), line_color, 0.0f, 0, 3.0f);
			draw_list->AddRect(ImVec2(srect.x1, srect.y1), ImVec2(srect.x2, srect.y2), inner_line_color, 0.0f, 0, 1.5f);
		} else {
			ImVec2 center((srect.x1 + srect.x2) * 0.5f, (srect.y1 + srect.y2) * 0.5f);
			float r = (srect.x2 - srect.x1) * 0.5f;
			draw_list->AddCircleFilled(center, r, fill_color);
			draw_list->AddCircle(center, r, line_color, 0, 3.0f);
			draw_list->AddCircle(center, r, inner_line_color, 0, 1.5f);
		}
	}
}

void UI::handle_zoom_and_pan(ImGuiIO& io) {
	auto [win_w, win_h] = Window::get_size();
	float rem_w = static_cast<float>(win_w);
	float rem_h = static_cast<float>(win_h);
	float sim_aspect = static_cast<float>(SIM_WIDTH) / SIM_HEIGHT;

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

	// WASD camera movement
	if (!io.WantTextInput && !io.KeyCtrl && !io.KeyAlt) {
		float pan_dir_x = 0.0f;
		float pan_dir_y = 0.0f;
		if (ImGui::IsKeyDown(ImGuiKey_W))
			pan_dir_y -= 1.0f;
		if (ImGui::IsKeyDown(ImGuiKey_S))
			pan_dir_y += 1.0f;
		if (ImGui::IsKeyDown(ImGuiKey_A))
			pan_dir_x -= 1.0f;
		if (ImGui::IsKeyDown(ImGuiKey_D))
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
			target_pan_x = std::clamp(target_pan_x, -static_cast<float>(SIM_WIDTH), static_cast<float>(SIM_WIDTH));
			target_pan_y = std::clamp(target_pan_y, -static_cast<float>(SIM_HEIGHT), static_cast<float>(SIM_HEIGHT));
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
		target_pan_x -= delta.x * (SIM_WIDTH / w_new);
		target_pan_y -= delta.y * (SIM_HEIGHT / h_new);
		target_pan_x = std::clamp(target_pan_x, -static_cast<float>(SIM_WIDTH), static_cast<float>(SIM_WIDTH));
		target_pan_y = std::clamp(target_pan_y, -static_cast<float>(SIM_HEIGHT), static_cast<float>(SIM_HEIGHT));
	}

	pan_x = pan_x + (target_pan_x - pan_x) * 0.15f;
	pan_y = pan_y + (target_pan_y - pan_y) * 0.15f;

	float offset_screen_x = pan_x * (w_new / SIM_WIDTH);
	float offset_screen_y = pan_y * (h_new / SIM_HEIGHT);
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

	if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, true)) {
		if (io.KeyShift) {
			UndoManager::redo();
		} else {
			UndoManager::undo();
		}
	} else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, true)) {
		UndoManager::redo();
	}

	// Tool mode & Copy / Cut / Paste / Duplicate
	if (ImGui::IsKeyPressed(ImGuiKey_C, false)) {
		if (io.KeyCtrl) {
			if (current_tool != ToolMode::Select) {
				set_tool_mode(ToolMode::Select);
			}
			copy_selection();
		} else {
			set_tool_mode(ToolMode::Select);
		}
	} else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X, false)) {
		if (selection_state != SelectionState::Selected) {
			set_tool_mode(ToolMode::Select);
		}
		cut_selection();
	} else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
		if (selection_state != SelectionState::Selected) {
			set_tool_mode(ToolMode::Select);
		}
		paste_clipboard();
	} else if (ImGui::IsKeyPressed(ImGuiKey_B, false)) {
		set_tool_mode(ToolMode::Brush);
	} else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		if (selection_state == SelectionState::Selected) {
			fill_selection(0);
			deselect();
		}
	} else if (!update && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, true)) {
		step_frame = true;
	} else if (selection_state == SelectionState::Selected) {
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, true)) {
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
			int new_x = std::clamp(min_x + nudge_x, 0, static_cast<int>(SIM_WIDTH) - bw);
			int new_y = std::clamp(min_y + nudge_y, 0, static_cast<int>(SIM_HEIGHT) - bh);
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

	if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
		update = !update;
		if (update) {
			UndoManager::push_snapshot("Resume Simulation");
		}
	} else if (ImGui::IsKeyPressed(ImGuiKey_T, false)) {
		brush_shape = static_cast<BrushShape>((static_cast<int>(brush_shape) + 1) % static_cast<int>(BrushShape::Size));
	} else if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
		ui_compact = !ui_compact;
	}

	// Rotate selection: Q for Clockwise, E for Counter-Clockwise
	bool can_rotate = (selection_state == SelectionState::Selected || selection_state == SelectionState::Moving ||
					   (selection_state == SelectionState::Pasting && !clipboard.empty()));
	if (can_rotate) {
		if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
			rotate_selection(true);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
			rotate_selection(false);
		}
	}

	// Reset / Clear grid: R (plain R or with Ctrl/Shift)
	if (ImGui::IsKeyPressed(ImGuiKey_R, false) ||
		(io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Delete, false))) {
		Grid::clear();
		UndoManager::push_snapshot("Clear Grid");
	} else if (ImGui::IsKeyPressed(ImGuiKey_PageUp) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
		target_zoom *= 1.2f;
	} else if (ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
		target_zoom /= 1.2f;
	}

	uint8_t material_count = MaterialManager::get_material_count();
	for (int i = 1; i <= 9; ++i) {
		if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + (i - 1))) && material_count > i) {
			selected_id = MaterialManager::get_materials()[i].id;
		}
	}

	if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
		SDL_Window* window = Window::get_window();
		SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
	} else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
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
	}
}

void UI::handle_mouse_wheel_brush_size(ImGuiIO& io) {
	if (!io.WantCaptureMouse && io.MouseWheel != 0.0f && !io.KeyShift) {
		bool fast = io.KeyCtrl;
		mouse_size += static_cast<int>(io.MouseWheel) * (fast ? 5 : 1);
		mouse_size = std::clamp(mouse_size, 1, 512);
	}
}

void UI::handle_canvas_interaction() {
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse && selection_state != SelectionState::Moving && selection_state != SelectionState::Resizing)
		return;

	ImVec2 grid_pos = screen_to_grid_pos(io.MousePos);

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle)) {
		ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
		if (drag_delta.x * drag_delta.x + drag_delta.y * drag_delta.y < 25.0f) {
			uint32_t x_cell = static_cast<uint32_t>(grid_pos.x);
			uint32_t y_cell = static_cast<uint32_t>(grid_pos.y);
			if (x_cell < SIM_WIDTH && y_cell < SIM_HEIGHT) {
				uint8_t cell = Grid::get_cell(x_cell, y_cell);
				selected_id = MaterialManager::get_material(cell).id;
			}
		}
	}

	if (current_tool == ToolMode::Select) {
		// Pasting state
		if (selection_state == SelectionState::Pasting) {
			if (clipboard.empty()) {
				selection_state = SelectionState::None;
				return;
			}
			int px = static_cast<int>(grid_pos.x) - clipboard.width / 2;
			int py = static_cast<int>(grid_pos.y) - clipboard.height / 2;
			current_floating_x = std::clamp(px, 0, std::max(0, static_cast<int>(SIM_WIDTH) - clipboard.width));
			current_floating_y = std::clamp(py, 0, std::max(0, static_cast<int>(SIM_HEIGHT) - clipboard.height));

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				for (int y = 0; y < clipboard.height; ++y) {
					for (int x = 0; x < clipboard.width; ++x) {
						int gx = current_floating_x + x;
						int gy = current_floating_y + y;
						if (gx >= 0 && gx < static_cast<int>(SIM_WIDTH) && gy >= 0 &&
							gy < static_cast<int>(SIM_HEIGHT)) {
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

		// Moving state
		if (selection_state == SelectionState::Moving) {
			int bw = selection_box.width();
			int bh = selection_box.height();
			int gx = static_cast<int>(grid_pos.x);
			int gy = static_cast<int>(grid_pos.y);
			current_floating_x = std::clamp(gx - move_grab_offset_x, 0, std::max(0, static_cast<int>(SIM_WIDTH) - bw));
			current_floating_y = std::clamp(gy - move_grab_offset_y, 0, std::max(0, static_cast<int>(SIM_HEIGHT) - bh));

			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				for (int y = 0; y < bh; ++y) {
					for (int x = 0; x < bw; ++x) {
						int fgx = current_floating_x + x;
						int fgy = current_floating_y + y;
						if (fgx >= 0 && fgx < static_cast<int>(SIM_WIDTH) && fgy >= 0 &&
							fgy < static_cast<int>(SIM_HEIGHT)) {
							uint8_t cell = floating_cells[y * bw + x];
							if (!transparent_mode || cell != 0) {
								Grid::set_cell(fgx, fgy, cell);
							}
						}
					}
				}
				floating_cells.clear();
				if (current_floating_x != move_origin_x || current_floating_y != move_origin_y) {
					UndoManager::push_snapshot("Move Selection");
				}
				selection_box.start_x = current_floating_x;
				selection_box.start_y = current_floating_y;
				selection_box.current_x = current_floating_x + bw - 1;
				selection_box.current_y = current_floating_y + bh - 1;
				selection_state = SelectionState::Selected;
			} else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				deselect();
			}
			return;
		}

		// Resizing state
		if (selection_state == SelectionState::Resizing) {
			int gx = std::clamp(static_cast<int>(grid_pos.x), 0, static_cast<int>(SIM_WIDTH) - 1);
			int gy = std::clamp(static_cast<int>(grid_pos.y), 0, static_cast<int>(SIM_HEIGHT) - 1);

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

		// None, Selecting, Selected
		int gx = std::clamp(static_cast<int>(grid_pos.x), 0, static_cast<int>(SIM_WIDTH) - 1);
		int gy = std::clamp(static_cast<int>(grid_pos.y), 0, static_cast<int>(SIM_HEIGHT) - 1);

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
					// Start moving
					int min_x = selection_box.min_x();
					int min_y = selection_box.min_y();
					int bw = selection_box.width();
					int bh = selection_box.height();
					floating_cells.resize(bw * bh);
					for (int y = 0; y < bh; ++y) {
						for (int x = 0; x < bw; ++x) {
							floating_cells[y * bw + x] = Grid::get_cell(min_x + x, min_y + y);
							Grid::set_cell(min_x + x, min_y + y, 0);
						}
					}
					move_origin_x = min_x;
					move_origin_y = min_y;
					move_grab_offset_x = gx - min_x;
					move_grab_offset_y = gy - min_y;
					current_floating_x = min_x;
					current_floating_y = min_y;
					selection_state = SelectionState::Moving;
					return;
				}
			}

			// Start new selection (or will deselect on release if not dragged)
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
	}

	if (is_shift && is_alt && (left_clicked || right_clicked)) {
		uint32_t target_x = static_cast<uint32_t>(grid_pos.x);
		uint32_t target_y = static_cast<uint32_t>(grid_pos.y);
		uint8_t fill_mat = left_clicked ? static_cast<uint8_t>(selected_id) : 0;
		flood_fill(target_x, target_y, fill_mat);
		UndoManager::commit_grid_snapshot_if_changed("Flood Fill");
	} else if (is_shift && !is_alt && (left_released || right_released)) {
		ImGuiMouseButton btn = left_released ? ImGuiMouseButton_Left : ImGuiMouseButton_Right;
		ImVec2 start_grid = screen_to_grid_pos(io.MouseClickedPos[btn]);
		uint8_t mat_id = left_released ? static_cast<uint8_t>(selected_id) : 0;
		paint_line(start_grid, grid_pos, mouse_size, mat_id, brush_shape);
		UndoManager::commit_grid_snapshot_if_changed("Draw Line");
	} else if (!is_shift && !is_alt) {
		if (left_down || right_down) {
			GridRect brush = calculate_brush_bounds(grid_pos, mouse_size);
			uint8_t mat_id = left_down ? static_cast<uint8_t>(selected_id) : 0;
			paint_brush_at(brush.x, brush.y, brush.w, mat_id, brush_shape);
			ImGui::ResetMouseDragDelta();
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
	ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "SAND3 SIMULATOR");
	ImGui::Text("FPS: %.1f (%.3f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
	ImGui::Text("Active cells: %u", Grid::get_changed_cells());
	ImGui::Separator();
	render_selection_controls();
	ImGui::Separator();
}

void UI::render_sim_content() {
	ImGui::Spacing();
	if (update) {
		if (ImGui::Button("Pause", ImVec2(-1, 30))) {
			update = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Pauses the simulation.");
		}
	} else {
		if (ImGui::Button("Resume", ImVec2(-1, 30))) {
			update = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Resumes the simulation.");
		}
	}

	if (update) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Step Frame", ImVec2(-1, 30))) {
		step_frame = true;
		UndoManager::push_snapshot("Step Simulation");
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Steps the simulation by one frame (F).");
	}
	if (update) {
		ImGui::EndDisabled();
	}

	ImGui::Spacing();

	if (ImGui::Button("Clear Grid (R)", ImVec2(-1, 30))) {
		Grid::clear();
		UndoManager::push_snapshot("Clear Grid");
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Clears all cells on the grid (R).");
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
	}
	ImVec4 color =
		ImVec4(materials[0].color[0] / 255.f, materials[0].color[1] / 255.f, materials[0].color[2] / 255.f, 1.f);
	ImGui::ColorButton("##sim_color_0", color, ImGuiColorEditFlags_NoTooltip, ImVec2(15, 15));
	ImGui::SameLine();
	if (ImGui::Selectable((materials[0].name + "##sim_0").c_str(), selected_id == 0)) {
		selected_id = 0;
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

void UI::render_material_editor() {
	std::vector<MaterialDefinition>& materials = MaterialManager::get_materials();

	if (ImGui::BeginTabItem("Materials")) {
		ImGui::Spacing();
		bool rebuild_needed = false;

		if (ImGui::CollapsingHeader("Materials List", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::BeginChild("MaterialsListScroll", ImVec2(0, 150), true);

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
					ImGui::SetTooltip("Has %zu rule%s", materials[i].rules.size(),
									  (materials[i].rules.size() == 1 ? "" : "s"));
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
				ImGui::SetTooltip("Has %zu rule%s", materials[0].rules.size(),
								  (materials[0].rules.size() == 1 ? "" : "s"));
			}

			ImGui::EndChild();

			ImGui::Spacing();
			const bool at_max = materials.size() == 255;
			if (at_max) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("New", ImVec2(80, 25))) {
				UndoManager::push_snapshot("New Material");
				MaterialDefinition new_mat;
				new_mat.name = "material_" + std::to_string(materials.size());
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
			if (ImGui::Button("Copy", ImVec2(80, 25))) {
				UndoManager::push_snapshot("Copy Material");
				MaterialDefinition duplicated_mat = MaterialManager::get_material(selected_id);
				duplicated_mat.name = duplicated_mat.name + "_copy";
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
			if (ImGui::Button("Delete", ImVec2(80, 25))) {
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

		auto& mat = const_cast<MaterialDefinition&>(MaterialManager::get_material(selected_id));

		ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "Editing: %s", mat.name.c_str());

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

		if (ImGui::Button("Add Rule")) {
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

			if (ImGui::Button("Copy", ImVec2(45, 0))) {
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
			if (ImGui::Button("X", ImVec2(btn_size, btn_size))) {
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
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete this rule.");
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

void UI::render_manage_sets() {
	const std::string& current_set = SetManager::get_current_set();
	SetMetadata meta = SetManager::get_current_metadata();

	if (ImGui::BeginTabItem("Sets")) {
		ImGui::Spacing();

		ImGui::Text("Available Sets:");
		ImGui::BeginChild("SetsListScroll", ImVec2(0, 150), true);
		for (const auto& s : SetManager::get_sets()) {
			SetMetadata s_meta = SetManager::load_set_metadata(s);
			if (ImGui::Selectable((s + "##selectable_set_" + s).c_str(), s == current_set)) {
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
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (ImGui::Button("New", ImVec2(80, 25))) {
			open_create_set_popup = true;
			duplicate_set_checkbox = false;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create a new empty material set.");
		}

		ImGui::SameLine();
		if (ImGui::Button("Copy", ImVec2(80, 25))) {
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
			if (ImGui::Button("Delete", ImVec2(80, 25))) {
				open_delete_set_popup = true;
			}
			ImGui::PopStyleColor(3);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Delete the current set.");
			}
		} else {
			ImGui::BeginDisabled();
			ImGui::Button("Delete", ImVec2(80, 25));
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("Cannot delete the only set.");
			}
			ImGui::EndDisabled();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "Set Settings:");

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
		if (ImGui::Button("Save Current Set", ImVec2(-1, 30))) {
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

		ImGui::Text("Save Current Simulation:");
		ImGui::InputText("Save Name##save_name", save_file_name_buf, sizeof(save_file_name_buf));
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Input a name for the save file.");
		}

		if (ImGui::Button("Save Simulation", ImVec2(-1, 30))) {
			std::string s_name = save_file_name_buf;
			if (!s_name.empty()) {
				if (SaveManager::save_to_file(s_name, current_set)) {
					save_file_name_buf[0] = '\0';
				}
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Saves grid state to a .save file.");
		}

		ImGui::Separator();
		ImGui::Text("Available Saves in current set folder:");

		std::vector<std::string> save_files;
		std::string set_dir = SETS_DIRECTORY + current_set;
		if (fs::exists(set_dir) && fs::is_directory(set_dir)) {
			for (const auto& entry : fs::directory_iterator(set_dir)) {
				if (entry.is_regular_file() && entry.path().extension() == ".save") {
					save_files.push_back(entry.path().filename().string());
				}
			}
		}

		ImGui::BeginChild("SavesListScroll", ImVec2(0, 180), true);
		for (uint32_t i = 0; i < static_cast<uint32_t>(save_files.size()); ++i) {
			bool is_selected = (selected_save_id == i);
			if (ImGui::Selectable(save_files[i].c_str(), selected_save_id == i)) {
				selected_save_id = static_cast<int>(i);
			}
			if (is_selected && ImGui::IsMouseDoubleClicked(0)) {
				if (unsaved_changes) {
					pending_save_load = save_files[i];
					open_switch_popup = true;
				} else {
					std::string loaded_set;
					if (SaveManager::load_from_file(save_files[i], current_set, loaded_set)) {
						selected_id = 0;
						unsaved_changes = false;
					}
				}
			}
		}
		ImGui::EndChild();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Double click to load. Switches current material set.");
		}

		ImGui::Spacing();
		if (selected_save_id >= 0 && selected_save_id < static_cast<int>(save_files.size())) {
			if (ImGui::Button("Load", ImVec2(80, 25))) {
				if (unsaved_changes) {
					pending_save_load = save_files[selected_save_id];
					open_switch_popup = true;
				} else {
					std::string loaded_set;
					if (SaveManager::load_from_file(save_files[selected_save_id], current_set, loaded_set)) {
						selected_id = 0;
						unsaved_changes = false;
					}
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Load the selected save file.");
			}

			ImGui::SameLine();
			if (ImGui::Button("Copy", ImVec2(80, 25))) {
				const std::string old_name = save_files[selected_save_id];
				std::string new_name = old_name;
				const size_t dot = new_name.find_last_of('.');
				if (dot != std::string::npos) {
					new_name = new_name.substr(0, dot) + "_copy" + new_name.substr(dot);
				} else {
					new_name += "_copy";
				}
				const std::string old_path = SETS_DIRECTORY + current_set + "/" + old_name;
				const std::string new_path = SETS_DIRECTORY + current_set + "/" + new_name;
				try {
					fs::copy(old_path, new_path);
				} catch (...) {}
				selected_save_id = -1;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Duplicate the selected save file.");
			}

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.15f, 0.15f, 1.0f));
			if (ImGui::Button("Delete", ImVec2(80, 25))) {
				std::string filepath = SETS_DIRECTORY + current_set + "/" + save_files[selected_save_id];
				try {
					fs::remove(filepath);
				} catch (...) {}
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
}

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
			if (ImGui::SliderInt("Active Threads", &thread_count, 1, NUM_STRIPS_Y / 2)) {
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

		ImGui::EndTabItem();
	}
}

void UI::render_shortcuts() {
	if (ImGui::BeginTabItem("Shortcuts")) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "Keyboard & Mouse Shortcuts");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Simulation:");
		ImGui::BulletText("Space: Toggle simulation");
		ImGui::BulletText("F: Step simulation by one frame (when paused)");
		ImGui::BulletText("R: Clear / Reset grid");

		ImGui::Spacing();
		ImGui::Text("Camera:");
		ImGui::BulletText("W / A / S / D: Move camera (Hold Shift for fast pan)");
		ImGui::BulletText("Middle Mouse Drag: Pan camera");
		ImGui::BulletText("Shift + Scroll / PageUp/PageDown / +/-: Zoom camera");

		ImGui::Spacing();
		ImGui::Text("General:");
		ImGui::BulletText("V: Toggle compact UI");
		ImGui::BulletText("Ctrl + Z: Undo last action");
		ImGui::BulletText("Ctrl + Y / Ctrl + Shift + Z: Redo action");
		ImGui::BulletText("F11: Toggle fullscreen");
		ImGui::BulletText("Escape: Cancel selection/paste/move, or Quit");

		ImGui::Spacing();
		ImGui::Text("Tools & Selection:");
		ImGui::BulletText("B: Switch to Brush tool");
		ImGui::BulletText("Left Mouse Drag (Select mode): Select box region");
		ImGui::BulletText("Left Mouse Drag (inside box): Move selected cells");
		ImGui::BulletText("Left Mouse Drag (handles): Resize selection (corners & midpoints)");
		ImGui::BulletText("Q: Rotate selection 90° clockwise");
		ImGui::BulletText("E: Rotate selection 90° counter-clockwise");
		ImGui::BulletText("Ctrl + C: Copy selection");
		ImGui::BulletText("Ctrl + X: Cut selected cells to clipboard");
		ImGui::BulletText("Ctrl + V: Paste clipboard at cursor (Left click to stamp)");
		ImGui::BulletText("Ctrl + F: Fill selected cells with selected material");
		ImGui::BulletText("Delete: Delete selected cells");
		ImGui::BulletText("Arrow Keys (Shift for 10x): Nudge selected cells");
		ImGui::BulletText("Escape / Right Click: Deselect / Cancel move or paste");

		ImGui::Spacing();
		ImGui::Text("Grid:");
		ImGui::BulletText("Left Mouse Button: Draw material");
		ImGui::BulletText("Right Mouse Button: Erase material");
		ImGui::BulletText("Shift + Mouse Drag: Draw straight line or erase");
		ImGui::BulletText("Shift + Alt + Mouse Click: Flood fill or erase");
		ImGui::BulletText("Middle Mouse Button: Eyedropper (pick material)");

		ImGui::Text("Brush:");
		ImGui::BulletText("C: Switch to Selection tool");
		ImGui::BulletText("T: Next brush shape (Square, Circle)");
		ImGui::BulletText("Scroll Up/Down: Adjust brush size");
		ImGui::BulletText("Ctrl + Scroll Up/Down: Faster brush size adjust");
		ImGui::Spacing();

		ImGui::Text("Rule Grid:");
		ImGui::BulletText("Left Click: Select material(s)");
		ImGui::BulletText("Shift + Left Click / Drag: Paint material(s)");
		ImGui::BulletText("Middle Click: Copy material(s)");
		ImGui::BulletText("Right Click: Clear cell(s)");
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

	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(440.0f, -1.0f), ImVec2(FLT_MAX, -1.0f));
	if (ImGui::BeginPopupModal("Performance Warning##EmptyRule", nullptr,
							   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.00f, 0.70f, 0.20f, 1.00f), "PERFORMANCE WARNING");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped(
			"Adding rules to the 'empty' material means every empty cell on the grid will be evaluated every frame.");
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
			ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "DUPLICATE CURRENT SET");
		} else {
			ImGui::TextColored(ImVec4(0.40f, 0.70f, 1.00f, 1.00f), "CREATE NEW EMPTY SET");
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
			std::string loaded_set;
			if (SaveManager::load_from_file(pending_save_load, current_set, loaded_set)) {
				selected_id = 0;
				unsaved_changes = false;
			}
			pending_save_load = "";
		}
	}
}