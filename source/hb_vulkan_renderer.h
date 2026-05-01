#pragma once

#include "hb_frame_stats.h"
#include "hb_image_resources.h"

struct GLFWwindow;

class VulkanRenderer {
public:
	static constexpr uint32_t NumFramesInFlight = 2;
	static constexpr uint32_t InitialWindowWidth = 1080;
	static constexpr uint32_t InitialWindowHeight = 1080;

	void init(GLFWwindow* window);
	void initImGui(GLFWwindow* window);
	void handleFramebufferResize(int width, int height);
	void updateImGuiFrame(HBFrameStats& frameStats);
	bool acquireNextFrame();
	void render(HBFrameStats& frameStats);
	void waitIdle();
	void shutdownImGui();
	/** Destroy GPU resources tied to stb-loaded textures (call after waitIdle, before shutdownImGui). */
	void releaseAssetResources();

private:
	void logDeviceInfo(const vk::PhysicalDevice& physicalDevice);
	void recreateSwapChain();
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
	void createTextureDescriptorSetLayout();
	void createTextureResources();
	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

	vk::UniqueInstance m_instance;
	vk::UniqueDebugUtilsMessengerEXT m_debugMessenger;
	vk::PhysicalDevice m_physicalDevice = nullptr;
	vk::UniqueDevice m_device;
	vk::UniqueSurfaceKHR m_surface;

	vkb::Instance m_vkbInstance;
	vkb::PhysicalDevice m_vkbPhysicalDevice;
	vkb::Device m_vkbDevice;

	vk::UniqueSwapchainKHR m_swapChain;
	std::vector<vk::Image> m_swapChainImages;
	vk::Format m_swapChainImageFormat;
	vk::Extent2D m_swapChainExtent;
	std::vector<vk::UniqueImageView> m_swapChainImageViews;

	vk::UniqueDescriptorPool m_descriptorPool;

	vk::UniqueRenderPass m_renderPass;
	vk::UniqueRenderPass m_offscreenRenderPass;
	vk::UniquePipelineLayout m_pipelineLayout;
	vk::UniquePipeline m_graphicsPipeline;
	vk::UniquePipeline m_graphicsPipelineWireframe;

	bool m_quadWireframe = false;

	std::vector<vk::UniqueFramebuffer> m_swapChainFramebuffers;
	vk::UniqueFramebuffer m_offscreenFramebuffer;

	vk::UniqueImage m_offscreenImage;
	vk::UniqueDeviceMemory m_offscreenImageMemory;
	vk::UniqueImageView m_offscreenImageView;
	vk::Format m_offscreenFormat = vk::Format::eB8G8R8A8Unorm;

	vk::UniqueCommandPool m_commandPool;
	std::vector<vk::UniqueCommandBuffer> m_commandBuffers;

	std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
	std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
	vk::UniqueSemaphore m_timelineSemaphore;
	uint64_t m_timelineValue = 0;
	std::vector<uint64_t> m_frameTimelineValues;

	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;

	uint32_t m_graphicsQueueFamilyIndex = 0;
	uint32_t m_presentQueueFamilyIndex = 0;

	uint32_t m_currentFrameIndex = 0;
	uint32_t m_currentSwapchainIndex = 0;

	uint32_t m_windowWidth = InitialWindowWidth;
	uint32_t m_windowHeight = InitialWindowHeight;

	bool m_vsync = false;

	vk::UniqueDescriptorSetLayout m_textureDescriptorSetLayout;
	vk::UniqueSampler m_pixelSampler;
	vk::UniqueDescriptorPool m_gameDescriptorPool;
	vk::DescriptorSet m_textureDescriptorSet = nullptr;
	hb::LoadedTextureRgba8 m_pixelTexture;
	vk::UniqueBuffer m_quadVertexBuffer;
	vk::UniqueDeviceMemory m_quadVertexMemory;
	vk::UniqueBuffer m_quadIndexBuffer;
	vk::UniqueDeviceMemory m_quadIndexMemory;

	vk::UniqueBuffer m_cameraUniformBuffer;
	vk::UniqueDeviceMemory m_cameraUniformMemory;
	void* m_cameraUniformMapped = nullptr;
	vk::DeviceSize m_cameraUniformBufferRange = 0;
};
