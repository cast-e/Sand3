#include "workshop_client.hpp"

#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

#include "window.hpp"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif

std::string WorkshopClient::base_url = "https://sand3.vercel.app/api";
std::string WorkshopClient::client_uuid = "";
std::string WorkshopClient::auth_token = "";
std::string WorkshopClient::logged_in_username = "";
std::mutex WorkshopClient::callback_mutex;
std::queue<std::function<void()>> WorkshopClient::main_thread_callbacks;

namespace {
	std::string url_encode(const std::string& val) {
		std::ostringstream escaped;
		escaped.fill('0');
		escaped << std::hex;
		for (char c : val) {
			if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
				escaped << c;
			} else {
				escaped << '%' << std::uppercase << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
			}
		}
		return escaped.str();
	}

	std::string base64_encode(const uint8_t* data, size_t len) {
		static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		out.reserve(((len + 2) / 3) * 4);
		for (size_t i = 0; i < len; i += 3) {
			uint32_t val = (static_cast<uint32_t>(data[i]) << 16);
			if (i + 1 < len)
				val |= (static_cast<uint32_t>(data[i + 1]) << 8);
			if (i + 2 < len)
				val |= static_cast<uint32_t>(data[i + 2]);
			out.push_back(b64[(val >> 18) & 0x3F]);
			out.push_back(b64[(val >> 12) & 0x3F]);
			out.push_back((i + 1 < len) ? b64[(val >> 6) & 0x3F] : '=');
			out.push_back((i + 2 < len) ? b64[val & 0x3F] : '=');
		}
		return out;
	}

#ifdef HAVE_CURL
	size_t string_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
		size_t realsize = size * nmemb;
		auto* str = static_cast<std::string*>(userp);
		str->append(static_cast<char*>(contents), realsize);
		return realsize;
	}

	size_t file_write_cb(void* ptr, size_t size, size_t nmemb, void* stream) {
		auto* f = static_cast<FILE*>(stream);
		return fwrite(ptr, size, nmemb, f);
	}
#endif
	std::string json_str(const nlohmann::json& j, const std::string& key, const std::string& def = "") {
		if (j.contains(key) && !j[key].is_null()) {
			if (j[key].is_string())
				return j[key].get<std::string>();
			return j[key].dump();
		}
		return def;
	}

	int json_int(const nlohmann::json& j, const std::string& key, int def = 0) {
		if (j.contains(key) && !j[key].is_null() && j[key].is_number()) {
			return j[key].get<int>();
		}
		return def;
	}

	bool json_bool(const nlohmann::json& j, const std::string& key, bool def = false) {
		if (j.contains(key) && !j[key].is_null() && j[key].is_boolean()) {
			return j[key].get<bool>();
		}
		return def;
	}

	void parse_client_item(const nlohmann::json& item_json, WorkshopItemClient& it) {
		it.id = json_str(item_json, "id");
		it.type = json_str(item_json, "type");
		it.title = json_str(item_json, "title");
		it.description = json_str(item_json, "description");
		it.author = json_str(item_json, "author");
		it.parent_set_id = json_str(item_json, "parent_set_id");
		it.parent_set_title = json_str(item_json, "parent_set_title");
		it.version = json_int(item_json, "version", 1);
		it.set_hash = json_str(item_json, "set_hash");
		it.thumbnail_path = json_str(item_json, "thumbnail_path");
		it.user_id = json_str(item_json, "user_id");
		it.child_saves_count = json_int(item_json, "child_saves_count");
		it.child_stamps_count = json_int(item_json, "child_stamps_count");
		it.file_size = json_int(item_json, "file_size");
		it.likes_count = json_int(item_json, "likes_count");
		it.favorites_count = json_int(item_json, "favorites_count");
		it.downloads_count = json_int(item_json, "downloads_count");
		it.is_liked = json_bool(item_json, "is_liked");
		it.is_favorited = json_bool(item_json, "is_favorited");
		it.created_at = json_str(item_json, "created_at");
		if (item_json.contains("meta_json")) {
			if (item_json["meta_json"].is_string()) {
				try {
					it.meta = nlohmann::json::parse(item_json["meta_json"].get<std::string>());
				} catch (...) {}
			} else if (item_json["meta_json"].is_object()) {
				it.meta = item_json["meta_json"];
			}
		} else if (item_json.contains("meta") && item_json["meta"].is_object()) {
			it.meta = item_json["meta"];
		}
	}
}  // namespace

