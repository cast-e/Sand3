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
	World,
	Heart,
	Star,
	Report,
	Package,
	Transmit,
	Magnifier,
	ArrowDown,
	User,
	Tag,
	Edit,
	Update,
	Accept,
	Information,
	Warning,
	Count
};

class IconManager {
public:
	static void init();
	static void shutdown();
	static SDL_Texture* get(IconID id);
};
