#include "save_manager.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "const.hpp"
#include "grid.hpp"
#include "set_manager.hpp"
#include "undo_manager.hpp"

void SaveManager::bwt_encode(const uint8_t* in_data, size_t N, std::vector<uint8_t>& out_L, uint16_t& out_primary_id) {
	if (N == 0) {
		out_L.clear();
		out_primary_id = 0;
		return;
	}

	std::vector<uint16_t> indices(N);
	for (uint16_t i = 0; i < N; ++i) {
		indices[i] = i;
	}

	std::stable_sort(indices.begin(), indices.end(), [in_data, N](uint16_t a, uint16_t b) {
		if (a == b)
			return false;
		for (size_t k = 0; k < N; ++k) {
			uint8_t ca = in_data[(a + k) % N];
			uint8_t cb = in_data[(b + k) % N];
			if (ca != cb)
				return ca < cb;
		}
		return a < b;
	});

	out_L.resize(N);
	out_primary_id = 0;
	for (size_t i = 0; i < N; ++i) {
		if (indices[i] == 0) {
			out_primary_id = static_cast<uint16_t>(i);
		}
		out_L[i] = in_data[(indices[i] + N - 1) % N];
	}
}

void SaveManager::bwt_decode(const uint8_t* L, size_t N, uint16_t primary_id, uint8_t* out_data) {
	if (N == 0 || primary_id >= N) {
		return;
	}

	std::array<size_t, 256> count{};
	for (size_t i = 0; i < N; ++i) {
		count[L[i]]++;
	}

	std::array<size_t, 256> C{};
	size_t sum = 0;
	for (int c = 0; c < 256; ++c) {
		C[c] = sum;
		sum += count[c];
	}

	std::vector<size_t> T(N);
	std::array<size_t, 256> current_count = C;
	for (size_t i = 0; i < N; ++i) {
		uint8_t val = L[i];
		T[i] = current_count[val]++;
	}

	size_t curr = primary_id;
	for (int i = static_cast<int>(N) - 1; i >= 0; --i) {
		out_data[i] = L[curr];
		curr = T[curr];
	}
}

std::vector<uint8_t> SaveManager::rle_encode(const uint8_t* data, size_t size) {
	std::vector<uint8_t> compressed;
	compressed.reserve(size);

	size_t i = 0;
	while (i < size) {
		uint8_t val = data[i];
		size_t run = 1;
		while (i + run < size && data[i + run] == val && run < 255) {
			run++;
		}
		compressed.push_back(static_cast<uint8_t>(run));
		compressed.push_back(val);
		i += run;
	}
	return compressed;
}

std::vector<uint8_t> SaveManager::rle_decode(const uint8_t* data, size_t compressed_size, size_t expected_size) {
	std::vector<uint8_t> decompressed;
	decompressed.reserve(expected_size);

	size_t pos = 0;
	while (pos + 1 < compressed_size && decompressed.size() < expected_size) {
		uint8_t count = data[pos++];
		uint8_t val = data[pos++];
		decompressed.insert(decompressed.end(), count, val);
	}
	return decompressed;
}

bool SaveManager::write_chunked_data(std::ofstream& file, const uint8_t* data, size_t total_cells) {
	constexpr size_t BLOCK_SIZE_BWT = 4096;
	uint32_t num_blocks = static_cast<uint32_t>((total_cells + BLOCK_SIZE_BWT - 1) / BLOCK_SIZE_BWT);
	file.write(reinterpret_cast<const char*>(&num_blocks), sizeof(num_blocks));

	for (uint32_t b = 0; b < num_blocks; ++b) {
		size_t block_start = b * BLOCK_SIZE_BWT;
		size_t block_len = std::min(BLOCK_SIZE_BWT, total_cells - block_start);

		std::vector<uint8_t> L;
		uint16_t primary_id = 0;
		bwt_encode(&data[block_start], block_len, L, primary_id);

		std::vector<uint8_t> rle_data = rle_encode(L.data(), L.size());
		uint32_t compressed_size = static_cast<uint32_t>(rle_data.size());

		file.write(reinterpret_cast<const char*>(&primary_id), sizeof(primary_id));
		file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
		file.write(reinterpret_cast<const char*>(rle_data.data()), rle_data.size());
	}
	return true;
}

