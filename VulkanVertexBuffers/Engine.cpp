#include "Engine.h"
#include "SDL.h"
#include "SDL_vulkan.h"
#include <vector>
#include <set>
#include <fstream>

void Engine::initVkInstance()
{
	VkApplicationInfo vkApplicationInfo = {};
	vkApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	vkApplicationInfo.pApplicationName = "Vulkan Init";
	vkApplicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	vkApplicationInfo.pEngineName = "Engine";
	vkApplicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	vkApplicationInfo.apiVersion = VK_API_VERSION_1_0;

	unsigned int extensionCount;
	SDL_Vulkan_GetInstanceExtensions(m_sdlWindow, &extensionCount, nullptr);
	std::vector<const char*> extensions(extensionCount);
	SDL_Vulkan_GetInstanceExtensions(m_sdlWindow, &extensionCount, extensions.data());

	VkInstanceCreateInfo vkInstanceCreateInfo = {};
	vkInstanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	vkInstanceCreateInfo.pApplicationInfo = &vkApplicationInfo;
	vkInstanceCreateInfo.enabledExtensionCount = extensionCount;
	vkInstanceCreateInfo.ppEnabledExtensionNames = extensions.data();

#ifdef _DEBUG
	std::vector<const char*> validationLayers;
	validationLayers.push_back("VK_LAYER_KHRONOS_validation");

	vkInstanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	vkInstanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
#endif

	VkResult result = vkCreateInstance(&vkInstanceCreateInfo, nullptr, &m_vkInstance);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create VkInstance.");
	}
}

void Engine::createVkSurface()
{
	SDL_bool result = SDL_Vulkan_CreateSurface(m_sdlWindow, m_vkInstance, &m_vkSurface);

	if (result == SDL_FALSE)
	{
		throw std::runtime_error("Failed to create VkSurfaceKHR.");
	}
}

void Engine::pickPhysicalDevice()
{
	m_deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	unsigned int deviceCount = 0;
	vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		throw std::runtime_error("None physical device is available.");
	}

	std::vector<VkPhysicalDevice> availableDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, availableDevices.data());

	m_vkPhysicalDevice = VK_NULL_HANDLE;
	for (VkPhysicalDevice availableDevice : availableDevices)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(availableDevice, &properties);

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			checkDeviceExtensionSupport(availableDevice) &&
			checkSwapchainSupport(availableDevice) &&
			checkQueueFamiliesSupport(availableDevice))
		{
			m_vkPhysicalDevice = availableDevice;
			break;
		}
	}

	if (m_vkPhysicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("None discrete GPU is available.");
	}
}

void Engine::createDevice()
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(2);

	QueueFamilyIndices queueFamilyIndices = findQueueFamilyIndices(m_vkPhysicalDevice);
	const float queuePriority = 1.0f;

	VkDeviceQueueCreateInfo& graphicsQueueCreateInfo = queueCreateInfos[0];
	graphicsQueueCreateInfo = {};
	graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsQueueCreateInfo.queueFamilyIndex = *queueFamilyIndices.graphics;
	graphicsQueueCreateInfo.queueCount = 1;
	graphicsQueueCreateInfo.pQueuePriorities = &queuePriority;

	VkDeviceQueueCreateInfo& presentationQueueCreateInfo = queueCreateInfos[1];
	presentationQueueCreateInfo = {};
	presentationQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	presentationQueueCreateInfo.queueFamilyIndex = *queueFamilyIndices.presentation;
	presentationQueueCreateInfo.queueCount = 1;
	presentationQueueCreateInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

	VkResult result = vkCreateDevice(m_vkPhysicalDevice, &createInfo, nullptr, &m_vkDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create device.");
	}

	vkGetDeviceQueue(m_vkDevice, *queueFamilyIndices.graphics, 0, &m_vkGraphicsQueue);
	vkGetDeviceQueue(m_vkDevice, *queueFamilyIndices.presentation, 0, &m_vkPresentationQueue);
}

