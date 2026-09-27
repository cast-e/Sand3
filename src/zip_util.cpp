#include "zip_util.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace ZipUtil {

	uint32_t crc32(const uint8_t* data, size_t length) {
		uint32_t crc = 0xFFFFFFFF;
		for (size_t i = 0; i < length; ++i) {
			crc ^= data[i];
			for (int j = 0; j < 8; ++j) {
				crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
			}
		}
		return ~crc;
	}

	namespace {
		void write_u16(std::vector<uint8_t>& buf, uint16_t val) {
			buf.push_back(static_cast<uint8_t>(val & 0xFF));
			buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
		}

		void write_u32(std::vector<uint8_t>& buf, uint32_t val) {
			buf.push_back(static_cast<uint8_t>(val & 0xFF));
			buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
			buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
			buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
		}
	}  // namespace

	std::vector<uint8_t> create_zip(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
		std::vector<uint8_t> zip_data;
		std::vector<uint32_t> local_header_offsets;
		std::vector<uint32_t> crcs;

		for (const auto& [fname, content] : files) {
			uint32_t offset = static_cast<uint32_t>(zip_data.size());
			local_header_offsets.push_back(offset);

			uint32_t c = crc32(content.data(), content.size());
			crcs.push_back(c);

			write_u32(zip_data, 0x04034b50);
			write_u16(zip_data, 20);									 // version needed to extract (2.0)
			write_u16(zip_data, 0);										 // general purpose bit flag
			write_u16(zip_data, 0);										 // compression method (0 = stored)
			write_u16(zip_data, 0);										 // last mod file time
			write_u16(zip_data, 0);										 // last mod file date
			write_u32(zip_data, c);										 // crc-32
			write_u32(zip_data, static_cast<uint32_t>(content.size()));	 // compressed size
			write_u32(zip_data, static_cast<uint32_t>(content.size()));	 // uncompressed size
			write_u16(zip_data, static_cast<uint16_t>(fname.size()));	 // file name length
			write_u16(zip_data, 0);										 // extra field length

			for (char ch : fname)
				zip_data.push_back(static_cast<uint8_t>(ch));

			zip_data.insert(zip_data.end(), content.begin(), content.end());
		}

		uint32_t cd_offset = static_cast<uint32_t>(zip_data.size());

		for (size_t i = 0; i < files.size(); ++i) {
			const auto& [fname, content] = files[i];
			uint32_t c = crcs[i];
			uint32_t offset = local_header_offsets[i];

			write_u32(zip_data, 0x02014b50);
			write_u16(zip_data, 20);  // version made by
			write_u16(zip_data, 20);  // version needed to extract
			write_u16(zip_data, 0);	  // general purpose bit flag
			write_u16(zip_data, 0);	  // compression method
			write_u16(zip_data, 0);	  // last mod file time
			write_u16(zip_data, 0);	  // last mod file date
			write_u32(zip_data, c);	  // crc-32
			write_u32(zip_data, static_cast<uint32_t>(content.size()));
			write_u32(zip_data, static_cast<uint32_t>(content.size()));
			write_u16(zip_data, static_cast<uint16_t>(fname.size()));
			write_u16(zip_data, 0);		  // extra field length
			write_u16(zip_data, 0);		  // file comment length
			write_u16(zip_data, 0);		  // disk number start
			write_u16(zip_data, 0);		  // internal file attributes
			write_u32(zip_data, 0);		  // external file attributes
			write_u32(zip_data, offset);  // relative offset of local header

			for (char ch : fname)
				zip_data.push_back(static_cast<uint8_t>(ch));
		}

		uint32_t cd_size = static_cast<uint32_t>(zip_data.size() - cd_offset);

		write_u32(zip_data, 0x06054b50);
		write_u16(zip_data, 0);									   // disk number
		write_u16(zip_data, 0);									   // disk with central directory
		write_u16(zip_data, static_cast<uint16_t>(files.size()));  // entries on this disk
		write_u16(zip_data, static_cast<uint16_t>(files.size()));  // total entries
		write_u32(zip_data, cd_size);							   // size of central directory
		write_u32(zip_data, cd_offset);							   // offset of central directory
		write_u16(zip_data, 0);									   // comment length

		return zip_data;
	}

	bool extract_zip(const std::string& zip_path, const std::string& dest_dir) {
		std::filesystem::create_directories(dest_dir);
		std::string cmd = "unzip -q -o \"" + zip_path + "\" -d \"" + dest_dir + "\" 2>/dev/null";
		int res = std::system(cmd.c_str());
		if (res == 0)
			return true;

		cmd = "tar -xf \"" + zip_path + "\" -C \"" + dest_dir + "\" 2>/dev/null";
		return (std::system(cmd.c_str()) == 0);
	}
}  // namespace ZipUtil
