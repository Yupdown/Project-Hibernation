#include "stdafx.h"
#include "hb_input.h"

std::array<bool, HBInput::KeyCount> HBInput::s_keyHeld{};
std::array<bool, HBInput::KeyCount> HBInput::s_keyPressed{};
std::array<bool, HBInput::KeyCount> HBInput::s_keyReleased{};

std::array<bool, HBInput::MouseButtonCount> HBInput::s_mouseHeld{};
std::array<bool, HBInput::MouseButtonCount> HBInput::s_mousePressed{};
std::array<bool, HBInput::MouseButtonCount> HBInput::s_mouseReleased{};

glm::vec2 HBInput::s_mousePosition{ 0.0f };
glm::vec2 HBInput::s_mousePreviousPosition{ 0.0f };
glm::vec2 HBInput::s_scrollDelta{ 0.0f };
bool HBInput::s_cursorInitialized = false;

void HBInput::newFrame() {
	s_keyPressed.fill(false);
	s_keyReleased.fill(false);
	s_mousePressed.fill(false);
	s_mouseReleased.fill(false);

	s_mousePreviousPosition = s_mousePosition;
	s_scrollDelta = glm::vec2(0.0f);
}

void HBInput::onKey(int key, int action) {
	if (key < 0 || static_cast<std::size_t>(key) >= KeyCount) {
		return;
	}

	if (action == GLFW_PRESS) {
		s_keyHeld[key] = true;
		s_keyPressed[key] = true;
	}
	else if (action == GLFW_RELEASE) {
		s_keyHeld[key] = false;
		s_keyReleased[key] = true;
	}
	// GLFW_REPEAT is ignored: the key stays held.
}

void HBInput::onMouseButton(int button, int action) {
	if (button < 0 || static_cast<std::size_t>(button) >= MouseButtonCount) {
		return;
	}

	if (action == GLFW_PRESS) {
		s_mouseHeld[button] = true;
		s_mousePressed[button] = true;
	}
	else if (action == GLFW_RELEASE) {
		s_mouseHeld[button] = false;
		s_mouseReleased[button] = true;
	}
}

void HBInput::onCursorPos(double x, double y) {
	s_mousePosition = glm::vec2(static_cast<float>(x), static_cast<float>(y));

	if (!s_cursorInitialized) {
		s_mousePreviousPosition = s_mousePosition;
		s_cursorInitialized = true;
	}
}

void HBInput::onScroll(double xOffset, double yOffset) {
	s_scrollDelta += glm::vec2(static_cast<float>(xOffset), static_cast<float>(yOffset));
}

bool HBInput::isKey(int key) {
	if (key < 0 || static_cast<std::size_t>(key) >= KeyCount) {
		return false;
	}
	return s_keyHeld[key];
}

bool HBInput::isKeyDown(int key) {
	if (key < 0 || static_cast<std::size_t>(key) >= KeyCount) {
		return false;
	}
	return s_keyPressed[key];
}

bool HBInput::isKeyUp(int key) {
	if (key < 0 || static_cast<std::size_t>(key) >= KeyCount) {
		return false;
	}
	return s_keyReleased[key];
}

bool HBInput::isMouseButton(int button) {
	if (button < 0 || static_cast<std::size_t>(button) >= MouseButtonCount) {
		return false;
	}
	return s_mouseHeld[button];
}

bool HBInput::isMouseButtonDown(int button) {
	if (button < 0 || static_cast<std::size_t>(button) >= MouseButtonCount) {
		return false;
	}
	return s_mousePressed[button];
}

bool HBInput::isMouseButtonUp(int button) {
	if (button < 0 || static_cast<std::size_t>(button) >= MouseButtonCount) {
		return false;
	}
	return s_mouseReleased[button];
}

glm::vec2 HBInput::mousePosition() {
	return s_mousePosition;
}

glm::vec2 HBInput::mouseDelta() {
	return s_mousePosition - s_mousePreviousPosition;
}

glm::vec2 HBInput::scrollDelta() {
	return s_scrollDelta;
}
