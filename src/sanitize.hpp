#pragma once

#include <string>

static inline std::string trim(std::string_view str) {
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return "";
	size_t last = str.find_last_not_of(" \t\r\n");
	return std::string(str.substr(first, (last - first + 1)));
}

static inline std::string sanitize_name(std::string_view name) {
	std::string trimmed = trim(name);
	std::string result;
	result.reserve(trimmed.size());
	for (char c : trimmed) {
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
			continue;
		}
		result.push_back(c);
	}
	return trim(result);
}