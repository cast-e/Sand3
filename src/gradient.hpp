#pragma once

#include <algorithm>
#include <array>
#include <vector>

struct GradientStop {
	float pos = 0.0f;										// Range: [0.0, 1.0]
	std::array<float, 4> color = {1.0f, 1.0f, 1.0f, 1.0f};	// RGBA in [0.0, 1.0]

	bool operator==(const GradientStop& other) const { return pos == other.pos && color == other.color; }
};

struct ColorGradient {
	std::vector<GradientStop> stops = {{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
	int selected_stop = 0;

	void sort_stops() {
		if (stops.empty())
			return;

		GradientStop sel{};
		bool had_sel = (selected_stop >= 0 && selected_stop < static_cast<int>(stops.size()));
		if (had_sel) {
			sel = stops[selected_stop];
		}

		std::stable_sort(stops.begin(), stops.end(),
						 [](const GradientStop& a, const GradientStop& b) { return a.pos < b.pos; });

		if (had_sel) {
			for (size_t i = 0; i < stops.size(); ++i) {
				if (stops[i].pos == sel.pos && stops[i].color == sel.color) {
					selected_stop = static_cast<int>(i);
					return;
				}
			}
		}
		if (selected_stop >= static_cast<int>(stops.size())) {
			selected_stop = static_cast<int>(stops.size() - 1);
		}
	}

	std::array<float, 4> sample(float t) const {
		if (stops.empty()) {
			return {1.0f, 1.0f, 1.0f, 1.0f};
		}
		if (stops.size() == 1 || t <= stops.front().pos) {
			return stops.front().color;
		}
		if (t >= stops.back().pos) {
			return stops.back().color;
		}

		for (size_t i = 0; i < stops.size() - 1; ++i) {
			if (t >= stops[i].pos && t <= stops[i + 1].pos) {
				float span = stops[i + 1].pos - stops[i].pos;
				float factor = (span > 0.00001f) ? ((t - stops[i].pos) / span) : 0.0f;
				factor = std::clamp(factor, 0.0f, 1.0f);
				std::array<float, 4> result;
				for (int c = 0; c < 4; ++c) {
					result[c] = stops[i].color[c] + factor * (stops[i + 1].color[c] - stops[i].color[c]);
				}
				return result;
			}
		}
		return stops.back().color;
	}

	int add_stop(float pos, const std::array<float, 4>& color) {
		GradientStop s;
		s.pos = pos;
		s.color = color;
		stops.push_back(s);
		sort_stops();
		for (size_t i = 0; i < stops.size(); ++i) {
			if (stops[i].pos == s.pos && stops[i].color == s.color) {
				selected_stop = static_cast<int>(i);
				return selected_stop;
			}
		}
		selected_stop = static_cast<int>(stops.size() - 1);
		return selected_stop;
	}

	bool remove_stop(int index) {
		if (stops.size() <= 1)
			return false;
		if (index < 0 || index >= static_cast<int>(stops.size()))
			return false;
		stops.erase(stops.begin() + index);
		if (selected_stop >= static_cast<int>(stops.size())) {
			selected_stop = static_cast<int>(stops.size() - 1);
		}
		return true;
	}

	static ColorGradient create_default(float min_val = 0.0f, float max_val = 1.0f) {
		ColorGradient g;
		g.stops = {
			{min_val, {0.0f, 0.0f, 0.0f, 1.0f}},
			{max_val, {1.0f, 1.0f, 1.0f, 1.0f}}
		};
		g.selected_stop = 0;
		return g;
	}

	static ColorGradient default_heatmap(float min_val = 0.0f, float max_val = 100.0f) {
		float span = max_val - min_val;
		ColorGradient g;
		g.stops = {
			{min_val,                {0.0f, 0.0f, 1.0f, 1.0f}}, // Blue
			{min_val + span * 0.25f, {0.0f, 1.0f, 1.0f, 1.0f}}, // Cyan
			{min_val + span * 0.50f, {0.0f, 1.0f, 0.0f, 1.0f}}, // Green
			{min_val + span * 0.75f, {1.0f, 1.0f, 0.0f, 1.0f}}, // Yellow
			{max_val,                {1.0f, 0.0f, 0.0f, 1.0f}}  // Red
		};
		g.selected_stop = 0;
		return g;
	}
};
