#include "stdafx.h"
#include "hb_vulkan_renderer.h"
#include "hb_resource_path.h"
#include "compiled_shaders/shaders.h"

#ifdef NDEBUG
constexpr bool g_enableValidationLayers = false;
#else
constexpr bool g_enableValidationLayers = true;
#endif

void VulkanRenderer::logDeviceInfo(const vk::PhysicalDevice& physicalDevice) {
	vk::PhysicalDeviceProperties deviceProperties = physicalDevice.getProperties();
	vk::PhysicalDeviceFeatures deviceFeatures = physicalDevice.getFeatures();
	std::string deviceTypeString;
	switch (deviceProperties.deviceType) {
	case vk::PhysicalDeviceType::eOther:
		deviceTypeString = "Other";
		break;
	case vk::PhysicalDeviceType::eIntegratedGpu:
		deviceTypeString = "Integrated GPU";
		break;
	case vk::PhysicalDeviceType::eDiscreteGpu:
		deviceTypeString = "Discrete GPU";
		break;
	case vk::PhysicalDeviceType::eVirtualGpu:
		deviceTypeString = "Virtual GPU";
		break;
	case vk::PhysicalDeviceType::eCpu:
		deviceTypeString = "CPU";
		break;
	default:
		deviceTypeString = "Unknown";
		break;
	}
	std::cout << " > Selected GPU: " << deviceProperties.deviceName << std::endl;
	std::cout << " > Device Type: " << deviceTypeString << std::endl;
	std::cout << " > API Version: "
		<< VK_API_VERSION_MAJOR(deviceProperties.apiVersion) << "."
		<< VK_API_VERSION_MINOR(deviceProperties.apiVersion) << "."
		<< VK_API_VERSION_PATCH(deviceProperties.apiVersion) << std::endl;
	std::cout << " > Driver Version: " << deviceProperties.driverVersion << std::endl;
	std::cout << " > Vendor ID: " << deviceProperties.vendorID << std::endl;
	std::cout << " > Device ID: " << deviceProperties.deviceID << std::endl;
	std::cout << " > Geometry Shader Support: "
		<< (deviceFeatures.geometryShader ? "Yes" : "No") << std::endl;
}

void VulkanRenderer::init(GLFWwindow* window) {
#pragma region Volk Initialization
	// Initialize volk
	 VkResult result = volkInitialize(); 
	if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to initialize volk");
	}

	std::cout << "> Volk Initialization" << std::endl;
#pragma endregion

#pragma region Vulkan Instance Creation
	// 1. Instance Creation using vk-bootstrap
	vkb::InstanceBuilder builder;
	auto instanceRet = builder.set_app_name("Project Hibernation")
		.request_validation_layers(g_enableValidationLayers)
		.require_api_version(1, 2, 0)
		.use_default_debug_messenger()
		.build();

	if (!instanceRet) {
		throw std::runtime_error("Failed to create Vulkan instance: " + instanceRet.error().message());
	}

	vkb::Instance vkb_instance = instanceRet.value();
	vk::Instance vk_instance = vkb_instance.instance;

	// Load instance functions for volk
	volkLoadInstance(vk_instance);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_instance, vkGetInstanceProcAddr);

	m_instance = vk::UniqueInstance(vk_instance,
		vk::detail::ObjectDestroy<vk::detail::NoParent, vk::detail::DispatchLoaderDynamic>(nullptr, VULKAN_HPP_DEFAULT_DISPATCHER)
	);
	m_vkbInstance = vkb_instance;

	// Debug Messenger is handled by vkb
	if (g_enableValidationLayers) {
		m_debugMessenger = vk::UniqueDebugUtilsMessengerEXT(vkb_instance.debug_messenger, *m_instance);
	}

	std::cout << "> Vulkan Instance Creation (vk-bootstrap)" << std::endl;
#pragma endregion

#pragma region Surface Creation
	// 2. Surface Creation
	VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
	VkResult glfw_result = glfwCreateWindowSurface(*m_instance, window, nullptr, &rawSurface);
	if (glfw_result != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface");
	}
	m_surface = vk::UniqueSurfaceKHR(rawSurface, *m_instance);

	std::cout << "> Surface Creation (GLFW)" << std::endl;
#pragma endregion

#pragma region Physical Device and Logical Device Creation
	// 3. Physical Device Selection using vk-bootstrap
	vkb::PhysicalDeviceSelector selector(vkb_instance);
	auto physRet = selector.set_surface(*m_surface)
		.add_required_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)
		.select_devices();

	if (!physRet) {
		throw std::runtime_error("Failed to select physical device: " + physRet.error().message());
	}

	std::cout << "> Physical Device Selection (vk-bootstrap)" << std::endl;
	std::cout << " > Available Physical Devices: " << physRet.value().size() << " Devices" << std::endl;

	vkb::PhysicalDevice vkb_physicalDevice = physRet.value().front();
	logDeviceInfo(vkb_physicalDevice.physical_device);

	VkPhysicalDeviceVulkan12Features features12 = {};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.timelineSemaphore = VK_TRUE;

	// 4. Logical Device Creation using vk-bootstrap
	vkb::DeviceBuilder deviceBuilder{ vkb_physicalDevice };
	auto devRet = deviceBuilder.add_pNext(&features12).build();

	if (!devRet) {
		throw std::runtime_error("Failed to create logical device: " + devRet.error().message());
	}

	vkb::Device vkb_device = devRet.value();
	vk::Device vk_device = vkb_device.device;

	// Load device functions for volk
	volkLoadDevice(vk_device);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(vk_device);

	m_physicalDevice = vkb_physicalDevice.physical_device;
	m_vkbPhysicalDevice = vkb_physicalDevice;
	m_device = vk::UniqueDevice(vk_device,
		vk::detail::ObjectDestroy<vk::detail::NoParent, vk::detail::DispatchLoaderDynamic>(nullptr, VULKAN_HPP_DEFAULT_DISPATCHER)
	);
	m_vkbDevice = vkb_device;

	std::cout << "> Logical Device Creation (vk-bootstrap)" << std::endl;
