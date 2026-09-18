#include "shortcut_manager.hpp"

#include <cctype>
#include <sstream>

static std::vector<ShortcutDef> s_shortcuts;
static bool s_initialized = false;

static std::string to_lower_str(std::string_view s) {
	std::string res;
	res.reserve(s.size());
	for (char c : s) {
		res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
	}
	return res;
}

static std::string trim_str(std::string_view s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string_view::npos)
		return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return std::string(s.substr(start, end - start + 1));
}

ImGuiKey ShortcutManager::string_to_imgui_key(const std::string& name_raw) {
	std::string name = to_lower_str(trim_str(name_raw));
	if (name.empty())
		return ImGuiKey_None;

	// Single letter A-Z
	if (name.length() == 1 && name[0] >= 'a' && name[0] <= 'z') {
		return static_cast<ImGuiKey>(ImGuiKey_A + (name[0] - 'a'));
	}
	// Single digit 0-9
	if (name.length() == 1 && name[0] >= '0' && name[0] <= '9') {
		return static_cast<ImGuiKey>(ImGuiKey_0 + (name[0] - '0'));
	}

	// Function keys F1-F12
	if (name.length() >= 2 && name[0] == 'f' && std::isdigit(name[1])) {
		try {
			int f_num = std::stoi(name.substr(1));
			if (f_num >= 1 && f_num <= 12) {
				return static_cast<ImGuiKey>(ImGuiKey_F1 + (f_num - 1));
			}
		} catch (...) {}
	}

	// Named keys
	if (name == "space")
		return ImGuiKey_Space;
	if (name == "enter" || name == "return")
		return ImGuiKey_Enter;
	if (name == "escape" || name == "esc")
		return ImGuiKey_Escape;
	if (name == "tab")
		return ImGuiKey_Tab;
	if (name == "backspace")
		return ImGuiKey_Backspace;
	if (name == "delete" || name == "del")
		return ImGuiKey_Delete;
	if (name == "insert" || name == "ins")
		return ImGuiKey_Insert;
	if (name == "home")
		return ImGuiKey_Home;
	if (name == "end")
		return ImGuiKey_End;
	if (name == "pageup" || name == "page_up" || name == "pgup")
		return ImGuiKey_PageUp;
	if (name == "pagedown" || name == "page_down" || name == "pgdn")
		return ImGuiKey_PageDown;
	if (name == "left" || name == "leftarrow")
		return ImGuiKey_LeftArrow;
	if (name == "right" || name == "rightarrow")
		return ImGuiKey_RightArrow;
	if (name == "up" || name == "uparrow")
		return ImGuiKey_UpArrow;
	if (name == "down" || name == "downarrow")
		return ImGuiKey_DownArrow;
	if (name == "keypad+" || name == "keypadadd")
		return ImGuiKey_KeypadAdd;
	if (name == "keypad-" || name == "keypadsubtract")
		return ImGuiKey_KeypadSubtract;
	if (name == "+" || name == "plus" || name == "equal")
		return ImGuiKey_Equal;
	if (name == "-" || name == "minus")
		return ImGuiKey_Minus;
	if (name == "[" || name == "leftbracket")
		return ImGuiKey_LeftBracket;
	if (name == "]" || name == "rightbracket")
		return ImGuiKey_RightBracket;
	if (name == ";" || name == "semicolon")
		return ImGuiKey_Semicolon;
	if (name == "'" || name == "apostrophe")
		return ImGuiKey_Apostrophe;
	if (name == "," || name == "comma")
		return ImGuiKey_Comma;
	if (name == "." || name == "period")
		return ImGuiKey_Period;
	if (name == "/" || name == "slash")
		return ImGuiKey_Slash;
	if (name == "`" || name == "grave")
		return ImGuiKey_GraveAccent;

	return ImGuiKey_None;
}

std::string ShortcutManager::imgui_key_to_string(ImGuiKey key) {
	if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
		return std::string(1, static_cast<char>('A' + (key - ImGuiKey_A)));
	}
	if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
		return std::string(1, static_cast<char>('0' + (key - ImGuiKey_0)));
	}
	if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) {
		return "F" + std::to_string(key - ImGuiKey_F1 + 1);
	}

	switch (key) {
		case ImGuiKey_Space:
			return "Space";
		case ImGuiKey_Enter:
			return "Enter";
		case ImGuiKey_Escape:
			return "Escape";
		case ImGuiKey_Tab:
			return "Tab";
		case ImGuiKey_Backspace:
			return "Backspace";
		case ImGuiKey_Delete:
			return "Delete";
		case ImGuiKey_Insert:
			return "Insert";
		case ImGuiKey_Home:
			return "Home";
		case ImGuiKey_End:
			return "End";
		case ImGuiKey_PageUp:
			return "PageUp";
		case ImGuiKey_PageDown:
			return "PageDown";
		case ImGuiKey_LeftArrow:
			return "Left";
		case ImGuiKey_RightArrow:
			return "Right";
		case ImGuiKey_UpArrow:
			return "Up";
		case ImGuiKey_DownArrow:
			return "Down";
		case ImGuiKey_KeypadAdd:
			return "Keypad+";
		case ImGuiKey_KeypadSubtract:
			return "Keypad-";
		case ImGuiKey_Equal:
			return "+";
		case ImGuiKey_Minus:
			return "-";
		default:
			break;
	}

	const char* name = ImGui::GetKeyName(key);
	return name ? std::string(name) : "";
}

