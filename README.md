# Sand3
 Fast celullar automaton with fully customizable materials and rules.

![Sand3](./bin/sand3.png)

## Features

- Material editor for creating up to 255 materials with custom color and rules.
- 5x5 neighborhood rules for complex behaviours and patterns.
- Smooth zoom and panning controls for easy navigation.
- Optimized multithreaded and single-threaded simulation for best performance on any device.
- Multiplatform support (Windows, Linux).
- Step-by-step simulation for debugging rules.
- Helpful tooltips and shortcuts in the GUI.
- Standalone executable for easy portability.
- Compressed save files using a BWT + RLE compression algorithm.

## Building

### Prerequisites

Install a C++20 compatible compiler (GCC recommended) and CMake.

Make sure to update all submodules:

```bash
git submodule update --init --recursive
```

## Using CMake

### Linux Building

#### Native compilation using GCC

To build as a Release (default):

```bash
cmake -B build --preset linux-gcc
cmake --build build
```

#### Windows cross-compilation using MinGW-w64

```bash
cmake -B build --preset windows-mingw
cmake --build build
```

### Windows Building

#### Native compilation using MSYS2 MinGW-w64

```bash
cmake -B build --preset windows-mingw
cmake --build build
```

## Running

Just run it from the "./bin" folder.

```bash
cd ./bin
./Sand3
```

## Dependencies

All dependencies are included as a submodule in the "third-party/" directory.

- [fmt](https://github.com/fmtlib/fmt) - Fast formatting library.
- [nlohmann_json](https://github.com/nlohmann/json) - Modern JSON for C++.
- [SDL3](https://github.com/libsdl-org/SDL) - Simple DirectMedia Layer.
- [imgui](https://github.com/ocornut/imgui) - Dear ImGui.