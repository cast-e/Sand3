#pragma once

#include <cstdint>

constexpr const uint32_t DEFAULT_SIM_WIDTH = 1024;
constexpr const uint32_t DEFAULT_SIM_HEIGHT = 1024;

constexpr const uint32_t NEIGHBOR_SIZE = 5;
constexpr const uint32_t HALF_NEIGHBOR_SIZE = NEIGHBOR_SIZE / 2;
constexpr const uint32_t NEIGHBOR_COUNT = NEIGHBOR_SIZE * NEIGHBOR_SIZE;

constexpr const uint32_t STRIP_HEIGHT = 16;

constexpr const char* SETS_DIRECTORY = "./sets/";