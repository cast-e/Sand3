#include "icon_manager.hpp"

#include <fmt/base.h>

#include <array>
#include <filesystem>

#include "resources/icons_embedded.h"
#include "window.hpp"

namespace {

	struct EmbeddedIconEntry {
		IconID id;
		const char* filename;
		const unsigned char* data;
		unsigned int len;
	};

	const EmbeddedIconEntry ICON_ENTRIES[] = {
		{IconID::Brush, "paintbrush.png", icon_brush_png, icon_brush_png_len},
		{IconID::Select, "shape_handles.png", icon_select_png, icon_select_png_len},
		{IconID::Copy, "page_copy.png", icon_copy_png, icon_copy_png_len},
		{IconID::Cut, "cut.png", icon_cut_png, icon_cut_png_len},
		{IconID::Paste, "page_paste.png", icon_paste_png, icon_paste_png_len},
		{IconID::Delete, "delete.png", icon_delete_png, icon_delete_png_len},
		{IconID::Cross, "cross.png", icon_cross_png, icon_cross_png_len},
		{IconID::Fill, "paintcan.png", icon_fill_png, icon_fill_png_len},
		{IconID::RotateCW, "arrow_rotate_clockwise.png", icon_rotate_cw_png, icon_rotate_cw_png_len},
		{IconID::RotateCCW, "arrow_rotate_anticlockwise.png", icon_rotate_ccw_png, icon_rotate_ccw_png_len},
		{IconID::Save, "disk.png", icon_save_png, icon_save_png_len},
		{IconID::Folder, "folder.png", icon_folder_png, icon_folder_png_len},
		{IconID::Pause, "control_pause_blue.png", icon_pause_png, icon_pause_png_len},
		{IconID::Play, "control_play_blue.png", icon_play_png, icon_play_png_len},
		{IconID::Step, "control_fastforward_blue.png", icon_step_png, icon_step_png_len},
		{IconID::Clear, "bin.png", icon_clear_png, icon_clear_png_len},
		{IconID::Add, "add.png", icon_add_png, icon_add_png_len},
		{IconID::Refresh, "arrow_refresh.png", icon_refresh_png, icon_refresh_png_len},
		{IconID::World, "world.png", icon_world_png, icon_world_png_len},
		{IconID::Heart, "heart.png", icon_heart_png, icon_heart_png_len},
		{IconID::Star, "star.png", icon_star_png, icon_star_png_len},
		{IconID::Report, "flag_red.png", icon_flag_red_png, icon_flag_red_png_len},
		{IconID::Package, "package.png", icon_package_png, icon_package_png_len},
		{IconID::Transmit, "transmit.png", icon_transmit_png, icon_transmit_png_len},
		{IconID::Magnifier, "magnifier.png", icon_magnifier_png, icon_magnifier_png_len},
		{IconID::ArrowDown, "arrow_down.png", icon_arrow_down_png, icon_arrow_down_png_len},
		{IconID::User, "user.png", icon_user_png, icon_user_png_len},
		{IconID::Tag, "tag_blue.png", icon_tag_blue_png, icon_tag_blue_png_len},
		{IconID::Edit, "pencil.png", icon_edit_png, icon_edit_png_len},
		{IconID::Update, "arrow_up.png", icon_update_png, icon_update_png_len},
		{IconID::Accept, "accept.png", icon_accept_png, icon_accept_png_len},
		{IconID::Information, "information.png", icon_information_png, icon_information_png_len},
		{IconID::Warning, "error.png", icon_error_png, icon_error_png_len},
	};

	std::array<SDL_Texture*, static_cast<size_t>(IconID::Count)> textures{};

}  // namespace

void IconManager::init() {
	shutdown();

	SDL_Renderer* renderer = Window::get_renderer();
	if (!renderer) {
		fmt::print("IconManager::init: Window renderer is null\n");
		return;
	}

	for (const auto& entry : ICON_ENTRIES) {
		SDL_IOStream* io = nullptr;

		const char* search_paths[] = {"src/resources/icons/", "../../src/resources/icons/", "../src/resources/icons/",
									  "src/resources/Icons/", "../../src/resources/Icons/", "../src/resources/Icons/"};

		for (const char* prefix : search_paths) {
			std::string p = std::string(prefix) + entry.filename;
			if (std::filesystem::exists(p)) {
				io = SDL_IOFromFile(p.c_str(), "rb");
				if (io)
					break;
			}
		}

		if (!io) {
			io = SDL_IOFromConstMem(entry.data, entry.len);
		}

		if (!io) {
			fmt::print("IconManager: Failed to open IO for {}\n", entry.filename);
			continue;
		}

		SDL_Surface* surface = SDL_LoadPNG_IO(io, true);
		if (!surface) {
			fmt::print("IconManager: Failed to decode PNG for {}: {}\n", entry.filename, SDL_GetError());
			continue;
		}

		SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surface);
		SDL_DestroySurface(surface);

		if (!tex) {
			fmt::print("IconManager: Failed to create texture for {}: {}\n", entry.filename, SDL_GetError());
			continue;
		}

		SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
		textures[static_cast<size_t>(entry.id)] = tex;
	}
}

void IconManager::shutdown() {
	for (auto& tex : textures) {
		if (tex) {
			SDL_DestroyTexture(tex);
			tex = nullptr;
		}
	}
}

SDL_Texture* IconManager::get(IconID id) {
	size_t idx = static_cast<size_t>(id);
	if (idx < textures.size()) {
		return textures[idx];
	}
	return nullptr;
}
