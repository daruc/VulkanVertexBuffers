#pragma once

#include <vulkan.h>
#include <optional>
#include <vector>
#include <array>
#include "glm/common.hpp"
#include "glm/vec3.hpp"

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphics;
	std::optional<uint32_t> presentation;
};

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;
};

class Engine
{
private:
	const int MAX_FRAMES_IN_FLIGHT;

	struct SDL_Window* m_sdlWindow;
	VkInstance m_vkInstance;
	VkPhysicalDevice m_vkPhysicalDevice;
	VkDevice m_vkDevice;
	VkQueue m_vkGraphicsQueue;
	VkQueue m_vkPresentationQueue;
	VkSurfaceKHR m_vkSurface;
	VkSwapchainKHR m_vkSwapchain;
	std::vector<VkImage> m_vkSwapchainImages;
	std::vector<VkImageView> m_vkSwapchainImageViews;
	VkFormat m_vkSwapchainImageFormat;
	VkExtent2D m_vkSwapchainExtent;
	std::vector<const char*> m_deviceExtensions;
	VkRenderPass m_vkRenderPass;
	VkPipelineLayout m_vkPipelineLayout;
	VkPipeline m_vkPipeline;
	std::vector<VkFramebuffer> m_vkSwapchainFramebuffers;
	VkCommandPool m_vkCommandPool;
	std::vector<VkCommandBuffer> m_vkCommandBuffers;
	std::vector<VkSemaphore> m_vkImageAvailableSemaphores;
	std::vector<VkSemaphore> m_vkRenderFinishedSemaphores;
	std::vector<VkFence> m_vkFences;
	std::vector<VkFence> m_vkImagesInFlightFences;
	int m_currentFrame;
	std::vector<Vertex> m_vertices;
	VkBuffer m_vkVertexBuffer;
	VkDeviceMemory m_vkVertexDeviceMemory;
	std::vector<uint32_t> m_indices;
	VkBuffer m_vkIndexBuffer;
	VkDeviceMemory m_vkIndexDeviceMemory;

	void initVkInstance();
	void createVkSurface();
	void pickPhysicalDevice();
	void createDevice();
	void createSwapChain();
	void createSwapChainImageViews();
	void createRenderPass();
	void createGraphicsPipeline();
	void createFramebuffers();

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags,
		VkBuffer* outBuffer, VkDeviceMemory* outDeviceMemory);

	void copyBuffer(VkDeviceSize size, VkBuffer source, VkBuffer destination);

	void createVertexBuffer();
	void createIndexBuffer();
	void createCommandPool();
	void createCommandBuffers();
	void createSemaphores();
	void createFences();

	VkShaderModule loadShader(const char* fileName);
	QueueFamilyIndices findQueueFamilyIndices(VkPhysicalDevice physicalDevice);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkVertexInputBindingDescription buildVertexBindingDescription();
	std::array<VkVertexInputAttributeDescription, 2> buildVertexAttributeDescription();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	bool checkDeviceExtensionSupport(VkPhysicalDevice physicalDevice);
	bool checkSwapchainSupport(VkPhysicalDevice physicalDevice);
	bool checkQueueFamiliesSupport(VkPhysicalDevice physicalDevice);
	
public:
	Engine();

	void init(struct SDL_Window* sdlWindow);
	void update();
	void render();
	void cleanUp();
};

