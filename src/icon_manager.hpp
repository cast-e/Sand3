#pragma once

#include <SDL3/SDL.h>

enum class IconID {
	Brush,
	Select,
	Copy,
	Cut,
	Paste,
	Delete,
	Cross,
	Fill,
	RotateCW,
	RotateCCW,
	Save,
	Folder,
	Pause,
	Play,
	Step,
	Clear,
	Add,
	Refresh,
	Count
};

class IconManager {
public:
	static void init();
	static void shutdown();
	static SDL_Texture* get(IconID id);
};
