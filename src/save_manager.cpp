#include "save_manager.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

#include "const.hpp"
#include "grid.hpp"
#include "miniz.h"
#include "set_manager.hpp"
#include "sha256.hpp"
#include "undo_manager.hpp"
#include "window.hpp"
#include "workshop_cache.hpp"
#include "workshop_client.hpp"

std::string SaveManager::compute_file_hash(const std::string& filepath) {
	std::ifstream in(std::filesystem::path(filepath), std::ios::binary);
	if (!in.is_open())
		return "";
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (bytes.empty())
		return "";
	return sha256(bytes.data(), bytes.size());
}

void SaveManager::bwt_encode(const uint8_t* in_data, size_t N, std::vector<uint8_t>& out_L, uint16_t& out_primary_id) {
	if (N == 0) {
		out_L.clear();
		out_primary_id = 0;
		return;
	}

	bool all_same = true;
	for (size_t i = 1; i < N; ++i) {
		if (in_data[i] != in_data[0]) {
			all_same = false;
			break;
		}
	}
	if (all_same) {
		out_L.assign(N, in_data[0]);
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

constexpr uint32_t SAVE_MAGIC = 0x53334346;	 // "S3CF"

static bool write_cells_file(const std::string& filepath, const std::string& current_set,
							 const std::vector<Cell>& cells, uint32_t width, uint32_t height,
							 CompressionQuality quality) {
	std::ofstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	file << current_set << "\n";

	size_t N = static_cast<size_t>(width) * height;
	if (cells.size() < N) {
		return false;
	}

	std::vector<uint8_t> mat_buffer(N);
	bool has_attr = false;
	for (size_t i = 0; i < N; ++i) {
		mat_buffer[i] = cells[i].material;
		if (cells[i].a_aux != 0 || cells[i].a_prim != 0) {
			has_attr = true;
		}
	}

	int deflate_level = MZ_DEFAULT_LEVEL;
	if (quality == CompressionQuality::Low) {
		deflate_level = MZ_BEST_SPEED;
	} else if (quality == CompressionQuality::High) {
		deflate_level = MZ_BEST_COMPRESSION;
	}

	mz_ulong mat_bound = mz_compressBound(N);
	std::vector<uint8_t> mat_compressed(mat_bound);
	mz_ulong mat_comp_size = mat_bound;
	if (mz_compress2(mat_compressed.data(), &mat_comp_size, mat_buffer.data(), N, deflate_level) != MZ_OK) {
		return false;
	}
	mat_compressed.resize(mat_comp_size);

	std::vector<uint8_t> attr_compressed;
	uint8_t flags = 0;
	if (has_attr) {
		flags |= 1;
		std::vector<uint8_t> raw_attrs(N * 6);
		int16_t* aux_p = reinterpret_cast<int16_t*>(raw_attrs.data());
		int32_t* prim_p = reinterpret_cast<int32_t*>(raw_attrs.data() + N * 2);
		for (size_t i = 0; i < N; ++i) {
			aux_p[i] = cells[i].a_aux;
			prim_p[i] = cells[i].a_prim;
		}

		if (quality == CompressionQuality::Low) {
			flags |= 2;	 // raw uncompressed attributes
			attr_compressed = std::move(raw_attrs);
		} else {
			mz_ulong attr_bound = mz_compressBound(raw_attrs.size());
			attr_compressed.resize(attr_bound);
			mz_ulong a_comp_size = attr_bound;
			if (mz_compress2(attr_compressed.data(), &a_comp_size, raw_attrs.data(), raw_attrs.size(), deflate_level) !=
				MZ_OK) {
				return false;
			}
			attr_compressed.resize(a_comp_size);
		}
	}

	uint32_t magic = SAVE_MAGIC;
	uint16_t version = 1;
	uint8_t quality_u8 = static_cast<uint8_t>(quality);
	uint32_t m_size = static_cast<uint32_t>(mat_compressed.size());
	uint32_t a_size = static_cast<uint32_t>(attr_compressed.size());

	file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
	file.write(reinterpret_cast<const char*>(&version), sizeof(version));
	file.write(reinterpret_cast<const char*>(&quality_u8), sizeof(quality_u8));
	file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
	file.write(reinterpret_cast<const char*>(&width), sizeof(width));
	file.write(reinterpret_cast<const char*>(&height), sizeof(height));
	file.write(reinterpret_cast<const char*>(&m_size), sizeof(m_size));
	file.write(reinterpret_cast<const char*>(&a_size), sizeof(a_size));

	file.write(reinterpret_cast<const char*>(mat_compressed.data()), mat_compressed.size());
	if (a_size > 0) {
		file.write(reinterpret_cast<const char*>(attr_compressed.data()), attr_compressed.size());
	}
	return file.good();
}

static bool read_cells_file(const std::string& filepath, std::string& loaded_set, std::vector<Cell>& out_cells,
							uint32_t& out_width, uint32_t& out_height) {
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

	uint32_t val1 = 0;
	file.read(reinterpret_cast<char*>(&val1), sizeof(val1));
	if (!file)
		return false;

	if (val1 == SAVE_MAGIC) {
		uint16_t version = 0;
		uint8_t quality_u8 = 0, flags = 0;
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		file.read(reinterpret_cast<char*>(&quality_u8), sizeof(quality_u8));
		file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
		file.read(reinterpret_cast<char*>(&out_width), sizeof(out_width));
		file.read(reinterpret_cast<char*>(&out_height), sizeof(out_height));
		if (!file || out_width == 0 || out_height == 0)
			return false;

		uint32_t mat_comp_size = 0, attr_comp_size = 0;
		file.read(reinterpret_cast<char*>(&mat_comp_size), sizeof(mat_comp_size));
		file.read(reinterpret_cast<char*>(&attr_comp_size), sizeof(attr_comp_size));
		if (!file || mat_comp_size == 0)
			return false;

		size_t N = static_cast<size_t>(out_width) * out_height;
		std::vector<uint8_t> mat_compressed(mat_comp_size);
		file.read(reinterpret_cast<char*>(mat_compressed.data()), mat_comp_size);
		if (!file)
			return false;

		std::vector<uint8_t> uncompressed_mats(N);
		mz_ulong uncomp_mat_len = N;
		if (mz_uncompress(uncompressed_mats.data(), &uncomp_mat_len, mat_compressed.data(), mat_comp_size) != MZ_OK ||
			uncomp_mat_len != N) {
			return false;
		}

		std::vector<uint8_t> uncompressed_attrs;
		if ((flags & 1) && attr_comp_size > 0) {
			std::vector<uint8_t> attr_compressed(attr_comp_size);
			file.read(reinterpret_cast<char*>(attr_compressed.data()), attr_comp_size);
			if (!file)
				return false;

			uncompressed_attrs.resize(N * 6);
			if (flags & 2) {
				if (attr_comp_size != N * 6)
					return false;
				std::memcpy(uncompressed_attrs.data(), attr_compressed.data(), N * 6);
			} else {
				mz_ulong uncomp_attr_len = N * 6;
				if (mz_uncompress(uncompressed_attrs.data(), &uncomp_attr_len, attr_compressed.data(),
								  attr_comp_size) != MZ_OK ||
					uncomp_attr_len != N * 6) {
					return false;
				}
			}
		}

		out_cells.resize(N);
		const int16_t* aux_p =
			uncompressed_attrs.empty() ? nullptr : reinterpret_cast<const int16_t*>(uncompressed_attrs.data());
		const int32_t* prim_p =
			uncompressed_attrs.empty() ? nullptr : reinterpret_cast<const int32_t*>(uncompressed_attrs.data() + N * 2);

		for (size_t i = 0; i < N; ++i) {
			out_cells[i].material = uncompressed_mats[i];
			if (aux_p && prim_p) {
				out_cells[i].a_aux = aux_p[i];
				out_cells[i].a_prim = prim_p[i];
			} else {
				out_cells[i].a_aux = 0;
				out_cells[i].a_prim = 0;
			}
		}
		return true;
	} else {
		out_width = val1;
		file.read(reinterpret_cast<char*>(&out_height), sizeof(out_height));
		uint32_t num_blocks = 0;
		file.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));
		if (!file || out_width == 0 || out_height == 0)
			return false;

		size_t N = static_cast<size_t>(out_width) * out_height;
		std::vector<uint8_t> raw_mats(N);
		if (!SaveManager::read_chunked_data(file, num_blocks, N, raw_mats.data())) {
			return false;
		}

		out_cells.resize(N);
		for (size_t i = 0; i < N; ++i) {
			out_cells[i].material = raw_mats[i];
			out_cells[i].a_aux = 0;
			out_cells[i].a_prim = 0;
		}
		return true;
	}
}