void WorkshopClient::init() {
#ifdef HAVE_CURL
	curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
	const char* env_url = std::getenv("SAND3_API_URL");
	if (env_url && env_url[0] != '\0') {
		set_base_url(env_url);
	}

	if (client_uuid.empty()) {
		std::random_device rd;
		std::mt19937_64 gen(rd());
		std::uniform_int_distribution<uint64_t> dis;
		client_uuid = "cpp_client_" + std::to_string(dis(gen));
	}
}

void WorkshopClient::shutdown() {
#ifdef HAVE_CURL
	curl_global_cleanup();
#endif
}

void WorkshopClient::set_base_url(const std::string& url) {
	std::string clean = url;
	while (!clean.empty() && (clean.back() == '/' || clean.back() == ' ')) {
		clean.pop_back();
	}
	if (!clean.empty() && clean.find("/api") == std::string::npos) {
		clean += "/api";
	}
	base_url = clean;
}

const std::string& WorkshopClient::get_base_url() { return base_url; }

std::string WorkshopClient::get_client_uuid() {
	if (client_uuid.empty()) {
		init();
	}
	return client_uuid;
}

bool WorkshopClient::is_logged_in() { return !auth_token.empty(); }

const std::string& WorkshopClient::get_logged_in_username() { return logged_in_username; }

const std::string& WorkshopClient::get_auth_token() { return auth_token; }

void WorkshopClient::set_auth_token(const std::string& token) { auth_token = token; }

void WorkshopClient::enqueue_main_thread(std::function<void()> cb) {
	std::lock_guard<std::mutex> lock(callback_mutex);
	main_thread_callbacks.push(std::move(cb));
}

void WorkshopClient::enqueue_task_completion(std::function<void()> cb) {
	enqueue_main_thread([cb = std::move(cb)]() {
		Window::decrement_busy();
		if (cb) {
			cb();
		}
	});
}

void WorkshopClient::update() {
	std::queue<std::function<void()>> local_queue;
	{
		std::lock_guard<std::mutex> lock(callback_mutex);
		std::swap(local_queue, main_thread_callbacks);
	}
	while (!local_queue.empty()) {
		local_queue.front()();
		local_queue.pop();
	}
}