void Engine::createSwapChain()
{
	SwapChainSupportDetails supportDetails = querySwapChainSupport(m_vkPhysicalDevice);
	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(supportDetails.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(supportDetails.presentModes);
	VkExtent2D extent = chooseSwapExtent(supportDetails.capabilities);

	uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
	
	if (supportDetails.capabilities.maxImageCount > 0 &&
		imageCount > supportDetails.capabilities.maxImageCount)
	{
		imageCount = supportDetails.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR swapChainCreateInfo = {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.surface = m_vkSurface;
	swapChainCreateInfo.minImageCount = imageCount;
	swapChainCreateInfo.imageFormat = surfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = extent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	QueueFamilyIndices queueFamilyIndices = findQueueFamilyIndices(m_vkPhysicalDevice);
	std::vector<uint32_t> indices;
	indices.push_back(queueFamilyIndices.graphics.value());
	indices.push_back(queueFamilyIndices.presentation.value());

	if (queueFamilyIndices.graphics != queueFamilyIndices.presentation)
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(indices.size());
		swapChainCreateInfo.pQueueFamilyIndices = indices.data();
	}
	else
	{
		swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	swapChainCreateInfo.preTransform = supportDetails.capabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = presentMode;
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(m_vkDevice, &swapChainCreateInfo, nullptr, &m_vkSwapchain);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain.");
	}

	uint32_t finalImageCount;
	vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &finalImageCount, nullptr);
	m_vkSwapchainImages.resize(finalImageCount);
	vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &finalImageCount, m_vkSwapchainImages.data());

	m_vkSwapchainImageFormat = surfaceFormat.format;
	m_vkSwapchainExtent = extent;
}

void Engine::createSwapChainImageViews()
{
	m_vkSwapchainImageViews.resize(m_vkSwapchainImages.size());

	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = m_vkSwapchainImageFormat;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	for (size_t i = 0; i < m_vkSwapchainImageViews.size(); ++i)
	{
		createInfo.image = m_vkSwapchainImages[i];

		VkResult result = vkCreateImageView(m_vkDevice, &createInfo, nullptr, &m_vkSwapchainImageViews[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain image view.");
		}
	}
}

void Engine::createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = m_vkSwapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachment;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	VkResult result = vkCreateRenderPass(m_vkDevice, &renderPassCreateInfo, nullptr, &m_vkRenderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass.");
	}
}