#pragma endregion

#pragma region Queue Retrieval
	auto graphicsQueueRet = vkb_device.get_queue(vkb::QueueType::graphics);
	auto graphicsQueueIndexRet = vkb_device.get_queue_index(vkb::QueueType::graphics);
	if (!graphicsQueueRet) {
		std::cerr << "Failed to get graphics queue. Error: " << graphicsQueueRet.error().message() << "\n";
	}
	if (!graphicsQueueIndexRet) {
		std::cerr << "Failed to get graphics queue index. Error: " << graphicsQueueIndexRet.error().message() << "\n";
	}
	auto presentQueueRet = vkb_device.get_queue(vkb::QueueType::present);
	auto presentQueueIndexRet = vkb_device.get_queue_index(vkb::QueueType::present);
	if (!presentQueueRet) {
		std::cerr << "Failed to get present queue. Error: " << presentQueueRet.error().message() << "\n";
	}
	if (!presentQueueIndexRet) {
		std::cerr << "Failed to get present queue index. Error: " << presentQueueIndexRet.error().message() << "\n";
	}

	// Get Queues
	m_graphicsQueue = graphicsQueueRet.value();
	m_presentQueue = presentQueueRet.value();

	m_graphicsQueueFamilyIndex = graphicsQueueIndexRet.value();
	m_presentQueueFamilyIndex = presentQueueIndexRet.value();

	std::cout << "> Queue Retrieval (vk-bootstrap)" << std::endl;
#pragma endregion

	createSwapChain(m_vkbDevice);
	createImageViews();
	createRenderPass();
	createOffscreenResources();
	createTextureDescriptorSetLayout();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool(m_vkbDevice);
	createCommandBuffers();
	createDescriptorPool();
	createSyncObjects();
	createTextureResources();
}

void VulkanRenderer::recreateSwapChain() {
	m_device->waitIdle();

	m_graphicsPipeline.reset();
	m_pipelineLayout.reset();

	m_offscreenFramebuffer.reset();
	m_offscreenRenderPass.reset();
	m_offscreenImageView.reset();
	m_offscreenImage.reset();
	m_offscreenImageMemory.reset();

	createSwapChain(m_vkbDevice);
	createImageViews();
	createOffscreenResources();
	createGraphicsPipeline();
	createFramebuffers();
}

void VulkanRenderer::handleFramebufferResize(int width, int height) {
	m_windowWidth = static_cast<uint32_t>(width);
	m_windowHeight = static_cast<uint32_t>(height);

	if (m_windowWidth == 0 || m_windowHeight == 0) {
		return;
	}

	recreateSwapChain();
}

void VulkanRenderer::initImGui(GLFWwindow* imguiWindow) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	// Enable Gamepad Controls

	if (!ImGui_ImplGlfw_InitForVulkan(imguiWindow, true)) {
		std::cerr << "[ImGUI Vulkan] ImGui_ImplGlfw Initialization failed!" << std::endl;
	}

	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.ApiVersion = m_physicalDevice.getProperties().apiVersion;
	initInfo.Instance = *m_instance;
	initInfo.PhysicalDevice = m_physicalDevice;
	initInfo.Device = *m_device;
	initInfo.QueueFamily = m_graphicsQueueFamilyIndex;
	initInfo.DescriptorPool = *m_descriptorPool;
	initInfo.Queue = m_graphicsQueue;
	initInfo.PipelineInfoMain.RenderPass = *m_renderPass;
	initInfo.MinImageCount = NumFramesInFlight;
	initInfo.ImageCount = static_cast<uint32_t>(m_swapChainImages.size());
	initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.CheckVkResultFn = [](VkResult err) {
		if (err != VK_SUCCESS) {
			std::cerr << "[ImGUI Vulkan] Error: " << err << std::endl;
		}
		};
	if (!ImGui_ImplVulkan_Init(&initInfo)) {
		std::cerr << "[ImGUI Vulkan] ImGui_ImplVulkan Initialization failed!" << std::endl;
	}

	std::cout << "> ImGUI Initialization" << std::endl;
}