std::string WorkshopClient::http_get(const std::string& url, int& out_status) {
	out_status = 0;
#ifdef HAVE_CURL
	CURL* curl = curl_easy_init();
	if (!curl)
		return "";

	std::string response;
	struct curl_slist* headers = nullptr;
	if (!auth_token.empty()) {
		std::string auth_header = "Authorization: Bearer " + auth_token;
		headers = curl_slist_append(headers, auth_header.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK) {
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		out_status = static_cast<int>(http_code);
	} else {
		std::cerr << "CURL GET error: " << curl_easy_strerror(res) << " on url: " << url << std::endl;
	}
	if (headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
#else
	std::string auth_arg = "";
	if (!auth_token.empty()) {
		auth_arg = " -H \"Authorization: Bearer " + auth_token + "\"";
	}
	std::string cmd = "curl -s" + auth_arg + " -w \"\\n%{http_code}\" \"" + url + "\"";
	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return "";
	char buf[512];
	std::string full_output;
	while (fgets(buf, sizeof(buf), pipe)) {
		full_output += buf;
	}
	pclose(pipe);

	size_t last_nl = full_output.find_last_of('\n');
	if (last_nl != std::string::npos) {
		size_t prev_nl = full_output.find_last_of('\n', last_nl - 1);
		std::string status_str = (prev_nl != std::string::npos) ? full_output.substr(prev_nl + 1, last_nl - prev_nl - 1)
																: full_output.substr(0, last_nl);
		try {
			out_status = std::stoi(status_str);
		} catch (...) {
			out_status = 200;
		}
		return (prev_nl != std::string::npos) ? full_output.substr(0, prev_nl) : "";
	}
	out_status = 200;
	return full_output;
#endif
}

std::string WorkshopClient::http_post_json(const std::string& url, const std::string& json_body, int& out_status) {
	out_status = 0;
#ifdef HAVE_CURL
	CURL* curl = curl_easy_init();
	if (!curl)
		return "";

	std::string response;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (!auth_token.empty()) {
		std::string auth_header = "Authorization: Bearer " + auth_token;
		headers = curl_slist_append(headers, auth_header.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK) {
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		out_status = static_cast<int>(http_code);
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
#else
	std::string temp_file =
		"/tmp/sand3_req_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".json";
	std::ofstream out(temp_file);
	out << json_body;
	out.close();

	std::string auth_arg = "";
	if (!auth_token.empty()) {
		auth_arg = " -H \"Authorization: Bearer " + auth_token + "\"";
	}
	std::string cmd =
		"curl -s -X POST -H \"Content-Type: application/json\"" + auth_arg + " -d @" + temp_file + " \"" + url + "\"";
	FILE* pipe = popen(cmd.c_str(), "r");
	std::string resp;
	if (pipe) {
		char buf[512];
		while (fgets(buf, sizeof(buf), pipe)) {
			resp += buf;
		}
		pclose(pipe);
	}
	std::filesystem::remove(temp_file);
	out_status = 200;
	return resp;
#endif
}

std::string WorkshopClient::http_put_json(const std::string& url, const std::string& json_body, int& out_status) {
	out_status = 0;
#ifdef HAVE_CURL
	CURL* curl = curl_easy_init();
	if (!curl)
		return "";

	std::string response;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (!auth_token.empty()) {
		std::string auth_header = "Authorization: Bearer " + auth_token;
		headers = curl_slist_append(headers, auth_header.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK) {
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		out_status = static_cast<int>(http_code);
	}
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
#else
	std::string temp_file =
		"/tmp/sand3_req_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".json";
	std::ofstream out(temp_file);
	out << json_body;
	out.close();

	std::string auth_arg = "";
	if (!auth_token.empty()) {
		auth_arg = " -H \"Authorization: Bearer " + auth_token + "\"";
	}
	std::string cmd =
		"curl -s -X PUT -H \"Content-Type: application/json\"" + auth_arg + " -d @" + temp_file + " \"" + url + "\"";
	FILE* pipe = popen(cmd.c_str(), "r");
	std::string resp;
	if (pipe) {
		char buf[512];
		while (fgets(buf, sizeof(buf), pipe)) {
			resp += buf;
		}
		pclose(pipe);
	}
	std::filesystem::remove(temp_file);
	out_status = 200;
	return resp;
#endif
}

std::string WorkshopClient::http_delete(const std::string& url, int& out_status) {
	out_status = 0;
#ifdef HAVE_CURL
	CURL* curl = curl_easy_init();
	if (!curl)
		return "";

	std::string response;
	struct curl_slist* headers = nullptr;
	if (!auth_token.empty()) {
		std::string auth_header = "Authorization: Bearer " + auth_token;
		headers = curl_slist_append(headers, auth_header.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, string_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

	CURLcode res = curl_easy_perform(curl);
	if (res == CURLE_OK) {
		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		out_status = static_cast<int>(http_code);
	}
	if (headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return response;
#else
	std::string auth_arg = "";
	if (!auth_token.empty()) {
		auth_arg = " -H \"Authorization: Bearer " + auth_token + "\"";
	}
	std::string cmd = "curl -s -X DELETE" + auth_arg + " \"" + url + "\"";
	FILE* pipe = popen(cmd.c_str(), "r");
	std::string resp;
	if (pipe) {
		char buf[512];
		while (fgets(buf, sizeof(buf), pipe)) {
			resp += buf;
		}
		pclose(pipe);
	}
	out_status = 200;
	return resp;
#endif
}

bool WorkshopClient::http_download_file(const std::string& url, const std::string& dest_path) {
#ifdef HAVE_CURL
	CURL* curl = curl_easy_init();
	if (!curl)
		return false;

	FILE* fp = fopen(dest_path.c_str(), "wb");
	if (!fp) {
		curl_easy_cleanup(curl);
		return false;
	}

	struct curl_slist* headers = nullptr;
	if (!auth_token.empty()) {
		std::string auth_header = "Authorization: Bearer " + auth_token;
		headers = curl_slist_append(headers, auth_header.c_str());
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

	CURLcode res = curl_easy_perform(curl);
	fclose(fp);
	if (headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return (res == CURLE_OK);
#else
	std::string auth_arg = "";
	if (!auth_token.empty()) {
		auth_arg = " -H \"Authorization: Bearer " + auth_token + "\"";
	}
	std::string cmd = "curl -s -L" + auth_arg + " -o \"" + dest_path + "\" \"" + url + "\"";
	int code = std::system(cmd.c_str());
	return (code == 0 && std::filesystem::exists(dest_path) && std::filesystem::file_size(dest_path) > 0);
#endif
}

void WorkshopClient::login(const std::string& username, const std::string& password,
						   std::function<void(bool success, const std::string& error)> callback) {
	Window::increment_busy();
	std::thread([username, password, callback]() {
		std::string url = base_url + "/workshop/auth/login";
		nlohmann::json body;
		body["username"] = username;
		body["password"] = password;
		int status = 0;
		std::string resp = http_post_json(url, body.dump(), status);
		bool ok = (status >= 200 && status < 300);
		std::string err_str;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				auth_token = j.value("token", "");
				if (j.contains("user") && j["user"].is_object()) {
					logged_in_username = j["user"].value("username", username);
				} else {
					logged_in_username = username;
				}
			} catch (const std::exception& e) {
				ok = false;
				err_str = e.what();
			}
		} else {
			try {
				auto j = nlohmann::json::parse(resp);
				err_str = j.value("error", "Login failed (HTTP " + std::to_string(status) + ")");
			} catch (...) {
				err_str = "Login failed (HTTP " + std::to_string(status) + ")";
			}
		}

		enqueue_task_completion([callback, ok, err_str]() { callback(ok, err_str); });
	}).detach();
}

void WorkshopClient::register_user(const std::string& username, const std::string& password,
								   std::function<void(bool success, const std::string& error)> callback) {
	Window::increment_busy();
	std::thread([username, password, callback]() {
		std::string url = base_url + "/workshop/auth/register";
		nlohmann::json body;
		body["username"] = username;
		body["password"] = password;
		int status = 0;
		std::string resp = http_post_json(url, body.dump(), status);
		bool ok = (status >= 200 && status < 300);
		std::string err_str;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				auth_token = j.value("token", "");
				if (j.contains("user") && j["user"].is_object()) {
					logged_in_username = j["user"].value("username", username);
				} else {
					logged_in_username = username;
				}
			} catch (const std::exception& e) {
				ok = false;
				err_str = e.what();
			}
		} else {
			try {
				auto j = nlohmann::json::parse(resp);
				err_str = j.value("error", "Registration failed (HTTP " + std::to_string(status) + ")");
			} catch (...) {
				err_str = "Registration failed (HTTP " + std::to_string(status) + ")";
			}
		}

		enqueue_task_completion([callback, ok, err_str]() { callback(ok, err_str); });
	}).detach();
}

void WorkshopClient::check_auth(std::function<void(bool success, const std::string& username)> callback) {
	Window::increment_busy();
	if (auth_token.empty()) {
		enqueue_task_completion([callback]() { callback(false, ""); });
		return;
	}
	std::thread([callback]() {
		std::string url = base_url + "/workshop/auth/me";
		int status = 0;
		std::string resp = http_get(url, status);
		bool ok = (status >= 200 && status < 300);
		std::string uname;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				if (j.contains("user") && j["user"].is_object()) {
					uname = j["user"].value("username", "");
					logged_in_username = uname;
				}
			} catch (...) {
				ok = false;
			}
		} else {
			auth_token = "";
			logged_in_username = "";
		}

		enqueue_task_completion([callback, ok, uname]() { callback(ok, uname); });
	}).detach();
}

void WorkshopClient::logout() {
	if (!auth_token.empty()) {
		std::string url = base_url + "/workshop/auth/logout";
		int status = 0;
		http_post_json(url, "{}", status);
	}
	auth_token = "";
	logged_in_username = "";
}

void WorkshopClient::fetch_items(
	const std::string& type, const std::string& sort, const std::string& query,
	std::function<void(bool success, const std::vector<WorkshopItemClient>& items, int total)> callback) {
	fetch_items(type, sort, query, "", callback);
}

void WorkshopClient::fetch_items(
	const std::string& type, const std::string& sort, const std::string& query, const std::string& author,
	std::function<void(bool success, const std::vector<WorkshopItemClient>& items, int total)> callback) {
	Window::increment_busy();
	std::thread([type, sort, query, author, callback]() {
		std::string url =
			base_url + "/workshop/items?type=" + type + "&sort=" + sort + "&client_uuid=" + get_client_uuid();
		if (!query.empty()) {
			url += "&q=" + url_encode(query);
		}
		if (!author.empty()) {
			url += "&author=" + url_encode(author);
		}

		int status = 0;
		std::string resp = http_get(url, status);
		bool ok = (status >= 200 && status < 300);
		std::vector<WorkshopItemClient> result_items;
		int total = 0;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				if (j.contains("pagination") && j["pagination"].contains("total")) {
					total = j["pagination"]["total"].get<int>();
				}
				if (j.contains("items") && j["items"].is_array()) {
					for (const auto& item_json : j["items"]) {
						WorkshopItemClient it;
						parse_client_item(item_json, it);
						result_items.push_back(it);
					}
				}
			} catch (const std::exception& e) {
				std::cerr << "JSON parse error: " << e.what() << " resp: " << resp << std::endl;
				ok = false;
			}
		}

		enqueue_task_completion([callback, ok, result_items, total]() { callback(ok, result_items, total); });
	}).detach();
}

void WorkshopClient::fetch_item(const std::string& id,
								std::function<void(bool success, const WorkshopItemClient& item)> callback) {
	Window::increment_busy();
	std::thread([id, callback]() {
		std::string url = base_url + "/workshop/items/" + id + "?client_uuid=" + get_client_uuid();
		int status = 0;
		std::string resp = http_get(url, status);
		bool ok = (status >= 200 && status < 300);
		WorkshopItemClient it;

		if (ok) {
			try {
				auto item_json = nlohmann::json::parse(resp);
				parse_client_item(item_json, it);
			} catch (...) {
				ok = false;
			}
		}

		enqueue_task_completion([callback, ok, it]() { callback(ok, it); });
	}).detach();
}

void WorkshopClient::check_item_exists(const std::string& id,
									   std::function<void(bool exists, int http_status)> callback) {
	if (id.empty()) {
		if (callback) {
			callback(false, 404);
		}
		return;
	}
	Window::increment_busy();
	std::thread([id, callback]() {
		std::string url = base_url + "/workshop/items/" + id + "?client_uuid=" + get_client_uuid();
		int status = 0;
		std::string resp = http_get(url, status);
		bool exists = (status >= 200 && status < 300);
		enqueue_task_completion([callback, exists, status]() {
			if (callback) {
				callback(exists, status);
			}
		});
	}).detach();
}

void WorkshopClient::fetch_set_saves(
	const std::string& set_id,
	std::function<void(bool success, const std::vector<WorkshopItemClient>& saves)> callback) {
	Window::increment_busy();
	std::thread([set_id, callback]() {
		std::string url = base_url + "/workshop/items/sets/" + set_id + "/saves";
		int status = 0;
		std::string resp = http_get(url, status);
		bool ok = (status >= 200 && status < 300);
		std::vector<WorkshopItemClient> saves;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				if (j.is_array()) {
					for (const auto& item_json : j) {
						WorkshopItemClient it;
						parse_client_item(item_json, it);
						saves.push_back(it);
					}
				}
			} catch (...) {
				ok = false;
			}
		}

		enqueue_task_completion([callback, ok, saves]() { callback(ok, saves); });
	}).detach();
}