std::string SaveManager::get_saves_directory(const std::string& current_set) {
	return std::string(SETS_DIRECTORY) + current_set + "/saves/";
}

bool SaveManager::save_to_file(const std::string& name, const std::string& current_set, CompressionQuality quality) {
	std::string saves_dir = get_saves_directory(current_set);
	std::error_code ec;
	std::filesystem::create_directories(saves_dir, ec);
	std::string filename = name;
	if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".save") {
		filename += ".save";
	}
	std::string filepath = saves_dir + filename;

	if (Grid::get_processing_mode() == ProcessingMode::GPU) {
		Grid::sync_from_gpu();
	}
	std::vector<Cell> cells = Grid::get_all_cells_full();
	return write_cells_file(filepath, current_set, cells, Grid::get_width(), Grid::get_height(), quality);
}

void SaveManager::save_to_file_async(const std::string& name, const std::string& current_set,
									 CompressionQuality quality, std::function<void(bool success)> callback) {
	Window::increment_busy();

	std::string saves_dir = get_saves_directory(current_set);
	std::error_code ec;
	std::filesystem::create_directories(saves_dir, ec);
	std::string filename = name;
	if (filename.length() < 5 || filename.substr(filename.length() - 5) != ".save") {
		filename += ".save";
	}
	std::string filepath = saves_dir + filename;

	uint32_t width = Grid::get_width();
	uint32_t height = Grid::get_height();
	if (Grid::get_processing_mode() == ProcessingMode::GPU) {
		Grid::sync_from_gpu();
	}
	std::vector<Cell> cells = Grid::get_all_cells_full();

	std::thread([filepath, current_set, width, height, cells = std::move(cells), quality, callback]() {
		bool ok = write_cells_file(filepath, current_set, cells, width, height, quality);

		WorkshopClient::enqueue_main_thread([callback, ok]() {
			Window::decrement_busy();
			if (callback) {
				callback(ok);
			}
		});
	}).detach();
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

	uint32_t saved_width = 0, saved_height = 0;
	std::vector<Cell> saved_cells;
	if (!read_cells_file(filepath, loaded_set, saved_cells, saved_width, saved_height)) {
		return false;
	}

	if (loaded_set != current_set) {
		SetManager::set_current_set(loaded_set);
	}

	if (placement == LoadPlacement::ResizeGrid &&
		(saved_width != Grid::get_width() || saved_height != Grid::get_height())) {
		Grid::resize(saved_width, saved_height, false);
	}

	Grid::clear();

	if (saved_width == Grid::get_width() && saved_height == Grid::get_height()) {
		Grid::restore_state_full(saved_cells);
	} else if (placement == LoadPlacement::TopLeft) {
		uint32_t copy_w = std::min(saved_width, Grid::get_width());
		uint32_t copy_h = std::min(saved_height, Grid::get_height());
		for (uint32_t y = 0; y < copy_h; ++y) {
			for (uint32_t x = 0; x < copy_w; ++x) {
				Grid::set_cell_full(x, y, saved_cells[y * saved_width + x]);
			}
		}
	} else {
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
				Grid::set_cell_full(static_cast<uint32_t>(gx), static_cast<uint32_t>(gy),
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

void SaveManager::load_from_file_async(const std::string& path_or_name, const std::string& current_set,
									   LoadPlacement placement,
									   std::function<void(bool success, const std::string& loaded_set)> callback) {
	Window::increment_busy();

	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + path_or_name;
	}
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}

	std::thread([filepath, current_set, placement, callback]() {
		std::string loaded_set;
		uint32_t saved_width = 0, saved_height = 0;
		std::vector<Cell> saved_cells;
		bool read_ok = read_cells_file(filepath, loaded_set, saved_cells, saved_width, saved_height);

		WorkshopClient::enqueue_main_thread([callback, read_ok, loaded_set, current_set, placement, saved_width,
											 saved_height, saved_cells = std::move(saved_cells)]() {
			Window::decrement_busy();
			if (!read_ok) {
				if (callback) {
					callback(false, "");
				}
				return;
			}

			if (loaded_set != current_set) {
				SetManager::set_current_set(loaded_set);
			}

			if (placement == LoadPlacement::ResizeGrid &&
				(saved_width != Grid::get_width() || saved_height != Grid::get_height())) {
				Grid::resize(saved_width, saved_height, false);
			}

			Grid::clear();

			if (saved_width == Grid::get_width() && saved_height == Grid::get_height()) {
				Grid::restore_state_full(saved_cells);
			} else if (placement == LoadPlacement::TopLeft) {
				uint32_t copy_w = std::min(saved_width, Grid::get_width());
				uint32_t copy_h = std::min(saved_height, Grid::get_height());
				for (uint32_t y = 0; y < copy_h; ++y) {
					for (uint32_t x = 0; x < copy_w; ++x) {
						Grid::set_cell_full(x, y, saved_cells[y * saved_width + x]);
					}
				}
			} else {
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
						Grid::set_cell_full(static_cast<uint32_t>(gx), static_cast<uint32_t>(gy),
											saved_cells[sy * saved_width + sx]);
					}
				}
			}

			if (Grid::get_processing_mode() == ProcessingMode::GPU) {
				Grid::sync_to_gpu();
			}
			UndoManager::init();

			if (callback) {
				callback(true, loaded_set);
			}
		});
	}).detach();
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

	uint32_t val1 = 0;
	file.read(reinterpret_cast<char*>(&val1), sizeof(val1));
	if (!file)
		return false;

	uint32_t width = 0, height = 0;
	if (val1 == SAVE_MAGIC) {
		uint16_t version = 0;
		uint8_t quality_u8 = 0, flags = 0;
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		file.read(reinterpret_cast<char*>(&quality_u8), sizeof(quality_u8));
		file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
		file.read(reinterpret_cast<char*>(&width), sizeof(width));
		file.read(reinterpret_cast<char*>(&height), sizeof(height));
	} else {
		width = val1;
		file.read(reinterpret_cast<char*>(&height), sizeof(height));
	}
	if (!file)
		return false;

	info.filename = std::filesystem::path(filepath).filename().string();
	info.name = std::filesystem::path(filepath).stem().string();
	info.set_name = set_line;
	info.width = width;
	info.height = height;
	info.dimensions_differ = (width != Grid::get_width() || height != Grid::get_height());
	info.is_online = false;
	info.workshop_id = "";
	info.author = "";
	info.version = 1;
	try {
		info.file_size = std::filesystem::file_size(filepath);
	} catch (...) {
		info.file_size = 0;
	}

	std::string meta_path = filepath + ".ini";
	if (std::filesystem::exists(meta_path)) {
		std::ifstream mf(meta_path);
		std::string mline;
		while (std::getline(mf, mline)) {
			size_t eq = mline.find('=');
			if (eq == std::string::npos)
				continue;
			std::string k = mline.substr(0, eq);
			std::string v = mline.substr(eq + 1);
			while (!k.empty() && (k.back() == ' ' || k.back() == '\t' || k.back() == '\r'))
				k.pop_back();
			while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r'))
				v.pop_back();
			size_t kp = k.find_first_not_of(" \t");
			if (kp != std::string::npos)
				k = k.substr(kp);
			size_t vp = v.find_first_not_of(" \t");
			if (vp != std::string::npos)
				v = v.substr(vp);

			if (k == "workshop_id") {
				info.workshop_id = v;
				if (!v.empty())
					info.is_online = true;
			} else if (k == "workshop_hash") {
				info.workshop_hash = v;
			} else if (k == "file_hash") {
				info.file_hash = v;
			} else if (k == "author") {
				info.author = v;
			} else if (k == "version") {
				try {
					info.version = static_cast<uint32_t>(std::stoul(v));
				} catch (...) {}
			} else if (k == "forked_from_id") {
				info.forked_from_id = v;
			} else if (k == "forked_from_author") {
				info.forked_from_author = v;
			} else if (k == "forked_from_version") {
				try {
					info.forked_from_version = static_cast<uint32_t>(std::stoul(v));
				} catch (...) {}
			}
		}
	}
	return true;
}