void VulkanRenderer::createSwapChain(vkb::Device& vkb_dev) {
	vkb::SwapchainBuilder swapchainBuilder(vkb_dev);
	VkPresentModeKHR presentMode = m_vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

	auto swapRet = swapchainBuilder.set_old_swapchain(*m_swapChain)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.set_desired_extent(m_windowWidth, m_windowHeight)
		.set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(presentMode)
		.set_clipped(true)
		.set_composite_alpha_flags(VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		.set_required_min_image_count(NumFramesInFlight)
		.build();

	if (!swapRet) {
		// Fallback to FIFO present mode if requested mode is not available
		std::cout << "Requested present mode not available, falling back to FIFO." << std::endl;
		swapchainBuilder.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR);
		swapRet = swapchainBuilder.build();
		if (!swapRet) {
			throw std::runtime_error("Failed to create swapchain: " + swapRet.error().message());
		}
	}

	vkb::Swapchain vkb_swap = swapRet.value();
	m_swapChain = vk::UniqueSwapchainKHR(vkb_swap.swapchain, *m_device);

	auto images = vkb_swap.get_images();
	if (!images) {
		throw std::runtime_error("Failed to get swapchain images");
	}

	m_swapChainImages.clear();
	for (auto& img : images.value()) {
		m_swapChainImages.emplace_back(img);
	}

	m_swapChainImageFormat = static_cast<vk::Format>(vkb_swap.image_format);
	m_swapChainExtent = vk::Extent2D(vkb_swap.extent.width, vkb_swap.extent.height);

	std::cout << "> Swap Chain Creation (vk-bootstrap): (" << m_swapChainExtent.width << ", " << m_swapChainExtent.height << ")" << std::endl;
}

void VulkanRenderer::createImageViews() {
	m_swapChainImageViews.clear();
	m_swapChainImageViews.reserve(m_swapChainImages.size());

	for (const auto& image : m_swapChainImages) {
		vk::ImageViewCreateInfo createInfo = {};
		createInfo.sType = vk::StructureType::eImageViewCreateInfo;
		createInfo.pNext = nullptr;
		createInfo.flags = {};
		createInfo.image = image;
		createInfo.viewType = vk::ImageViewType::e2D;
		createInfo.format = m_swapChainImageFormat;
		createInfo.components.r = vk::ComponentSwizzle::eIdentity;
		createInfo.components.g = vk::ComponentSwizzle::eIdentity;
		createInfo.components.b = vk::ComponentSwizzle::eIdentity;
		createInfo.components.a = vk::ComponentSwizzle::eIdentity;
		createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		m_swapChainImageViews.emplace_back(m_device->createImageViewUnique(createInfo));
	}
}

void VulkanRenderer::createRenderPass() {
	vk::AttachmentDescription colorAttachment = {};
	colorAttachment.flags = {};
	colorAttachment.format = m_swapChainImageFormat;
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad; // Change to eLoad
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal; // Change to eColorAttachmentOptimal
	colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	vk::AttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpass = {};
	subpass.flags = {};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = nullptr;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pResolveAttachments = nullptr;
	subpass.pDepthStencilAttachment = nullptr;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = nullptr;

	vk::SubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.srcAccessMask = {};
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	dependency.dependencyFlags = {};

	vk::RenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = vk::StructureType::eRenderPassCreateInfo;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.flags = {};
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	m_renderPass = m_device->createRenderPassUnique(renderPassInfo);
}