bool ShortcutManager::parse_key_combo(const std::string& str, ImGuiKey& out_key, bool& out_ctrl, bool& out_shift,
									  bool& out_alt) {
	out_key = ImGuiKey_None;
	out_ctrl = false;
	out_shift = false;
	out_alt = false;

	if (str.empty())
		return false;

	std::stringstream ss(str);
	std::string token;
	while (std::getline(ss, token, '+')) {
		token = trim_str(token);
		std::string ltoken = to_lower_str(token);
		if (ltoken == "ctrl" || ltoken == "control") {
			out_ctrl = true;
		} else if (ltoken == "shift") {
			out_shift = true;
		} else if (ltoken == "alt") {
			out_alt = true;
		} else {
			ImGuiKey k = string_to_imgui_key(token);
			if (k != ImGuiKey_None) {
				out_key = k;
			}
		}
	}

	return out_key != ImGuiKey_None;
}

std::string ShortcutManager::format_key_combo(ImGuiKey key, bool ctrl, bool shift, bool alt) {
	if (key == ImGuiKey_None)
		return "";
	std::string res;
	if (ctrl)
		res += "Ctrl+";
	if (shift)
		res += "Shift+";
	if (alt)
		res += "Alt+";
	res += imgui_key_to_string(key);
	return res;
}

void ShortcutManager::init() {
	if (s_initialized)
		return;

	auto add = [](ShortcutAction act, const std::string& id, const std::string& name, const std::string& cat,
				  const std::string& def) {
		ShortcutDef s;
		s.action = act;
		s.id = id;
		s.display_name = name;
		s.category = cat;
		s.default_key = def;
		s.current_key = def;
		parse_key_combo(def, s.key, s.ctrl, s.shift, s.alt);
		s_shortcuts.push_back(s);
	};

	s_shortcuts.clear();

	// Simulation
	add(ShortcutAction::ToggleSimulation, "toggle_simulation", "Toggle Simulation", "Simulation", "Space");
	add(ShortcutAction::StepFrame, "step_frame", "Step Frame (Paused)", "Simulation", "F");
	add(ShortcutAction::ClearGrid, "clear_grid", "Clear / Reset Grid", "Simulation", "R");

	// Camera
	add(ShortcutAction::CameraUp, "camera_up", "Move Camera Up", "Camera", "W");
	add(ShortcutAction::CameraLeft, "camera_left", "Move Camera Left", "Camera", "A");
	add(ShortcutAction::CameraDown, "camera_down", "Move Camera Down", "Camera", "S");
	add(ShortcutAction::CameraRight, "camera_right", "Move Camera Right", "Camera", "D");
	add(ShortcutAction::ZoomIn, "zoom_in", "Zoom In", "Camera", "PageUp");
	add(ShortcutAction::ZoomOut, "zoom_out", "Zoom Out", "Camera", "PageDown");

	// General
	add(ShortcutAction::QuickSelect1, "quick_select_1", "Select Material Slot 1", "General", "1");
	add(ShortcutAction::QuickSelect2, "quick_select_2", "Select Material Slot 2", "General", "2");
	add(ShortcutAction::QuickSelect3, "quick_select_3", "Select Material Slot 3", "General", "3");
	add(ShortcutAction::QuickSelect4, "quick_select_4", "Select Material Slot 4", "General", "4");
	add(ShortcutAction::QuickSelect5, "quick_select_5", "Select Material Slot 5", "General", "5");
	add(ShortcutAction::QuickSelect6, "quick_select_6", "Select Material Slot 6", "General", "6");
	add(ShortcutAction::QuickSelect7, "quick_select_7", "Select Material Slot 7", "General", "7");
	add(ShortcutAction::QuickSelect8, "quick_select_8", "Select Material Slot 8", "General", "8");
	add(ShortcutAction::QuickSelect9, "quick_select_9", "Select Material Slot 9", "General", "9");
	add(ShortcutAction::ToggleCompact, "toggle_compact", "Toggle Compact UI", "General", "V");
	add(ShortcutAction::Undo, "undo", "Undo Last Action", "General", "Ctrl+Z");
	add(ShortcutAction::Redo, "redo", "Redo Last Action", "General", "Ctrl+Y");
	add(ShortcutAction::Fullscreen, "fullscreen", "Toggle Fullscreen", "General", "F11");
	add(ShortcutAction::CancelOrQuit, "cancel_or_quit", "Cancel / Deselect / Quit", "General", "Escape");

	// Tools & Selection
	add(ShortcutAction::ToolBrush, "tool_brush", "Switch to Brush Tool", "Tools & Selection", "B");
	add(ShortcutAction::ToolSelect, "tool_select", "Switch to Selection Tool", "Tools & Selection", "C");
	add(ShortcutAction::Copy, "copy", "Copy Selection", "Tools & Selection", "Ctrl+C");
	add(ShortcutAction::Cut, "cut", "Cut Selection", "Tools & Selection", "Ctrl+X");
	add(ShortcutAction::Paste, "paste", "Paste Clipboard", "Tools & Selection", "Ctrl+V");
	add(ShortcutAction::Fill, "fill", "Fill Selection with Material", "Tools & Selection", "Ctrl+F");
	add(ShortcutAction::Delete, "delete", "Delete Selected Cells", "Tools & Selection", "Delete");
	add(ShortcutAction::RotateCW, "rotate_cw", "Rotate 90° Clockwise", "Tools & Selection", "Q");
	add(ShortcutAction::RotateCCW, "rotate_ccw", "Rotate 90° Counter-Clockwise", "Tools & Selection", "E");

	// Brush
	add(ShortcutAction::BrushShape, "brush_shape", "Toggle Brush Shape", "Brush", "T");

	s_initialized = true;
}

