#pragma once

#include <string>

class UIWorkshop {
public:
	UIWorkshop() = delete;

	static void render();
	static void refresh_workshop_items();
	static void remove_workshop_item(const std::string& item_id);
	static void validate_online_items(bool force = false);
	static void handle_uri(const std::string& uri);
	static void check_item_existence(const std::string& item_id, const std::string& type,
									 const std::string& name_or_file, const std::string& set_context = "",
									 bool force = false);
};