bool SaveManager::read_chunked_data(std::ifstream& file, uint32_t num_blocks, size_t total_cells, uint8_t* out_data) {
	constexpr size_t BLOCK_SIZE_BWT = 4096;
	std::vector<uint8_t> block_raw(BLOCK_SIZE_BWT);

	for (uint32_t b = 0; b < num_blocks; ++b) {
		size_t block_start = b * BLOCK_SIZE_BWT;
		size_t block_len = std::min(BLOCK_SIZE_BWT, total_cells - block_start);

		uint16_t primary_id = 0;
		uint32_t compressed_size = 0;
		file.read(reinterpret_cast<char*>(&primary_id), sizeof(primary_id));
		file.read(reinterpret_cast<char*>(&compressed_size), sizeof(compressed_size));
		if (!file)
			return false;

		std::vector<uint8_t> compressed_data(compressed_size);
		file.read(reinterpret_cast<char*>(compressed_data.data()), compressed_size);
		if (!file)
			return false;

		std::vector<uint8_t> L = rle_decode(compressed_data.data(), compressed_size, block_len);
		bwt_decode(L.data(), block_len, primary_id, block_raw.data());

		std::memcpy(out_data + block_start, block_raw.data(), block_len);
	}
	return true;
}

std::string SaveManager::get_saves_directory(const std::string& current_set) {
	std::string dir = SETS_DIRECTORY + current_set + "/saves/";
	std::filesystem::create_directories(dir);
	return dir;
}

bool SaveManager::save_to_file(const std::string& name, const std::string& current_set) {
	std::string saves_dir = get_saves_directory(current_set);
	std::string filename = name;
	if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".save") {
		filename += ".save";
	}
	std::string filepath = saves_dir + filename;

	std::ofstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	file << current_set << "\n";

	uint32_t width = Grid::get_width();
	uint32_t height = Grid::get_height();
	file.write(reinterpret_cast<const char*>(&width), sizeof(width));
	file.write(reinterpret_cast<const char*>(&height), sizeof(height));

	std::vector<uint8_t> grid_bytes(Grid::get_size());
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			grid_bytes[y * width + x] = Grid::get_cell(x, y);
		}
	}

	return write_chunked_data(file, grid_bytes.data(), grid_bytes.size());
}