void SaveManager::set_save_workshop_info(const std::string& name_or_filename, const std::string& current_set,
										 const std::string& workshop_id, const std::string& workshop_hash,
										 const std::string& author, uint32_t version) {
	std::string filepath = name_or_filename;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + name_or_filename;
	}
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}
	std::string meta_path = filepath + ".ini";
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(meta_path).parent_path(), ec);
	std::ofstream out(meta_path);
	if (out.is_open()) {
		out << "[Workshop]\n";
		out << "workshop_id = " << workshop_id << "\n";
		out << "workshop_hash = " << workshop_hash << "\n";
		out << "author = " << author << "\n";
		out << "version = " << version << "\n";
		std::string f_hash = compute_file_hash(filepath);
		if (!f_hash.empty()) {
			out << "file_hash = " << f_hash << "\n";
		}
	}
}

void SaveManager::clear_save_workshop_info(const std::string& name_or_filename, const std::string& current_set) {
	std::string filepath = name_or_filename;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + name_or_filename;
	}
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}
	std::string meta_path = filepath + ".ini";
	std::error_code ec;
	std::filesystem::remove(meta_path, ec);
	WorkshopCache::remove_transient(filepath);
}

void SaveManager::set_stamp_workshop_info(const std::string& name_or_filename, const std::string& current_set,
										  const std::string& workshop_id, const std::string& workshop_hash,
										  const std::string& author, uint32_t version) {
	std::string filepath = name_or_filename;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + name_or_filename;
	}
	if (filepath.length() < 6 || filepath.substr(filepath.length() - 6) != ".stamp") {
		filepath += ".stamp";
	}
	std::string meta_path = filepath + ".ini";
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(meta_path).parent_path(), ec);
	std::ofstream out(meta_path);
	if (out.is_open()) {
		out << "[Workshop]\n";
		out << "workshop_id = " << workshop_id << "\n";
		out << "workshop_hash = " << workshop_hash << "\n";
		out << "author = " << author << "\n";
		out << "version = " << version << "\n";
		std::string f_hash = compute_file_hash(filepath);
		if (!f_hash.empty()) {
			out << "file_hash = " << f_hash << "\n";
		}
	}
}

