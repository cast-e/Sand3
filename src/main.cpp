#include <SDL3/SDL.h>
#include <fmt/base.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <cstdlib>
#include <filesystem>

#include "config_manager.hpp"
#include "grid.hpp"
#include "ipc.hpp"
#include "set_manager.hpp"
#include "ui.hpp"
#include "window.hpp"

static void ensure_working_directory(const char* argv0) {
	if (!std::filesystem::exists("sets")) {
		try {
			std::filesystem::path exe_path = std::filesystem::canonical(argv0);
			std::filesystem::path dir = exe_path.parent_path();
			while (!dir.empty() && dir != dir.root_path()) {
				if (std::filesystem::exists(dir / "sets")) {
					std::filesystem::current_path(dir);
					break;
				}
				dir = dir.parent_path();
			}
		} catch (...) {}
	}
}

int main(int argc, char* argv[]) {
	if (argc > 0 && argv[0]) {
		ensure_working_directory(argv[0]);
		ProtocolHandler::ensure_registered(argv[0]);
	}

	std::string uri_arg;
	for (int i = 1; i < argc; ++i) {
		if (argv[i] && argv[i][0] != '-') {
			uri_arg = argv[i];
			break;
		}
	}

	if (IPC::send_to_existing_instance(uri_arg)) {
		return 0;
	}

	IPC::start_server();
	std::atexit(IPC::cleanup);

	Window::init(1600, 900);
	Grid::init();
	ConfigManager::load();
	UI::init();

	SetManager::set_current_set(SetManager::get_sets()[0]);

	if (!uri_arg.empty()) {
		UI::handle_uri(uri_arg);
	}

	while (true) {
		IPC::poll([](const std::string& uri) { UI::handle_uri(uri); });

		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT) {
				IPC::cleanup();
				UI::trigger_exit();
			}
		}

		UI::render();

		UI::handle_interaction();

		bool running = (UI::should_update() || UI::should_step());
		bool desired_vsync = Window::get_vsync();
		static int active_vsync = -1;
		if (active_vsync != static_cast<int>(desired_vsync)) {
			active_vsync = static_cast<int>(desired_vsync);
			SDL_SetRenderVSync(Window::get_renderer(), desired_vsync ? 1 : 0);
		}

		if (running) {
			Grid::update();
			UI::reset_step();
		} else {
			Grid::keep_awake_gpu();
		}

		Grid::draw();

		ImGui::Render();
		Window::present();
	}
}