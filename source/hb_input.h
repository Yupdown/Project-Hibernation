#pragma once

#include <array>
#include <cstddef>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Per-frame polling input. Single-threaded game loop, so state is held in
// static members and accessed statically (see hb_input.cpp).
class HBInput {
public:
	HBInput() = delete;

	// Clears per-frame flags and snapshots the cursor. Call once per frame
	// before HBWindow::pollEvents() so this frame's events repopulate the flags.
	static void newFrame();

	// Event sinks, forwarded from HBWindow's GLFW callbacks.
	static void onKey(int key, int action);
	static void onMouseButton(int button, int action);
	static void onCursorPos(double x, double y);
	static void onScroll(double xOffset, double yOffset);

	// Keyboard state (GLFW_KEY_*).
	static bool isKey(int key);       // held
	static bool isKeyDown(int key);   // pressed this frame
	static bool isKeyUp(int key);     // released this frame

	// Mouse button state (GLFW_MOUSE_BUTTON_*).
	static bool isMouseButton(int button);       // held
	static bool isMouseButtonDown(int button);   // pressed this frame
	static bool isMouseButtonUp(int button);     // released this frame

	// Cursor / scroll.
	static glm::vec2 mousePosition();   // current cursor position (pixels)
	static glm::vec2 mouseDelta();      // cursor movement since last frame
	static glm::vec2 scrollDelta();     // wheel movement this frame

private:
	static constexpr std::size_t KeyCount = GLFW_KEY_LAST + 1;
	static constexpr std::size_t MouseButtonCount = GLFW_MOUSE_BUTTON_LAST + 1;

	static std::array<bool, KeyCount> s_keyHeld;
	static std::array<bool, KeyCount> s_keyPressed;
	static std::array<bool, KeyCount> s_keyReleased;

	static std::array<bool, MouseButtonCount> s_mouseHeld;
	static std::array<bool, MouseButtonCount> s_mousePressed;
	static std::array<bool, MouseButtonCount> s_mouseReleased;

	static glm::vec2 s_mousePosition;
	static glm::vec2 s_mousePreviousPosition;
	static glm::vec2 s_scrollDelta;
	static bool s_cursorInitialized;
};
