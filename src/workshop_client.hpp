#pragma once

#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <vector>

struct WorkshopItemClient {
	std::string id;
	std::string type;  // "set", "save", "stamp", "theme"
	std::string title;
	std::string description;
	std::string author;
	std::string parent_set_id;
	std::string parent_set_title;
	int version = 1;
	std::string set_hash;
	std::string thumbnail_path;
	std::string user_id;
	int child_saves_count = 0;
	int child_stamps_count = 0;
	int file_size = 0;
	int likes_count = 0;
	int favorites_count = 0;
	int downloads_count = 0;
	bool is_liked = false;
	bool is_favorited = false;
	std::string created_at;
	nlohmann::json meta;
};

class WorkshopClient {
public:
	WorkshopClient() = delete;

	static void init();
	static void shutdown();
	static void update();

	static void set_base_url(const std::string& url);
	static const std::string& get_base_url();

	static std::string get_client_uuid();

	static bool is_logged_in();
	static const std::string& get_logged_in_username();
	static const std::string& get_auth_token();
	static void set_auth_token(const std::string& token);
	static void login(const std::string& username, const std::string& password,
					  std::function<void(bool success, const std::string& error)> callback);
	static void register_user(const std::string& username, const std::string& password,
							  std::function<void(bool success, const std::string& error)> callback);
	static void check_auth(std::function<void(bool success, const std::string& username)> callback);
	static void logout();

	static void
	fetch_items(const std::string& type, const std::string& sort, const std::string& query,
				std::function<void(bool success, const std::vector<WorkshopItemClient>& items, int total)> callback);

	static void
	fetch_items(const std::string& type, const std::string& sort, const std::string& query, const std::string& author,
				std::function<void(bool success, const std::vector<WorkshopItemClient>& items, int total)> callback);

	static void fetch_item(const std::string& id,
						   std::function<void(bool success, const WorkshopItemClient& item)> callback);

	static void
	fetch_set_saves(const std::string& set_id,
					std::function<void(bool success, const std::vector<WorkshopItemClient>& saves)> callback);

	static void
	fetch_set_stamps(const std::string& set_id,
					 std::function<void(bool success, const std::vector<WorkshopItemClient>& stamps)> callback);

	static void download_item(const std::string& id, const std::string& target_path,
							  std::function<void(bool success, const std::string& path)> callback);

	static void download_thumbnail(const std::string& id, const std::string& target_path,
								   std::function<void(bool success, const std::string& path)> callback);

	static void toggle_like(const std::string& id,
							std::function<void(bool success, bool is_liked, int count)> callback);

	static void toggle_favorite(const std::string& id,
								std::function<void(bool success, bool is_favorited, int count)> callback);

	static void submit_report(const std::string& id, const std::string& reason, const std::string& details,
							  std::function<void(bool success, const std::string& message)> callback);

	static void
	publish_item(const std::string& type, const std::string& title, const std::string& description,
				 const std::string& author, const std::string& parent_set_id, const std::vector<uint8_t>& file_bytes,
				 const std::string& file_ext, const std::string& meta_json, const std::string& set_hash,
				 const std::string& thumbnail_data,
				 std::function<void(bool success, const std::string& created_id, const std::string& error)> callback);

	static void update_item(const std::string& id, const std::string& title, const std::string& description,
							int version, const std::string& changelog, const std::vector<uint8_t>& file_bytes,
							const std::string& file_ext, const std::string& meta_json, const std::string& set_hash,
							const std::string& thumbnail_data,
							std::function<void(bool success, const std::string& error)> callback);

	static void update_item_metadata(const std::string& id, const std::string& title, const std::string& description,
									 const std::string& meta_json,
									 std::function<void(bool success, const std::string& error)> callback);

	static void delete_item(const std::string& id,
							std::function<void(bool success, const std::string& error)> callback);

	static void enqueue_main_thread(std::function<void()> cb);
	static void enqueue_task_completion(std::function<void()> cb);

private:
	static std::string base_url;
	static std::string client_uuid;
	static std::string auth_token;
	static std::string logged_in_username;
	static std::mutex callback_mutex;
	static std::queue<std::function<void()>> main_thread_callbacks;

	static std::string http_get(const std::string& url, int& out_status);
	static std::string http_post_json(const std::string& url, const std::string& json_body, int& out_status);
	static std::string http_put_json(const std::string& url, const std::string& json_body, int& out_status);
	static std::string http_delete(const std::string& url, int& out_status);
	static bool http_download_file(const std::string& url, const std::string& dest_path);
};