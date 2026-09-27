#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

enum class ToastType {
	Info,
	Success,
	Warning,
	Error
};

struct ToastNotification {
	uint64_t id = 0;
	std::string message;
	ToastType type = ToastType::Info;
	float duration = 3.5f;
	float elapsed = 0.0f;
};

class ToastManager {
public:
	ToastManager() = delete;

	static void show(const std::string& message, ToastType type = ToastType::Info, float duration = 3.5f);
	static void success(const std::string& message, float duration = 3.5f);
	static void info(const std::string& message, float duration = 3.5f);
	static void warning(const std::string& message, float duration = 3.5f);
	static void error(const std::string& message, float duration = 4.0f);

	static void clear();
	static void render();

private:
	static std::vector<ToastNotification> s_toasts;
	static std::mutex s_mutex;
	static uint64_t s_next_id;
};