void SaveManager::clear_stamp_workshop_info(const std::string& name_or_filename, const std::string& current_set) {
	std::string filepath = name_or_filename;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + name_or_filename;
	}
	if (filepath.length() < 6 || filepath.substr(filepath.length() - 6) != ".stamp") {
		filepath += ".stamp";
	}
	std::string meta_path = filepath + ".ini";
	std::error_code ec;
	std::filesystem::remove(meta_path, ec);
	WorkshopCache::remove_transient(filepath);
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
	std::sort(list.begin(), list.end(),
			  [](const SaveFileInfo& a, const SaveFileInfo& b) { return a.filename < b.filename; });
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
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
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

bool SaveManager::migrate_save_file(const std::string& filename, const std::string& source_set,
									const std::string& target_set) {
	std::string src_path = get_saves_directory(source_set) + filename;
	std::ifstream in(src_path, std::ios::binary);
	if (!in.is_open())
		return false;

	std::string old_set_line;
	std::getline(in, old_set_line);

	std::vector<char> remaining_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();

	std::string dst_dir = get_saves_directory(target_set);
	std::error_code ec;
	std::filesystem::create_directories(dst_dir, ec);
	std::string dst_path = dst_dir + filename;
	std::ofstream out(dst_path, std::ios::binary);
	if (!out.is_open())
		return false;

	out << target_set << "\n";
	if (!remaining_data.empty()) {
		out.write(remaining_data.data(), remaining_data.size());
	}
	out.close();

	SaveFileInfo src_info;
	inspect_save_file(filename, source_set, src_info);

	std::string dst_ini = dst_path + ".ini";
	std::ofstream out_ini(dst_ini);
	if (out_ini.is_open()) {
		out_ini << "[Workshop]\n";
		out_ini << "workshop_id = \n";
		out_ini << "workshop_hash = \n";
		out_ini << "author = " << src_info.author << "\n";
		out_ini << "version = 1\n";
		if (!src_info.workshop_id.empty()) {
			out_ini << "forked_from_id = " << src_info.workshop_id << "\n";
			out_ini << "forked_from_author = " << src_info.author << "\n";
			out_ini << "forked_from_version = " << src_info.version << "\n";
		} else if (!src_info.forked_from_id.empty()) {
			out_ini << "forked_from_id = " << src_info.forked_from_id << "\n";
			out_ini << "forked_from_author = " << src_info.forked_from_author << "\n";
			out_ini << "forked_from_version = " << src_info.forked_from_version << "\n";
		}
	}
	return true;
}

