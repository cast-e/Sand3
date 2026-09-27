#pragma once

#include <cstdint>
#include <string>

std::string sha256(const std::string& input);
std::string sha256(const uint8_t* data, size_t len);