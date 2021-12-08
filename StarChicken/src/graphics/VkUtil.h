#pragma once

#include <iostream>
#include <fstream>
#define GLFW_INCLUDE_VULKAN
#include <vector>
#include <optional>
#include <assert.h>
#include "DeviceMemoryAllocator.h"

#define VKU_CHECK_RESULT(result, error) if(result != VK_SUCCESS){\
	throw std::runtime_error(error);\
}assert(true)

#define DESCRIPTOR_DEFAULT_SIZE 32

namespace vku {

	class Uniform;
	class DescriptorSet;
	class Framebuffer;
	class SingleBufferAllocator;
	class DeviceMemoryAllocator;
	class Texture;

	extern VkInstance instance;
	extern VkPhysicalDeviceProperties deviceProperties;
	extern VkPhysicalDeviceFeatures deviceFeatures;
	extern VkPhysicalDevice physicalDevice;
	extern VkDevice device;

	extern VkCommandPool graphicsCommandPool;
	extern VkCommandPool transferCommandPool;
	extern std::vector<VkCommandBuffer> graphicsCommandBuffers;
	extern VkCommandBuffer transferCommandBuffer;
	extern std::vector<DeviceBuffer> transferStagingBuffersToFree;

	extern VkQueue graphicsQueue;
	extern VkQueue computeQueue;
	extern VkQueue transferQueue;
	extern VkQueue presentQueue;

	extern VkDescriptorPool descriptorPool;

	extern VkSemaphore transferSemaphore;
	extern VkFence transferFence;

	extern VkFormat swapChainImageFormat;
	extern VkExtent2D swapChainExtent;
	extern bool frameBufferResized;

	extern uint32_t swapchainImageCount;
	extern uint32_t swapchainImageIndex;

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		std::optional<uint32_t> transferFamily;
		std::optional<uint32_t> computeFamily;

		inline bool isComplete() {
			return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value() && computeFamily.has_value();
		}
	};
	extern QueueFamilyIndices deviceQueueFamilyIndices;

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> surfaceFormats;
		std::vector<VkPresentModeKHR> presentModes;
	};

#ifdef _DEBUG
	const bool enableValidationLayers = true;
#else
	const bool enableValidationLayers = false;
#endif

	extern VkInstance instance;

	extern SingleBufferAllocator* vertexBufferAllocator;
	extern SingleBufferAllocator* indexBufferAllocator;
	extern DeviceMemoryAllocator* generalAllocator;

	extern Texture* missingTexture;

	std::vector<uint8_t> readFromFile(const std::string& fileName);

	void init_vulkan();
	void cleanup_vulkan();
	void recreate_swapchain();

	void cleanup_staging_buffers();

	void begin_command_buffer(VkCommandBuffer buf);
	void end_command_buffer(VkCommandBuffer buf);
	void mark_transfer_dirty();

	VkCommandBuffer graphics_cmd_buf();
	Framebuffer& current_swapchain_framebuffer();

	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags flags);

	void create_instance();

	DescriptorSet& create_descriptor_set(const std::initializer_list<Uniform*> uniforms);

	void create_descriptor_sets();

	bool setup_next_frame();
	void begin_frame();
	void end_frame();
}
