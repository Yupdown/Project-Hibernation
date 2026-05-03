#include "stdafx.h"
#include "hb_application.h"
#include "hb_frame_stats.h"

void HBApplication::run() {
	m_window.setFramebufferResizeCallback([this](int width, int height) {
		m_renderer.handleFramebufferResize(width, height);
	});

	m_window.create();
	m_renderer.init(m_window.get());
	m_renderer.initImGui(m_window.get());
	mainLoop();
	cleanup();
}

void HBApplication::mainLoop() {
	while (!m_window.shouldClose()) {
		g_frameStats.newFrame();
		m_window.pollEvents();
		m_renderer.updateImGuiFrame();
		const bool frameOk = m_renderer.acquireNextFrame();
		if (frameOk) {
			m_renderer.render();
		}
	}

	m_renderer.waitIdle();
}

void HBApplication::cleanup() {
	m_renderer.releaseAssetResources();
	m_renderer.shutdownImGui();
	m_window.destroy();

	std::cout << "> Cleanup and Exit" << std::endl;
}
