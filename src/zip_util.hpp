#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ZipUtil {
	uint32_t crc32(const uint8_t* data, size_t length);
	std::vector<uint8_t> create_zip(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files);
	bool extract_zip(const std::string& zip_path, const std::string& dest_dir);
}  // namespace ZipUtil
