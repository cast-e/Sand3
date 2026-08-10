#pragma once

#include <deque>
#include <stdint.h>

#include <string>
#include <vector>

#include "material_manager.hpp"

struct UndoSnapshot {
	std::vector<uint8_t> grid_materials;
	std::vector<MaterialDefinition> materials;
	int selected_id = -1;
	std::string action_name;
};

class UndoManager {
public:
	UndoManager() = delete;

	static void init();
	static void push_snapshot(const std::string& action_name = "");
	static bool can_undo();
	static bool can_redo();
	static void undo();
	static void redo();
	static void clear();

	static void set_pending_grid_snapshot();
	static void commit_grid_snapshot_if_changed(const std::string& action_name);

private:
	static constexpr size_t MAX_HISTORY = 50;
	static std::deque<UndoSnapshot> history;
	static int current_index;
	static std::vector<uint8_t> pending_grid_state;
};
