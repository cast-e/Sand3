#pragma once

constexpr const unsigned int SIM_WIDTH = 1024;
constexpr const unsigned int SIM_HEIGHT = 1024;
constexpr const unsigned int SIM_SIZE = SIM_WIDTH * SIM_HEIGHT;

constexpr const unsigned char NEIGHBOR_SIZE = 5;
constexpr const unsigned char HALF_NEIGHBOR_SIZE = NEIGHBOR_SIZE / 2;
constexpr const unsigned int NEIGHBOR_COUNT = NEIGHBOR_SIZE * NEIGHBOR_SIZE;

constexpr const unsigned int STRIP_HEIGHT = 16;
constexpr const unsigned int NUM_STRIPS_Y = (SIM_HEIGHT + STRIP_HEIGHT - 1) / STRIP_HEIGHT;

constexpr const char* SETS_DIRECTORY = "./sets/";