bool SaveManager::load_from_file(const std::string& path_or_name, const std::string& current_set,
								 std::string& loaded_set, LoadPlacement placement) {
	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + path_or_name;
	}
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}

	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::string set_line;
	if (!std::getline(file, set_line))
		return false;
	while (!set_line.empty() && (set_line.back() == '\r' || set_line.back() == '\n' || set_line.back() == ' ')) {
		set_line.pop_back();
	}
	loaded_set = set_line;
	if (loaded_set != current_set) {
		SetManager::set_current_set(loaded_set);
	}

	uint32_t saved_width = 0, saved_height = 0, num_blocks = 0;
	file.read(reinterpret_cast<char*>(&saved_width), sizeof(saved_width));
	file.read(reinterpret_cast<char*>(&saved_height), sizeof(saved_height));
	file.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
	if (!file || saved_width == 0 || saved_height == 0) {
		return false;
	}

	size_t saved_total = static_cast<size_t>(saved_width) * saved_height;
	std::vector<uint8_t> saved_cells(saved_total);
	if (!read_chunked_data(file, num_blocks, saved_total, saved_cells.data())) {
		return false;
	}

	if (placement == LoadPlacement::ResizeGrid &&
		(saved_width != Grid::get_width() || saved_height != Grid::get_height())) {
		Grid::resize(saved_width, saved_height, false);
	}

	Grid::clear();

	if (saved_width == Grid::get_width() && saved_height == Grid::get_height()) {
		for (uint32_t y = 0; y < saved_height; ++y) {
			for (uint32_t x = 0; x < saved_width; ++x) {
				Grid::set_cell(x, y, saved_cells[y * saved_width + x]);
			}
		}
	} else if (placement == LoadPlacement::TopLeft) {
		uint32_t copy_w = std::min(saved_width, Grid::get_width());
		uint32_t copy_h = std::min(saved_height, Grid::get_height());
		for (uint32_t y = 0; y < copy_h; ++y) {
			for (uint32_t x = 0; x < copy_w; ++x) {
				Grid::set_cell(x, y, saved_cells[y * saved_width + x]);
			}
		}
	} else {  // LoadPlacement::Center
		int offset_x = (static_cast<int>(Grid::get_width()) - static_cast<int>(saved_width)) / 2;
		int offset_y = (static_cast<int>(Grid::get_height()) - static_cast<int>(saved_height)) / 2;
		for (uint32_t sy = 0; sy < saved_height; ++sy) {
			int gy = offset_y + static_cast<int>(sy);
			if (gy < 0 || gy >= static_cast<int>(Grid::get_height()))
				continue;
			for (uint32_t sx = 0; sx < saved_width; ++sx) {
				int gx = offset_x + static_cast<int>(sx);
				if (gx < 0 || gx >= static_cast<int>(Grid::get_width()))
					continue;
				Grid::set_cell(static_cast<uint32_t>(gx), static_cast<uint32_t>(gy),
							   saved_cells[sy * saved_width + sx]);
			}
		}
	}

	if (Grid::get_processing_mode() == ProcessingMode::GPU) {
		Grid::sync_to_gpu();
	}
	UndoManager::init();
	return true;
}

bool SaveManager::inspect_save_file(const std::string& path_or_name, const std::string& current_set,
									SaveFileInfo& info) {
	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + path_or_name;
	}
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}

	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::string set_line;
	if (!std::getline(file, set_line))
		return false;
	while (!set_line.empty() && (set_line.back() == '\r' || set_line.back() == '\n' || set_line.back() == ' ')) {
		set_line.pop_back();
	}

	uint32_t width = 0, height = 0;
	file.read(reinterpret_cast<char*>(&width), sizeof(width));
	file.read(reinterpret_cast<char*>(&height), sizeof(height));
	if (!file)
		return false;

	info.filename = std::filesystem::path(filepath).filename().string();
	info.name = std::filesystem::path(filepath).stem().string();
	info.set_name = set_line;
	info.width = width;
	info.height = height;
	info.dimensions_differ = (width != Grid::get_width() || height != Grid::get_height());
	try {
		info.file_size = std::filesystem::file_size(filepath);
	} catch (...) {
		info.file_size = 0;
	}
	return true;
}

std::vector<SaveFileInfo> SaveManager::get_save_files(const std::string& current_set) {
	std::vector<SaveFileInfo> list;
	std::string dir = get_saves_directory(current_set);
	if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".save") {
				SaveFileInfo info;
				if (inspect_save_file(entry.path().string(), current_set, info)) {
					list.push_back(info);
				}
			}
		}
	}
	std::sort(list.begin(), list.end(), [](const SaveFileInfo& a, const SaveFileInfo& b) {
		return a.filename < b.filename;
	});
	return list;
}

bool SaveManager::delete_save_file(const std::string& filename, const std::string& current_set) {
	std::string filepath = get_saves_directory(current_set) + filename;
	try {
		return std::filesystem::remove(filepath);
	} catch (...) {
		return false;
	}
}

bool SaveManager::duplicate_save_file(const std::string& filename, const std::string& new_name,
									  const std::string& current_set) {
	std::string dir = get_saves_directory(current_set);
	std::string old_path = dir + filename;
	std::string new_filename = new_name;
	if (new_filename.length() < 5 || new_filename.substr(new_filename.length() - 5) != ".save") {
		new_filename += ".save";
	}
	std::string new_path = dir + new_filename;
	try {
		return std::filesystem::copy_file(old_path, new_path, std::filesystem::copy_options::overwrite_existing);
	} catch (...) {
		return false;
	}
}