void WorkshopClient::fetch_set_stamps(
	const std::string& set_id,
	std::function<void(bool success, const std::vector<WorkshopItemClient>& stamps)> callback) {
	Window::increment_busy();
	std::thread([set_id, callback]() {
		std::string url = base_url + "/workshop/items/sets/" + set_id + "/stamps";
		int status = 0;
		std::string resp = http_get(url, status);
		bool ok = (status >= 200 && status < 300);
		std::vector<WorkshopItemClient> stamps;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				if (j.is_array()) {
					for (const auto& item_json : j) {
						WorkshopItemClient it;
						parse_client_item(item_json, it);
						stamps.push_back(it);
					}
				}
			} catch (...) {
				ok = false;
			}
		}

		enqueue_task_completion([callback, ok, stamps]() { callback(ok, stamps); });
	}).detach();
}

void WorkshopClient::download_item(const std::string& id, const std::string& target_path,
								   std::function<void(bool success, const std::string& path)> callback) {
	Window::increment_busy();
	std::thread([id, target_path, callback]() {
		std::string url = base_url + "/workshop/items/" + id + "/download";
		bool ok = http_download_file(url, target_path);

		enqueue_task_completion([callback, ok, target_path]() { callback(ok, target_path); });
	}).detach();
}

void WorkshopClient::download_thumbnail(const std::string& id, const std::string& target_path,
										std::function<void(bool success, const std::string& path)> callback) {
	std::thread([id, target_path, callback]() {
		std::string url = base_url + "/workshop/items/" + id + "/thumbnail";
		bool ok = http_download_file(url, target_path);

		enqueue_main_thread([callback, ok, target_path]() { callback(ok, target_path); });
	}).detach();
}

