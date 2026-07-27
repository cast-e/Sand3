#pragma once

constexpr const uint32_t SIM_WIDTH = 1024;
constexpr const uint32_t SIM_HEIGHT = 1024;
constexpr const uint32_t SIM_SIZE = SIM_WIDTH * SIM_HEIGHT;

constexpr const uint8_t NEIGHBOR_SIZE = 5;
constexpr const uint8_t HALF_NEIGHBOR_SIZE = NEIGHBOR_SIZE / 2;
constexpr const uint32_t NEIGHBOR_COUNT = NEIGHBOR_SIZE * NEIGHBOR_SIZE;

constexpr const uint32_t STRIP_HEIGHT = 16;
constexpr const uint32_t NUM_STRIPS_Y = (SIM_HEIGHT + STRIP_HEIGHT - 1) / STRIP_HEIGHT;

constexpr const char* SETS_DIRECTORY = "./sets/";