const ShortcutDef& ShortcutManager::get(ShortcutAction action) {
	init();
	size_t idx = static_cast<size_t>(action);
	if (idx < s_shortcuts.size()) {
		return s_shortcuts[idx];
	}
	static ShortcutDef dummy{};
	return dummy;
}

std::string ShortcutManager::get_key_string(ShortcutAction action) { return get(action).current_key; }

bool ShortcutManager::set_key_string(ShortcutAction action, const std::string& key_str) {
	init();
	size_t idx = static_cast<size_t>(action);
	if (idx >= s_shortcuts.size())
		return false;

	ImGuiKey k;
	bool c, s, a;
	if (parse_key_combo(key_str, k, c, s, a)) {
		s_shortcuts[idx].key = k;
		s_shortcuts[idx].ctrl = c;
		s_shortcuts[idx].shift = s;
		s_shortcuts[idx].alt = a;
		s_shortcuts[idx].current_key = format_key_combo(k, c, s, a);
		return true;
	}
	return false;
}

void ShortcutManager::reset_to_default(ShortcutAction action) {
	init();
	size_t idx = static_cast<size_t>(action);
	if (idx < s_shortcuts.size()) {
		set_key_string(action, s_shortcuts[idx].default_key);
	}
}

void ShortcutManager::reset_all_to_defaults() {
	init();
	for (auto& s : s_shortcuts) {
		set_key_string(s.action, s.default_key);
	}
}

bool ShortcutManager::is_modified(ShortcutAction action) {
	const auto& def = get(action);
	return def.current_key != def.default_key;
}

bool ShortcutManager::any_modified() {
	init();
	for (const auto& s : s_shortcuts) {
		if (s.current_key != s.default_key)
			return true;
	}
	return false;
}

void ShortcutManager::load_from_config(const std::string& id, const std::string& key_str) {
	init();
	for (auto& s : s_shortcuts) {
		if (s.id == id) {
			set_key_string(s.action, key_str);
			break;
		}
	}
}

const std::vector<ShortcutDef>& ShortcutManager::get_all() {
	init();
	return s_shortcuts;
}

bool ShortcutManager::is_action_pressed(ShortcutAction action, bool repeat) {
	const auto& def = get(action);
	if (def.key == ImGuiKey_None)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput)
		return false;

	if (def.ctrl != io.KeyCtrl)
		return false;
	if (def.shift != io.KeyShift)
		return false;
	if (def.alt != io.KeyAlt)
		return false;

	if (ImGui::IsKeyPressed(def.key, repeat))
		return true;

	// Redo fallback (Ctrl+Shift+Z) when using default Ctrl+Y
	if (action == ShortcutAction::Redo && !is_modified(action)) {
		if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, repeat)) {
			return true;
		}
	}

	// ClearGrid fallback (Ctrl+Shift+Delete) when using default R
	if (action == ShortcutAction::ClearGrid && !is_modified(action)) {
		if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
			return true;
		}
	}

	// Zoom fallbacks (Keypad+ / Keypad-) when using default PageUp / PageDown
	if (action == ShortcutAction::ZoomIn && !is_modified(action)) {
		if (ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, repeat))
			return true;
	}
	if (action == ShortcutAction::ZoomOut && !is_modified(action)) {
		if (ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, repeat))
			return true;
	}

	return false;
}

bool ShortcutManager::is_action_down(ShortcutAction action) {
	const auto& def = get(action);
	if (def.key == ImGuiKey_None)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput)
		return false;

	if (def.ctrl != io.KeyCtrl)
		return false;
	// Allow holding Shift for fast pan on camera keys if shift is not explicitly required
	if (def.shift && !io.KeyShift)
		return false;
	if (def.alt != io.KeyAlt)
		return false;

	return ImGui::IsKeyDown(def.key);
}