void WorkshopClient::toggle_like(const std::string& id,
								 std::function<void(bool success, bool is_liked, int count)> callback) {
	std::thread([id, callback]() {
		std::string url = base_url + "/workshop/items/" + id + "/like";
		nlohmann::json body;
		body["client_uuid"] = get_client_uuid();
		int status = 0;
		std::string resp = http_post_json(url, body.dump(), status);
		bool ok = (status >= 200 && status < 300);
		bool is_liked = false;
		int count = 0;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				is_liked = j.value("is_liked", false);
				count = j.value("likes_count", 0);
			} catch (...) {
				ok = false;
			}
		}

		enqueue_main_thread([callback, ok, is_liked, count]() { callback(ok, is_liked, count); });
	}).detach();
}

void WorkshopClient::toggle_favorite(const std::string& id,
									 std::function<void(bool success, bool is_favorited, int count)> callback) {
	std::thread([id, callback]() {
		std::string url = base_url + "/workshop/items/" + id + "/favorite";
		nlohmann::json body;
		body["client_uuid"] = get_client_uuid();
		int status = 0;
		std::string resp = http_post_json(url, body.dump(), status);
		bool ok = (status >= 200 && status < 300);
		bool is_favorited = false;
		int count = 0;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				is_favorited = j.value("is_favorited", false);
				count = j.value("favorites_count", 0);
			} catch (...) {
				ok = false;
			}
		}

		enqueue_main_thread([callback, ok, is_favorited, count]() { callback(ok, is_favorited, count); });
	}).detach();
}

