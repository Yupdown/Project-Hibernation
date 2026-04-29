#include "stdafx.h"
#include "hb_window.h"

void HBWindow::setFramebufferResizeCallback(std::function<void(int, int)> callback) {
	m_onFramebufferResize = std::move(callback);
}

void HBWindow::create() {
	glfwSetErrorCallback([](int error, const char* description) {
		std::cerr << "Error: " << description << std::endl;
	});

	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}

	std::cout << "> GLFW Initialization" << std::endl;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(static_cast<int>(m_width), static_cast<int>(m_height), "project-hibernation", nullptr, nullptr);
	if (!m_window) {
		glfwTerminate();
		throw std::runtime_error("Failed to create window");
	}

	glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window,
		[](GLFWwindow* window, int key, int scancode, int action, int mods) {
			static_cast<HBWindow*>(glfwGetWindowUserPointer(window))->onKey(key, scancode, action, mods);
		});
	glfwSetFramebufferSizeCallback(m_window,
		[](GLFWwindow* window, int width, int height) {
			static_cast<HBWindow*>(glfwGetWindowUserPointer(window))->onFramebufferResize(width, height);
		});

	std::cout << "> Window Creation" << std::endl;
	m_created = true;
}

void HBWindow::destroy() {
	if (!m_created) {
		return;
	}
	m_created = false;

	if (m_window) {
		glfwDestroyWindow(m_window);
		m_window = nullptr;
	}
	glfwTerminate();

	std::cout << "> Window cleanup" << std::endl;
}

bool HBWindow::shouldClose() const {
	return m_window && glfwWindowShouldClose(m_window);
}

void HBWindow::pollEvents() {
	glfwPollEvents();
}

void HBWindow::onFramebufferResize(int width, int height) {
	m_width = static_cast<uint32_t>(width);
	m_height = static_cast<uint32_t>(height);

	if (m_width == 0 || m_height == 0) {
		return;
	}

	if (m_onFramebufferResize) {
		m_onFramebufferResize(width, height);
	}
}

void HBWindow::onKey(int key, int scancode, int action, int mods) {
	(void)scancode;
	if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && (mods & GLFW_MOD_ALT)) {
		toggleFullscreen(!m_fullscreen);
	}
}

void HBWindow::toggleFullscreen(bool fullScreen) {
	if (!m_window || m_fullscreen == fullScreen) {
		return;
	}

	m_fullscreen = fullScreen;
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	if (fullScreen) {
		std::cout << "> Switching to Borderless Fullscreen" << std::endl;

		glfwGetWindowPos(m_window, &m_windowPosX, &m_windowPosY);
		glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);

		glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
		glfwSetWindowMonitor(m_window, nullptr, 0, 0, mode->width, mode->height, mode->refreshRate);
	}
	else {
		std::cout << "> Switching to Windowed Mode" << std::endl;

		glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowMonitor(m_window, nullptr, m_windowPosX, m_windowPosY, m_windowedWidth, m_windowedHeight, 0);
	}
}