void Engine::createGraphicsPipeline()
{
	VkShaderModule vertexShader = loadShader("vertex.spv");
	VkShaderModule fragmentShader = loadShader("fragment.spv");

	VkPipelineShaderStageCreateInfo vertexStageCreateInfo = {};
	vertexStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexStageCreateInfo.module = vertexShader;
	vertexStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentStageCreateInfo = {};
	fragmentStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentStageCreateInfo.module = fragmentShader;
	fragmentStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStageInfos[] = { vertexStageCreateInfo, fragmentStageCreateInfo };

	const VkVertexInputBindingDescription vertexBindingDesc =
		buildVertexBindingDescription();

	const std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDesc =
		buildVertexAttributeDescription();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDesc;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexAttributeDescriptions = vertexAttributeDesc.data();
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDesc.size());

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_vkSwapchainExtent.width);
	viewport.height = static_cast<float>(m_vkSwapchainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.extent = m_vkSwapchainExtent;
	scissor.offset = { 0, 0 };

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo = {};
	multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisamplingStateCreateInfo.minSampleShading = 1.0f;
	multisamplingStateCreateInfo.pSampleMask = nullptr;
	multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.logicOp = VK_LOGIC_OP_COPY;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;
	colorBlendState.blendConstants[0] = 0.0f;
	colorBlendState.blendConstants[1] = 0.0f;
	colorBlendState.blendConstants[2] = 0.0f;
	colorBlendState.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	VkResult result = vkCreatePipelineLayout(m_vkDevice, &pipelineLayoutInfo, nullptr, &m_vkPipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout.");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStageInfos;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineInfo.pViewportState = &viewportStateCreateInfo;
	pipelineInfo.pRasterizationState = &rasterizationStateCreateInfo;
	pipelineInfo.pMultisampleState = &multisamplingStateCreateInfo;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlendState;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = m_vkPipelineLayout;
	pipelineInfo.renderPass = m_vkRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	result = vkCreateGraphicsPipelines(m_vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_vkPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create graphics pipeline.");
	}

	vkDestroyShaderModule(m_vkDevice, vertexShader, nullptr);
	vkDestroyShaderModule(m_vkDevice, fragmentShader, nullptr);
}

void Engine::createFramebuffers()
{
	m_vkSwapchainFramebuffers.resize(m_vkSwapchainImageViews.size());

	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = m_vkRenderPass;
	framebufferCreateInfo.attachmentCount = 1;
	framebufferCreateInfo.width = m_vkSwapchainExtent.width;
	framebufferCreateInfo.height = m_vkSwapchainExtent.height;
	framebufferCreateInfo.layers = 1;

	for (int i = 0; i < m_vkSwapchainImageViews.size(); ++i)
	{
		VkImageView attachments[] = {
			m_vkSwapchainImageViews[i]
		};

		framebufferCreateInfo.pAttachments = attachments;

		VkResult result = vkCreateFramebuffer(m_vkDevice, &framebufferCreateInfo, nullptr, &m_vkSwapchainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain frame buffer.");
		}
	}
}

void Engine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
	VkBuffer* outBuffer, VkDeviceMemory* outDeviceMemory)
{
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usageFlags;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(m_vkDevice, &bufferCreateInfo, nullptr, outBuffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create vertex buffer.");
	}

	VkMemoryRequirements vertexBufferMemRequirements;
	vkGetBufferMemoryRequirements(m_vkDevice, *outBuffer, &vertexBufferMemRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = vertexBufferMemRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = findMemoryType(vertexBufferMemRequirements.memoryTypeBits,
		propertyFlags);

	result = vkAllocateMemory(m_vkDevice, &memoryAllocateInfo, nullptr, outDeviceMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate buffer memory.");
	}

	vkBindBufferMemory(m_vkDevice, *outBuffer, *outDeviceMemory, 0);
}

void Engine::copyBuffer(VkDeviceSize size, VkBuffer srcBuffer, VkBuffer dstBuffer)
{
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandBufferCount = 1;
	commandBufferAllocateInfo.commandPool = m_vkCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_vkDevice, &commandBufferAllocateInfo, &commandBuffer);

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
	
	VkBufferCopy copyRegion = {};
	copyRegion.size = size;

	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_vkGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_vkGraphicsQueue);
	vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &commandBuffer);
}

void Engine::createVertexBuffer()
{
	m_vertices.resize(4);

	m_vertices[0].color = { 1.0f, 0.0f, 0.0f };
	m_vertices[0].position = { -0.5f, -0.5f, 0.0f };
	
	m_vertices[1].color = { 0.0f, 1.0f, 0.0f };
	m_vertices[1].position = { 0.5f, -0.5f, 0.0f };
	
	m_vertices[2].color = { 0.0f, 0.0f, 1.0f };
	m_vertices[2].position = { 0.5f, 0.5f, 0.0f };

	m_vertices[3].color = { 1.0f, 0.0f, 1.0f };
	m_vertices[3].position = { -0.5f, 0.5f, 0.0f };

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	const VkDeviceSize bufferSize = sizeof(Vertex) * m_vertices.size();
	const VkBufferUsageFlags stagingBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	const VkMemoryPropertyFlags stagingMemPropertyFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	createBuffer(bufferSize, stagingBufferUsageFlags, stagingMemPropertyFlags,
		&stagingBuffer, &stagingMemory);

	void* mappedMemory;
	VkResult result = vkMapMemory(m_vkDevice, stagingMemory, 0, bufferSize, 0, &mappedMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to map vertex memory.");
	}

	memcpy(mappedMemory, m_vertices.data(), bufferSize);
	vkUnmapMemory(m_vkDevice, stagingMemory);

	const VkBufferUsageFlags vertexBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	const VkMemoryPropertyFlags vertexMemPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	createBuffer(bufferSize, vertexBufferUsageFlags, vertexMemPropertyFlags, &m_vkVertexBuffer,
		&m_vkVertexDeviceMemory);

	copyBuffer(bufferSize, stagingBuffer, m_vkVertexBuffer);

	vkDestroyBuffer(m_vkDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_vkDevice, stagingMemory, nullptr);
}

