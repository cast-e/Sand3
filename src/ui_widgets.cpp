#include "ui_widgets.hpp"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "config_manager.hpp"

static ImU32 to_imu32(const std::array<float, 4>& c) {
	return ImGui::ColorConvertFloat4ToU32(ImVec4(c[0], c[1], c[2], c[3]));
}

bool UIWidgets::button_with_icon(const char* label, SDL_Texture* icon, const ImVec2& size_arg) {
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
	float icon_sz =
		icon ? std::min(base_icon_sz, std::max(8.0f, (req_h >= base_icon_sz + 2.0f) ? base_icon_sz : (req_h - 2.0f)))
			 : 0.0f;
	const float spacing = (icon && text_size.x > 0.0f) ? 6.0f : 0.0f;
	const float total_content_w = (icon ? icon_sz : 0.0f) + spacing + text_size.x;

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

bool UIWidgets::selectable_with_color(const char* label, const ImVec4& color, bool selected, ImGuiSelectableFlags flags,
									  const ImVec2& size_arg, bool show_checkbox) {
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiStyle& style = g.Style;

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	const ImVec2 text_size = ImGui::CalcTextSize(label, label_end, false);

	const float swatch_sz = 14.0f;
	const float check_sz = 14.0f;
	const float spacing = 6.0f;

	float total_content_w =
		(show_checkbox ? (check_sz + spacing) : 0.0f) + (color.w > 0.0f ? (swatch_sz + spacing) : 0.0f) + text_size.x;
	float min_h = std::max(text_size.y, swatch_sz);
	if (show_checkbox) {
		min_h = std::max(min_h, check_sz);
	}
	float req_h = (size_arg.y != 0.0f) ? size_arg.y : (min_h + style.FramePadding.y * 0.5f);

	ImGuiSelectableFlags actual_flags = flags;
	ImVec2 actual_size;
	if (size_arg.x != 0.0f) {
		actual_size = ImVec2(size_arg.x, req_h);
	} else {
		actual_size = ImVec2(total_content_w, req_h);
		actual_flags |= ImGuiSelectableFlags_SpanAvailWidth;
	}

	std::string id_str = "##sel_col_" + std::string(label);
	bool pressed = ImGui::Selectable(id_str.c_str(), selected, actual_flags, actual_size);

	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();
	float center_y = std::round(min.y + (max.y - min.y) * 0.5f);
	float cur_x = min.x + std::max(6.0f, style.FramePadding.x);

	if (show_checkbox) {
		ImVec2 check_min(cur_x, std::round(center_y - check_sz * 0.5f));
		ImVec2 check_max(cur_x + check_sz, check_min.y + check_sz);
		ImU32 check_bg = ImGui::GetColorU32(selected ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg);
		ImGui::RenderFrame(check_min, check_max, check_bg, true, style.FrameRounding);
		if (selected) {
			float pad = std::max(1.0f, IM_TRUNC(check_sz / 6.0f));
			ImGui::RenderCheckMark(window->DrawList, ImVec2(check_min.x + pad, check_min.y + pad),
								   ImGui::GetColorU32(ImGuiCol_CheckMark), check_sz - pad * 2.0f);
		}
		cur_x = check_max.x + spacing;
	}

	if (color.w > 0.0f) {
		ImVec2 swatch_min(cur_x, std::round(center_y - swatch_sz * 0.5f));
		ImVec2 swatch_max(cur_x + swatch_sz, swatch_min.y + swatch_sz);
		window->DrawList->AddRectFilled(swatch_min, swatch_max, ImGui::GetColorU32(color), 2.0f);
		window->DrawList->AddRect(swatch_min, swatch_max, IM_COL32(255, 255, 255, 45), 2.0f);
		window->DrawList->AddRect(swatch_min, swatch_max, ImGui::GetColorU32(style.Colors[ImGuiCol_Border]), 2.0f);
		cur_x = swatch_max.x + spacing;
	}

	if (text_size.x > 0.0f) {
		ImVec2 text_min(cur_x, min.y);
		ImVec2 text_max(max.x - style.FramePadding.x, max.y);
		ImRect clip_rect(min, max);
		ImGui::RenderTextClipped(text_min, text_max, label, label_end, &text_size, ImVec2(0.0f, 0.5f), &clip_rect);
	}

	return pressed;
}

bool UIWidgets::custom_checkbox(const char* label, bool* val, IconID icon_id, const char* tooltip,
								const ImVec2& size_arg) {
	if (*val) {
		const ImVec4& active_col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
		ImVec4 hover_col = ImVec4(std::min(1.0f, active_col.x * 1.30f), std::min(1.0f, active_col.y * 1.30f),
								  std::min(1.0f, active_col.z * 1.30f), 1.0f);
		ImVec4 act_col = ImVec4(active_col.x * 0.85f, active_col.y * 0.85f, active_col.z * 0.85f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, active_col);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover_col);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, act_col);
	} else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.28f, 0.50f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.38f, 0.8f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
	}

	SDL_Texture* tex = (icon_id != IconID::Count) ? IconManager::get(icon_id) : nullptr;
	bool clicked = button_with_icon(label, tex, size_arg);
	ImGui::PopStyleColor(3);

	if (clicked) {
		*val = !*val;
	}
	if (tooltip && tooltip[0] != '\0' && ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", tooltip);
	}
	return clicked;
}

