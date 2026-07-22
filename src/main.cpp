#include <SDL3/SDL.h>
#include <fmt/base.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include "grid.hpp"
#include "material.hpp"
#include "ui.hpp"
#include "window.hpp"

int main() {
	Window window(1280, 720);
	Grid grid;

	auto sets = MaterialManager::get_sets();
	std::string start_set = "default";
	bool default_found = false;
	for (const auto& s : sets) {
		if (s == "default") {
			default_found = true;
			break;
		}
	}
	if (!default_found && !sets.empty()) {
		start_set = sets[0];
	}
	MaterialManager::set_current_set(start_set, grid);

	UI ui(window);

	while (true) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				ui.trigger_exit();
			}
		}

		ui.new_frame();
		ui.render(window, grid);

		ui.handle_interaction(window, grid);

		grid.draw(window);

		if (ui.should_update() || ui.should_step()) {
			grid.update();
			ui.reset_step();
		}

		ImGui::Render();
		window.present();
	}

	return 0;
}