void Engine::createIndexBuffer()
{
	m_indices = { 0, 1, 2, 0, 2, 3 };
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	const VkDeviceSize bufferSize = sizeof(uint32_t) * m_indices.size();
	const VkBufferUsageFlags stagingBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	const VkMemoryPropertyFlags stagingMemPropertyFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	createBuffer(bufferSize, stagingBufferUsageFlags, stagingMemPropertyFlags,
		&stagingBuffer, &stagingMemory);

	void* mappedMemory;
	VkResult result = vkMapMemory(m_vkDevice, stagingMemory, 0, bufferSize, 0, &mappedMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to map index memory.");
	}

	memcpy(mappedMemory, m_indices.data(), bufferSize);
	vkUnmapMemory(m_vkDevice, stagingMemory);

	const VkBufferUsageFlags indexBufferUsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	const VkMemoryPropertyFlags indexMemPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	createBuffer(bufferSize, indexBufferUsageFlags, indexMemPropertyFlags, &m_vkIndexBuffer,
		&m_vkIndexDeviceMemory);

	copyBuffer(bufferSize, stagingBuffer, m_vkIndexBuffer);

	vkDestroyBuffer(m_vkDevice, stagingBuffer, nullptr);
	vkFreeMemory(m_vkDevice, stagingMemory, nullptr);
}

void Engine::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = findQueueFamilyIndices(m_vkPhysicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphics.value();

	VkResult result = vkCreateCommandPool(m_vkDevice, &commandPoolCreateInfo, nullptr, &m_vkCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool.");
	}
}

