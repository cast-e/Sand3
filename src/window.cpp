#include "window.hpp"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "const.hpp"
#include "icon.h"

SDL_Window* Window::window = nullptr;
SDL_Renderer* Window::renderer = nullptr;
SDL_Texture* Window::texture = nullptr;
std::vector<uint32_t> Window::buffer;
SDL_FRect Window::dst_rect{0.0f, 0.0f, 0.0f, 0.0f};
uint64_t Window::frame_count = 0;
uint64_t Window::next_frame_counter = 0;
uint32_t Window::target_fps = 500;

void Window::init(uint32_t t_width, uint32_t t_height) {
	dst_rect = {0.0f, 0.0f, static_cast<float>(t_width), static_cast<float>(t_height)};
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fmt::print("SDL_Init failed: {}\n", SDL_GetError());
	}
	window = SDL_CreateWindow("Sand3", t_width, t_height, SDL_WINDOW_RESIZABLE);
	if (window == NULL) {
		fmt::print("SDL_CreateWindow failed: {}\n", SDL_GetError());
	}
	SDL_IOStream* io_stream = SDL_IOFromMem(sand3_png, sizeof(sand3_png));
	SDL_Surface* surface = SDL_LoadPNG_IO(io_stream, true);
	if (surface == NULL) {
		fmt::print("SDL_LoadPNG_IO failed: {}\n", SDL_GetError());
	}
	SDL_SetWindowIcon(window, surface);
	SDL_DestroySurface(surface);

	renderer = SDL_CreateRenderer(window, nullptr);
	if (renderer == NULL) {
		fmt::print("SDL_CreateRenderer failed: {}\n", SDL_GetError());
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, SIM_WIDTH, SIM_HEIGHT);
	if (texture == NULL) {
		fmt::print("SDL_CreateTexture failed: {}\n", SDL_GetError());
	}
	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);

	buffer.resize(SIM_SIZE, 0);
}

void Window::shutdown() {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
	if (texture) {
		SDL_DestroyTexture(texture);
		texture = nullptr;
	}
	if (renderer) {
		SDL_DestroyRenderer(renderer);
		renderer = nullptr;
	}
	if (window) {
		SDL_DestroyWindow(window);
		window = nullptr;
	}
	SDL_Quit();
}

void Window::present() {
	SDL_UpdateTexture(texture, nullptr, buffer.data(), SIM_WIDTH * 4);
	SDL_RenderClear(renderer);
	SDL_RenderTexture(renderer, texture, nullptr, &dst_rect);

	if (ImGui::GetCurrentContext() && ImGui::GetDrawData()) {
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
	}

	SDL_RenderPresent(renderer);

	if (target_fps < 500 && target_fps > 0) {
		const uint64_t perf_freq = SDL_GetPerformanceFrequency();
		const double target_frame_duration = static_cast<double>(perf_freq) / static_cast<double>(target_fps);
		const uint64_t target_duration_ticks = static_cast<uint64_t>(target_frame_duration);

		uint64_t current = SDL_GetPerformanceCounter();

		if (next_frame_counter == 0 || current > next_frame_counter + perf_freq / 2) {
			next_frame_counter = current;
		}

		next_frame_counter += target_duration_ticks;

		current = SDL_GetPerformanceCounter();
		if (next_frame_counter > current) {
			uint64_t remaining_ticks = next_frame_counter - current;
			double remaining_ms = (static_cast<double>(remaining_ticks) * 1000.0) / static_cast<double>(perf_freq);

			if (remaining_ms > 2.0) {
				SDL_Delay(static_cast<uint32_t>(remaining_ms - 1.5));
			}

			while (SDL_GetPerformanceCounter() < next_frame_counter) {
				SDL_DelayNS(0);
			}
		}
	} else {
		next_frame_counter = 0;
	}
	frame_count++;
}

SDL_Window* Window::get_window() { return window; }
SDL_Renderer* Window::get_renderer() { return renderer; }
uint32_t* Window::get_buffer() { return buffer.data(); }

uint64_t Window::get_frame_count() { return frame_count; }

bool Window::vsync_enabled = false;

uint32_t Window::get_target_fps() { return target_fps; }
void Window::set_target_fps(uint32_t target) { target_fps = target; }

bool Window::get_vsync() { return vsync_enabled; }
void Window::set_vsync(bool enabled) {
	vsync_enabled = enabled;
	bool target = vsync_enabled;
	if (renderer) {
		SDL_SetRenderVSync(renderer, target ? 1 : 0);
	}
}

std::pair<int, int> Window::get_size() {
	int w = 0, h = 0;
	if (window) {
		SDL_GetWindowSize(window, &w, &h);
	}
	return {w, h};
}

void Window::set_dst_rect(const SDL_FRect& rect) { dst_rect = rect; }
SDL_FRect Window::get_dst_rect() { return dst_rect; }