bool SaveManager::fork_save_to_set(const std::string& src_filepath, const std::string& target_set,
								   const std::string& new_name, const std::string& forked_from_id,
								   const std::string& forked_from_author, uint32_t forked_from_version) {
	std::ifstream in(src_filepath, std::ios::binary);
	if (!in.is_open())
		return false;

	std::string old_set_line;
	std::getline(in, old_set_line);

	std::vector<char> remaining_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();

	std::string dst_dir = get_saves_directory(target_set);
	std::error_code ec;
	std::filesystem::create_directories(dst_dir, ec);
	std::string dst_name = new_name;
	if (dst_name.length() < 5 || dst_name.substr(dst_name.length() - 5) != ".save") {
		dst_name += ".save";
	}
	std::string dst_path = dst_dir + dst_name;
	std::ofstream out(dst_path, std::ios::binary);
	if (!out.is_open())
		return false;

	out << target_set << "\n";
	if (!remaining_data.empty()) {
		out.write(remaining_data.data(), remaining_data.size());
	}
	out.close();

	std::string dst_ini = dst_path + ".ini";
	std::ofstream out_ini(dst_ini);
	if (out_ini.is_open()) {
		out_ini << "[Workshop]\n";
		out_ini << "workshop_id = \n";
		out_ini << "workshop_hash = \n";
		out_ini << "author = " << (forked_from_author.empty() ? "Player" : forked_from_author) << "\n";
		out_ini << "version = 1\n";
		if (!forked_from_id.empty()) {
			out_ini << "forked_from_id = " << forked_from_id << "\n";
			out_ini << "forked_from_author = " << forked_from_author << "\n";
			out_ini << "forked_from_version = " << forked_from_version << "\n";
		}
	}
	return true;
}

bool SaveManager::save_stamp_to_file(const std::string& name, const std::string& current_set,
									 const std::vector<Cell>& cells, uint32_t width, uint32_t height,
									 CompressionQuality quality) {
	if (cells.empty() || width == 0 || height == 0 || cells.size() != static_cast<size_t>(width) * height) {
		return false;
	}
	std::string saves_dir = get_saves_directory(current_set);
	std::error_code ec;
	std::filesystem::create_directories(saves_dir, ec);
	std::string filename = name;
	if (filename.length() < 6 || filename.substr(filename.length() - 6) != ".stamp") {
		filename += ".stamp";
	}
	std::string filepath = saves_dir + filename;
	return write_cells_file(filepath, current_set, cells, width, height, quality);
}

bool SaveManager::save_stamp_to_file(const std::string& name, const std::string& current_set,
									 const std::vector<uint8_t>& cells, uint32_t width, uint32_t height,
									 CompressionQuality quality) {
	std::vector<Cell> full_cells(cells.size());
	for (size_t i = 0; i < cells.size(); ++i) {
		full_cells[i].material = cells[i];
		full_cells[i].a_aux = 0;
		full_cells[i].a_prim = 0;
	}
	return save_stamp_to_file(name, current_set, full_cells, width, height, quality);
}

void SaveManager::save_stamp_to_file_async(const std::string& name, const std::string& current_set,
										   const std::vector<Cell>& cells, uint32_t width, uint32_t height,
										   CompressionQuality quality, std::function<void(bool success)> callback) {
	if (cells.empty() || width == 0 || height == 0 || cells.size() != static_cast<size_t>(width) * height) {
		if (callback) {
			callback(false);
		}
		return;
	}
	Window::increment_busy();

	std::string saves_dir = get_saves_directory(current_set);
	std::error_code ec;
	std::filesystem::create_directories(saves_dir, ec);
	std::string filename = name;
	if (filename.length() < 6 || filename.substr(filename.length() - 6) != ".stamp") {
		filename += ".stamp";
	}
	std::string filepath = saves_dir + filename;

	std::thread([filepath, current_set, width, height, cells, quality, callback]() {
		bool ok = write_cells_file(filepath, current_set, cells, width, height, quality);

		WorkshopClient::enqueue_main_thread([callback, ok]() {
			Window::decrement_busy();
			if (callback) {
				callback(ok);
			}
		});
	}).detach();
}

