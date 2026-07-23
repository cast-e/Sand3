#pragma once

#include <SDL3/SDL.h>
#include <fmt/base.h>

#include <utility>
#include <vector>

class Window {
public:
	Window() = delete;

	static void init(unsigned int t_width = 1280, unsigned int t_height = 720);
	static void shutdown();

	static void present();

	static SDL_Window* get_window();
	static SDL_Renderer* get_renderer();
	static uint32_t* get_buffer();

	static std::pair<int, int> get_size();

	static void set_dst_rect(const SDL_FRect& rect);
	static SDL_FRect get_dst_rect();

private:
	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static SDL_Texture* texture;

	static std::vector<uint32_t> buffer;
	static SDL_FRect dst_rect;
};