bool SaveManager::save_stamp_to_file(const std::string& name, const std::string& current_set,
									 const std::vector<uint8_t>& cells, uint32_t width, uint32_t height) {
	if (cells.empty() || width == 0 || height == 0 || cells.size() != static_cast<size_t>(width) * height) {
		return false;
	}
	std::string saves_dir = get_saves_directory(current_set);
	std::string filename = name;
	if (filename.length() < 6 || filename.substr(filename.length() - 6) != ".stamp") {
		filename += ".stamp";
	}
	std::string filepath = saves_dir + filename;

	std::ofstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	file << current_set << "\n";
	file.write(reinterpret_cast<const char*>(&width), sizeof(width));
	file.write(reinterpret_cast<const char*>(&height), sizeof(height));

	return write_chunked_data(file, cells.data(), cells.size());
}

bool SaveManager::load_stamp_from_file(const std::string& path_or_name, const std::string& current_set,
									   std::vector<uint8_t>& out_cells, uint32_t& out_width, uint32_t& out_height) {
	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + path_or_name;
	}
	if (filepath.length() < 6 || filepath.substr(filepath.length() - 6) != ".stamp") {
		filepath += ".stamp";
	}

	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::string set_line;
	if (!std::getline(file, set_line))
		return false;

	uint32_t width = 0, height = 0, num_blocks = 0;
	file.read(reinterpret_cast<char*>(&width), sizeof(width));
	file.read(reinterpret_cast<char*>(&height), sizeof(height));
	file.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
	if (!file || width == 0 || height == 0) {
		return false;
	}

	out_width = width;
	out_height = height;
	out_cells.resize(static_cast<size_t>(width) * height);

	return read_chunked_data(file, num_blocks, out_cells.size(), out_cells.data());
}

bool SaveManager::inspect_stamp_file(const std::string& path_or_name, const std::string& current_set,
									 StampFileInfo& info) {
	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + path_or_name;
	}
	if (filepath.length() < 6 || filepath.substr(filepath.length() - 6) != ".stamp") {
		filepath += ".stamp";
	}

	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::string set_line;
	if (!std::getline(file, set_line))
		return false;
	while (!set_line.empty() && (set_line.back() == '\r' || set_line.back() == '\n' || set_line.back() == ' ')) {
		set_line.pop_back();
	}

	uint32_t width = 0, height = 0;
	file.read(reinterpret_cast<char*>(&width), sizeof(width));
	file.read(reinterpret_cast<char*>(&height), sizeof(height));
	if (!file)
		return false;

	info.filename = std::filesystem::path(filepath).filename().string();
	info.name = std::filesystem::path(filepath).stem().string();
	info.set_name = set_line;
	info.width = width;
	info.height = height;
	try {
		info.file_size = std::filesystem::file_size(filepath);
	} catch (...) {
		info.file_size = 0;
	}
	return true;
}

std::vector<StampFileInfo> SaveManager::get_stamp_files(const std::string& current_set) {
	std::vector<StampFileInfo> list;
	std::string dir = get_saves_directory(current_set);
	if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			if (entry.is_regular_file() && entry.path().extension() == ".stamp") {
				StampFileInfo info;
				if (inspect_stamp_file(entry.path().string(), current_set, info)) {
					list.push_back(info);
				}
			}
		}
	}
	std::sort(list.begin(), list.end(), [](const StampFileInfo& a, const StampFileInfo& b) {
		return a.filename < b.filename;
	});
	return list;
}

bool SaveManager::delete_stamp_file(const std::string& filename, const std::string& current_set) {
	std::string filepath = get_saves_directory(current_set) + filename;
	try {
		return std::filesystem::remove(filepath);
	} catch (...) {
		return false;
	}
}