void VulkanRenderer::createGraphicsPipeline() {
	vk::ShaderModuleCreateInfo vertShaderInfo = {};
	vertShaderInfo.sType = vk::StructureType::eShaderModuleCreateInfo;
	vertShaderInfo.pNext = nullptr;
	vertShaderInfo.flags = {};
	vertShaderInfo.codeSize = sizeof(compiled_shaders::g_shader_vert);
	vertShaderInfo.pCode = compiled_shaders::g_shader_vert;

	vk::ShaderModuleCreateInfo fragShaderInfo = {};
	fragShaderInfo.sType = vk::StructureType::eShaderModuleCreateInfo;
	fragShaderInfo.pNext = nullptr;
	fragShaderInfo.flags = {};
	fragShaderInfo.codeSize = sizeof(compiled_shaders::g_shader_frag);
	fragShaderInfo.pCode = compiled_shaders::g_shader_frag;

	vk::UniqueShaderModule vertShaderModule = m_device->createShaderModuleUnique(vertShaderInfo);
	vk::UniqueShaderModule fragShaderModule = m_device->createShaderModuleUnique(fragShaderInfo);

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = vk::StructureType::ePipelineShaderStageCreateInfo;
	vertShaderStageInfo.pNext = nullptr;
	vertShaderStageInfo.flags = {};
	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = *vertShaderModule;
	vertShaderStageInfo.pName = "main";
	vertShaderStageInfo.pSpecializationInfo = nullptr;

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = vk::StructureType::ePipelineShaderStageCreateInfo;
	fragShaderStageInfo.pNext = nullptr;
	fragShaderStageInfo.flags = {};
	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = *fragShaderModule;
	fragShaderStageInfo.pName = "main";
	fragShaderStageInfo.pSpecializationInfo = nullptr;

	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	vk::VertexInputBindingDescription vertexBinding{};
	vertexBinding.binding = 0;
	vertexBinding.stride = sizeof(glm::vec2) * 2;
	vertexBinding.inputRate = vk::VertexInputRate::eVertex;

	std::array<vk::VertexInputAttributeDescription, 2> vertexAttributes{};
	vertexAttributes[0].location = 0;
	vertexAttributes[0].binding = 0;
	vertexAttributes[0].format = vk::Format::eR32G32Sfloat;
	vertexAttributes[0].offset = 0;
	vertexAttributes[1].location = 1;
	vertexAttributes[1].binding = 0;
	vertexAttributes[1].format = vk::Format::eR32G32Sfloat;
	vertexAttributes[1].offset = sizeof(glm::vec2);

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = vk::StructureType::ePipelineVertexInputStateCreateInfo;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.flags = {};
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = vk::StructureType::ePipelineInputAssemblyStateCreateInfo;
	inputAssembly.pNext = nullptr;
	inputAssembly.flags = {};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	vk::Viewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_swapChainExtent.width;
	viewport.height = (float)m_swapChainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vk::Rect2D scissor = {};
	scissor.offset = vk::Offset2D{ 0, 0 };
	scissor.extent = m_swapChainExtent;

	vk::PipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = vk::StructureType::ePipelineViewportStateCreateInfo;
	viewportState.pNext = nullptr;
	viewportState.flags = {};
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	vk::DynamicState dynamicStates[] = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	vk::PipelineDynamicStateCreateInfo dynamicStateInfo = {};
	dynamicStateInfo.sType = vk::StructureType::ePipelineDynamicStateCreateInfo;
	dynamicStateInfo.pNext = nullptr;
	dynamicStateInfo.flags = {};
	dynamicStateInfo.dynamicStateCount = 2;
	dynamicStateInfo.pDynamicStates = dynamicStates;

	vk::PipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = vk::StructureType::ePipelineRasterizationStateCreateInfo;
	rasterizer.pNext = nullptr;
	rasterizer.flags = {};
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.cullMode = vk::CullModeFlagBits::eNone;
	rasterizer.frontFace = vk::FrontFace::eClockwise;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;
	rasterizer.lineWidth = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = vk::StructureType::ePipelineMultisampleStateCreateInfo;
	multisampling.pNext = nullptr;
	multisampling.flags = {};
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	vk::PipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = vk::StructureType::ePipelineColorBlendStateCreateInfo;
	colorBlending.pNext = nullptr;
	colorBlending.flags = {};
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	vk::DescriptorSetLayout dsl = *m_textureDescriptorSetLayout;

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = vk::StructureType::ePipelineLayoutCreateInfo;
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = {};
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &dsl;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	m_pipelineLayout = m_device->createPipelineLayoutUnique(pipelineLayoutInfo);

	vk::GraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = vk::StructureType::eGraphicsPipelineCreateInfo;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pTessellationState = nullptr;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = *m_pipelineLayout;
	pipelineInfo.renderPass = *m_offscreenRenderPass;
	pipelineInfo.subpass = 0;

	m_graphicsPipeline = m_device->createGraphicsPipelineUnique(nullptr, pipelineInfo).value;
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
	vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void VulkanRenderer::createOffscreenResources() {
	uint32_t width = std::max(1u, m_swapChainExtent.width / 10);
	uint32_t height = std::max(1u, m_swapChainExtent.height / 10);

	// Create Offscreen Image
	vk::ImageCreateInfo imageInfo = {};
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.extent = vk::Extent3D{ width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = m_offscreenFormat;
	imageInfo.tiling = vk::ImageTiling::eOptimal;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;
	imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;

	m_offscreenImage = m_device->createImageUnique(imageInfo);

	// Allocate Memory
	vk::MemoryRequirements memRequirements = m_device->getImageMemoryRequirements(*m_offscreenImage);

	vk::MemoryAllocateInfo allocInfo = {};
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

	m_offscreenImageMemory = m_device->allocateMemoryUnique(allocInfo);
	m_device->bindImageMemory(*m_offscreenImage, *m_offscreenImageMemory, 0);

	// Create Image View
	vk::ImageViewCreateInfo viewInfo = {};
	viewInfo.image = *m_offscreenImage;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = m_offscreenFormat;
	viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	m_offscreenImageView = m_device->createImageViewUnique(viewInfo);

	// Create Offscreen Render Pass
	vk::AttachmentDescription colorAttachment = {};
	colorAttachment.format = m_offscreenFormat;
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachment.finalLayout = vk::ImageLayout::eTransferSrcOptimal;

	vk::AttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::SubpassDescription subpass = {};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	// Dependency ensures writing finishes before blit
	vk::SubpassDependency dependency = {};
	dependency.srcSubpass = 0;
	dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstStageMask = vk::PipelineStageFlagBits::eTransfer;
	dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	dependency.dstAccessMask = vk::AccessFlagBits::eTransferRead;

	vk::RenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	m_offscreenRenderPass = m_device->createRenderPassUnique(renderPassInfo);

	// Create Offscreen Framebuffer
	vk::ImageView attachments[] = { *m_offscreenImageView };

	vk::FramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.renderPass = *m_offscreenRenderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = attachments;
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;

	m_offscreenFramebuffer = m_device->createFramebufferUnique(framebufferInfo);
}

void VulkanRenderer::createFramebuffers() {
	m_swapChainFramebuffers.clear();
	m_swapChainFramebuffers.reserve(m_swapChainImageViews.size());

	for (const auto& imageView : m_swapChainImageViews) {
		vk::ImageView attachments[] = { *imageView };

		vk::FramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = vk::StructureType::eFramebufferCreateInfo;
		framebufferInfo.pNext = nullptr;
		framebufferInfo.flags = {};
		framebufferInfo.renderPass = *m_renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_swapChainExtent.width;
		framebufferInfo.height = m_swapChainExtent.height;
		framebufferInfo.layers = 1;

		m_swapChainFramebuffers.emplace_back(m_device->createFramebufferUnique(framebufferInfo));
	}
}

void VulkanRenderer::createCommandPool(vkb::Device& vkb_dev) {
	auto queueIndexRet = vkb_dev.get_queue_index(vkb::QueueType::graphics);
	if (!queueIndexRet) {
		throw std::runtime_error("Failed to get graphics queue index");
	}
	uint32_t graphicsQueueFamilyIndex = queueIndexRet.value();

	vk::CommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = vk::StructureType::eCommandPoolCreateInfo;
	poolInfo.pNext = nullptr;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

	m_commandPool = m_device->createCommandPoolUnique(poolInfo);
}

void VulkanRenderer::createCommandBuffers() {
	vk::CommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
	allocInfo.pNext = nullptr;
	allocInfo.commandPool = *m_commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = NumFramesInFlight;

	m_commandBuffers = m_device->allocateCommandBuffersUnique(allocInfo);
}

void VulkanRenderer::recordCommandBuffer(vk::CommandBuffer& commandBuffer, uint32_t imageIndex) {
	vk::CommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = vk::StructureType::eCommandBufferBeginInfo;
	beginInfo.pNext = nullptr;
	beginInfo.flags = {};
	beginInfo.pInheritanceInfo = nullptr;

	commandBuffer.begin(beginInfo);

	uint32_t offscreenWidth = std::max(1u, m_swapChainExtent.width / 10);
	uint32_t offscreenHeight = std::max(1u, m_swapChainExtent.height / 10);

	// Phase A: Offscreen Render Pass
	vk::ClearValue offscreenClearColor(std::array<float, 4>{0.2f, 0.2f, 0.2f, 1.0f});

	vk::RenderPassBeginInfo offscreenRenderPassInfo = {};
	offscreenRenderPassInfo.sType = vk::StructureType::eRenderPassBeginInfo;
	offscreenRenderPassInfo.pNext = nullptr;
	offscreenRenderPassInfo.renderPass = *m_offscreenRenderPass;
	offscreenRenderPassInfo.framebuffer = *m_offscreenFramebuffer;
	offscreenRenderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	offscreenRenderPassInfo.renderArea.extent = vk::Extent2D{ offscreenWidth, offscreenHeight };
	offscreenRenderPassInfo.clearValueCount = 1;
	offscreenRenderPassInfo.pClearValues = &offscreenClearColor;

	commandBuffer.beginRenderPass(offscreenRenderPassInfo, vk::SubpassContents::eInline);

	{
		// Fit image inside window (same aspect as swapchain) preserving texture aspect ratio (contain).
		float winW = static_cast<float>(std::max(1u, m_swapChainExtent.width));
		float winH = static_cast<float>(std::max(1u, m_swapChainExtent.height));
		float a_win = winW / winH;
		float imgW = static_cast<float>(std::max(1u, m_pixelTexture.width));
		float imgH = static_cast<float>(std::max(1u, m_pixelTexture.height));
		float a_img = imgW / imgH;
		float sx = 1.f;
		float sy = 1.f;
		if (a_win >= a_img) {
			sy = 1.f;
			sx = a_img / a_win;
		}
		else {
			sx = 1.f;
			sy = a_win / a_img;
		}
		glm::mat4 projection = glm::scale(glm::mat4(1.f), glm::vec3(sx, sy, 1.f));
		std::memcpy(m_cameraUniformMapped, &projection, sizeof(glm::mat4));
	}

	commandBuffer.setViewport(0, vk::Viewport{
		0.0f, 0.0f,
		static_cast<float>(offscreenWidth),
		static_cast<float>(offscreenHeight),
		0.0f, 1.0f
		});
	commandBuffer.setScissor(0, vk::Rect2D{
		vk::Offset2D{ 0, 0 },
		vk::Extent2D{ offscreenWidth, offscreenHeight }
		});

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);

	vk::Buffer vb = *m_quadVertexBuffer;
	vk::DeviceSize vbOffset = 0;
	commandBuffer.bindVertexBuffers(0, 1, &vb, &vbOffset);

	vk::DescriptorSet texSets[] = { m_textureDescriptorSet };
	commandBuffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		*m_pipelineLayout,
		0,
		1,
		texSets,
		0,
		nullptr);

	commandBuffer.draw(6, 1, 0, 0);

	commandBuffer.endRenderPass();

	// Phase B: Blit to Swapchain Image
	// Barrier to transition swapchain image to TransferDstOptimal
	vk::ImageMemoryBarrier barrierToDst = {};
	barrierToDst.oldLayout = vk::ImageLayout::eUndefined;
	barrierToDst.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrierToDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToDst.image = m_swapChainImages[imageIndex];
	barrierToDst.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrierToDst.subresourceRange.baseMipLevel = 0;
	barrierToDst.subresourceRange.levelCount = 1;
	barrierToDst.subresourceRange.baseArrayLayer = 0;
	barrierToDst.subresourceRange.layerCount = 1;
	barrierToDst.srcAccessMask = {};
	barrierToDst.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
		{},
		nullptr, nullptr, barrierToDst
	);

	// Blit operation
	vk::ImageBlit blit = {};
	blit.srcOffsets[0] = vk::Offset3D{ 0, 0, 0 };
	blit.srcOffsets[1] = vk::Offset3D{ static_cast<int>(offscreenWidth), static_cast<int>(offscreenHeight), 1 };
	blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.dstOffsets[0] = vk::Offset3D{ 0, 0, 0 };
	blit.dstOffsets[1] = vk::Offset3D{ static_cast<int>(m_swapChainExtent.width), static_cast<int>(m_swapChainExtent.height), 1 };
	blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	blit.dstSubresource.mipLevel = 0;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;

	commandBuffer.blitImage(
		*m_offscreenImage, vk::ImageLayout::eTransferSrcOptimal,
		m_swapChainImages[imageIndex], vk::ImageLayout::eTransferDstOptimal,
		blit, vk::Filter::eNearest
	);

	// Barrier to transition swapchain image to ColorAttachmentOptimal for ImGui
	vk::ImageMemoryBarrier barrierToColor = {};
	barrierToColor.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrierToColor.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
	barrierToColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierToColor.image = m_swapChainImages[imageIndex];
	barrierToColor.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrierToColor.subresourceRange.baseMipLevel = 0;
	barrierToColor.subresourceRange.levelCount = 1;
	barrierToColor.subresourceRange.baseArrayLayer = 0;
	barrierToColor.subresourceRange.layerCount = 1;
	barrierToColor.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrierToColor.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eColorAttachmentOutput,
		{},
		nullptr, nullptr, barrierToColor
	);

	// Phase C: ImGui Rendering (Main Render Pass over Blitted Image)
	vk::RenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = vk::StructureType::eRenderPassBeginInfo;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.renderPass = *m_renderPass;
	renderPassInfo.framebuffer = *m_swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = vk::Offset2D{ 0, 0 };
	renderPassInfo.renderArea.extent = m_swapChainExtent;
	// No clear color since we are loading the blitted image

	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	commandBuffer.endRenderPass();
	commandBuffer.end();
}

