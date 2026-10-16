#include "ui_thumbnails.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>

#include "config_manager.hpp"
#include "const.hpp"
#include "grid.hpp"
#include "material_manager.hpp"
#include "save_manager.hpp"
#include "set_manager.hpp"
#include "ui.hpp"

struct SimpleMaterialInfo {
	std::string name;
	uint8_t id = 0;
	std::array<uint8_t, 3> color = {255, 255, 255};
};

static std::vector<SimpleMaterialInfo> get_materials_for_set(const std::string& set_name) {
	std::vector<SimpleMaterialInfo> result;
	if (set_name.empty() || set_name == SetManager::get_current_set()) {
		for (const auto& m : MaterialManager::get_materials()) {
			result.push_back({m.name, m.id, m.color});
		}
		return result;
	}

	std::string set_dir = std::string(SETS_DIRECTORY) + set_name;
	if (!std::filesystem::exists(set_dir) || !std::filesystem::is_directory(set_dir)) {
		return result;
	}

	SimpleMaterialInfo empty_mat{"empty", 0, {64, 64, 64}};
	std::vector<SimpleMaterialInfo> loaded;

	for (const auto& entry : std::filesystem::directory_iterator(set_dir)) {
		if (entry.is_regular_file() && entry.path().extension().string() == ".mat") {
			std::ifstream file(entry.path());
			if (!file.is_open())
				continue;
			try {
				nlohmann::json j = nlohmann::json::parse(file);
				SimpleMaterialInfo info;
				info.name = j.value("name", entry.path().stem().string());
				info.id = j.value("id", static_cast<uint8_t>(0));
				if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
					info.color[0] = j["color"][0].get<uint8_t>();
					info.color[1] = j["color"][1].get<uint8_t>();
					info.color[2] = j["color"][2].get<uint8_t>();
				}
				if (info.id == 0) {
					empty_mat = info;
				} else {
					loaded.push_back(info);
				}
			} catch (...) {}
		}
	}

	std::sort(loaded.begin(), loaded.end(),
			  [](const SimpleMaterialInfo& a, const SimpleMaterialInfo& b) { return a.id < b.id; });

	result.push_back(empty_mat);
	result.insert(result.end(), loaded.begin(), loaded.end());
	return result;
}

