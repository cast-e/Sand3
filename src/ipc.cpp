#include "ipc.hpp"

#include <filesystem>
#include <fstream>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

static int s_server_fd = -1;

static std::string get_socket_path() {
	const char* xdg = getenv("XDG_RUNTIME_DIR");
	if (xdg && *xdg) {
		return std::string(xdg) + "/sand3_ipc.sock";
	}
	return "/tmp/sand3_ipc_" + std::to_string(getuid()) + ".sock";
}
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

static SOCKET s_win_sock = INVALID_SOCKET;
#endif

bool IPC::send_to_existing_instance(const std::string& uri) {
#ifndef _WIN32
	std::string sock_path = get_socket_path();
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return false;

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
		std::string payload = uri.empty() ? "\n" : (uri + "\n");
		ssize_t sent = write(fd, payload.c_str(), payload.size());
		(void)sent;
		close(fd);
		return true;
	}

	close(fd);
	unlink(sock_path.c_str());
	return false;
#else
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		WSACleanup();
		return false;
	}

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(43231);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
		std::string payload = uri.empty() ? "\n" : (uri + "\n");
		send(sock, payload.c_str(), (int)payload.size(), 0);
		closesocket(sock);
		WSACleanup();
		return true;
	}

	closesocket(sock);
	WSACleanup();
	return false;
#endif
}

void IPC::start_server() {
#ifndef _WIN32
	std::string sock_path = get_socket_path();
	unlink(sock_path.c_str());

	s_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s_server_fd < 0)
		return;

	int flags = fcntl(s_server_fd, F_GETFL, 0);
	fcntl(s_server_fd, F_SETFL, flags | O_NONBLOCK);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);

	if (bind(s_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		close(s_server_fd);
		s_server_fd = -1;
		return;
	}

	listen(s_server_fd, 5);
#else
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return;
	s_win_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s_win_sock == INVALID_SOCKET) {
		WSACleanup();
		return;
	}

	u_long mode = 1;
	ioctlsocket(s_win_sock, FIONBIO, &mode);

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(43231);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(s_win_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		closesocket(s_win_sock);
		s_win_sock = INVALID_SOCKET;
		WSACleanup();
		return;
	}

	listen(s_win_sock, 5);
#endif
}

void IPC::poll(std::function<void(const std::string&)> on_uri_received) {
#ifndef _WIN32
	if (s_server_fd < 0)
		return;

	int client_fd = accept(s_server_fd, nullptr, nullptr);
	if (client_fd >= 0) {
		char buf[512];
		std::string received = "";
		ssize_t n = 0;
		while ((n = read(client_fd, buf, sizeof(buf) - 1)) > 0) {
			buf[n] = '\0';
			received += buf;
			if (received.find('\n') != std::string::npos)
				break;
		}
		close(client_fd);

		while (!received.empty() && (received.back() == '\n' || received.back() == '\r' || received.back() == ' ')) {
			received.pop_back();
		}

		if (!received.empty() && on_uri_received) {
			on_uri_received(received);
		}
	}
#else
	if (s_win_sock == INVALID_SOCKET)
		return;

	SOCKET client = accept(s_win_sock, nullptr, nullptr);
	if (client != INVALID_SOCKET) {
		char buf[512];
		std::string received = "";
		int n = 0;
		while ((n = recv(client, buf, sizeof(buf) - 1, 0)) > 0) {
			buf[n] = '\0';
			received += buf;
			if (received.find('\n') != std::string::npos)
				break;
		}
		closesocket(client);

		while (!received.empty() && (received.back() == '\n' || received.back() == '\r' || received.back() == ' ')) {
			received.pop_back();
		}

		if (!received.empty() && on_uri_received) {
			on_uri_received(received);
		}
	}
#endif
}

void IPC::cleanup() {
#ifndef _WIN32
	if (s_server_fd >= 0) {
		close(s_server_fd);
		s_server_fd = -1;
	}
	std::string sock_path = get_socket_path();
	unlink(sock_path.c_str());
#else
	if (s_win_sock != INVALID_SOCKET) {
		closesocket(s_win_sock);
		s_win_sock = INVALID_SOCKET;
		WSACleanup();
	}
#endif
}

void ProtocolHandler::ensure_registered(const char* argv0) {
#if defined(__linux__)
	if (!argv0 || !*argv0)
		return;

	std::string exe_path;
	try {
		exe_path = std::filesystem::canonical(argv0).string();
	} catch (...) {
		exe_path = argv0;
	}

	std::filesystem::path bin_dir = std::filesystem::path(exe_path).parent_path();
	std::filesystem::path working_dir = bin_dir;
	if (std::filesystem::exists(bin_dir.parent_path() / "sets")) {
		working_dir = bin_dir.parent_path();
	}

	const char* home = getenv("HOME");
	if (!home)
		return;

	std::filesystem::path app_dir = std::filesystem::path(home) / ".local/share/applications";
	std::filesystem::create_directories(app_dir);
	std::filesystem::path desktop_file = app_dir / "sand3.desktop";

	bool needs_write = true;
	if (std::filesystem::exists(desktop_file)) {
		std::ifstream in(desktop_file);
		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (content.find(exe_path) != std::string::npos &&
			content.find("x-scheme-handler/sand3") != std::string::npos) {
			needs_write = false;
		}
	}

	if (needs_write) {
		std::ofstream out(desktop_file);
		if (out.is_open()) {
			out << "[Desktop Entry]\n";
			out << "Name=Sand3\n";
			out << "Comment=Falling Sand Game & Cellular Automata Simulator\n";
			out << "Exec=" << exe_path << " %u\n";
			out << "Path=" << working_dir.string() << "\n";
			out << "Terminal=false\n";
			out << "Type=Application\n";
			out << "Categories=Game;Simulation;\n";
			out << "MimeType=x-scheme-handler/sand3;\n";
			out << "StartupNotify=true\n";
			out.close();

			int r1 = system("xdg-mime default sand3.desktop x-scheme-handler/sand3 2>/dev/null");
			int r2 = system("update-desktop-database ~/.local/share/applications 2>/dev/null");
			(void)r1;
			(void)r2;
		}
	}
#endif
}
