#include "stdafx.h"
#include "hb_application.h"

#ifdef NDEBUG
constexpr bool g_enableValidationLayers = false;
#else
constexpr bool g_enableValidationLayers = true;
#endif

void HBApplication::run() {
	initWindow();
	initVulkan();
	initImGUI();
	mainLoop();
	cleanup();
}

void HBApplication::initWindow() {
	glfwSetErrorCallback([](int error, const char* description) {
		std::cerr << "Error: " << description << std::endl;
		});

	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}

	std::cout << "> GLFW Initialization" << std::endl;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "project-hibernation", nullptr, nullptr);
	if (!m_window) {
		throw std::runtime_error("Failed to create window");
	}

	glfwSetWindowUserPointer(m_window, this);
	glfwSetKeyCallback(m_window,
		[](GLFWwindow* window, int key, int scancode, int action, int mods) {
			reinterpret_cast<decltype(this)>(glfwGetWindowUserPointer(window))->handleKey(window, key, scancode, action, mods);
		});
	glfwSetFramebufferSizeCallback(m_window,
		[](GLFWwindow* window, int width, int height) {
			reinterpret_cast<decltype(this)>(glfwGetWindowUserPointer(window))->handleFramebufferResize(window, width, height);
		});

	std::cout << "> Window Creation" << std::endl;
}

void HBApplication::logDeviceInfo(const vk::PhysicalDevice& physicalDevice) {
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

void HBApplication::initVulkan() {
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
	VkResult glfw_result = glfwCreateWindowSurface(*m_instance, m_window, nullptr, &rawSurface);
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
	createGraphicsPipeline();
	createFramebuffers();
	createOffscreenResources();
	createCommandPool(m_vkbDevice);
	createCommandBuffers();
	createDescriptorPool();
	createSyncObjects();
}

void HBApplication::recreateSwapChain() {
	m_device->waitIdle();
	
	m_offscreenFramebuffer.reset();
	m_offscreenRenderPass.reset();
	m_offscreenImageView.reset();
	m_offscreenImage.reset();
	m_offscreenImageMemory.reset();
	
	createSwapChain(m_vkbDevice);
	createImageViews();
	createFramebuffers();
	createOffscreenResources();
}

void HBApplication::handleFramebufferResize(GLFWwindow* window, int width, int height) {
	m_windowWidth = width;
	m_windowHeight = height;

	if (m_windowWidth == 0 || m_windowHeight == 0) {
		return;
	}

	m_recreateSwapchain = true;
}

void HBApplication::handleKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
	// Toggle fullscreen on Alt + Enter
	if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && (mods & GLFW_MOD_ALT)) {
		toggleFullscreen(window, !m_windowFullscreen);
	}
}

void HBApplication::initImGUI() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	// Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	// Enable Gamepad Controls

	if (!ImGui_ImplGlfw_InitForVulkan(m_window, true)) {
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
	initInfo.RenderPass = *m_renderPass;
	initInfo.MinImageCount = NumFramesInFlight;
	initInfo.ImageCount = static_cast<uint32_t>(m_swapChainImages.size());
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.CheckVkResultFn = [](VkResult err) {
		if (err != VK_SUCCESS) {
			std::cerr << "[ImGUI Vulkan] Error: " << err << std::endl;
		}
		};
	if (!ImGui_ImplVulkan_Init(&initInfo)) {
		std::cerr << "[ImGUI Vulkan] ImGui_ImplVulkan Initialization failed!" << std::endl;
	}

	// Initialize frame time history
	for (auto& stats : m_frameTimeHistory) {
		stats.avg = 0.0f;
		stats.min = 0.0f;
		stats.max = 0.0f;
	}
	m_frameTimeAccumulator.reserve(100);

	std::cout << "> ImGUI Initialization" << std::endl;
}

