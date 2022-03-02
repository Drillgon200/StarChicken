#pragma once

#include <vulkan/vulkan.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <assert.h>

#define VKU_CHECK_RESULT(result, error) if(result != VK_SUCCESS){\
	throw std::runtime_error(error);\
}assert(true)

#define DESCRIPTOR_DEFAULT_SIZE 32
#define NUM_FRAME_DATA 2

namespace vku {

	class Uniform;
	class DescriptorSet;
	class DescriptorAllocator;
	class Framebuffer;
	class SingleBufferAllocator;
	class DeviceMemoryAllocator;
	class Texture;
	class StagingManager;
	class RenderPass;

	extern VkInstance instance;
	extern VkPhysicalDeviceProperties deviceProperties;
	extern VkPhysicalDeviceFeatures deviceFeatures;
	extern VkPhysicalDevice physicalDevice;
	extern VkDevice device;

	//extern VkCommandPool graphicsCommandPool;
	extern VkCommandPool graphicsCommandPools[NUM_FRAME_DATA];
	extern VkCommandPool transferCommandPool;
	extern std::vector<VkCommandBuffer> graphicsCommandBuffers;
	extern VkCommandBuffer transferCommandBuffer;

	extern VkQueue graphicsQueue;
	extern VkQueue computeQueue;
	extern VkQueue transferQueue;
	extern VkQueue presentQueue;

	extern DescriptorAllocator* descriptorSetAllocator;

	extern VkSemaphore transferSemaphore;
	extern VkFence transferFence;

	extern VkFormat swapChainImageFormat;
	extern VkExtent2D swapChainExtent;
	extern bool frameBufferResized;
	extern std::vector<VkImage> swapChainImages;
	extern std::vector<VkImageView> swapChainImageViews;
	extern uint32_t swapchainImageIndex;

	extern uint32_t currentFrame;

	struct QueueFamilyIndices {
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		std::optional<uint32_t> transferFamily;
		std::optional<uint32_t> computeFamily;

		inline bool is_complete() {
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
	constexpr bool enableValidationLayers = true;
#else
	constexpr bool enableValidationLayers = false;
#endif

	extern VkInstance instance;

	extern DeviceMemoryAllocator* generalAllocator;

	extern StagingManager* transferStagingManager;
	extern StagingManager* graphicsStagingManager;

	extern Texture* missingTexture;
	extern Texture* whiteTexture;

	std::vector<uint8_t> readFromFile(const std::string& fileName);

	void init_vulkan(void (*resizeCallback)(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY));
	void cleanup_vulkan();
	void recreate_swapchain();

	void begin_command_buffer(VkCommandBuffer buf);
	void end_command_buffer(VkCommandBuffer buf);
	void mark_transfer_dirty();

	VkCommandBuffer graphics_cmd_buf();

	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags flags);

	void create_instance();

	DescriptorSet* create_descriptor_set(const std::initializer_list<Uniform*> uniforms);
	void destroy_descriptor_set(DescriptorSet* set);

	void create_descriptor_sets();
	void destroy_descriptor_sets();
	void recreate_descriptor_set_layouts();
	void delete_descriptor_sets();

	bool setup_next_frame();
	void begin_frame();
	void end_frame();

	void recompile_modified_shaders(std::string shaderDir);

	inline void execution_barrier(VkCommandBuffer cmdBuf, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
		vkCmdPipelineBarrier(cmdBuf, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 0, nullptr);
	}

	inline void memory_barrier(VkCommandBuffer cmdBuf, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkDependencyFlagBits dependencyFlags = static_cast<VkDependencyFlagBits>(0)) {
		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		vkCmdPipelineBarrier(cmdBuf, srcStageMask, dstStageMask, dependencyFlags, 1, &barrier, 0, nullptr, 0, nullptr);
	}

	inline void buffer_barrier(VkCommandBuffer cmdBuf, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkBuffer buffer, uint32_t offset, uint32_t size) {
		VkBufferMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.buffer = buffer;
		barrier.offset = offset;
		barrier.size = size;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.srcQueueFamilyIndex = 0;
		barrier.dstQueueFamilyIndex = 0;
		vkCmdPipelineBarrier(cmdBuf, srcStageMask, dstStageMask, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	}

	inline VkImageMemoryBarrier get_image_barrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImage image, VkImageAspectFlags aspectMask, uint32_t mipBase, uint32_t mipLevels, uint32_t arrayBase, uint32_t arrayLayers, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcAccessMask = srcAccessMask;
		barrier.dstAccessMask = dstAccessMask;
		barrier.srcQueueFamilyIndex = 0;
		barrier.dstQueueFamilyIndex = 0;
		barrier.subresourceRange.aspectMask = aspectMask;
		barrier.subresourceRange.baseMipLevel = mipBase;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = arrayBase;
		barrier.subresourceRange.layerCount = arrayLayers;
		return barrier;
	}

	inline void image_barrier(VkCommandBuffer cmdBuf, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImage image, VkImageAspectFlags aspectMask, uint32_t mipBase, uint32_t mipLevels, uint32_t arrayBase, uint32_t arrayLayers, VkImageLayout oldLayout, VkImageLayout newLayout) {
		VkImageMemoryBarrier barrier = get_image_barrier(srcAccessMask, dstAccessMask, image, aspectMask, mipBase, mipLevels, arrayBase, arrayLayers, oldLayout, newLayout);
		vkCmdPipelineBarrier(cmdBuf, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}
	inline void image_barrier(VkCommandBuffer cmdBuf, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkImage image, VkImageLayout layout) {
		image_barrier(cmdBuf, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, layout, layout);
	}
}
