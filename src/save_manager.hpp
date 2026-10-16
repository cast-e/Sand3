#pragma once

#include <cstddef>
#include <cstdint>
#include <fmt/format.h>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "grid.hpp"

enum class LoadPlacement { Center, TopLeft, ResizeGrid };
enum class CompressionQuality : uint8_t { Low = 0, Medium = 1, High = 2 };

inline std::string format_bytes(size_t bytes) {
	if (bytes < 1024) {
		return std::to_string(bytes) + " B";
	} else if (bytes < 1024 * 1024) {
		return fmt::format("{:.1f} KB", static_cast<double>(bytes) / 1024.0);
	} else {
		return fmt::format("{:.2f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
	}
}

struct SaveFileInfo {
	std::string filename;
	std::string name;
	std::string set_name;
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t file_size = 0;
	bool dimensions_differ = false;
	bool is_online = false;
	std::string workshop_id;
	std::string workshop_hash;
	std::string file_hash;
	std::string author;
	uint32_t version = 1;
	std::string forked_from_id;
	std::string forked_from_author;
	uint32_t forked_from_version = 0;
};

struct StampFileInfo {
	std::string filename;
	std::string name;
	std::string set_name;
	uint32_t width = 0;
	uint32_t height = 0;
	uint64_t file_size = 0;
	bool is_online = false;
	std::string workshop_id;
	std::string workshop_hash;
	std::string file_hash;
	std::string author;
	uint32_t version = 1;
	std::string forked_from_id;
	std::string forked_from_author;
	uint32_t forked_from_version = 0;
};

class SaveManager {
public:
	SaveManager() = delete;

	static std::string format_bytes(size_t bytes) {
		return ::format_bytes(bytes);
	}

	static std::string compute_file_hash(const std::string& filepath);
	static std::string get_saves_directory(const std::string& current_set);

	static void set_save_workshop_info(const std::string& name_or_filename, const std::string& current_set,
									   const std::string& workshop_id, const std::string& workshop_hash,
									   const std::string& author, uint32_t version = 1);
	static void clear_save_workshop_info(const std::string& name_or_filename, const std::string& current_set);
	static void set_stamp_workshop_info(const std::string& name_or_filename, const std::string& current_set,
										const std::string& workshop_id, const std::string& workshop_hash,
										const std::string& author, uint32_t version = 1);
	static void clear_stamp_workshop_info(const std::string& name_or_filename, const std::string& current_set);

	static bool is_file_highest_compression(const std::string& filepath);
	static bool is_buffer_highest_compression(const std::vector<uint8_t>& bytes);
	static bool recompress_to_highest(std::vector<uint8_t>& in_out_bytes, const std::string& current_set);

	static bool save_to_file(const std::string& name, const std::string& current_set,
							 CompressionQuality quality = CompressionQuality::High);
	static void save_to_file_async(const std::string& name, const std::string& current_set,
								   CompressionQuality quality,
								   std::function<void(bool success)> callback);
	static void save_to_file_async(const std::string& name, const std::string& current_set,
								   std::function<void(bool success)> callback) {
		save_to_file_async(name, current_set, CompressionQuality::High, std::move(callback));
	}

	static bool load_from_file(const std::string& path_or_name, const std::string& current_set, std::string& loaded_set,
							   LoadPlacement placement = LoadPlacement::Center);
	static void load_from_file_async(const std::string& path_or_name, const std::string& current_set,
									 LoadPlacement placement,
									 std::function<void(bool success, const std::string& loaded_set)> callback);

	static bool inspect_save_file(const std::string& path_or_name, const std::string& current_set, SaveFileInfo& info);
	static std::vector<SaveFileInfo> get_save_files(const std::string& current_set);
	static bool delete_save_file(const std::string& filename, const std::string& current_set);
	static bool duplicate_save_file(const std::string& filename, const std::string& new_name,
									const std::string& current_set);
	static bool migrate_save_file(const std::string& filename, const std::string& source_set,
								  const std::string& target_set);
	static bool fork_save_to_set(const std::string& src_filepath, const std::string& target_set,
								 const std::string& new_name, const std::string& forked_from_id,
								 const std::string& forked_from_author, uint32_t forked_from_version);

	static bool save_stamp_to_file(const std::string& name, const std::string& current_set,
								   const std::vector<Cell>& cells, uint32_t width, uint32_t height,
								   CompressionQuality quality = CompressionQuality::High);
	static bool save_stamp_to_file(const std::string& name, const std::string& current_set,
								   const std::vector<uint8_t>& cells, uint32_t width, uint32_t height,
								   CompressionQuality quality = CompressionQuality::High);
	static void save_stamp_to_file_async(const std::string& name, const std::string& current_set,
										 const std::vector<Cell>& cells, uint32_t width, uint32_t height,
										 CompressionQuality quality,
										 std::function<void(bool success)> callback);
	static void save_stamp_to_file_async(const std::string& name, const std::string& current_set,
										 const std::vector<Cell>& cells, uint32_t width, uint32_t height,
										 std::function<void(bool success)> callback) {
		save_stamp_to_file_async(name, current_set, cells, width, height, CompressionQuality::High, std::move(callback));
	}
	static void save_stamp_to_file_async(const std::string& name, const std::string& current_set,
										 const std::vector<uint8_t>& cells, uint32_t width, uint32_t height,
										 std::function<void(bool success)> callback);

	static bool load_stamp_from_file(const std::string& path_or_name, const std::string& current_set,
									 std::vector<Cell>& out_cells, uint32_t& out_width, uint32_t& out_height);
	static bool load_stamp_from_file(const std::string& path_or_name, const std::string& current_set,
									 std::vector<uint8_t>& out_cells, uint32_t& out_width, uint32_t& out_height);
	static void load_stamp_from_file_async(
		const std::string& path_or_name, const std::string& current_set,
		std::function<void(bool success, const std::vector<Cell>& cells, uint32_t width, uint32_t height)> callback);
	static void load_stamp_from_file_async(
		const std::string& path_or_name, const std::string& current_set,
		std::function<void(bool success, const std::vector<uint8_t>& cells, uint32_t width, uint32_t height)> callback);

	static bool inspect_stamp_file(const std::string& path_or_name, const std::string& current_set,
								   StampFileInfo& info);
	static std::vector<StampFileInfo> get_stamp_files(const std::string& current_set);
	static bool delete_stamp_file(const std::string& filename, const std::string& current_set);
	static bool migrate_stamp_file(const std::string& filename, const std::string& source_set,
								   const std::string& target_set);
	static bool fork_stamp_to_set(const std::string& src_filepath, const std::string& target_set,
								  const std::string& new_name, const std::string& forked_from_id,
								  const std::string& forked_from_author, uint32_t forked_from_version);

	static void estimate_save_size(uint32_t width, uint32_t height, CompressionQuality quality,
								   size_t& out_orig_size, size_t& out_final_size);
	static void estimate_stamp_size(const std::vector<Cell>& cells, uint32_t width, uint32_t height,
									CompressionQuality quality,
									size_t& out_orig_size, size_t& out_final_size);

	static void bwt_encode(const uint8_t* in_data, size_t N, std::vector<uint8_t>& out_L, uint16_t& out_primary_id);
	static void bwt_decode(const uint8_t* L, size_t N, uint16_t primary_id, uint8_t* out_data);

	static std::vector<uint8_t> rle_encode(const uint8_t* data, size_t size);
	static std::vector<uint8_t> rle_decode(const uint8_t* data, size_t compressed_size, size_t expected_size);

	static bool write_chunked_data(std::ofstream& file, const uint8_t* data, size_t total_cells);
	static bool read_chunked_data(std::ifstream& file, uint32_t num_blocks, size_t total_cells, uint8_t* out_data);
};