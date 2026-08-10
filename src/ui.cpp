#include "ui.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "config_manager.hpp"
#include "grid.hpp"
#include "material_manager.hpp"
#include "roboto.h"
#include "sanitize.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "undo_manager.hpp"
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

static GridRect calculate_brush_bounds(ImVec2 grid_pos, int brush_size) {
	int x_start = static_cast<int>(grid_pos.x - static_cast<float>(brush_size) / 2.0f + 0.5f);
	int y_start = static_cast<int>(grid_pos.y - static_cast<float>(brush_size) / 2.0f + 0.5f);
	return {x_start, y_start, brush_size, brush_size};
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
	io.Fonts->AddFontFromMemoryTTF(const_cast<uint8_t*>(___assets_Roboto_Regular_ttf), ___assets_Roboto_Regular_ttf_len,
								   16.0f, &font_cfg);

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
}

void UI::render_mouse_overlay() {
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse)
		return;

	ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
	ImVec2 cur_mouse = io.MousePos;

	bool is_shift_down = io.KeyShift;
	bool is_alt_down = io.KeyAlt;
	bool is_left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool is_right_down = ImGui::IsMouseDown(ImGuiMouseButton_Right);

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
	if (!io.WantTextInput) {
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			if (io.KeyShift) {
				UndoManager::redo();
			} else {
				UndoManager::undo();
			}
		} else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			UndoManager::redo();
		}
	}

	if (io.WantTextInput)
		return;

	if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
		update = !update;
		if (update) {
			UndoManager::push_snapshot("Resume Simulation");
		}
	}
	if (ImGui::IsKeyPressed(ImGuiKey_F) && !update) {
		step_frame = true;
		UndoManager::push_snapshot("Step Simulation");
	}
	if (ImGui::IsKeyPressed(ImGuiKey_T)) {
		brush_shape = static_cast<BrushShape>((static_cast<int>(brush_shape) + 1) % static_cast<int>(BrushShape::Size));
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Q)) {
		ui_compact = !ui_compact;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_R)) {
		UndoManager::push_snapshot("Clear Grid");
		Grid::clear();
	}
	if (ImGui::IsKeyPressed(ImGuiKey_PageUp) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
		target_zoom *= 1.2f;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
		target_zoom /= 1.2f;
	}
	uint8_t material_count = MaterialManager::get_material_count();
	for (int i = 1; i <= 9; ++i) {
		if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + (i - 1))) && material_count > i) {
			selected_id = MaterialManager::get_materials()[i].id;
		}
	}
	if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
		SDL_Window* window = Window::get_window();
		SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
	}
	if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
		show_exit_popup = true;
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
	if (io.WantCaptureMouse)
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
			UndoManager::push_snapshot("Resume Simulation");
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

	if (ImGui::Button("Clear Grid", ImVec2(-1, 30))) {
		Grid::clear();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Clears all cells on the grid.");
	}

	ImGui::Separator();
	ImGui::Text("Brush Settings:");
	ImGui::SliderInt("Brush Size", &mouse_size, 1, 512);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Adjust brush width.");
	}

	const char* shape_names[] = {"Square", "Circle"};
	int current_shape = static_cast<int>(brush_shape);
	if (ImGui::Combo("Brush Shape", &current_shape, shape_names, IM_ARRAYSIZE(shape_names))) {
		brush_shape = static_cast<BrushShape>(current_shape);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Select brush shape (Square or Circle).");
	}

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
				ImGui::PopStyleColor(3);
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

		ImGui::Text("Simulation Quality Preset");
		QualityPreset q = Grid::get_quality_preset();

		if (ImGui::RadioButton("Slow (Accuracy)", q == QualityPreset::Slow)) {
			Grid::set_quality_preset(QualityPreset::Slow);
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Total accuracy for precise cellular automata (e.g. Sierpinski triangles or other fractal patterns).");
		}

		if (ImGui::RadioButton("Fast (Multithreaded)", q == QualityPreset::Fast)) {
			Grid::set_quality_preset(QualityPreset::Fast);
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Fast multithreaded simulation. Ideal for fast physics simulations (e.g. sand, fire, etc.)");
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		bool mt_active = (q != QualityPreset::Slow);
		if (!mt_active) {
			ImGui::BeginDisabled();
		}
		int thread_count = static_cast<int>(Grid::get_thread_count());
		if (ImGui::SliderInt("Active Threads", &thread_count, 1, NUM_STRIPS_Y / 2)) {
			Grid::configure_threads(static_cast<uint32_t>(thread_count));
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered() && mt_active) {
			ImGui::SetTooltip("Sets the size of the worker thread pool. Automatically set to the maximum number of\n"
							  "parallelizable strips.");
		}
		if (!mt_active) {
			ImGui::EndDisabled();
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
		ImGui::BulletText("F: Step simulation by one frame");
		ImGui::BulletText("R: Clear grid");

		ImGui::Spacing();
		ImGui::Text("General:");
		ImGui::BulletText("Q: Toggle compact UI");
		ImGui::BulletText("Ctrl + Z: Undo last action");
		ImGui::BulletText("Ctrl + Y / Ctrl + Shift + Z: Redo action");
		ImGui::BulletText("F11: Toggle fullscreen");
		ImGui::BulletText("Escape: Quit");

		ImGui::Spacing();
		ImGui::Text("Grid:");
		ImGui::BulletText("Left Mouse Button: Draw material");
		ImGui::BulletText("Right Mouse Button: Erase material");
		ImGui::BulletText("Shift + Mouse Drag: Draw straight line or erase");
		ImGui::BulletText("Shift + Alt + Mouse Click: Flood fill or erase");
		ImGui::BulletText("Middle Mouse Button: Eyedropper (pick material)");

		ImGui::Text("Brush:");
		ImGui::BulletText("T: Next brush shape (Square, Circle)");
		ImGui::BulletText("Scroll Up/Down: Adjust brush size");
		ImGui::BulletText("Ctrl + Scroll Up/Down: Faster brush size adjust");
		ImGui::Spacing();

		ImGui::Text("Camera:");
		ImGui::BulletText("Shift + Middle Mouse Drag: Pan camera");
		ImGui::BulletText("Shift + Scroll / PageUp/PageDown / +/-: Zoom camera");
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