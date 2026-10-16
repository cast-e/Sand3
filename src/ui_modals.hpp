#pragma once

#include <functional>
#include <string>
#include <vector>

#include "save_manager.hpp"
#include "workshop_client.hpp"

enum class ModalType {
	None,
	ExitConfirmation,
	UnsavedChangesSwitch,
	CreateSet,
	DeleteSetConfirmation,
	ForkSet,
	CanvasResizeConfirmation,
	DiffSizeSave,
	EmptyRulePerformanceWarning,
	LargeNeighborhoodWarning,
	SetOverrideWarning,
	WorkshopAuth,
	WorkshopReport,
	WorkshopShare,
	WorkshopVersionHistory,
	MigrateForkedSet,
	ForkItem
};

class UIModals {
public:
	UIModals() = delete;

	static void open_exit();
	static void open_unsaved_switch(const std::string& target_set = "", const std::string& target_save = "");
	static void open_create_set(bool duplicate = false);
	static void open_delete_set();
	static void open_fork_set();
	static void open_canvas_resize(int target_w, int target_h, bool preserve_content);
	static void open_diff_size_save(const SaveFileInfo& save_info);
	static void open_empty_rule_warning(uint8_t inherit_id = 255);
	static void open_large_neighborhood_warning(int rule_idx, uint8_t mat_id, int side);
	static void open_set_override_warning(const WorkshopItemClient& item, uint32_t local_version,
										  uint32_t target_version, bool is_update, bool is_version_load, bool is_switch,
										  std::function<void()> on_confirm,
										  std::function<void()> on_keep_local = nullptr);
	static void open_workshop_auth(bool is_register = false);
	static void open_workshop_report(const std::string& item_id, const std::string& item_title);
	static void open_workshop_share_set(const std::string& set_name);
	static void open_workshop_share_save(const std::string& filename, const std::string& name,
										 const std::string& parent_set, int w, int h);
	static void open_workshop_share_stamp(const std::string& filename, const std::string& name,
										  const std::string& parent_set, int w, int h);
	static void open_workshop_share_theme();
	static void open_workshop_versions(const WorkshopItemClient& item);
	static void open_migrate(bool is_save, const std::string& filename, const std::string& item_name,
							 const std::string& source_set, const std::vector<std::string>& forked_sets);
	static void open_fork_item(const WorkshopItemClient& item, int version = 0);

	static void close_current();
	static bool has_active_modal();

	static void render();
};