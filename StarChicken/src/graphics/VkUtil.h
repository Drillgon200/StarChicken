#pragma once

#include <iostream>
#include <fstream>
#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <vector>
#include <optional>

#define VKU_CHECK_RESULT(result, error) if(result != VK_SUCCESS){\
	throw std::runtime_error(error);\
}assert(true)

namespace vku {

	extern VkInstance instance;
	extern VkPhysicalDevice physicalDevice;
	extern VkDevice device;

	extern VkQueue graphicsQueue;
	extern VkQueue computeQueue;
	extern VkQueue transferQueue;
	extern VkQueue presentQueue;

	extern VkFormat swapChainImageFormat;
	extern VkExtent2D swapChainExtent;
	extern bool frameBufferResized;

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

	struct ModelViewProjectionMatrices {
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
	};

#ifdef _DEBUG
	const bool enableValidationLayers = true;
#else
	const bool enableValidationLayers = false;
#endif

	extern VkInstance instance;

	std::vector<uint8_t> readFromFile(const std::string& fileName);

	void init_vulkan();
	void cleanup_vulkan();
	void recreate_swapchain();

	VkBuffer create_buffer();

	void create_instance();
	bool draw_frame();
}