void WorkshopClient::submit_report(const std::string& id, const std::string& reason, const std::string& details,
								   std::function<void(bool success, const std::string& message)> callback) {
	std::thread([id, reason, details, callback]() {
		std::string url = base_url + "/workshop/items/" + id + "/report";
		nlohmann::json body;
		body["client_uuid"] = get_client_uuid();
		body["reason"] = reason;
		body["details"] = details;
		int status = 0;
		std::string resp = http_post_json(url, body.dump(), status);
		bool ok = (status >= 200 && status < 300);
		std::string msg = "Report submitted.";

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				msg = j.value("message", msg);
			} catch (...) {}
		}

		enqueue_main_thread([callback, ok, msg]() { callback(ok, msg); });
	}).detach();
}

void WorkshopClient::publish_item(
	const std::string& type, const std::string& title, const std::string& description, const std::string& author,
	const std::string& parent_set_id, const std::vector<uint8_t>& file_bytes, const std::string& file_ext,
	const std::string& meta_json, const std::string& set_hash, const std::string& thumbnail_data,
	std::function<void(bool success, const std::string& created_id, const std::string& error)> callback) {
	Window::increment_busy();
	std::thread([type, title, description, author, parent_set_id, file_bytes, file_ext, meta_json, set_hash,
				 thumbnail_data, callback]() {
		std::string url = base_url + "/workshop/items";
		nlohmann::json body;
		body["type"] = type;
		body["title"] = title;
		body["description"] = description;
		body["author"] = author;
		if (!parent_set_id.empty()) {
			body["parent_set_id"] = parent_set_id;
		}
		if (!set_hash.empty()) {
			body["set_hash"] = set_hash;
		}
		if (!thumbnail_data.empty()) {
			body["thumbnail_data"] = thumbnail_data;
		}
		body["file_data"] = base64_encode(file_bytes.data(), file_bytes.size());
		body["file_ext"] = file_ext;
		body["meta_json"] = meta_json;

		int status = 0;
		std::string resp = http_post_json(url, body.dump(), status);
		bool ok = (status >= 200 && status < 300);
		std::string created_id;
		std::string err_str;

		if (ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				created_id = j.value("id", "");
			} catch (const std::exception& e) {
				ok = false;
				err_str = e.what();
			}
		} else {
			try {
				auto j = nlohmann::json::parse(resp);
				err_str = j.value("error", "HTTP " + std::to_string(status));
			} catch (...) {
				err_str = "HTTP " + std::to_string(status);
			}
		}

		enqueue_task_completion([callback, ok, created_id, err_str]() { callback(ok, created_id, err_str); });
	}).detach();
}

