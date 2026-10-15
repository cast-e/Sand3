#include "undo_manager.hpp"

#include "grid.hpp"
#include "ui.hpp"

std::deque<UndoSnapshot> UndoManager::history;
int UndoManager::current_index = -1;
std::vector<uint8_t> UndoManager::pending_grid_state;
std::vector<Cell> UndoManager::pending_grid_cells;

void UndoManager::init() {
	clear();
	push_snapshot("Initial State");
}

void UndoManager::push_snapshot(const std::string& action_name) {
	if (action_name != "Step Simulation") {
		UI::reset_stepping_sequence();
	}

	if (current_index >= 0 && current_index < static_cast<int>(history.size()) - 1) {
		history.erase(history.begin() + current_index + 1, history.end());
	}

	UndoSnapshot snap;
	snap.grid_materials = Grid::get_all_cells();
	snap.grid_cells = Grid::get_all_cells_full();
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
	UI::reset_stepping_sequence();
	UI::pause_simulation();
	current_index--;
	const auto& snap = history[current_index];
	MaterialManager::get_materials() = snap.materials;
	MaterialManager::rebuild_compiled_rules();
	if (!snap.grid_cells.empty()) {
		Grid::restore_state_full(snap.grid_cells);
	} else {
		Grid::restore_state(snap.grid_materials);
	}

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
	UI::reset_stepping_sequence();
	UI::pause_simulation();
	current_index++;
	const auto& snap = history[current_index];
	MaterialManager::get_materials() = snap.materials;
	MaterialManager::rebuild_compiled_rules();
	if (!snap.grid_cells.empty()) {
		Grid::restore_state_full(snap.grid_cells);
	} else {
		Grid::restore_state(snap.grid_materials);
	}

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
	UI::reset_stepping_sequence();
	history.clear();
	current_index = -1;
	pending_grid_state.clear();
	pending_grid_cells.clear();
}

void UndoManager::set_pending_grid_snapshot() {
	UI::reset_stepping_sequence();
	pending_grid_state = Grid::get_all_cells();
	pending_grid_cells = Grid::get_all_cells_full();
}

void UndoManager::commit_grid_snapshot_if_changed(const std::string& action_name) {
	if (pending_grid_cells.empty() && pending_grid_state.empty())
		return;
	bool changed = false;
	if (!pending_grid_cells.empty()) {
		std::vector<Cell> current_cells = Grid::get_all_cells_full();
		if (current_cells.size() != pending_grid_cells.size()) {
			changed = true;
		} else {
			for (size_t i = 0; i < current_cells.size(); ++i) {
				if (current_cells[i].material != pending_grid_cells[i].material ||
					current_cells[i].a_aux != pending_grid_cells[i].a_aux ||
					current_cells[i].a_prim != pending_grid_cells[i].a_prim) {
					changed = true;
					break;
				}
			}
		}
	} else {
		std::vector<uint8_t> current_state = Grid::get_all_cells();
		changed = (current_state != pending_grid_state);
	}
	if (changed) {
		push_snapshot(action_name);
	}
	pending_grid_state.clear();
	pending_grid_cells.clear();
}