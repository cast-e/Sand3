#include <SDL3/SDL.h>
#include <fmt/base.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include "config_manager.hpp"
#include "grid.hpp"
#include "set_manager.hpp"
#include "ui.hpp"
#include "window.hpp"

int main() {
	Window::init(1600, 900);
	Grid::init();
	UI::init();
	ConfigManager::load();

	SetManager::set_current_set(SetManager::get_sets()[0]);

	while (true) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				UI::trigger_exit();
			}
		}

		UI::render();

		UI::handle_interaction();

		Grid::draw();

		if (UI::should_update() || UI::should_step()) {
			Grid::update();
			UI::reset_step();
			SDL_SetRenderVSync(Window::get_renderer(), Window::get_vsync());
		} else {
			SDL_SetRenderVSync(Window::get_renderer(), true);
		}

		ImGui::Render();
		Window::present();
	}
}