void VulkanRenderer::createDescriptorPool() {
	vk::DescriptorPoolSize poolSizes[] = {
		{ vk::DescriptorType::eSampler, 1000 },
		{ vk::DescriptorType::eCombinedImageSampler, 1000 },
		{ vk::DescriptorType::eSampledImage, 1000 },
		{ vk::DescriptorType::eStorageImage, 1000 },
		{ vk::DescriptorType::eUniformTexelBuffer, 1000 },
		{ vk::DescriptorType::eStorageTexelBuffer, 1000 },
		{ vk::DescriptorType::eUniformBuffer, 1000 },
		{ vk::DescriptorType::eStorageBuffer, 1000 },
		{ vk::DescriptorType::eUniformBufferDynamic, 1000 },
		{ vk::DescriptorType::eStorageBufferDynamic, 1000 },
		{ vk::DescriptorType::eInputAttachment, 1000 }
	};

	vk::DescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = vk::StructureType::eDescriptorPoolCreateInfo;
	poolInfo.pNext = nullptr;
	poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	poolInfo.maxSets = 0;
	for (vk::DescriptorPoolSize& poolSize : poolSizes)
		poolInfo.maxSets += poolSize.descriptorCount;
	poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	m_descriptorPool = m_device->createDescriptorPoolUnique(poolInfo);
}

