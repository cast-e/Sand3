#pragma once

#include <SDL3/SDL.h>
#include <fmt/base.h>

#include <vector>

class Window {
public:
	Window(unsigned int t_width, unsigned int t_height);
	~Window();

	void present();

	SDL_Window* get_window() const { return window; }
	SDL_Renderer* get_renderer() const { return renderer; }
	uint32_t* get_buffer() { return buffer.data(); }

	std::pair<int, int> get_size() const {
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		return {w, h};
	}

	void set_dst_rect(const SDL_FRect& rect) { dst_rect = rect; }
	SDL_FRect get_dst_rect() const { return dst_rect; }

private:
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;

	std::vector<uint32_t> buffer;
	SDL_FRect dst_rect;
};