void SaveManager::save_stamp_to_file_async(const std::string& name, const std::string& current_set,
										   const std::vector<uint8_t>& cells, uint32_t width, uint32_t height,
										   std::function<void(bool success)> callback) {
	std::vector<Cell> full_cells(cells.size());
	for (size_t i = 0; i < cells.size(); ++i) {
		full_cells[i].material = cells[i];
		full_cells[i].a_aux = 0;
		full_cells[i].a_prim = 0;
	}
	save_stamp_to_file_async(name, current_set, full_cells, width, height, CompressionQuality::High,
							 std::move(callback));
}

bool SaveManager::load_stamp_from_file(const std::string& path_or_name, const std::string& current_set,
									   std::vector<Cell>& out_cells, uint32_t& out_width, uint32_t& out_height) {
	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + path_or_name;
	}
	if (filepath.length() < 6 || filepath.substr(filepath.length() - 6) != ".stamp") {
		filepath += ".stamp";
	}

	std::string loaded_set;
	return read_cells_file(filepath, loaded_set, out_cells, out_width, out_height);
}

bool SaveManager::load_stamp_from_file(const std::string& path_or_name, const std::string& current_set,
									   std::vector<uint8_t>& out_cells, uint32_t& out_width, uint32_t& out_height) {
	std::vector<Cell> full_cells;
	if (!load_stamp_from_file(path_or_name, current_set, full_cells, out_width, out_height)) {
		return false;
	}
	out_cells.resize(full_cells.size());
	for (size_t i = 0; i < full_cells.size(); ++i) {
		out_cells[i] = full_cells[i].material;
	}
	return true;
}

void SaveManager::load_stamp_from_file_async(
	const std::string& path_or_name, const std::string& current_set,
	std::function<void(bool success, const std::vector<Cell>& cells, uint32_t width, uint32_t height)> callback) {
	Window::increment_busy();

	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = get_saves_directory(current_set) + path_or_name;
	}
	if (filepath.length() < 6 || filepath.substr(filepath.length() - 6) != ".stamp") {
		filepath += ".stamp";
	}

	std::thread([filepath, callback]() {
		std::vector<Cell> cells;
		uint32_t width = 0, height = 0;
		std::string loaded_set;
		bool ok = read_cells_file(filepath, loaded_set, cells, width, height);

		WorkshopClient::enqueue_main_thread([callback, ok, cells = std::move(cells), width, height]() {
			Window::decrement_busy();
			if (callback) {
				callback(ok, cells, width, height);
			}
		});
	}).detach();
}