void VulkanRenderer::createSyncObjects() {
	m_imageAvailableSemaphores.clear();
	m_renderFinishedSemaphores.clear();
	m_frameTimelineValues.clear();

	m_imageAvailableSemaphores.reserve(NumFramesInFlight);
	m_renderFinishedSemaphores.reserve(NumFramesInFlight);
	m_frameTimelineValues.resize(NumFramesInFlight, 0);

	vk::SemaphoreCreateInfo semaphoreInfo = {};

	for (size_t i = 0; i < NumFramesInFlight; ++i) {
		m_imageAvailableSemaphores.emplace_back(m_device->createSemaphoreUnique(semaphoreInfo));
		m_renderFinishedSemaphores.emplace_back(m_device->createSemaphoreUnique(semaphoreInfo));
	}

	vk::SemaphoreTypeCreateInfo timelineCreateInfo;
	timelineCreateInfo.semaphoreType = vk::SemaphoreType::eTimeline;
	timelineCreateInfo.initialValue = 0;

	vk::SemaphoreCreateInfo timelineSemaphoreInfo;
	timelineSemaphoreInfo.pNext = &timelineCreateInfo;
	
	m_timelineSemaphore = m_device->createSemaphoreUnique(timelineSemaphoreInfo);
	m_timelineValue = 0;
}

