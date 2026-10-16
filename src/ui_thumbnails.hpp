#pragma once

#include <string>

class UIThumbnails {
public:
	UIThumbnails() = delete;

	static std::string generate_thumbnail_base64(int type_idx, const std::string& target_set,
												 const std::string& target_file = "");
};