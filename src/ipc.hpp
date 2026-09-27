#pragma once

#include <functional>
#include <string>

class IPC {
public:
	IPC() = delete;

	static bool send_to_existing_instance(const std::string& uri);

	static void start_server();

	static void poll(std::function<void(const std::string&)> on_uri_received);

	static void cleanup();
};

class ProtocolHandler {
public:
	ProtocolHandler() = delete;

	static void ensure_registered(const char* argv0);
};