void SaveManager::load_stamp_from_file_async(
	const std::string& path_or_name, const std::string& current_set,
	std::function<void(bool success, const std::vector<uint8_t>& cells, uint32_t width, uint32_t height)> callback) {
	load_stamp_from_file_async(path_or_name, current_set,
							   [callback](bool ok, const std::vector<Cell>& cells, uint32_t width, uint32_t height) {
								   if (!callback)
									   return;
								   std::vector<uint8_t> mats(cells.size());
								   for (size_t i = 0; i < cells.size(); ++i) {
									   mats[i] = cells[i].material;
								   }
								   callback(ok, mats, width, height);
							   });
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

	uint32_t val1 = 0;
	file.read(reinterpret_cast<char*>(&val1), sizeof(val1));
	if (!file)
		return false;

	uint32_t width = 0, height = 0;
	if (val1 == SAVE_MAGIC) {
		uint16_t version = 0;
		uint8_t quality_u8 = 0, flags = 0;
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		file.read(reinterpret_cast<char*>(&quality_u8), sizeof(quality_u8));
		file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
		file.read(reinterpret_cast<char*>(&width), sizeof(width));
		file.read(reinterpret_cast<char*>(&height), sizeof(height));
	} else {
		width = val1;
		file.read(reinterpret_cast<char*>(&height), sizeof(height));
	}
	if (!file)
		return false;

	info.filename = std::filesystem::path(filepath).filename().string();
	info.name = std::filesystem::path(filepath).stem().string();
	info.set_name = set_line;
	info.width = width;
	info.height = height;
	info.is_online = false;
	info.workshop_id = "";
	info.author = "";
	info.version = 1;
	try {
		info.file_size = std::filesystem::file_size(filepath);
	} catch (...) {
		info.file_size = 0;
	}

	std::string meta_path = filepath + ".ini";
	if (std::filesystem::exists(meta_path)) {
		std::ifstream mf(meta_path);
		std::string mline;
		while (std::getline(mf, mline)) {
			size_t eq = mline.find('=');
			if (eq == std::string::npos)
				continue;
			std::string k = mline.substr(0, eq);
			std::string v = mline.substr(eq + 1);
			while (!k.empty() && (k.back() == ' ' || k.back() == '\t' || k.back() == '\r'))
				k.pop_back();
			while (!v.empty() && (v.back() == ' ' || v.back() == '\t' || v.back() == '\r'))
				v.pop_back();
			size_t kp = k.find_first_not_of(" \t");
			if (kp != std::string::npos)
				k = k.substr(kp);
			size_t vp = v.find_first_not_of(" \t");
			if (vp != std::string::npos)
				v = v.substr(vp);

			if (k == "workshop_id") {
				info.workshop_id = v;
				if (!v.empty())
					info.is_online = true;
			} else if (k == "workshop_hash") {
				info.workshop_hash = v;
			} else if (k == "file_hash") {
				info.file_hash = v;
			} else if (k == "author") {
				info.author = v;
			} else if (k == "version") {
				try {
					info.version = static_cast<uint32_t>(std::stoul(v));
				} catch (...) {}
			} else if (k == "forked_from_id") {
				info.forked_from_id = v;
			} else if (k == "forked_from_author") {
				info.forked_from_author = v;
			} else if (k == "forked_from_version") {
				try {
					info.forked_from_version = static_cast<uint32_t>(std::stoul(v));
				} catch (...) {}
			}
		}
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
	std::sort(list.begin(), list.end(),
			  [](const StampFileInfo& a, const StampFileInfo& b) { return a.filename < b.filename; });
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

bool SaveManager::migrate_stamp_file(const std::string& filename, const std::string& source_set,
									 const std::string& target_set) {
	std::string src_path = get_saves_directory(source_set) + filename;
	std::ifstream in(src_path, std::ios::binary);
	if (!in.is_open())
		return false;

	std::string old_set_line;
	std::getline(in, old_set_line);

	std::vector<char> remaining_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();

	std::string dst_dir = get_saves_directory(target_set);
	std::error_code ec;
	std::filesystem::create_directories(dst_dir, ec);
	std::string dst_path = dst_dir + filename;
	std::ofstream out(dst_path, std::ios::binary);
	if (!out.is_open())
		return false;

	out << target_set << "\n";
	if (!remaining_data.empty()) {
		out.write(remaining_data.data(), remaining_data.size());
	}
	out.close();

	StampFileInfo src_info;
	inspect_stamp_file(filename, source_set, src_info);

	std::string dst_ini = dst_path + ".ini";
	std::ofstream out_ini(dst_ini);
	if (out_ini.is_open()) {
		out_ini << "[Workshop]\n";
		out_ini << "workshop_id = \n";
		out_ini << "workshop_hash = \n";
		out_ini << "author = " << src_info.author << "\n";
		out_ini << "version = 1\n";
		if (!src_info.workshop_id.empty()) {
			out_ini << "forked_from_id = " << src_info.workshop_id << "\n";
			out_ini << "forked_from_author = " << src_info.author << "\n";
			out_ini << "forked_from_version = " << src_info.version << "\n";
		} else if (!src_info.forked_from_id.empty()) {
			out_ini << "forked_from_id = " << src_info.forked_from_id << "\n";
			out_ini << "forked_from_author = " << src_info.forked_from_author << "\n";
			out_ini << "forked_from_version = " << src_info.forked_from_version << "\n";
		}
	}
	return true;
}

bool SaveManager::fork_stamp_to_set(const std::string& src_filepath, const std::string& target_set,
									const std::string& new_name, const std::string& forked_from_id,
									const std::string& forked_from_author, uint32_t forked_from_version) {
	std::ifstream in(src_filepath, std::ios::binary);
	if (!in.is_open())
		return false;

	std::string old_set_line;
	std::getline(in, old_set_line);

	std::vector<char> remaining_data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();

	std::string dst_dir = get_saves_directory(target_set);
	std::error_code ec;
	std::filesystem::create_directories(dst_dir, ec);
	std::string dst_name = new_name;
	if (dst_name.length() < 6 || dst_name.substr(dst_name.length() - 6) != ".stamp") {
		dst_name += ".stamp";
	}
	std::string dst_path = dst_dir + dst_name;
	std::ofstream out(dst_path, std::ios::binary);
	if (!out.is_open())
		return false;

	out << target_set << "\n";
	if (!remaining_data.empty()) {
		out.write(remaining_data.data(), remaining_data.size());
	}
	out.close();

	std::string dst_ini = dst_path + ".ini";
	std::ofstream out_ini(dst_ini);
	if (out_ini.is_open()) {
		out_ini << "[Workshop]\n";
		out_ini << "workshop_id = \n";
		out_ini << "workshop_hash = \n";
		out_ini << "author = " << (forked_from_author.empty() ? "Player" : forked_from_author) << "\n";
		out_ini << "version = 1\n";
		if (!forked_from_id.empty()) {
			out_ini << "forked_from_id = " << forked_from_id << "\n";
			out_ini << "forked_from_author = " << forked_from_author << "\n";
			out_ini << "forked_from_version = " << forked_from_version << "\n";
		}
	}
	return true;
}

void SaveManager::estimate_save_size(uint32_t width, uint32_t height, CompressionQuality quality,
									 size_t& out_orig_size, size_t& out_final_size) {
	out_orig_size = static_cast<size_t>(width) * height * sizeof(Cell);
	if (Grid::get_processing_mode() == ProcessingMode::GPU) {
		Grid::sync_from_gpu();
	}
	std::vector<Cell> cells = Grid::get_all_cells_full();
	estimate_stamp_size(cells, width, height, quality, out_orig_size, out_final_size);
}

void SaveManager::estimate_stamp_size(const std::vector<Cell>& cells, uint32_t width, uint32_t height,
									  CompressionQuality quality, size_t& out_orig_size, size_t& out_final_size) {
	out_orig_size = static_cast<size_t>(width) * height * sizeof(Cell);
	if (cells.empty() || width == 0 || height == 0) {
		out_final_size = 0;
		return;
	}

	size_t N = std::min(cells.size(), static_cast<size_t>(width) * height);
	std::vector<uint8_t> mat_buffer(N);
	bool has_attr = false;
	for (size_t i = 0; i < N; ++i) {
		mat_buffer[i] = cells[i].material;
		if (cells[i].a_aux != 0 || cells[i].a_prim != 0) {
			has_attr = true;
		}
	}

	int deflate_level = MZ_DEFAULT_LEVEL;
	if (quality == CompressionQuality::Low) {
		deflate_level = MZ_BEST_SPEED;
	} else if (quality == CompressionQuality::High) {
		deflate_level = MZ_BEST_COMPRESSION;
	}

	mz_ulong mat_bound = mz_compressBound(N);
	std::vector<uint8_t> mat_comp(mat_bound);
	mz_ulong mat_comp_len = mat_bound;
	mz_compress2(mat_comp.data(), &mat_comp_len, mat_buffer.data(), N, deflate_level);

	size_t attr_size = 0;
	if (has_attr) {
		std::vector<uint8_t> raw_attrs(N * 6);
		int16_t* aux_p = reinterpret_cast<int16_t*>(raw_attrs.data());
		int32_t* prim_p = reinterpret_cast<int32_t*>(raw_attrs.data() + N * 2);
		for (size_t i = 0; i < N; ++i) {
			aux_p[i] = cells[i].a_aux;
			prim_p[i] = cells[i].a_prim;
		}

		if (quality == CompressionQuality::Low) {
			attr_size = raw_attrs.size();
		} else {
			mz_ulong attr_bound = mz_compressBound(raw_attrs.size());
			std::vector<uint8_t> attr_comp(attr_bound);
			mz_ulong attr_comp_len = attr_bound;
			mz_compress2(attr_comp.data(), &attr_comp_len, raw_attrs.data(), raw_attrs.size(), deflate_level);
			attr_size = attr_comp_len;
		}
	}

	size_t header_size = 32;
	out_final_size = header_size + mat_comp_len + attr_size;
}

bool SaveManager::is_file_highest_compression(const std::string& filepath) {
	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	std::string set_line;
	if (!std::getline(file, set_line))
		return false;

	uint32_t magic = 0;
	file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	if (!file || magic != SAVE_MAGIC)
		return false;

	uint16_t version = 0;
	uint8_t quality_u8 = 0;
	file.read(reinterpret_cast<char*>(&version), sizeof(version));
	file.read(reinterpret_cast<char*>(&quality_u8), sizeof(quality_u8));
	return (file.good() && quality_u8 == static_cast<uint8_t>(CompressionQuality::High));
}

bool SaveManager::is_buffer_highest_compression(const std::vector<uint8_t>& bytes) {
	if (bytes.size() < 12)
		return false;
	size_t pos = 0;
	while (pos < bytes.size() && bytes[pos] != '\n') {
		pos++;
	}
	if (pos >= bytes.size())
		return false;
	pos++; // skip '\n'
	if (pos + 7 > bytes.size())
		return false;
	uint32_t magic = 0;
	std::memcpy(&magic, bytes.data() + pos, sizeof(magic));
	if (magic != SAVE_MAGIC)
		return false;
	uint8_t quality_u8 = bytes[pos + 6]; // pos + 4 is version (uint16_t), pos + 6 is quality_u8 (uint8_t)
	return (quality_u8 == static_cast<uint8_t>(CompressionQuality::High));
}

bool SaveManager::recompress_to_highest(std::vector<uint8_t>& in_out_bytes, const std::string& current_set) {
	if (is_buffer_highest_compression(in_out_bytes)) {
		return false;
	}
	std::string saves_dir = get_saves_directory(current_set);
	std::string tmp_in = saves_dir + "__recomp_in.tmp";
	std::string tmp_out = saves_dir + "__recomp_out.tmp";

	{
		std::ofstream fout(tmp_in, std::ios::binary);
		if (!fout.is_open())
			return false;
		fout.write(reinterpret_cast<const char*>(in_out_bytes.data()), in_out_bytes.size());
	}

	std::string loaded_set;
	std::vector<Cell> cells;
	uint32_t width = 0, height = 0;
	bool read_ok = read_cells_file(tmp_in, loaded_set, cells, width, height);
	std::error_code ec;
	std::filesystem::remove(tmp_in, ec);

	if (!read_ok || cells.empty() || width == 0 || height == 0) {
		return false;
	}

	if (loaded_set.empty()) {
		loaded_set = current_set;
	}

	bool write_ok = write_cells_file(tmp_out, loaded_set, cells, width, height, CompressionQuality::High);
	if (!write_ok) {
		std::filesystem::remove(tmp_out, ec);
		return false;
	}

	std::ifstream fin(tmp_out, std::ios::binary);
	if (!fin.is_open()) {
		std::filesystem::remove(tmp_out, ec);
		return false;
	}
	in_out_bytes.assign((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
	fin.close();
	std::filesystem::remove(tmp_out, ec);
	return true;
}