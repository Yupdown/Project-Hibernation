#pragma once

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
	void createCommandPool(vkb::Device& vkb_dev);
	void createCommandBuffers();
	void recordCommandBuffer(vk::CommandBuffer& commandBuffer, uint32_t imageIndex);
	void createDescriptorPool();
	void createSyncObjects();
	bool acquireNextFrame();
	void render();
	void mainLoop();
	void updateFrameStats();
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
	vk::UniquePipelineLayout m_pipelineLayout;
	vk::UniquePipeline m_graphicsPipeline;

	// Framebuffers
	std::vector<vk::UniqueFramebuffer> m_swapChainFramebuffers;

	// Command
	vk::UniqueCommandPool m_commandPool;
	std::vector<vk::UniqueCommandBuffer> m_commandBuffers;

	// Synchronization
	std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
	std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
	std::vector<vk::UniqueFence> m_inFlightFences;

	// Queues
	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;

	uint32_t m_graphicsQueueFamilyIndex = 0;
	uint32_t m_presentQueueFamilyIndex = 0;

	uint32_t m_currentFrameIndex = 0;
	uint32_t m_currentSwapchainIndex = 0;

	// Frame rate measurement
	double m_lastTime = 0.0;
	int m_frameCount = 0;
	double m_fps = 0.0;

	uint32_t m_windowWidth = InitWidth;
	uint32_t m_windowHeight = InitHeight;

	bool m_windowFullscreen = false;
	bool m_vsync = false;
	bool m_recreateSwapchain = false;
	int m_windowPosX = 0;
	int m_windowPosY = 0;
	int m_windowedWidth = InitWidth;
	int m_windowedHeight = InitHeight;

	// Frame time histogram data
	static constexpr size_t FRAME_HISTORY_SIZE = 100;
	static constexpr double GRAPH_UPDATE_INTERVAL = 1.0 / 120.0;  // 60 FPS interval
	static constexpr double FPS_UPDATE_INTERVAL = 1.0 / 20.0;    // 20 times per second

	struct FrameTimeStats {
		float avg = 0.0f;
		float min = 0.0f;
		float max = 0.0f;
	};

	std::array<FrameTimeStats, FRAME_HISTORY_SIZE> m_frameTimeHistory;
	double m_lastFrameTime = 0.0;
	double m_graphUpdateTimer = 0.0;

	float m_currentFrameTime = 0.0f;
	float m_minFrameTime = FLT_MAX;
	float m_maxFrameTime = 0.0f;
	float m_avgFrameTime = 0.0f;
	float m_frameAxisLimit = 0.0f;

	// Accumulator for current graph point
	std::vector<float> m_frameTimeAccumulator;
};
