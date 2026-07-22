#include "save_manager.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>

#include "const.hpp"
#include "grid.hpp"
#include "material_manager.hpp"
#include "set_manager.hpp"

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

bool SaveManager::save_to_file(const std::string& name, const std::string& current_set, const Grid& grid) {
	std::string filepath = "../sets/" + current_set + "/" + name;
	if (filepath.length() < 5 || filepath.substr(filepath.length() - 5) != ".save") {
		filepath += ".save";
	}

	std::ofstream file(filepath, std::ios::binary);
	if (!file.is_open())
		return false;

	file << current_set << "\n";

	uint32_t width = SIM_WIDTH;
	uint32_t height = SIM_HEIGHT;
	file.write(reinterpret_cast<const char*>(&width), sizeof(width));
	file.write(reinterpret_cast<const char*>(&height), sizeof(height));

	std::vector<uint8_t> grid_bytes(SIM_SIZE);
	for (unsigned int y = 0; y < SIM_HEIGHT; ++y) {
		for (unsigned int x = 0; x < SIM_WIDTH; ++x) {
			grid_bytes[y * SIM_WIDTH + x] = grid.get_cell(x, y);
		}
	}

	constexpr size_t BLOCK_SIZE_BWT = 4096;
	uint32_t num_blocks = static_cast<uint32_t>((SIM_SIZE + BLOCK_SIZE_BWT - 1) / BLOCK_SIZE_BWT);
	file.write(reinterpret_cast<const char*>(&num_blocks), sizeof(num_blocks));

	for (uint32_t b = 0; b < num_blocks; ++b) {
		size_t block_start = b * BLOCK_SIZE_BWT;
		size_t block_len = std::min(BLOCK_SIZE_BWT, SIM_SIZE - block_start);

		std::vector<uint8_t> L;
		uint16_t primary_id = 0;
		bwt_encode(&grid_bytes[block_start], block_len, L, primary_id);

		std::vector<uint8_t> rle_data = rle_encode(L.data(), L.size());
		uint32_t compressed_size = static_cast<uint32_t>(rle_data.size());

		file.write(reinterpret_cast<const char*>(&primary_id), sizeof(primary_id));
		file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
		file.write(reinterpret_cast<const char*>(rle_data.data()), rle_data.size());
	}

	return true;
}

bool SaveManager::load_from_file(const std::string& path_or_name, const std::string& current_set,
								 std::string& loaded_set, Grid& grid) {
	std::string filepath = path_or_name;
	if (filepath.find('/') == std::string::npos && filepath.find('\\') == std::string::npos) {
		filepath = "../sets/" + current_set + "/" + path_or_name;
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
	SetManager::set_current_set(loaded_set, grid);

	uint32_t width = 0, height = 0, num_blocks = 0;
	file.read(reinterpret_cast<char*>(&width), sizeof(width));
	file.read(reinterpret_cast<char*>(&height), sizeof(height));
	file.read(reinterpret_cast<char*>(&num_blocks), sizeof(num_blocks));

	grid.clear();

	constexpr size_t BLOCK_SIZE_BWT = 4096;
	std::vector<uint8_t> block_raw(BLOCK_SIZE_BWT);

	for (uint32_t b = 0; b < num_blocks; ++b) {
		size_t block_start = b * BLOCK_SIZE_BWT;
		size_t block_len = std::min(BLOCK_SIZE_BWT, SIM_SIZE - block_start);

		uint16_t primary_id = 0;
		uint32_t compressed_size = 0;
		file.read(reinterpret_cast<char*>(&primary_id), sizeof(primary_id));
		file.read(reinterpret_cast<char*>(&compressed_size), sizeof(compressed_size));

		std::vector<uint8_t> compressed_data(compressed_size);
		file.read(reinterpret_cast<char*>(compressed_data.data()), compressed_size);

		std::vector<uint8_t> L = rle_decode(compressed_data.data(), compressed_size, block_len);
		bwt_decode(L.data(), block_len, primary_id, block_raw.data());

		for (size_t k = 0; k < block_len; ++k) {
			size_t id = block_start + k;
			unsigned int x = static_cast<unsigned int>(id % SIM_WIDTH);
			unsigned int y = static_cast<unsigned int>(id / SIM_WIDTH);
			grid.set_cell(x, y, block_raw[k]);
		}
	}
	return true;
}