void Engine::createCommandBuffers()
{
	m_vkCommandBuffers.resize(m_vkSwapchainFramebuffers.size());

	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandPool = m_vkCommandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = static_cast<uint32_t>(m_vkCommandBuffers.size());

	VkResult result = vkAllocateCommandBuffers(m_vkDevice, &commandBufferInfo, m_vkCommandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate command buffers.");
	}

	for (size_t i = 0; i < m_vkCommandBuffers.size(); ++i)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		
		result = vkBeginCommandBuffer(m_vkCommandBuffers[i], &beginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to begin command buffer.");
		}

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_vkRenderPass;
		renderPassInfo.framebuffer = m_vkSwapchainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_vkSwapchainExtent;

		VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };

		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(m_vkCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(m_vkCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_vkPipeline);

		VkBuffer buffers[] = { m_vkVertexBuffer };
		VkDeviceSize bufferOffsets[] = { 0 };
		vkCmdBindVertexBuffers(m_vkCommandBuffers[i], 0, 1, buffers, bufferOffsets);

		vkCmdBindIndexBuffer(m_vkCommandBuffers[i], m_vkIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(m_vkCommandBuffers[i], static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
		vkCmdEndRenderPass(m_vkCommandBuffers[i]);

		result = vkEndCommandBuffer(m_vkCommandBuffers[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record command buffer.");
		}
	}


}

void Engine::createSemaphores()
{
	m_vkImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_vkRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		VkResult result = vkCreateSemaphore(m_vkDevice, &semaphoreCreateInfo, nullptr, &m_vkImageAvailableSemaphores[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image available semaphore.");
		}

		result = vkCreateSemaphore(m_vkDevice, &semaphoreCreateInfo, nullptr, &m_vkRenderFinishedSemaphores[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create render finished semaphore.");
		}
	}

}

void Engine::createFences()
{
	m_vkFences.resize(MAX_FRAMES_IN_FLIGHT);
	m_vkImagesInFlightFences.resize(m_vkSwapchainImages.size(), VK_NULL_HANDLE);

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		VkResult result = vkCreateFence(m_vkDevice, &fenceCreateInfo, nullptr, &m_vkFences[i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create fance.");
		}
	}
}

VkVertexInputBindingDescription Engine::buildVertexBindingDescription()
{
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(Vertex);
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Engine::buildVertexAttributeDescription()
{
	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, color);

	return attributeDescriptions;
}

uint32_t Engine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags)
{
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(m_vkPhysicalDevice, &deviceMemoryProperties);

	for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
	{
		if ((typeFilter & (1 << i)) && 
			(deviceMemoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)
		{
			return i;
		}
	}

	throw std::runtime_error("Can't find memory type.");
}

VkShaderModule Engine::loadShader(const char* fileName)
{
	std::ifstream istr(fileName, std::ios::ate | std::ios::binary);

	if (!istr.is_open())
	{
		throw std::runtime_error("Failed to open shader file.");
	}

	size_t fileSize = static_cast<size_t>(istr.tellg());
	std::vector<char> buffer(fileSize);
	istr.seekg(0);
	istr.read(buffer.data(), fileSize);
	istr.close();

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = fileSize;
	shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t*>(buffer.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(m_vkDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module.");
	}

	return shaderModule;
}

QueueFamilyIndices Engine::findQueueFamilyIndices(VkPhysicalDevice physicalDevice)
{
	QueueFamilyIndices queueFamilyIndices;

	unsigned int familyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(familyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &familyCount, queueFamilies.data());

	unsigned int index = 0;
	for (VkQueueFamilyProperties queueFamily : queueFamilies)
	{
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			if (!queueFamilyIndices.graphics.has_value())
			{
				queueFamilyIndices.graphics = index;
			}
		}
		else if (!queueFamilyIndices.presentation.has_value())
		{
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, index, m_vkSurface, &presentSupport);

			if (presentSupport)
			{
				queueFamilyIndices.presentation = index;
			}
		}

		if (queueFamilyIndices.graphics.has_value() && queueFamilyIndices.presentation.has_value())
		{
			return queueFamilyIndices;
		}

		++index;
	}

	throw std::runtime_error("Graphics with presentation queue family not found.");
}

SwapChainSupportDetails Engine::querySwapChainSupport(VkPhysicalDevice physicalDevice)
{
	SwapChainSupportDetails supportDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_vkSurface, &supportDetails.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_vkSurface, &formatCount, nullptr);

	if (formatCount > 0)
	{
		supportDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_vkSurface, &formatCount, supportDetails.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_vkSurface, &presentModeCount, nullptr);

	if (presentModeCount > 0)
	{
		supportDetails.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
			physicalDevice, m_vkSurface, &presentModeCount, supportDetails.presentModes.data());
	}

	return supportDetails;
}

VkSurfaceFormatKHR Engine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	return formats[0];
}

VkPresentModeKHR Engine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Engine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	VkExtent2D extent;

	if (capabilities.currentExtent.width == UINT32_MAX)
	{
		int width, height;
		SDL_Vulkan_GetDrawableSize(m_sdlWindow, &width, &height);

		glm::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		glm::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	}
	else
	{
		extent = capabilities.currentExtent;
	}

	return extent;
}

bool Engine::checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice)
{
	uint32_t availableExtensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(availableExtensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &availableExtensionCount, availableExtensions.data());

	std::set<std::string> unavailableExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

	for (VkExtensionProperties& available : availableExtensions)
	{
		unavailableExtensions.erase(available.extensionName);
	}

	return unavailableExtensions.empty();
}