void HBApplication::createSwapChain(vkb::Device& vkb_dev) {
	vkb::SwapchainBuilder swapchainBuilder(vkb_dev);
	VkPresentModeKHR presentMode = m_vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;

	auto swapRet = swapchainBuilder.set_old_swapchain(*m_swapChain)
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

void HBApplication::createImageViews() {
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

void HBApplication::createRenderPass() {
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

void HBApplication::createGraphicsPipeline() {
	auto vertShaderCode = readFile("shaders/shader.vert.spv");
	auto fragShaderCode = readFile("shaders/shader.frag.spv");

	vk::ShaderModuleCreateInfo vertShaderInfo = {};
	vertShaderInfo.sType = vk::StructureType::eShaderModuleCreateInfo;
	vertShaderInfo.pNext = nullptr;
	vertShaderInfo.flags = {};
	vertShaderInfo.codeSize = vertShaderCode.size();
	vertShaderInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());

	vk::ShaderModuleCreateInfo fragShaderInfo = {};
	fragShaderInfo.sType = vk::StructureType::eShaderModuleCreateInfo;
	fragShaderInfo.pNext = nullptr;
	fragShaderInfo.flags = {};
	fragShaderInfo.codeSize = fragShaderCode.size();
	fragShaderInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());

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

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = vk::StructureType::ePipelineVertexInputStateCreateInfo;
	vertexInputInfo.pNext = nullptr;
	vertexInputInfo.flags = {};
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
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

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = vk::StructureType::ePipelineLayoutCreateInfo;
	pipelineLayoutInfo.pNext = nullptr;
	pipelineLayoutInfo.flags = {};
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pSetLayouts = nullptr;
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
	pipelineInfo.renderPass = *m_renderPass;
	pipelineInfo.subpass = 0;

	m_graphicsPipeline = m_device->createGraphicsPipelineUnique(nullptr, pipelineInfo).value;
}

uint32_t HBApplication::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
	vk::PhysicalDeviceMemoryProperties memProperties = m_physicalDevice.getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

void HBApplication::createOffscreenResources() {
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

void HBApplication::createFramebuffers() {
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

void HBApplication::createCommandPool(vkb::Device& vkb_dev) {
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

void HBApplication::createCommandBuffers() {
	vk::CommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = vk::StructureType::eCommandBufferAllocateInfo;
	allocInfo.pNext = nullptr;
	allocInfo.commandPool = *m_commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = NumFramesInFlight;

	m_commandBuffers = m_device->allocateCommandBuffersUnique(allocInfo);
}

void HBApplication::recordCommandBuffer(vk::CommandBuffer& commandBuffer, uint32_t imageIndex) {
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

	commandBuffer.draw(3, 1, 0, 0);

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

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Frame Statistics Window
	ImGui::Begin("Frame Statistics", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	if (ImGui::Checkbox("VSync", &m_vsync)) {
		m_recreateSwapchain = true;
	}

	ImGui::Text("FPS: %.1f", m_fps);
	ImGui::Text("Frame Time: %.3f ms", m_currentFrameTime * 1000.0f);
	ImGui::Text("Min: %.3f ms | Max: %.3f ms | Avg: %.3f ms",
		m_minFrameTime, m_maxFrameTime, m_avgFrameTime);
	ImGui::Text("Timeline Value: %llu", m_timelineValue);

	ImGui::Separator();

	// Prepare data for plotting
	std::array<float, FRAME_HISTORY_SIZE> avgData;
	std::array<float, FRAME_HISTORY_SIZE> minData;
	std::array<float, FRAME_HISTORY_SIZE> maxData;

	for (size_t i = 0; i < FRAME_HISTORY_SIZE; ++i) {
		avgData[i] = m_frameTimeHistory[i].avg;
		minData[i] = m_frameTimeHistory[i].min;
		maxData[i] = m_frameTimeHistory[i].max;
	}

	if (ImPlot::BeginPlot("Frame Time Histogram", ImVec2(-1, 300))) {
		ImPlot::SetupAxes("Frame", "Time (ms)");
		ImPlot::SetupAxisTicks(ImAxis_X1, nullptr, 0);
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, FRAME_HISTORY_SIZE, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0, m_frameAxisLimit, ImGuiCond_Always);

		// Plot frame time data
		ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
		ImPlot::PlotLine("Max", maxData.data(), FRAME_HISTORY_SIZE);
		ImPlot::PopStyleColor();

		ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 1.0f, 1.0f));
		ImPlot::PlotLine("Min", minData.data(), FRAME_HISTORY_SIZE);
		ImPlot::PopStyleColor();

		ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		ImPlot::PlotLine("Avg", avgData.data(), FRAME_HISTORY_SIZE);
		ImPlot::PopStyleColor();

		// Target frame time lines (120 FPS = 8.33ms, 60 FPS = 16.67ms, 30 FPS = 33.33ms)
		float targetLine120[2] = { 8.33f, 8.33f };
		float targetLine60[2] = { 16.67f, 16.67f };
		float targetLine30[2] = { 33.33f, 33.33f };
		float xData[2] = { 0.0f, static_cast<float>(FRAME_HISTORY_SIZE) };

		ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 1.0f, 0.3f));
		ImPlot::PlotLine("120 FPS", xData, targetLine120, 2);
		ImPlot::PopStyleColor();

		ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.0f, 1.0f, 0.0f, 0.3f));
		ImPlot::PlotLine("60 FPS", xData, targetLine60, 2);
		ImPlot::PopStyleColor();

		ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 1.0f, 0.5f, 0.3f));
		ImPlot::PlotLine("30 FPS", xData, targetLine30, 2);
		ImPlot::PopStyleColor();

		ImPlot::EndPlot();
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	commandBuffer.endRenderPass();
	commandBuffer.end();
}

