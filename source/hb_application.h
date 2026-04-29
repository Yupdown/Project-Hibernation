#pragma once

#include "hb_frame_stats.h"
#include "hb_window.h"
#include "hb_vulkan_renderer.h"

class HBApplication {
public:
	void run();

private:
	void mainLoop();
	void cleanup();

	HBWindow m_window;
	VulkanRenderer m_renderer;
	HBFrameStats m_frameStats;
};
