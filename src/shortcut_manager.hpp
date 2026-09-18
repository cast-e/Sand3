#pragma once

#include <string>
#include <vector>
#include <imgui.h>

enum class ShortcutAction {
	ToggleSimulation,
	StepFrame,
	ClearGrid,
	CameraUp,
	CameraLeft,
	CameraDown,
	CameraRight,
	ZoomIn,
	ZoomOut,
	QuickSelect1,
	QuickSelect2,
	QuickSelect3,
	QuickSelect4,
	QuickSelect5,
	QuickSelect6,
	QuickSelect7,
	QuickSelect8,
	QuickSelect9,
	ToggleCompact,
	Undo,
	Redo,
	Fullscreen,
	CancelOrQuit,
	ToolBrush,
	ToolSelect,
	Copy,
	Cut,
	Paste,
	Fill,
	Delete,
	RotateCW,
	RotateCCW,
	BrushShape,
	Count
};

struct ShortcutDef {
	ShortcutAction action;
	std::string id;           // identifier in config.ini, e.g. "toggle_simulation"
	std::string display_name; // human-readable name, e.g. "Toggle Simulation"
	std::string category;     // "Simulation", "Camera", "General", "Tools & Selection", "Brush"
	std::string default_key;  // default key string, e.g. "Space"
	std::string current_key;  // configured key string, e.g. "Space"
	ImGuiKey key = ImGuiKey_None;
	bool ctrl = false;
	bool shift = false;
	bool alt = false;
};

class ShortcutManager {
public:
	static void init();

	static bool is_action_pressed(ShortcutAction action, bool repeat = false);
	static bool is_action_down(ShortcutAction action);

	static const ShortcutDef& get(ShortcutAction action);
	static std::string get_key_string(ShortcutAction action);
	static bool set_key_string(ShortcutAction action, const std::string& key_str);
	static void reset_to_default(ShortcutAction action);
	static void reset_all_to_defaults();

	static bool is_modified(ShortcutAction action);
	static bool any_modified();

	static void load_from_config(const std::string& id, const std::string& key_str);
	static const std::vector<ShortcutDef>& get_all();

	// String parsing & formatting helpers
	static bool parse_key_combo(const std::string& str, ImGuiKey& out_key, bool& out_ctrl, bool& out_shift, bool& out_alt);
	static std::string format_key_combo(ImGuiKey key, bool ctrl, bool shift, bool alt);
	static ImGuiKey string_to_imgui_key(const std::string& name);
	static std::string imgui_key_to_string(ImGuiKey key);
};
