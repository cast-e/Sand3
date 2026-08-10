#include "undo_manager.hpp"

#include "const.hpp"
#include "grid.hpp"
#include "ui.hpp"

std::deque<UndoSnapshot> UndoManager::history;
int UndoManager::current_index = -1;
std::vector<uint8_t> UndoManager::pending_grid_state;

static std::vector<uint8_t> capture_grid_state() {
	std::vector<uint8_t> state(SIM_SIZE);
	for (uint32_t y = 0; y < SIM_HEIGHT; ++y) {
		for (uint32_t x = 0; x < SIM_WIDTH; ++x) {
			state[y * SIM_WIDTH + x] = Grid::get_cell(x, y);
		}
	}
	return state;
}

static void restore_grid_state(const std::vector<uint8_t>& state) {
	if (state.size() != SIM_SIZE)
		return;
	for (uint32_t y = 0; y < SIM_HEIGHT; ++y) {
		for (uint32_t x = 0; x < SIM_WIDTH; ++x) {
			Grid::set_cell(x, y, state[y * SIM_WIDTH + x]);
		}
	}
}

void UndoManager::init() {
	clear();
	push_snapshot("Initial State");
}

void UndoManager::push_snapshot(const std::string& action_name) {
	if (current_index >= 0 && current_index < static_cast<int>(history.size()) - 1) {
		history.erase(history.begin() + current_index + 1, history.end());
	}

	UndoSnapshot snap;
	snap.grid_materials = capture_grid_state();
	snap.materials = MaterialManager::get_materials();
	snap.selected_id = UI::get_selected_id();
	snap.action_name = action_name;

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
	if (history[current_index].action_name == "Resume Simulation") {
		UI::pause_simulation();
	}
	current_index--;
	const auto& snap = history[current_index];
	restore_grid_state(snap.grid_materials);
	MaterialManager::get_materials() = snap.materials;
	MaterialManager::rebuild_compiled_rules();

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
}

void UndoManager::redo() {
	if (!can_redo())
		return;
	current_index++;
	const auto& snap = history[current_index];
	restore_grid_state(snap.grid_materials);
	MaterialManager::get_materials() = snap.materials;
	MaterialManager::rebuild_compiled_rules();

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
}

void UndoManager::clear() {
	history.clear();
	current_index = -1;
	pending_grid_state.clear();
}

void UndoManager::set_pending_grid_snapshot() { pending_grid_state = capture_grid_state(); }

void UndoManager::commit_grid_snapshot_if_changed(const std::string& action_name) {
	if (pending_grid_state.empty())
		return;
	std::vector<uint8_t> current_state = capture_grid_state();
	if (current_state != pending_grid_state) {
		push_snapshot(action_name);
	}
	pending_grid_state.clear();
}
