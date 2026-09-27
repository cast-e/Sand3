#pragma once

#include <functional>
#include <string>

class IPC {
public:
	IPC() = delete;

	// Checks if another instance is already running. If so, sends the URI to it and returns true.
	static bool send_to_existing_instance(const std::string& uri);

	// Starts the IPC listening server for the primary instance.
	static void start_server();

	// Non-blocking poll for incoming URIs from other instances.
	static void poll(std::function<void(const std::string&)> on_uri_received);

	// Cleans up listening socket / port.
	static void cleanup();
};

class ProtocolHandler {
public:
	ProtocolHandler() = delete;

	// Ensures sand3:// scheme is registered on the OS.
	static void ensure_registered(const char* argv0);
};
