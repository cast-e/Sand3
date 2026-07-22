#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Grid;

class SaveManager {
public:
	SaveManager() = delete;

	static bool save_to_file(const std::string& name, const std::string& current_set, const Grid& grid);
	static bool load_from_file(const std::string& path_or_name, const std::string& current_set, std::string& loaded_set,
							   Grid& grid);

	static void bwt_encode(const uint8_t* in_data, size_t N, std::vector<uint8_t>& out_L, uint16_t& out_primary_id);
	static void bwt_decode(const uint8_t* L, size_t N, uint16_t primary_id, uint8_t* out_data);

	static std::vector<uint8_t> rle_encode(const uint8_t* data, size_t size);
	static std::vector<uint8_t> rle_decode(const uint8_t* data, size_t compressed_size, size_t expected_size);
};