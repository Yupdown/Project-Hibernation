#pragma once

#include <functional>
#include <cstdint>

struct GLFWwindow;

class HBWindow {
public:
	static constexpr uint32_t InitWidth = 1080;
	static constexpr uint32_t InitHeight = 1080;

	void setFramebufferResizeCallback(std::function<void(int, int)> callback);

	void create();
	void destroy();

	GLFWwindow* get() const { return m_window; }
	uint32_t width() const { return m_width; }
	uint32_t height() const { return m_height; }

	bool shouldClose() const;
	void pollEvents();

private:
	void onFramebufferResize(int width, int height);
	void onKey(int key, int scancode, int action, int mods);
	void toggleFullscreen(bool fullScreen);

	GLFWwindow* m_window = nullptr;
	uint32_t m_width = InitWidth;
	uint32_t m_height = InitHeight;

	bool m_fullscreen = false;
	int m_windowPosX = 0;
	int m_windowPosY = 0;
	int m_windowedWidth = static_cast<int>(InitWidth);
	int m_windowedHeight = static_cast<int>(InitHeight);

	std::function<void(int, int)> m_onFramebufferResize;

	bool m_created = false;
};