bool Engine::checkSwapchainSupport(VkPhysicalDevice physicalDevice)
{
	SwapChainSupportDetails swapchainSupportDetails = querySwapChainSupport(physicalDevice);
	return !swapchainSupportDetails.presentModes.empty() &&
		!swapchainSupportDetails.formats.empty();
}

bool Engine::checkQueueFamiliesSupport(VkPhysicalDevice physicalDevice)
{
	QueueFamilyIndices indices = findQueueFamilyIndices(physicalDevice);
	return indices.graphics.has_value() && indices.presentation.has_value();
}

Engine::Engine()
	: MAX_FRAMES_IN_FLIGHT(2)
{
}

void Engine::init(SDL_Window* sdlWindow)
{
	m_sdlWindow = sdlWindow;
	m_currentFrame = 0;

	initVkInstance();
	createVkSurface();
	pickPhysicalDevice();
	createDevice();
	createSwapChain();
	createSwapChainImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandPool();
	createVertexBuffer();
	createIndexBuffer();
	createCommandBuffers();
	createSemaphores();
	createFences();
}

void Engine::update()
{
}

void Engine::render()
{
	vkWaitForFences(m_vkDevice, 1, &m_vkFences[m_currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(m_vkDevice, m_vkSwapchain, UINT64_MAX,
		m_vkImageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to acquire next image.");
	}

	if (m_vkImagesInFlightFences[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(m_vkDevice, 1, &m_vkImagesInFlightFences[imageIndex], VK_TRUE, UINT64_MAX);
	}

	VkSemaphore waitSemaphores[] = { m_vkImageAvailableSemaphores[m_currentFrame] };
	VkSemaphore signalSemaphores[] = { m_vkRenderFinishedSemaphores[m_currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_vkCommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(m_vkDevice, 1, &m_vkFences[m_currentFrame]);

	result = vkQueueSubmit(m_vkGraphicsQueue, 1, &submitInfo, m_vkFences[m_currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to queue submit.");
	}

	VkSwapchainKHR swapChains[] = { m_vkSwapchain };

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(m_vkPresentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to queue presentation.");
	}

	vkQueueWaitIdle(m_vkPresentationQueue);

	m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::cleanUp()
{
	vkDeviceWaitIdle(m_vkDevice);

	vkDestroyBuffer(m_vkDevice, m_vkVertexBuffer, nullptr);
	vkFreeMemory(m_vkDevice, m_vkVertexDeviceMemory, nullptr);

	vkDestroyBuffer(m_vkDevice, m_vkIndexBuffer, nullptr);
	vkFreeMemory(m_vkDevice, m_vkIndexDeviceMemory, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(m_vkDevice, m_vkImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_vkDevice, m_vkRenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_vkDevice, m_vkFences[i], nullptr);
	}

	for (VkFence imagesInFlightFence : m_vkImagesInFlightFences)
	{
		vkDestroyFence(m_vkDevice, imagesInFlightFence, nullptr);
	}

	vkDestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr);
	vkDestroyPipeline(m_vkDevice, m_vkPipeline, nullptr);
	vkDestroyPipelineLayout(m_vkDevice, m_vkPipelineLayout, nullptr);
	vkDestroyRenderPass(m_vkDevice, m_vkRenderPass, nullptr);
	vkDestroySwapchainKHR(m_vkDevice, m_vkSwapchain, nullptr);

	for (VkFramebuffer framebuffer : m_vkSwapchainFramebuffers)
	{
		vkDestroyFramebuffer(m_vkDevice, framebuffer, nullptr);
	}

	for (VkImageView swapchainImageView : m_vkSwapchainImageViews)
	{
		vkDestroyImageView(m_vkDevice, swapchainImageView, nullptr);
	}

	vkDestroySurfaceKHR(m_vkInstance, m_vkSurface, nullptr);
	vkDestroyDevice(m_vkDevice, nullptr);
	vkDestroyInstance(m_vkInstance, nullptr);
}