bool UIWidgets::gradient_editor(const char* label, ColorGradient& gradient, float min_val, float max_val, float width) {
	if (max_val <= min_val) {
		max_val = min_val + 1.0f;
	}
	float range_span = max_val - min_val;

	if (gradient.stops.empty()) {
		gradient.stops = {{min_val, {0.0f, 0.0f, 0.0f, 1.0f}}, {max_val, {1.0f, 1.0f, 1.0f, 1.0f}}};
		gradient.selected_stop = 0;
	} else {
		bool all_01 = true;
		for (const auto& s : gradient.stops) {
			if (s.pos < 0.0f || s.pos > 1.0f) {
				all_01 = false;
				break;
			}
		}
		if (all_01 && range_span > 1.5f) {
			for (auto& s : gradient.stops) {
				s.pos = min_val + s.pos * range_span;
			}
			gradient.sort_stops();
		}
	}

	bool changed = false;
	ImGui::PushID(label);

	float avail_w = (width > 0.0f) ? width : ImGui::GetContentRegionAvail().x;
	if (avail_w < 120.0f)
		avail_w = 120.0f;

	const float swatch_w = 26.0f;
	const float spacing = 6.0f;
	const float bar_h = 30.0f;
	const float bar_w = std::max(60.0f, avail_w - swatch_w - spacing);

	ImVec2 bar_p0 = ImGui::GetCursorScreenPos();
	ImVec2 bar_p1(bar_p0.x + bar_w, bar_p0.y + bar_h);

	ImGui::InvisibleButton("grad_bar", ImVec2(bar_w, bar_h));
	bool is_hovered = ImGui::IsItemHovered();
	ImGuiIO& io = ImGui::GetIO();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	if (gradient.selected_stop < 0 || gradient.selected_stop >= static_cast<int>(gradient.stops.size())) {
		gradient.selected_stop = 0;
	}

	static int s_drag_stop_idx = -1;
	static ImGuiID s_active_grad_id = 0;
	ImGuiID current_grad_id = ImGui::GetID("grad_bar");

	const float handle_w = 8.0f;
	const float handle_h = 16.0f;

	auto pos_to_x = [&](float p) -> float {
		float u = std::clamp((p - min_val) / range_span, 0.0f, 1.0f);
		return bar_p0.x + u * bar_w;
	};

	auto x_to_pos = [&](float x) -> float {
		float u = std::clamp((x - bar_p0.x) / bar_w, 0.0f, 1.0f);
		return min_val + u * range_span;
	};

	const float check_sz = 6.0f;
	draw_list->PushClipRect(bar_p0, bar_p1, true);
	for (float y = bar_p0.y; y < bar_p1.y; y += check_sz) {
		for (float x = bar_p0.x; x < bar_p1.x; x += check_sz) {
			int ix = static_cast<int>((x - bar_p0.x) / check_sz);
			int iy = static_cast<int>((y - bar_p0.y) / check_sz);
			ImU32 c = ((ix + iy) & 1) ? IM_COL32(50, 50, 50, 255) : IM_COL32(35, 35, 35, 255);
			draw_list->AddRectFilled(ImVec2(x, y),
									 ImVec2(std::min(x + check_sz, bar_p1.x), std::min(y + check_sz, bar_p1.y)), c);
		}
	}

	const int segs = 64;
	float step_px = bar_w / static_cast<float>(segs);
	for (int i = 0; i < segs; ++i) {
		float xA = bar_p0.x + i * step_px;
		float xB = (i == segs - 1) ? bar_p1.x : (xA + step_px);
		float valA = x_to_pos(xA);
		float valB = x_to_pos(xB);
		ImU32 colA = to_imu32(gradient.sample(valA));
		ImU32 colB = to_imu32(gradient.sample(valB));
		draw_list->AddRectFilledMultiColor(ImVec2(xA, bar_p0.y), ImVec2(xB, bar_p1.y), colA, colB, colB, colA);
	}

	if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		int clicked_stop = -1;
		for (int i = static_cast<int>(gradient.stops.size()) - 1; i >= 0; --i) {
			float px = pos_to_x(gradient.stops[i].pos);
			if (std::abs(io.MousePos.x - px) <= handle_w * 0.75f + 1.0f) {
				clicked_stop = i;
				break;
			}
		}

		if (clicked_stop >= 0) {
			gradient.selected_stop = clicked_stop;
			s_drag_stop_idx = clicked_stop;
			s_active_grad_id = current_grad_id;
		} else {
			float new_pos = x_to_pos(io.MousePos.x);
			std::array<float, 4> new_col = gradient.sample(new_pos);
			int new_idx = gradient.add_stop(new_pos, new_col);
			gradient.selected_stop = new_idx;
			s_drag_stop_idx = new_idx;
			s_active_grad_id = current_grad_id;
			changed = true;
		}
	}

	if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		if (gradient.stops.size() > 1) {
			for (int i = static_cast<int>(gradient.stops.size()) - 1; i >= 0; --i) {
				float px = pos_to_x(gradient.stops[i].pos);
				if (std::abs(io.MousePos.x - px) <= handle_w * 0.75f + 1.0f) {
					gradient.remove_stop(i);
					changed = true;
					break;
				}
			}
		}
	}

	if (s_drag_stop_idx >= 0 && s_active_grad_id == current_grad_id) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			float new_pos = x_to_pos(io.MousePos.x);
			float cur_p = gradient.stops[s_drag_stop_idx].pos;
			if (std::abs(pos_to_x(new_pos) - pos_to_x(cur_p)) > 4.0f) {
				gradient.stops[s_drag_stop_idx].pos = new_pos;
				std::array<float, 4> c = gradient.stops[s_drag_stop_idx].color;
				gradient.sort_stops();
				for (int i = 0; i < static_cast<int>(gradient.stops.size()); ++i) {
					if (gradient.stops[i].pos == new_pos && gradient.stops[i].color == c) {
						s_drag_stop_idx = i;
						gradient.selected_stop = i;
						break;
					}
				}
				changed = true;
			}
		} else {
			s_drag_stop_idx = -1;
			s_active_grad_id = 0;
		}
	}

	for (int i = 0; i < static_cast<int>(gradient.stops.size()); ++i) {
		float px = std::round(pos_to_x(gradient.stops[i].pos));
		bool is_sel = (i == gradient.selected_stop);
		ImVec2 h_min(px - handle_w * 0.5f, bar_p1.y - handle_h);
		ImVec2 h_max(px + handle_w * 0.5f, bar_p1.y - 1.0f);

		draw_list->AddRectFilled(h_min, h_max, to_imu32(gradient.stops[i].color));
		draw_list->AddRect(ImVec2(h_min.x - 1.0f, h_min.y - 1.0f), ImVec2(h_max.x + 1.0f, h_max.y + 1.0f),
						   IM_COL32(0, 0, 0, 255), 1.0f, 0, 1.0f);

		if (is_sel) {
			draw_list->AddRect(h_min, h_max, IM_COL32(255, 50, 40, 255), 1.0f, 0, 2.0f);
			draw_list->AddRect(ImVec2(h_min.x + 1.0f, h_min.y + 1.0f), ImVec2(h_max.x - 1.0f, h_max.y - 1.0f),
							   IM_COL32(0, 0, 0, 180), 0.0f, 0, 1.0f);
		} else {
			draw_list->AddRect(h_min, h_max, IM_COL32(60, 60, 60, 255), 1.0f, 0, 1.0f);
		}
	}

	draw_list->AddRect(bar_p0, bar_p1, IM_COL32(70, 70, 70, 255), 0.0f, 0, 1.0f);
	draw_list->PopClipRect();

	ImGui::SameLine(0.0f, spacing);
	auto& sel_stop = gradient.stops[gradient.selected_stop];
	ImGui::SetNextItemWidth(swatch_w);
	if (ImGui::ColorEdit4("##stop_color", sel_stop.color.data(),
						  ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf |
							  ImGuiColorEditFlags_NoInputs)) {
		changed = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Edit selected stop RGBA color and alpha");
	}

	ImGui::Spacing();
	ImGui::Text("Position:");
	ImGui::SameLine(0.0f, 4.0f);
	float del_btn_w = 26.0f;
	float pos_w = std::max(60.0f, avail_w - 75.0f - del_btn_w);
	ImGui::SetNextItemWidth(pos_w);
	float pos_val = sel_stop.pos;
	float drag_step = range_span * 0.005f;
	const char* fmt = (range_span >= 50.0f) ? "%.0f" : (range_span >= 5.0f ? "%.1f" : "%.2f");
	if (ImGui::DragFloat("##stop_pos", &pos_val, drag_step, min_val, max_val, fmt)) {
		sel_stop.pos = std::clamp(pos_val, min_val, max_val);
		gradient.sort_stops();
		changed = true;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Position of selected stop [%.1f - %.1f]", min_val, max_val);
	}

	ImGui::SameLine(0.0f, 6.0f);
	bool can_delete = gradient.stops.size() > 1;
	if (!can_delete) {
		ImGui::BeginDisabled();
	}
	if (button_with_icon("##del_stop", IconManager::get(IconID::Delete), ImVec2(24.0f, 22.0f))) {
		gradient.remove_stop(gradient.selected_stop);
		changed = true;
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(can_delete ? "Delete selected stop (or right-click stop)" : "Cannot delete last stop");
	}
	if (!can_delete) {
		ImGui::EndDisabled();
	}

	ImGui::PopID();
	return changed;
}