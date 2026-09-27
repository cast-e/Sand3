#pragma once

#include <functional>

#include "workshop_client.hpp"

class WorkshopItemEditor {
public:
	WorkshopItemEditor() = delete;

	static void open_edit_modal(const WorkshopItemClient& item, std::function<void()> on_success = nullptr);
	static void open_update_modal(const WorkshopItemClient& item, std::function<void()> on_success = nullptr);
	static void open_delete_modal(const WorkshopItemClient& item, std::function<void()> on_success = nullptr);

	static void render();

private:
	static void render_edit_modal();
	static void render_update_modal();
	static void render_delete_modal();
};