void HBApplication::createDescriptorPool() {
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

void HBApplication::createSyncObjects() {
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

bool HBApplication::acquireNextFrame() {
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

void HBApplication::render() {
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

void HBApplication::mainLoop() {
	m_lastTime = glfwGetTime();
	m_lastFrameTime = m_lastTime;

	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();

		if (m_recreateSwapchain) {
			recreateSwapChain();
			m_recreateSwapchain = false;
		}

		if (acquireNextFrame()) {
			render();
			updateFrameStats();
		}
	}

	m_device->waitIdle();
}

void HBApplication::updateFrameStats() {
	double currentTime = glfwGetTime();

	// Calculate frame time
	m_currentFrameTime = static_cast<float>((currentTime - m_lastFrameTime));
	m_lastFrameTime = currentTime;

	// Add to accumulator
	m_frameTimeAccumulator.push_back(m_currentFrameTime * 1000.0f);

	// Check if it's time to update graph (1/60 second interval or if frame time exceeds the interval)
	m_graphUpdateTimer += m_currentFrameTime;
	if (m_graphUpdateTimer >= GRAPH_UPDATE_INTERVAL) {
		// Calculate statistics from accumulated frame times
		if (!m_frameTimeAccumulator.empty()) {
			float sum = 0.0f;
			float minTime = FLT_MAX;
			float maxTime = 0.0f;

			for (float frameTime : m_frameTimeAccumulator) {
				sum += frameTime;
				minTime = std::min(minTime, frameTime);
				maxTime = std::max(maxTime, frameTime);
			}

			float avgTime = sum / m_frameTimeAccumulator.size();

			std::rotate(m_frameTimeHistory.begin(), m_frameTimeHistory.begin() + 1, m_frameTimeHistory.end());

			// Add to history (shift left)
			m_frameTimeHistory.back().avg = avgTime;
			m_frameTimeHistory.back().min = minTime;
			m_frameTimeHistory.back().max = maxTime;

			// Clear accumulator
			m_frameTimeAccumulator.clear();
		}

		m_graphUpdateTimer = std::fmod(m_graphUpdateTimer, GRAPH_UPDATE_INTERVAL);
	}

	// Calculate overall statistics from history
	float sum = 0.0f;
	int validCount = 0;
	m_minFrameTime = FLT_MAX;
	m_maxFrameTime = 0.0f;

	for (const auto& stats : m_frameTimeHistory) {
		if (stats.avg > 0.0f) {
			sum += stats.avg;
			validCount++;
			m_minFrameTime = std::min(m_minFrameTime, stats.min);
			m_maxFrameTime = std::max(m_maxFrameTime, stats.max);
		}
	}

	if (validCount > 0) {
		m_avgFrameTime = sum / validCount;
	}

	// Update FPS counter (20 times per second)
	m_frameCount++;
	if (currentTime - m_lastTime >= FPS_UPDATE_INTERVAL) {
		m_fps = m_frameCount / (currentTime - m_lastTime);

		m_frameCount = 0;
		m_lastTime = currentTime;
	}

	m_frameAxisLimit = std::lerp(m_frameAxisLimit, m_maxFrameTime * 1.2f, m_currentFrameTime * 16.0f);
}

void HBApplication::toggleFullscreen(GLFWwindow* window, bool fullScreen) {
	if (m_windowFullscreen == fullScreen) {
		return;
	}

	m_windowFullscreen = fullScreen;
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	if (fullScreen) {
		// [Windowed -> Borderless Fullscreen]
		std::cout << "> Switching to Borderless Fullscreen" << std::endl;

		glfwGetWindowPos(window, &m_windowPosX, &m_windowPosY);
		glfwGetWindowSize(window, &m_windowedWidth, &m_windowedHeight);

		glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
		glfwSetWindowMonitor(window, nullptr, 0, 0, mode->width, mode->height, mode->refreshRate);
	}
	else {
		// [Borderless Fullscreen -> Windowed]
		std::cout << "> Switching to Windowed Mode" << std::endl;

		glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowMonitor(window, nullptr, m_windowPosX, m_windowPosY, m_windowedWidth, m_windowedHeight, 0);
	}
}

void HBApplication::cleanup() {
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	glfwDestroyWindow(m_window);
	glfwTerminate();

	std::cout << "> Cleanup and Exit" << std::endl;
}

std::vector<char> HBApplication::readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}
