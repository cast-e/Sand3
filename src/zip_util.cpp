#include "zip_util.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "miniz.h"

namespace ZipUtil {

	uint32_t crc32(const uint8_t* data, size_t length) {
		return static_cast<uint32_t>(mz_crc32(MZ_CRC32_INIT, data, length));
	}

	std::vector<uint8_t> create_zip(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
		mz_zip_archive zip;
		memset(&zip, 0, sizeof(zip));
		if (!mz_zip_writer_init_heap(&zip, 0, 65536)) {
			return {};
		}

		for (const auto& [fname, content] : files) {
			mz_zip_writer_add_mem(&zip, fname.c_str(), content.data(), content.size(), MZ_DEFAULT_LEVEL);
		}

		void* buf = nullptr;
		size_t size = 0;
		if (!mz_zip_writer_finalize_heap_archive(&zip, &buf, &size)) {
			mz_zip_writer_end(&zip);
			return {};
		}

		std::vector<uint8_t> result(static_cast<const uint8_t*>(buf), static_cast<const uint8_t*>(buf) + size);
		mz_free(buf);
		mz_zip_writer_end(&zip);
		return result;
	}

	bool extract_zip(const std::string& zip_path, const std::string& dest_dir) {
		std::filesystem::path dest(dest_dir);
		std::error_code ec;
		std::filesystem::create_directories(dest, ec);

		std::ifstream in(std::filesystem::path(zip_path), std::ios::binary | std::ios::ate);
		if (!in.is_open()) {
			return false;
		}
		std::streamsize file_size = in.tellg();
		if (file_size <= 0) {
			return false;
		}
		in.seekg(0, std::ios::beg);
		std::vector<uint8_t> zip_data(static_cast<size_t>(file_size));
		if (!in.read(reinterpret_cast<char*>(zip_data.data()), file_size)) {
			return false;
		}
		in.close();

		mz_zip_archive zip_archive;
		memset(&zip_archive, 0, sizeof(zip_archive));
		if (!mz_zip_reader_init_mem(&zip_archive, zip_data.data(), zip_data.size(), 0)) {
			return false;
		}

		mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
		bool all_ok = true;

		for (mz_uint i = 0; i < num_files; ++i) {
			mz_zip_archive_file_stat file_stat;
			if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
				continue;
			}

			std::string filename = file_stat.m_filename;
			if (filename.find("..") != std::string::npos) {
				continue;
			}

			std::filesystem::path out_path = dest / filename;

			if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
				std::filesystem::create_directories(out_path, ec);
				continue;
			}

			if (out_path.has_parent_path()) {
				std::filesystem::create_directories(out_path.parent_path(), ec);
			}

			size_t uncomp_size = 0;
			void* p = mz_zip_reader_extract_to_heap(&zip_archive, i, &uncomp_size, 0);
			if (!p) {
				all_ok = false;
				continue;
			}

			std::ofstream out(out_path, std::ios::binary);
			if (out.is_open()) {
				out.write(reinterpret_cast<const char*>(p), uncomp_size);
				out.close();
			} else {
				all_ok = false;
			}
			mz_free(p);
		}

		mz_zip_reader_end(&zip_archive);
		return all_ok;
	}
}  // namespace ZipUtil