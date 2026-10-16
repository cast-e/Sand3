#include "ui_advanced.hpp"

#include <imgui.h>

#include <algorithm>

#include "config_manager.hpp"
#include "grid.hpp"
#include "icon_manager.hpp"
#include "ui.hpp"
#include "ui_modals.hpp"
#include "ui_widgets.hpp"
#include "vulkan.hpp"
#include "window.hpp"

void UIAdvanced::render() {
	if (ImGui::BeginTabItem("Advanced")) {
		ImGui::Spacing();
		ImGui::Text("Advanced Options");
		ImGui::Separator();
		ImGui::Spacing();

		bool vsync_enabled = Window::get_vsync();
		if (UIWidgets::custom_checkbox("VSync", &vsync_enabled, IconID::Monitor,
									   "Synchronizes the frame rate with the monitor's refresh rate.")) {
			Window::set_vsync(vsync_enabled);
			ConfigManager::save();
		}

		if (vsync_enabled) {
			ImGui::BeginDisabled();
		}
		int target_fps = Window::get_target_fps();
		const char* fps_format = (target_fps >= 500) ? "Unlimited" : "%d";
		if (ImGui::SliderInt("Target FPS", &target_fps, 10, 500, fps_format)) {
			Window::set_target_fps(target_fps);
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Sets the target frame rate (10 - 500, 500 = Unlimited).\nCtrl+Click to type exact value.");
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
			int max_threads = std::max(1, static_cast<int>(Grid::get_num_strips_y() / 2));
			if (ImGui::InputInt("Active Threads", &thread_count, 1, 2)) {
				thread_count = std::clamp(thread_count, 1, max_threads);
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
			if (UIWidgets::custom_checkbox(
					"Prevent Downclocking", &prevent_downclock, IconID::Lightning,
					"Stops the GPU from slowing down to save power when idle. Makes unpausing instant "
					"after a few seconds of being paused.")) {
				Vulkan::set_prevent_downclock(prevent_downclock);
				ConfigManager::save();
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

		UIWidgets::custom_checkbox("Preserve Cells", &resize_preserve_content, IconID::Photo,
								   "Keeps existing cells when changing canvas dimensions.");

		bool size_differs =
			(target_sim_w != static_cast<int>(current_w) || target_sim_h != static_cast<int>(current_h));
		if (!size_differs)
			ImGui::BeginDisabled();
		if (ImGui::Button("Apply Simulation Size", ImVec2(-1, 30))) {
			UIModals::open_canvas_resize(target_sim_w, target_sim_h, resize_preserve_content);
		}
		if (!size_differs)
			ImGui::EndDisabled();

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::Text("Rule Neighborhoods");
		bool allow_large = ConfigManager::get_config().allow_large_neighborhoods;
		if (UIWidgets::custom_checkbox(
				"Large Neighborhoods", &allow_large, IconID::Zoom,
				"Allows rule neighborhoods to exceed 100 cells.\nWarning: Very large neighborhoods can "
				"significantly impact simulation performance.")) {
			ConfigManager::get_config().allow_large_neighborhoods = allow_large;
			ConfigManager::save();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Allows rule neighborhoods to exceed 100 cells.\n"
							  "Warning: Very large neighborhoods can significantly impact simulation performance.");
		}

		ImGui::EndTabItem();
	}
}