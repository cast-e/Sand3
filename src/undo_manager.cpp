#include "undo_manager.hpp"

#include "grid.hpp"
#include "ui.hpp"

std::deque<UndoSnapshot> UndoManager::history;
int UndoManager::current_index = -1;
std::vector<uint8_t> UndoManager::pending_grid_state;

void UndoManager::init() {
	clear();
	push_snapshot("Initial State");
}

void UndoManager::push_snapshot(const std::string& action_name) {
	if (current_index >= 0 && current_index < static_cast<int>(history.size()) - 1) {
		history.erase(history.begin() + current_index + 1, history.end());
	}

	UndoSnapshot snap;
	snap.grid_materials = Grid::get_all_cells();
	snap.materials = MaterialManager::get_materials();
	snap.selected_id = UI::get_selected_id();
	snap.action_name = action_name;
	snap.tool_mode = UI::get_tool_mode();
	snap.selection_state = UI::get_selection_state();
	snap.selection_box = UI::get_selection_box();

	history.push_back(snap);
	if (history.size() > MAX_HISTORY) {
		history.pop_front();
	}
	current_index = static_cast<int>(history.size()) - 1;
}

bool UndoManager::can_undo() { return current_index > 0; }

bool UndoManager::can_redo() { return current_index >= 0 && current_index < static_cast<int>(history.size()) - 1; }

void UndoManager::undo() {
	if (!can_undo())
		return;
	UI::pause_simulation();
	current_index--;
	const auto& snap = history[current_index];
	MaterialManager::get_materials() = snap.materials;
	MaterialManager::rebuild_compiled_rules();
	Grid::restore_state(snap.grid_materials);

	uint8_t current_id = UI::get_selected_id();
	bool exists = false;
	for (const auto& m : snap.materials) {
		if (m.id == current_id) {
			exists = true;
			break;
		}
	}
	if (!exists) {
		UI::set_selected_id(snap.selected_id);
	}
	UI::restore_selection_state(snap.tool_mode, snap.selection_state, snap.selection_box);
}

void UndoManager::redo() {
	if (!can_redo())
		return;
	UI::pause_simulation();
	current_index++;
	const auto& snap = history[current_index];
	MaterialManager::get_materials() = snap.materials;
	MaterialManager::rebuild_compiled_rules();
	Grid::restore_state(snap.grid_materials);

	uint8_t current_id = UI::get_selected_id();
	bool exists = false;
	for (const auto& m : snap.materials) {
		if (m.id == current_id) {
			exists = true;
			break;
		}
	}
	if (!exists) {
		UI::set_selected_id(snap.selected_id);
	}
	UI::restore_selection_state(snap.tool_mode, snap.selection_state, snap.selection_box);
}

void UndoManager::clear() {
	history.clear();
	current_index = -1;
	pending_grid_state.clear();
}

void UndoManager::set_pending_grid_snapshot() { pending_grid_state = Grid::get_all_cells(); }

void UndoManager::commit_grid_snapshot_if_changed(const std::string& action_name) {
	if (pending_grid_state.empty())
		return;
	std::vector<uint8_t> current_state = Grid::get_all_cells();
	if (current_state != pending_grid_state) {
		push_snapshot(action_name);
	}
	pending_grid_state.clear();
}