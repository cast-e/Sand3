#include "toast_manager.hpp"
#include "icon_manager.hpp"

#include <algorithm>
#include <imgui.h>

std::vector<ToastNotification> ToastManager::s_toasts;
std::mutex ToastManager::s_mutex;
uint64_t ToastManager::s_next_id = 1;

void ToastManager::show(const std::string& message, ToastType type, float duration) {
	std::lock_guard<std::mutex> lock(s_mutex);
	ToastNotification t;
	t.id = s_next_id++;
	t.message = message;
	t.type = type;
	t.duration = std::max(1.0f, duration);
	t.elapsed = 0.0f;
	s_toasts.push_back(t);

	// Limit active toasts to avoid clutter
	if (s_toasts.size() > 6) {
		s_toasts.erase(s_toasts.begin());
	}
}

void ToastManager::success(const std::string& message, float duration) {
	show(message, ToastType::Success, duration);
}

void ToastManager::info(const std::string& message, float duration) {
	show(message, ToastType::Info, duration);
}

void ToastManager::warning(const std::string& message, float duration) {
	show(message, ToastType::Warning, duration);
}

void ToastManager::error(const std::string& message, float duration) {
	show(message, ToastType::Error, duration);
}

void ToastManager::clear() {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_toasts.clear();
}

void ToastManager::render() {
	std::lock_guard<std::mutex> lock(s_mutex);
	if (s_toasts.empty()) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	float dt = io.DeltaTime;

	const ImGuiViewport* vp = ImGui::GetMainViewport();
	if (!vp) {
		return;
	}

	float target_y = vp->WorkPos.y + vp->WorkSize.y - 20.0f;
	float right_x = vp->WorkPos.x + vp->WorkSize.x - 20.0f;

	// Iterate backwards so newest toasts stack nicely from bottom up
	for (int i = static_cast<int>(s_toasts.size()) - 1; i >= 0; --i) {
		auto& t = s_toasts[i];

		float alpha = 1.0f;
		if (t.elapsed < 0.2f) {
			alpha = std::clamp(t.elapsed / 0.2f, 0.05f, 1.0f);
		} else if ((t.duration - t.elapsed) < 0.4f) {
			alpha = std::clamp((t.duration - t.elapsed) / 0.4f, 0.0f, 1.0f);
		}

		ImVec4 type_col(0.35f, 0.65f, 1.0f, alpha);
		IconID toast_icon_id = IconID::Information;
		switch (t.type) {
			case ToastType::Success:
				type_col = ImVec4(0.25f, 0.85f, 0.45f, alpha);
				toast_icon_id = IconID::Accept;
				break;
			case ToastType::Info:
				type_col = ImVec4(0.35f, 0.65f, 1.0f, alpha);
				toast_icon_id = IconID::Information;
				break;
			case ToastType::Warning:
				type_col = ImVec4(1.0f, 0.75f, 0.2f, alpha);
				toast_icon_id = IconID::Warning;
				break;
			case ToastType::Error:
				type_col = ImVec4(0.95f, 0.35f, 0.35f, alpha);
				toast_icon_id = IconID::Cross;
				break;
		}

		std::string win_id = "##toast_win_" + std::to_string(t.id);

		ImGui::SetNextWindowPos(ImVec2(right_x, target_y), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
		ImGui::SetNextWindowBgAlpha(alpha * 0.92f);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, type_col);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.92f * alpha));

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
								 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
								 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

		if (ImGui::Begin(win_id.c_str(), nullptr, flags)) {
			if (ImGui::IsWindowHovered()) {
				// Pause dismissal while hovered
				t.elapsed = std::max(0.0f, t.elapsed - dt * 0.85f);
			}

			SDL_Texture* toast_icon = IconManager::get(toast_icon_id);
			if (toast_icon) {
				ImGui::Image((ImTextureID)toast_icon, ImVec2(16, 16));
				ImGui::SameLine(0, 8);
			}

			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 320.0f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, alpha));
			ImGui::TextUnformatted(t.message.c_str());
			ImGui::PopStyleColor();
			ImGui::PopTextWrapPos();

			ImGui::SameLine(0, 10);
			std::string close_lbl = "x##toast_close_" + std::to_string(t.id);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, alpha));
			if (ImGui::SmallButton(close_lbl.c_str())) {
				t.elapsed = t.duration;  // Mark expired
			}
			ImGui::PopStyleColor();

			float win_height = ImGui::GetWindowHeight();
			target_y -= win_height + 8.0f;
		}
		ImGui::End();

		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);

		t.elapsed += dt;
	}

	// Remove expired toasts
	s_toasts.erase(std::remove_if(s_toasts.begin(), s_toasts.end(),
								  [](const ToastNotification& t) { return t.elapsed >= t.duration; }),
				   s_toasts.end());
}
