#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

enum class LoadPlacement { Center, TopLeft, ResizeGrid };

struct SaveFileInfo {
	std::string filename;
	std::string name;
	std::string set_name;
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t file_size = 0;
	bool dimensions_differ = false;
};

struct StampFileInfo {
	std::string filename;
	std::string name;
	std::string set_name;
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t file_size = 0;
};

class SaveManager {
public:
	SaveManager() = delete;

	static std::string get_saves_directory(const std::string& current_set);

	// Grid Saves
	static bool save_to_file(const std::string& name, const std::string& current_set);
	static bool load_from_file(const std::string& path_or_name, const std::string& current_set,
							   std::string& loaded_set, LoadPlacement placement = LoadPlacement::Center);
	static bool inspect_save_file(const std::string& path_or_name, const std::string& current_set, SaveFileInfo& info);
	static std::vector<SaveFileInfo> get_save_files(const std::string& current_set);
	static bool delete_save_file(const std::string& filename, const std::string& current_set);
	static bool duplicate_save_file(const std::string& filename, const std::string& new_name,
									const std::string& current_set);

	// Stamp Prefabs
	static bool save_stamp_to_file(const std::string& name, const std::string& current_set,
								   const std::vector<uint8_t>& cells, uint32_t width, uint32_t height);
	static bool load_stamp_from_file(const std::string& path_or_name, const std::string& current_set,
									 std::vector<uint8_t>& out_cells, uint32_t& out_width, uint32_t& out_height);
	static bool inspect_stamp_file(const std::string& path_or_name, const std::string& current_set,
								   StampFileInfo& info);
	static std::vector<StampFileInfo> get_stamp_files(const std::string& current_set);
	static bool delete_stamp_file(const std::string& filename, const std::string& current_set);

	// Chunked BWT+RLE helpers
	static void bwt_encode(const uint8_t* in_data, size_t N, std::vector<uint8_t>& out_L, uint16_t& out_primary_id);
	static void bwt_decode(const uint8_t* L, size_t N, uint16_t primary_id, uint8_t* out_data);

	static std::vector<uint8_t> rle_encode(const uint8_t* data, size_t size);
	static std::vector<uint8_t> rle_decode(const uint8_t* data, size_t compressed_size, size_t expected_size);

	static bool write_chunked_data(std::ofstream& file, const uint8_t* data, size_t total_cells);
	static bool read_chunked_data(std::ifstream& file, uint32_t num_blocks, size_t total_cells, uint8_t* out_data);
};