void VulkanRenderer::createTextureDescriptorSetLayout() {
	vk::DescriptorSetLayoutBinding bindings[2]{};
	bindings[0].binding = 0;
	bindings[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = vk::ShaderStageFlagBits::eFragment;

	bindings[1].binding = 1;
	bindings[1].descriptorType = vk::DescriptorType::eUniformBuffer;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = vk::ShaderStageFlagBits::eVertex;

	vk::DescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;

	m_textureDescriptorSetLayout = m_device->createDescriptorSetLayoutUnique(layoutInfo);
}

void VulkanRenderer::createTextureResources() {
	vk::SamplerCreateInfo samp{};
	samp.magFilter = vk::Filter::eNearest;
	samp.minFilter = vk::Filter::eNearest;
	samp.mipmapMode = vk::SamplerMipmapMode::eNearest;
	samp.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samp.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samp.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samp.mipLodBias = 0.f;
	samp.anisotropyEnable = VK_FALSE;
	samp.maxAnisotropy = 1.f;
	samp.compareEnable = VK_FALSE;
	samp.minLod = 0.f;
	samp.maxLod = 0.f;
	samp.borderColor = vk::BorderColor::eIntOpaqueBlack;
	m_pixelSampler = m_device->createSamplerUnique(samp);

	vk::PhysicalDeviceProperties physProps = m_physicalDevice.getProperties();
	vk::DeviceSize uboAlign = physProps.limits.minUniformBufferOffsetAlignment;
	m_cameraUniformBufferRange = sizeof(glm::mat4);
	if (uboAlign > 0) {
		m_cameraUniformBufferRange = (m_cameraUniformBufferRange + uboAlign - 1) / uboAlign * uboAlign;
	}

	vk::BufferCreateInfo camBufInfo{};
	camBufInfo.size = m_cameraUniformBufferRange;
	camBufInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;
	camBufInfo.sharingMode = vk::SharingMode::eExclusive;
	m_cameraUniformBuffer = m_device->createBufferUnique(camBufInfo);

	vk::MemoryRequirements camReq = m_device->getBufferMemoryRequirements(*m_cameraUniformBuffer);
	vk::MemoryAllocateInfo camAlloc{};
	camAlloc.allocationSize = camReq.size;
	camAlloc.memoryTypeIndex = findMemoryType(
		camReq.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	m_cameraUniformMemory = m_device->allocateMemoryUnique(camAlloc);
	m_device->bindBufferMemory(*m_cameraUniformBuffer, *m_cameraUniformMemory, 0);
	m_cameraUniformMapped = m_device->mapMemory(*m_cameraUniformMemory, 0, camReq.size);

	vk::DescriptorPoolSize poolSizes[2]{};
	poolSizes[0].type = vk::DescriptorType::eCombinedImageSampler;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = vk::DescriptorType::eUniformBuffer;
	poolSizes[1].descriptorCount = 1;

	vk::DescriptorPoolCreateInfo poolInfo{};
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	m_gameDescriptorPool = m_device->createDescriptorPoolUnique(poolInfo);

	vk::DescriptorSetLayout dsl = *m_textureDescriptorSetLayout;
	vk::DescriptorSetAllocateInfo allocInfo{};
	allocInfo.descriptorPool = *m_gameDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &dsl;
	m_textureDescriptorSet = m_device->allocateDescriptorSets(allocInfo).front();

	const std::filesystem::path imagePath = hb::resourceRootDirectory() / "b13.png";
	m_pixelTexture = hb::uploadRgba8TextureFromFile(
		m_physicalDevice,
		*m_device,
		m_graphicsQueue,
		*m_commandPool,
		imagePath);

	vk::DescriptorImageInfo imgInfo{};
	imgInfo.sampler = *m_pixelSampler;
	imgInfo.imageView = *m_pixelTexture.view;
	imgInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

	vk::DescriptorBufferInfo camBufInfoDesc{};
	camBufInfoDesc.buffer = *m_cameraUniformBuffer;
	camBufInfoDesc.offset = 0;
	camBufInfoDesc.range = sizeof(glm::mat4);

	std::array<vk::WriteDescriptorSet, 2> writes{};
	writes[0].dstSet = m_textureDescriptorSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
	writes[0].pImageInfo = &imgInfo;

	writes[1].dstSet = m_textureDescriptorSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = vk::DescriptorType::eUniformBuffer;
	writes[1].pBufferInfo = &camBufInfoDesc;

	m_device->updateDescriptorSets(writes, nullptr);

	struct Vertex {
		glm::vec2 pos;
		glm::vec2 uv;
	};
	const vk::DeviceSize vbSize = sizeof(Vertex) * 6;
	vk::BufferCreateInfo vbInfo{};
	vbInfo.size = vbSize;
	vbInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	vbInfo.sharingMode = vk::SharingMode::eExclusive;
	m_quadVertexBuffer = m_device->createBufferUnique(vbInfo);

	vk::MemoryRequirements vbReq = m_device->getBufferMemoryRequirements(*m_quadVertexBuffer);
	vk::MemoryAllocateInfo vbAlloc{};
	vbAlloc.allocationSize = vbReq.size;
	vbAlloc.memoryTypeIndex = findMemoryType(
		vbReq.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
	m_quadVertexMemory = m_device->allocateMemoryUnique(vbAlloc);
	m_device->bindBufferMemory(*m_quadVertexBuffer, *m_quadVertexMemory, 0);

	// UVs flipped vertically (v -> 1-v) so sampling matches stb top-first row order in the texture.
	const Vertex quad[6] = {
		{ { -1.f, 1.f }, { 0.f, 1.f } },
		{ { 1.f, 1.f }, { 1.f, 1.f } },
		{ { -1.f, -1.f }, { 0.f, 0.f } },
		{ { 1.f, 1.f }, { 1.f, 1.f } },
		{ { 1.f, -1.f }, { 1.f, 0.f } },
		{ { -1.f, -1.f }, { 0.f, 0.f } },
	};

	void* vbMap = m_device->mapMemory(*m_quadVertexMemory, 0, vbSize);
	std::memcpy(vbMap, quad, static_cast<size_t>(vbSize));
	m_device->unmapMemory(*m_quadVertexMemory);
}

void VulkanRenderer::releaseAssetResources() {
	if (!m_device) {
		return;
	}
	m_graphicsPipeline.reset();
	m_pipelineLayout.reset();
	m_textureDescriptorSet = nullptr;
	m_gameDescriptorPool.reset();
	m_pixelSampler.reset();
	if (m_cameraUniformMapped) {
		m_device->unmapMemory(*m_cameraUniformMemory);
		m_cameraUniformMapped = nullptr;
	}
	m_quadVertexBuffer.reset();
	m_quadVertexMemory.reset();
	m_cameraUniformBuffer.reset();
	m_cameraUniformMemory.reset();
	m_pixelTexture = {};
	m_textureDescriptorSetLayout.reset();
}

bool VulkanRenderer::acquireNextFrame() {
	// Check if window is minimized
	if (m_windowWidth == 0 || m_windowHeight == 0) {
		// Skip rendering when minimized
		return false;
	}

	m_currentFrameIndex = (m_currentFrameIndex + 1) % NumFramesInFlight;

	if (m_frameTimelineValues[m_currentFrameIndex] > 0) {
		vk::SemaphoreWaitInfo waitInfo;
		waitInfo.semaphoreCount = 1;
		vk::Semaphore timelineSem = *m_timelineSemaphore;
		waitInfo.pSemaphores = &timelineSem;
		waitInfo.pValues = &m_frameTimelineValues[m_currentFrameIndex];
		
		auto result = m_device->waitSemaphores(waitInfo, UINT64_MAX);
	}

	auto acquireResult = m_device->acquireNextImageKHR(*m_swapChain, UINT64_MAX, *m_imageAvailableSemaphores[m_currentFrameIndex], nullptr);
	vk::Result res = acquireResult.result;
	m_currentSwapchainIndex = acquireResult.value;

	if (res == vk::Result::eErrorOutOfDateKHR) {
		// Skip this frame
		return false;
	}
	else if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("Failed to acquire swap chain image!");
	}
	return true;
}

void VulkanRenderer::render(HBFrameStats& frameStats) {
	m_commandBuffers[m_currentFrameIndex]->reset();
	recordCommandBuffer(*m_commandBuffers[m_currentFrameIndex], m_currentSwapchainIndex);

	vk::SubmitInfo submitInfo = {};
	submitInfo.sType = vk::StructureType::eSubmitInfo;
	submitInfo.pNext = nullptr;

	vk::Semaphore waitSemaphores[] = { *m_imageAvailableSemaphores[m_currentFrameIndex] };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &(*m_commandBuffers[m_currentFrameIndex]);

	m_timelineValue++;
	m_frameTimelineValues[m_currentFrameIndex] = m_timelineValue;
	frameStats.setTimelineValue(m_timelineValue);

	vk::Semaphore signalSemaphores[] = { *m_renderFinishedSemaphores[m_currentFrameIndex], *m_timelineSemaphore };
	uint64_t waitValues[] = { 0 };
	uint64_t signalValues[] = { 0, m_timelineValue };

	vk::TimelineSemaphoreSubmitInfo timelineInfo;
	timelineInfo.waitSemaphoreValueCount = 1;
	timelineInfo.pWaitSemaphoreValues = waitValues;
	timelineInfo.signalSemaphoreValueCount = 2;
	timelineInfo.pSignalSemaphoreValues = signalValues;

	submitInfo.pNext = &timelineInfo;
	submitInfo.signalSemaphoreCount = 2;
	submitInfo.pSignalSemaphores = signalSemaphores;

	m_graphicsQueue.submit(submitInfo, nullptr);

	vk::PresentInfoKHR presentInfo = {};
	presentInfo.sType = vk::StructureType::ePresentInfoKHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	vk::SwapchainKHR swapChains[] = { *m_swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_currentSwapchainIndex;
	presentInfo.pResults = nullptr;

	auto presentResult = m_presentQueue.presentKHR(presentInfo);
	if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR) {
		// Handle swapchain recreation if needed
	}
	else if (presentResult != vk::Result::eSuccess) {
		throw std::runtime_error("Failed to present swap chain image!");
	}
}

void VulkanRenderer::updateImGuiFrame(HBFrameStats& frameStats) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	bool recreateSwapchain = false;
	frameStats.renderImGui(m_vsync, recreateSwapchain);
	ImGui::Render();

	if (recreateSwapchain) {
		recreateSwapChain();
	}
}

void VulkanRenderer::waitIdle() {
	m_device->waitIdle();
}

void VulkanRenderer::shutdownImGui() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	std::cout << "> ImGui shutdown" << std::endl;
}
