#pragma once

#include "hb_frame_stats.h"

class HBApplication {
public:
	static constexpr uint32_t InitWidth = 1080;
	static constexpr uint32_t InitHeight = 1080;
	static constexpr uint32_t NumFramesInFlight = 2;

public:
	void run();
	void initWindow();
	void logDeviceInfo(const vk::PhysicalDevice& physicalDevice);
	void initVulkan();
	void recreateSwapChain();
	void handleFramebufferResize(GLFWwindow* window, int width, int height);
	void handleKey(GLFWwindow* window, int key, int scancode, int action, int mods);
	void initImGUI();
	void createSwapChain(vkb::Device& vkb_dev);
	void createImageViews();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();
	void createOffscreenResources();
	void createCommandPool(vkb::Device& vkb_dev);
	void createCommandBuffers();
	void recordCommandBuffer(vk::CommandBuffer& commandBuffer, uint32_t imageIndex);
	void createDescriptorPool();
	void createSyncObjects();
	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
	bool acquireNextFrame();
	void render();
	void mainLoop();
	void toggleFullscreen(GLFWwindow* window, bool fullScreen);
	void cleanup();
	static std::vector<char> readFile(const std::string& filename);

private:
	// Window
	GLFWwindow* m_window = nullptr;

	// Vulkan Core
	vk::UniqueInstance m_instance;
	vk::UniqueDebugUtilsMessengerEXT m_debugMessenger;
	vk::PhysicalDevice m_physicalDevice = nullptr;
	vk::UniqueDevice m_device;
	vk::UniqueSurfaceKHR m_surface;

	// Vulkan Bootstrap
	vkb::Instance m_vkbInstance;
	vkb::PhysicalDevice m_vkbPhysicalDevice;
	vkb::Device m_vkbDevice;

	// Swapchain
	vk::UniqueSwapchainKHR m_swapChain;
	std::vector<vk::Image> m_swapChainImages;
	vk::Format m_swapChainImageFormat;
	vk::Extent2D m_swapChainExtent;
	std::vector<vk::UniqueImageView> m_swapChainImageViews;

	// Descriptor Pool
	vk::UniqueDescriptorPool m_descriptorPool;

	// Pipeline
	vk::UniqueRenderPass m_renderPass;
	vk::UniqueRenderPass m_offscreenRenderPass;
	vk::UniquePipelineLayout m_pipelineLayout;
	vk::UniquePipeline m_graphicsPipeline;

	// Framebuffers
	std::vector<vk::UniqueFramebuffer> m_swapChainFramebuffers;
	vk::UniqueFramebuffer m_offscreenFramebuffer;

	// Offscreen Target
	vk::UniqueImage m_offscreenImage;
	vk::UniqueDeviceMemory m_offscreenImageMemory;
	vk::UniqueImageView m_offscreenImageView;
	vk::Format m_offscreenFormat = vk::Format::eB8G8R8A8Unorm;

	// Command
	vk::UniqueCommandPool m_commandPool;
	std::vector<vk::UniqueCommandBuffer> m_commandBuffers;

	// Synchronization
	std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
	std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
	vk::UniqueSemaphore m_timelineSemaphore;
	uint64_t m_timelineValue = 0;
	std::vector<uint64_t> m_frameTimelineValues;

	// Queues
	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;

	uint32_t m_graphicsQueueFamilyIndex = 0;
	uint32_t m_presentQueueFamilyIndex = 0;

	uint32_t m_currentFrameIndex = 0;
	uint32_t m_currentSwapchainIndex = 0;

	uint32_t m_windowWidth = InitWidth;
	uint32_t m_windowHeight = InitHeight;

	bool m_windowFullscreen = false;
	bool m_vsync = false;
	bool m_recreateSwapchain = false;
	int m_windowPosX = 0;
	int m_windowPosY = 0;
	int m_windowedWidth = InitWidth;
	int m_windowedHeight = InitHeight;

	// Frame Statistics
	HBFrameStats m_frameStats;
};
