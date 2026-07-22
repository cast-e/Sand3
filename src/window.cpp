#include "window.hpp"

#include "SDL3/SDL_surface.h"
#include "const.hpp"

Window::Window(unsigned int t_width, unsigned int t_height)
	: texture(nullptr), dst_rect{0.0f, 0.0f, static_cast<float>(t_width), static_cast<float>(t_height)} {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fmt::print("SDL_Init failed: {}\n", SDL_GetError());
	}
	window = SDL_CreateWindow("Sand3", t_width, t_height, SDL_WINDOW_RESIZABLE);
	if (window == NULL) {
		fmt::print("SDL_CreateWindow failed: {}\n", SDL_GetError());
	}
	SDL_SetWindowIcon(window, SDL_LoadPNG("../assets/sand3.png"));

	renderer = SDL_CreateRenderer(window, nullptr);
	if (renderer == NULL) {
		fmt::print("SDL_CreateRenderer failed: {}\n", SDL_GetError());
	}

	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, SIM_WIDTH, SIM_HEIGHT);
	if (texture == NULL) {
		fmt::print("SDL_CreateTexture failed: {}\n", SDL_GetError());
	}
	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);

	buffer.resize(static_cast<size_t>(SIM_WIDTH) * SIM_HEIGHT * 4, 0);
}

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

Window::~Window() {
	if (ImGui::GetCurrentContext()) {
		ImGui_ImplSDLRenderer3_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
	if (texture) {
		SDL_DestroyTexture(texture);
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
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
}