void WorkshopClient::update_item(const std::string& id, const std::string& title, const std::string& description,
								 int version, const std::string& changelog, const std::vector<uint8_t>& file_bytes,
								 const std::string& file_ext, const std::string& meta_json, const std::string& set_hash,
								 const std::string& thumbnail_data,
								 std::function<void(bool success, const std::string& error)> callback) {
	Window::increment_busy();
	std::thread([id, title, description, version, changelog, file_bytes, file_ext, meta_json, set_hash, thumbnail_data,
				 callback]() {
		std::string url = base_url + "/workshop/items/" + id;
		nlohmann::json body;
		if (!title.empty())
			body["title"] = title;
		body["description"] = description;
		if (version > 0)
			body["version"] = version;
		if (!changelog.empty())
			body["changelog"] = changelog;
		if (!meta_json.empty())
			body["meta_json"] = meta_json;
		if (!set_hash.empty())
			body["set_hash"] = set_hash;
		if (!file_bytes.empty()) {
			body["file_data"] = base64_encode(file_bytes.data(), file_bytes.size());
			body["file_ext"] = file_ext;
		}
		if (!thumbnail_data.empty()) {
			body["thumbnail_data"] = thumbnail_data;
		}

		int status = 0;
		std::string resp = http_put_json(url, body.dump(), status);
		bool ok = (status >= 200 && status < 300);
		std::string err_str;

		if (!ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				err_str = j.value("error", "HTTP " + std::to_string(status));
			} catch (...) {
				err_str = "HTTP " + std::to_string(status);
			}
		}

		enqueue_task_completion([callback, ok, err_str]() { callback(ok, err_str); });
	}).detach();
}

void WorkshopClient::update_item_metadata(const std::string& id, const std::string& title,
										  const std::string& description, const std::string& meta_json,
										  std::function<void(bool success, const std::string& error)> callback) {
	update_item(id, title, description, 0, "", {}, "", meta_json, "", "", callback);
}

void WorkshopClient::delete_item(const std::string& id,
								 std::function<void(bool success, const std::string& error)> callback) {
	Window::increment_busy();
	std::thread([id, callback]() {
		std::string url = base_url + "/workshop/items/" + id;
		int status = 0;
		std::string resp = http_delete(url, status);
		bool ok = (status >= 200 && status < 300);
		std::string err_str;

		if (!ok) {
			try {
				auto j = nlohmann::json::parse(resp);
				err_str = j.value("error", "HTTP " + std::to_string(status));
			} catch (...) {
				err_str = "HTTP " + std::to_string(status);
			}
		}

		enqueue_task_completion([callback, ok, err_str]() { callback(ok, err_str); });
	}).detach();
}