std::string UIThumbnails::generate_thumbnail_base64(int type_idx, const std::string& target_set,
													const std::string& target_file) {
	SDL_Surface* surf = SDL_CreateSurface(128, 128, SDL_PIXELFORMAT_RGBA32);
	if (!surf)
		return "";

	uint32_t* pixels = static_cast<uint32_t*>(surf->pixels);
	uint32_t bg_col = SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, 13, 17, 23, 255);
	for (int i = 0; i < 128 * 128; ++i) {
		pixels[i] = bg_col;
	}

	auto mats = get_materials_for_set(target_set);
	std::array<std::array<uint8_t, 3>, 256> id_to_color{};
	for (int i = 0; i < 256; ++i) {
		id_to_color[i] = {64, 64, 64};
	}
	for (const auto& m : mats) {
		id_to_color[m.id] = m.color;
	}

	if (type_idx == 0) {
		std::vector<uint8_t> grid_cells;
		uint32_t gw = 0, gh = 0;

		if (!target_file.empty()) {
			std::string fname = target_file;
			if (fname.length() < 5 || fname.substr(fname.length() - 5) != ".save") {
				fname += ".save";
			}
			std::string fpath = SaveManager::get_saves_directory(target_set) + fname;
			std::ifstream fin(std::filesystem::path(fpath), std::ios::binary);
			if (fin.is_open()) {
				std::string set_str;
				std::getline(fin, set_str);
				fin.read(reinterpret_cast<char*>(&gw), sizeof(gw));
				fin.read(reinterpret_cast<char*>(&gh), sizeof(gh));
				uint32_t num_blocks = 0;
				fin.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
				if (gw > 0 && gh > 0 && gw <= 4096 && gh <= 4096) {
					grid_cells.resize(gw * gh);
					if (!SaveManager::read_chunked_data(fin, num_blocks, grid_cells.size(), grid_cells.data())) {
						grid_cells.clear();
					}
				}
			}
		}

		if (grid_cells.empty() && target_set == SetManager::get_current_set()) {
			gw = Grid::get_width();
			gh = Grid::get_height();
			if (gw > 0 && gh > 0) {
				grid_cells.resize(gw * gh);
				for (uint32_t y = 0; y < gh; ++y) {
					for (uint32_t x = 0; x < gw; ++x) {
						grid_cells[y * gw + x] = Grid::get_cell(x, y);
					}
				}
			}
		}

		if (!grid_cells.empty() && gw > 0 && gh > 0) {
			for (int ty = 0; ty < 128; ++ty) {
				int gy = (ty * gh) / 128;
				for (int tx = 0; tx < 128; ++tx) {
					int gx = (tx * gw) / 128;
					uint8_t mid = grid_cells[gy * gw + gx];
					if (mid != 0) {
						pixels[ty * 128 + tx] =
							SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, id_to_color[mid][0],
										id_to_color[mid][1], id_to_color[mid][2], 255);
					}
				}
			}
		}
	} else if (type_idx == 1) {
		std::vector<SimpleMaterialInfo> valid_mats;
		for (const auto& m : mats) {
			if (m.id != 0) {
				valid_mats.push_back(m);
			}
		}
		int count = static_cast<int>(valid_mats.size());
		if (count > 0) {
			int cols = std::clamp(static_cast<int>(std::ceil(std::sqrt(count))), 2, 8);
			int rows = (count + cols - 1) / cols;
			int cell_w = 128 / cols;
			int cell_h = 128 / rows;
			for (int idx = 0; idx < count; ++idx) {
				const auto& mat = valid_mats[idx];
				uint32_t c = SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, mat.color[0], mat.color[1],
										 mat.color[2], 255);
				int cx = (idx % cols) * cell_w;
				int cy = (idx / cols) * cell_h;
				for (int y = cy + 2; y < cy + cell_h - 2 && y < 128; ++y) {
					for (int x = cx + 2; x < cx + cell_w - 2 && x < 128; ++x) {
						pixels[y * 128 + x] = c;
					}
				}
			}
		}
	} else if (type_idx == 2) {
		std::vector<Cell> stamp_cells;
		uint32_t sw = 0, sh = 0;
		if (!target_file.empty()) {
			SaveManager::load_stamp_from_file(target_file, target_set, stamp_cells, sw, sh);
		}
		if (stamp_cells.empty()) {
			const auto& clip = UI::get_clipboard();
			if (!clip.empty() && clip.width > 0 && clip.height > 0) {
				stamp_cells = clip.cells;
				sw = clip.width;
				sh = clip.height;
			}
		}

		if (!stamp_cells.empty() && sw > 0 && sh > 0) {
			for (int ty = 0; ty < 128; ++ty) {
				int sy = (ty * sh) / 128;
				for (int tx = 0; tx < 128; ++tx) {
					int sx = (tx * sw) / 128;
					uint8_t mid = stamp_cells[sy * sw + sx].material;
					if (mid != 0) {
						pixels[ty * 128 + tx] =
							SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr, id_to_color[mid][0],
										id_to_color[mid][1], id_to_color[mid][2], 255);
					}
				}
			}
		}
	} else if (type_idx == 3) {
		const auto& style = ImGui::GetStyle();
		const auto& cfg = ConfigManager::get_config();
		ImVec4 bg_col = cfg.ui.background_color;
		ImVec4 win_col = style.Colors[ImGuiCol_WindowBg];
		ImVec4 btn_col = style.Colors[ImGuiCol_Button];
		ImVec4 act_col = style.Colors[ImGuiCol_ButtonActive];
		ImVec4 text_col = style.Colors[ImGuiCol_Text];

		for (int ty = 0; ty < 128; ++ty) {
			for (int tx = 0; tx < 128; ++tx) {
				ImVec4 col = (ty < 54)	 ? win_col
							 : (ty < 64) ? bg_col
							 : (tx < 42) ? btn_col
							 : (tx < 85) ? act_col
										 : text_col;
				pixels[ty * 128 + tx] = SDL_MapRGBA(SDL_GetPixelFormatDetails(surf->format), nullptr,
													static_cast<uint8_t>(std::clamp(col.x, 0.0f, 1.0f) * 255.0f),
													static_cast<uint8_t>(std::clamp(col.y, 0.0f, 1.0f) * 255.0f),
													static_cast<uint8_t>(std::clamp(col.z, 0.0f, 1.0f) * 255.0f), 255);
			}
		}
	}

	std::string b64;
	SDL_IOStream* io = SDL_IOFromDynamicMem();
	if (io) {
		if (SDL_SavePNG_IO(surf, io, false)) {
			Sint64 size = SDL_TellIO(io);
			SDL_PropertiesID props = SDL_GetIOProperties(io);
			const uint8_t* mem = static_cast<const uint8_t*>(
				SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr));
			if (mem && size > 0) {
				static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				std::string enc;
				enc.reserve(((size + 2) / 3) * 4);
				for (Sint64 i = 0; i < size; i += 3) {
					uint32_t val = (static_cast<uint32_t>(mem[i]) << 16);
					if (i + 1 < size)
						val |= (static_cast<uint32_t>(mem[i + 1]) << 8);
					if (i + 2 < size)
						val |= static_cast<uint32_t>(mem[i + 2]);
					enc.push_back(b64_chars[(val >> 18) & 0x3F]);
					enc.push_back(b64_chars[(val >> 12) & 0x3F]);
					enc.push_back((i + 1 < size) ? b64_chars[(val >> 6) & 0x3F] : '=');
					enc.push_back((i + 2 < size) ? b64_chars[val & 0x3F] : '=');
				}
				b64 = "data:image/png;base64," + enc;
			}
		}
		SDL_CloseIO(io);
	} else {
		std::error_code tec;
		std::filesystem::path temp_dir = std::filesystem::temp_directory_path(tec);
		if (!tec) {
			std::filesystem::path temp_png =
				temp_dir /
				("sand3_thumb_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".png");
			if (SDL_SavePNG(surf, temp_png.string().c_str())) {
				std::ifstream fin(temp_png, std::ios::binary);
				if (fin.is_open()) {
					std::vector<uint8_t> png_bytes((std::istreambuf_iterator<char>(fin)),
												   std::istreambuf_iterator<char>());
					fin.close();
					std::filesystem::remove(temp_png, tec);
					if (!png_bytes.empty()) {
						static const char b64_chars[] =
							"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
						std::string enc;
						enc.reserve(((png_bytes.size() + 2) / 3) * 4);
						for (size_t i = 0; i < png_bytes.size(); i += 3) {
							uint32_t val = (static_cast<uint32_t>(png_bytes[i]) << 16);
							if (i + 1 < png_bytes.size())
								val |= (static_cast<uint32_t>(png_bytes[i + 1]) << 8);
							if (i + 2 < png_bytes.size())
								val |= static_cast<uint32_t>(png_bytes[i + 2]);
							enc.push_back(b64_chars[(val >> 18) & 0x3F]);
							enc.push_back(b64_chars[(val >> 12) & 0x3F]);
							enc.push_back((i + 1 < png_bytes.size()) ? b64_chars[(val >> 6) & 0x3F] : '=');
							enc.push_back((i + 2 < png_bytes.size()) ? b64_chars[val & 0x3F] : '=');
						}
						b64 = "data:image/png;base64," + enc;
					}
				}
			}
		}
	}
	SDL_DestroySurface(surf);
	return b64;
}