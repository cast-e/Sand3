#pragma once

#include <SDL3/SDL.h>
#include <fmt/base.h>

#include <utility>
#include <vector>

class Window {
public:
	Window() = delete;

	static void init(uint32_t t_width = 1280, uint32_t t_height = 720);
	static void shutdown();

	static void present();

	static SDL_Window* get_window();
	static SDL_Renderer* get_renderer();
	static uint32_t* get_buffer();

	static uint64_t get_frame_count();

	static uint32_t get_target_fps();
	static void set_target_fps(uint32_t target);

	static bool get_vsync();
	static void set_vsync(bool enabled);
	static void update_vsync_for_pause(bool paused);

	static std::pair<int, int> get_size();

	static void set_dst_rect(const SDL_FRect& rect);
	static SDL_FRect get_dst_rect();

private:
	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static SDL_Texture* texture;

	static std::vector<uint32_t> buffer;
	static SDL_FRect dst_rect;

	static uint64_t frame_count;
	static uint64_t next_frame_counter;
	static uint32_t target_fps;

	static bool vsync_enabled;
};