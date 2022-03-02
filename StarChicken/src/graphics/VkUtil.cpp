#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <set>
#include <algorithm>
#include "VkUtil.h"
#include "GraphicsPipeline.h"
#include "RenderPass.h"
#include "Framebuffer.h"
#include "..\Engine.h"
#include "ShaderUniforms.h"
#include "geometry/GeometryAllocator.h"
#include "DeviceMemoryAllocator.h"
#include "TextureManager.h"
#include "../util/Windowing.h"
#include "StagingManager.h"
#include <filesystem>
#include "..\util\Util.h"

namespace vku {

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_KHRONOS_synchronization2"
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	VkInstance instance;
	QueueFamilyIndices deviceQueueFamilyIndices;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	VkDebugUtilsMessengerEXT debugMessenger;

	VkSurfaceKHR surface;

	//VkCommandPool graphicsCommandPool;
	VkCommandPool graphicsCommandPools[NUM_FRAME_DATA];
	VkCommandPool transferCommandPool;
	std::vector<VkCommandBuffer> graphicsCommandBuffers;
	VkCommandBuffer transferCommandBuffer;
	//std::vector<DeviceBuffer> transferStagingBuffersToFree;

	VkQueue graphicsQueue;
	VkQueue computeQueue;
	VkQueue transferQueue;
	VkQueue presentQueue;

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	bool frameBufferResized = false;

	void (*windowResizeCallback)(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY) = nullptr;

	uint32_t currentFrame = 0;

	DescriptorAllocator* descriptorSetAllocator;

	std::vector<DescriptorSet*> descriptorSets;

	std::vector<VkSemaphore> swapchainImageAcquireSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;
	VkSemaphore transferSemaphore;
	VkFence transferFence;
	bool waitForTransfer = false;

	DeviceMemoryAllocator* generalAllocator;

	StagingManager* transferStagingManager;
	StagingManager* graphicsStagingManager;

	Texture* missingTexture = nullptr;
	Texture* whiteTexture = nullptr;

	uint32_t swapchainImageCount;
	uint32_t swapchainImageIndex;

	std::vector<uint8_t> readFromFile(const std::string& fileName) {
		std::ifstream file(fileName, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			std::cout << fileName;
			throw std::runtime_error("Failed to open file!");
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<uint8_t> buffer(fileSize);

		file.seekg(0);
		file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

		file.close();

		return buffer;
	}

	bool check_validation_layer_support() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layer : validationLayers) {
			bool available = false;
			for (VkLayerProperties& availableLayer : availableLayers) {
				if (strcmp(availableLayer.layerName, layer) == 0) {
					available = true;
					break;
				}
			}
			if (!available) {
				return false;
			}
		}

		return true;
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) {

		if (messageSeverity > VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
			std::cerr << "validation layer: " << pCallbackData->pMessage;
			std::cout << std::endl;
		}
		

		return VK_FALSE;
	}

	std::vector<const char*> get_required_extensions() {
		std::vector<const char*> extensions{};
		uint32_t requiredExtCount = 0;
		const char** requiredExts = windowing::get_required_extensions(&requiredExtCount);
		for (uint32_t i = 0; i < requiredExtCount; i++) {
			extensions.push_back(requiredExts[i]);
		}
		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return extensions;
	}

	void create_instance() {
		if (enableValidationLayers && !check_validation_layer_support()) {
			throw std::runtime_error("Validation Layers Requested But Not Supported!");
		}
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Star Chicken";
		appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		appInfo.pEngineName = "DrillEngine V2";
		appInfo.engineVersion = VK_MAKE_API_VERSION(0, 2, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		std::vector<const char*> requiredExtensions = get_required_extensions();

		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
		VkValidationFeaturesEXT features{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			debugCreateInfo.pfnUserCallback = debug_callback;
			createInfo.pNext = &debugCreateInfo;
			
			features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
			features.enabledValidationFeatureCount = 2;
			features.pEnabledValidationFeatures = enables;
			debugCreateInfo.pNext = &features;
		} else {
			createInfo.enabledLayerCount = 0;
		}
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create instance!");
		}

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	}

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		} else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

	void setup_debug_messenger() {
		if (!enableValidationLayers)
			return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debug_callback;
		createInfo.pUserData = nullptr;

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("Failed to set up debug messenger");
		}
	}

	void create_surface() {
		if (windowing::create_window_surface(instance, engine::window, &surface) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create surface");
		}
	}

	void create_image_views() {
		swapChainImageViews.resize(swapChainImages.size());
		for (int i = 0; i < swapChainImages.size(); i++) {
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = swapChainImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = swapChainImageFormat;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			viewInfo.flags = 0;

			if (vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				throw std::runtime_error("Failed to create image view!");
			}
		}
	}

	void create_logical_device() {
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { deviceQueueFamilyIndices.graphicsFamily.value(), deviceQueueFamilyIndices.presentFamily.value(), deviceQueueFamilyIndices.transferFamily.value(), deviceQueueFamilyIndices.computeFamily.value() };

		float queuePriority = 1.0F;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}


		VkPhysicalDeviceFeatures features{};
		features.samplerAnisotropy = VK_TRUE;
		features.multiDrawIndirect = VK_TRUE;
		features.shaderSampledImageArrayDynamicIndexing = VK_TRUE;
		features.textureCompressionBC = VK_TRUE;
		features.fillModeNonSolid = VK_TRUE;
		features.fullDrawIndexUint32 = VK_TRUE;
		VkPhysicalDeviceVulkan12Features features12{};
		features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		features12.descriptorIndexing = VK_TRUE;
		features12.runtimeDescriptorArray = VK_TRUE;
		features12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
		features12.samplerFilterMinmax = VK_TRUE;

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.pNext = &features12;

		deviceCreateInfo.pEnabledFeatures = &features;
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers) {
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		} else {
			deviceCreateInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create logical device!");
		}

		vkGetDeviceQueue(device, deviceQueueFamilyIndices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, deviceQueueFamilyIndices.presentFamily.value(), 0, &presentQueue);
		vkGetDeviceQueue(device, deviceQueueFamilyIndices.transferFamily.value(), 0, &transferQueue);
		vkGetDeviceQueue(device, deviceQueueFamilyIndices.computeFamily.value(), 0, &computeQueue);
	}

	QueueFamilyIndices find_queue_families(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
		std::vector<VkQueueFamilyProperties> properties(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, properties.data());

		uint32_t i = 0;
		for (VkQueueFamilyProperties& prop : properties) {
			if (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}
			if (prop.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				indices.transferFamily = i;
			}
			if (prop.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				indices.computeFamily = i;
			}
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}
			if (indices.is_complete()) {
				break;
			}
			i++;
		}

		return indices;
	}

	bool check_device_extension_support(VkPhysicalDevice device) {
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const char* ext : deviceExtensions) {
			bool contains = false;
			for (VkExtensionProperties& extProps : availableExtensions) {
				if (strcmp(ext, extProps.extensionName) == 0) {
					contains = true;
					break;
				}
			}
			if (!contains) {
				return false;
			}
		}
		return true;
	}

	VkSurfaceFormatKHR choose_swap_surface_format(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
			VkSurfaceFormatKHR result{};
			result.format = VK_FORMAT_B8G8R8A8_SRGB;
			result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			return result;
		}
		/*VkImageFormatProperties prop1;
		VkResult r1 = vkGetPhysicalDeviceImageFormatProperties(physicalDevice, availableFormats[0].format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 0, &prop1);
		VkImageFormatProperties prop2;
		VkResult r2 = vkGetPhysicalDeviceImageFormatProperties(physicalDevice, availableFormats[1].format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 0, &prop2);
		VkImageFormatProperties prop3;
		VkResult r3 = vkGetPhysicalDeviceImageFormatProperties(physicalDevice, availableFormats[2].format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT, 0, &prop3);
		*/
		for (const VkSurfaceFormatKHR& format : availableFormats) {
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return format;
			}
		}
		return availableFormats[0];
	}

	VkPresentModeKHR choose_swap_present_mode(const std::vector<VkPresentModeKHR> presentModes) {
		for (const VkPresentModeKHR& mode : presentModes) {
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return mode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& caps) {
		if (caps.currentExtent.width != UINT32_MAX) {
			return caps.currentExtent;
		} else {
			uint32_t width, height;
			windowing::get_framebuffer_size(engine::window, &width, &height);

			VkExtent2D actualExtent = { width, height };

			actualExtent.width = std::clamp(actualExtent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

			return actualExtent;
		}
	}

	SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.surfaceFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.surfaceFormats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	int32_t score_device(VkPhysicalDevice device) {
		int32_t score = -1;

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceSubgroupProperties subgroupProperties;
		subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
		subgroupProperties.pNext = VK_NULL_HANDLE;

		VkPhysicalDeviceProperties2 deviceProperties2;
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &subgroupProperties;
		vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

		VkPhysicalDeviceFeatures2 deviceFeatures2;
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		VkPhysicalDeviceVulkan12Features deviceFeatures12;
		deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		deviceFeatures12.pNext = nullptr;
		deviceFeatures2.pNext = &deviceFeatures12;
		vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);

		VkPhysicalDeviceFeatures deviceFeatures = deviceFeatures2.features;

		QueueFamilyIndices indices = find_queue_families(device);

		bool extensionsSupported = check_device_extension_support(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = query_swap_chain_support(device);
			swapChainAdequate = !swapChainSupport.presentModes.empty() && !swapChainSupport.surfaceFormats.empty() && deviceFeatures.samplerAnisotropy && deviceFeatures.shaderSampledImageArrayDynamicIndexing && deviceFeatures.multiDrawIndirect && deviceFeatures.textureCompressionBC && deviceFeatures.fullDrawIndexUint32
				&& ((subgroupProperties.supportedOperations & (VK_SUBGROUP_FEATURE_BASIC_BIT | VK_SUBGROUP_FEATURE_ARITHMETIC_BIT | VK_SUBGROUP_FEATURE_BALLOT_BIT | VK_SUBGROUP_FEATURE_VOTE_BIT)) > 0) &&
				deviceFeatures12.descriptorBindingUpdateUnusedWhilePending && deviceFeatures12.samplerFilterMinmax;
		}

		if (indices.is_complete() && extensionsSupported && swapChainAdequate) {
			score += 1;
		} else {
			return score;
		}

		switch (deviceProperties.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			score += 200; break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			score += 400; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			score += 1000; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			score += 100; break; //???
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
			break;
		}

		score += deviceProperties.limits.maxImageDimension2D;

		return score;
	}

	void pick_physical_device() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			throw std::runtime_error("No devices with vulkan support found!");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		int32_t currentDeviceScore = -1;
		for (VkPhysicalDevice& device : devices) {
			if (score_device(device) > currentDeviceScore) {
				physicalDevice = device;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("Failed to find suitable GPU!");
		}
		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
		deviceQueueFamilyIndices = find_queue_families(physicalDevice);
	}

	void create_swap_chain() {
		SwapChainSupportDetails swapChainSupport = query_swap_chain_support(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(swapChainSupport.surfaceFormats);
		VkPresentModeKHR presentMode = choose_swap_present_mode(swapChainSupport.presentModes);
		VkExtent2D extent = choose_swap_extent(swapChainSupport.capabilities);

		/*swapchainImageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && swapchainImageCount > swapChainSupport.capabilities.maxImageCount) {
			swapchainImageCount = swapChainSupport.capabilities.maxImageCount;
		}*/

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = NUM_FRAME_DATA;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		uint32_t queueFamilyIndices[] = { deviceQueueFamilyIndices.graphicsFamily.value(), deviceQueueFamilyIndices.presentFamily.value() };
		if (deviceQueueFamilyIndices.graphicsFamily != deviceQueueFamilyIndices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create swap chain!");
		}

		VKU_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, nullptr), "Failed to get swapchain image count!");
		swapChainImages.resize(swapchainImageCount);
		VKU_CHECK_RESULT(vkGetSwapchainImagesKHR(device, swapChain, &swapchainImageCount, swapChainImages.data()), "Failed to get swapchain images!");

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	void create_command_pools() {
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = deviceQueueFamilyIndices.graphicsFamily.value();
		//poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.flags = 0;

		//VKU_CHECK_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &graphicsCommandPool), "Failed to create graphics command pools!");
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			VKU_CHECK_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &graphicsCommandPools[i]), "Failed to create graphics command pools!");
		}

		poolInfo.queueFamilyIndex = deviceQueueFamilyIndices.transferFamily.value();
		//poolInfo.flags = 0;
		VKU_CHECK_RESULT(vkCreateCommandPool(device, &poolInfo, nullptr, &transferCommandPool), "Failed to create transfer command pool!");
	}

	DescriptorSet* create_descriptor_set(const std::initializer_list<Uniform*> uniforms) {
		descriptorSets.push_back(new DescriptorSet(uniforms));
		descriptorSets.back()->create_descriptor_sets();
		return descriptorSets.back();
	}

	void destroy_descriptor_set(DescriptorSet* set) {
		descriptorSets.erase(std::find(descriptorSets.begin(), descriptorSets.end(), set));
		set->destroy_descriptor_sets();
		delete set;
	}

	void create_descriptor_sets() {
		for (DescriptorSet* set : descriptorSets) {
			set->create_descriptor_sets();
		}
	}

	void recreate_descriptor_set_layouts() {
		for (DescriptorSet* set : descriptorSets) {
			set->recreate_set_layout();
		}
	}

	void destroy_descriptor_sets() {
		for (DescriptorSet* set : descriptorSets) {
			set->destroy_descriptor_sets();
		}
	}

	void delete_descriptor_sets() {
		for (DescriptorSet* set : descriptorSets) {
			delete set;
		}
	}

	void mark_transfer_dirty() {
		waitForTransfer = true;
	}

	void begin_command_buffer(VkCommandBuffer buf) {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;

		VKU_CHECK_RESULT(vkBeginCommandBuffer(buf, &beginInfo), "Failed to begin command buffer recording!");
	}

	void end_command_buffer(VkCommandBuffer buf) {
		VKU_CHECK_RESULT(vkEndCommandBuffer(buf), "Failed to end command buffer recording!");
	}

	void begin_graphics_command_buffer(VkCommandBuffer commandBuffer) {
		begin_command_buffer(commandBuffer);

		//Dynamic state

		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	}

	void end_graphics_command_buffer(VkCommandBuffer commandBuffer) {
		end_command_buffer(commandBuffer);
	}

	void create_command_buffers() {
		graphicsCommandBuffers.resize(NUM_FRAME_DATA);

		VkCommandBufferAllocateInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufInfo.commandBufferCount = 1;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			cmdBufInfo.commandPool = graphicsCommandPools[i];
			VKU_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufInfo, &graphicsCommandBuffers[i]), "Failed to allocate graphics command buffer!");
		}
		//VKU_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufInfo, graphicsCommandBuffers.data()), "Failed to allocate graphics command buffers!");

		cmdBufInfo.commandPool = transferCommandPool;
		cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		VKU_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufInfo, &transferCommandBuffer), "Failed to allocate transfer command buffer!");
	}

	void create_sync_objects() {
		swapchainImageAcquireSemaphores.resize(swapchainImageCount);
		renderFinishedSemaphores.resize(NUM_FRAME_DATA);
		inFlightFences.resize(NUM_FRAME_DATA);
		imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreInfo.flags = 0;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (uint32_t i = 0; i < swapchainImageCount; i++) {
			VKU_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &swapchainImageAcquireSemaphores[i]), "Failed to create semaphore!");
		}
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			VKU_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]), "Failed to create semaphore!");

			VKU_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]), "Failed to create fence!");
		}
		
		VKU_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &transferSemaphore), "Failed to create semaphore!");
		VKU_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &transferFence), "Failed to create fence!");
	}
	
	void cleanup_swapchain() {
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			//vkFreeCommandBuffers(device, graphicsCommandPools[i], 1, &graphicsCommandBuffers[i]);
			vkResetCommandPool(device, graphicsCommandPools[i], 0);
		}
		vkResetCommandPool(device, transferCommandPool, 0);

		for (VkImageView& imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);

		/*for (size_t i = 0; i < swapChainImages.size(); i++) {
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		}*/

		//??? Why did I think I needed to delete and recreate uniform data on swapchain resize?
		/*for (Uniform* uni : Uniform::uniformsToRecreate) {
			uni->destroy();
		}*/
		//destroy_descriptor_sets();
	}

	void recreate_swapchain() {
		std::cout << "Recreating swapchain!" << std::endl;
		uint32_t width = 0, height = 0;
		windowing::get_framebuffer_size(engine::window, &width, &height);
		while (width == 0 || height == 0) {
			windowing::get_framebuffer_size(engine::window, &width, &height);
			windowing::poll_events(engine::window);
		}

		vkDeviceWaitIdle(device);

		cleanup_swapchain();

		create_swap_chain();
		create_image_views();
		//???
		//create_command_buffers();

		/*vkResetCommandPool(device, transferCommandPool, 0);
		begin_command_buffer(transferCommandBuffer);

		for (Uniform* uni : Uniform::uniformsToRecreate) {
			uni->create();
		}

		end_command_buffer(transferCommandBuffer);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &vku::transferCommandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &vku::transferSemaphore;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		vkQueueSubmit(vku::transferQueue, 1, &submitInfo, vku::transferFence);
		mark_transfer_dirty();*/

		//create_descriptor_sets();

		vkQueueWaitIdle(vku::graphicsQueue);
		VkCommandBuffer cmdBuf = vku::graphicsCommandBuffers[0];
		vkResetCommandPool(device, graphicsCommandPools[0], 0);
		vku::begin_command_buffer(cmdBuf);
		windowResizeCallback(cmdBuf, vku::swapChainExtent.width, vku::swapChainExtent.height);
		vku::end_command_buffer(cmdBuf);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		vkQueueSubmit(vku::graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

		vkQueueWaitIdle(vku::graphicsQueue);
	}

	bool setup_next_frame() {
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, swapchainImageAcquireSemaphores[currentFrame], VK_NULL_HANDLE, &swapchainImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreate_swapchain();
			return false;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Acquire next image failed!");
		}

		if (imagesInFlight[swapchainImageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(device, 1, &imagesInFlight[swapchainImageIndex], VK_TRUE, UINT64_MAX);
		}
		imagesInFlight[swapchainImageIndex] = inFlightFences[currentFrame];
		generalAllocator->free_old_data(currentFrame);
		Uniform::update_pending_sets(currentFrame);
		return true;
	}

	void begin_frame() {
		vkResetCommandPool(device, graphicsCommandPools[currentFrame], 0);
		begin_graphics_command_buffer(graphicsCommandBuffers[currentFrame]);
	}

	void end_frame() {
		end_graphics_command_buffer(graphicsCommandBuffers[currentFrame]);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[] = { swapchainImageAcquireSemaphores[currentFrame], transferSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
		submitInfo.waitSemaphoreCount = waitForTransfer ? 2 : 1;
		waitForTransfer = false;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphicsCommandBuffers[currentFrame];
		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		
		vkResetFences(device, 1, &inFlightFences[currentFrame]);
		VkResult submitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
		VKU_CHECK_RESULT(submitResult, "Failed to submit graphics queue!");

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &swapchainImageIndex;
		presentInfo.pResults = nullptr;

		VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || frameBufferResized) {
			recreate_swapchain();
			frameBufferResized = false;
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("Failed to queue present!");
		}

		currentFrame = (currentFrame + 1) % NUM_FRAME_DATA;
	}

	uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags flags) {
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memProps.memoryTypes[i].propertyFlags & flags) == flags) {
				return i;
			}
		}
		throw std::runtime_error("Failed to find suitable memory type!");
	}

	VkCommandBuffer graphics_cmd_buf() {
		return graphicsCommandBuffers[currentFrame];
	}

	void create_renderpass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription attachments[] = { colorAttachment };

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentRef.attachment = 0;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
	}

	void init_vulkan(void (*resizeCallback)(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY)) {
		windowResizeCallback = resizeCallback;
		create_instance();
		setup_debug_messenger();
		create_surface();
		pick_physical_device();
		create_logical_device();

		generalAllocator = new DeviceMemoryAllocator();
		descriptorSetAllocator = new DescriptorAllocator();
		descriptorSetAllocator->init();

		create_swap_chain();
		create_image_views();
		create_renderpass();
		create_command_pools();
		create_command_buffers();
		create_sync_objects();

		transferStagingManager = new StagingManager();
		graphicsStagingManager = new StagingManager();
		transferStagingManager->init(transferQueue, deviceQueueFamilyIndices.transferFamily.value());
		graphicsStagingManager->init(graphicsQueue, deviceQueueFamilyIndices.graphicsFamily.value());

		vkWaitForFences(vku::device, 1, &vku::transferFence, VK_TRUE, UINT64_MAX);
		vkResetFences(vku::device, 1, &vku::transferFence);

		//begin_command_buffer(transferCommandBuffer);
		missingTexture = load_texture(L"missing.dtf", 0);
		whiteTexture = load_texture(L"white.dtf", 0);
		/*end_command_buffer(transferCommandBuffer);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &transferCommandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		vkQueueSubmit(transferQueue, 1, &submitInfo, transferFence);
		mark_transfer_dirty();*/
	}

	void cleanup_vulkan() {
		delete_texture(missingTexture);
		delete_texture(whiteTexture);
		descriptorSets.clear();

		transferStagingManager->destroy();
		graphicsStagingManager->destroy();
		delete transferStagingManager;
		delete graphicsStagingManager;

		for (uint32_t i = 0; i < swapchainImageCount; i++) {
			vkDestroySemaphore(device, swapchainImageAcquireSemaphores[i], nullptr);
		}
		for (size_t i = 0; i < NUM_FRAME_DATA; i++) {
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}
		vkDestroySemaphore(device, transferSemaphore, nullptr);
		vkDestroyFence(device, transferFence, nullptr);
		cleanup_swapchain();
		descriptorSetAllocator->cleanup();
		delete descriptorSetAllocator;
		delete generalAllocator;
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			vkDestroyCommandPool(device, graphicsCommandPools[i], nullptr);
		}
		vkDestroyCommandPool(device, transferCommandPool, nullptr);
		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);

	}



	void recompile_modified_shaders(std::string shaderDir) {
		std::cout << "Recompiling modified shaders..." << std::endl;
		struct ShaderFile {
			std::string path;
			std::string name;
		};
		std::vector<ShaderFile> dirtyShaders{};
		//Look for a timestamp file (file doesn't contain anything, but we can read the last recompile time from its metadata)
		uint64_t previousRecompTime = 0;
		std::filesystem::directory_entry timestamp{ shaderDir + "\\spirv\\_timestamp" };
		if (timestamp.exists()) {
			previousRecompTime = timestamp.last_write_time().time_since_epoch().count();
		}
		//Search through all files in the shader directory, if it's a shader and it was modified since the last recompile, add its directory to the list
		for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(shaderDir)) {
			if (entry.last_write_time().time_since_epoch().count() < previousRecompTime) {
				continue;
			}
			std::string fileName = entry.path().filename().string();
			if (util::string_ends_with(fileName.c_str(), "vert") ||
				util::string_ends_with(fileName.c_str(), "frag") ||
				util::string_ends_with(fileName.c_str(), "comp")) {
				dirtyShaders.push_back(ShaderFile{ entry.path().string(), fileName });
			}
		}
		uint32_t startPathLength = shaderDir.length();
		//Go through all the shaders that need recompilation, execute glslc to compile them to spirv
		for (ShaderFile& dirtyShader : dirtyShaders) {
			std::cout << "Compiling shader " << dirtyShader.name << "..." << std::endl;
			const char* ext = "fail";
			if (util::string_ends_with(dirtyShader.name.c_str(), "vert")) {
				ext = "vspv";
			} else if (util::string_ends_with(dirtyShader.name.c_str(), "frag")) {
				ext = "fspv";
			} else if (util::string_ends_with(dirtyShader.name.c_str(), "comp")) {
				ext = "cspv";
			}
			std::string nameNoExt = dirtyShader.name.substr(0, dirtyShader.name.find('.'));
			std::string outputDir = shaderDir + "\\spirv" + dirtyShader.path.substr(startPathLength, dirtyShader.path.length() - startPathLength - dirtyShader.name.length()) + nameNoExt + "." + ext;
			std::string programRun = shaderDir + "\\spirv\\glslc.exe \"" + dirtyShader.path + "\" -o \"" + outputDir + "\" --target-env=vulkan1.2";
			int32_t result = util::run_program(programRun.c_str());
			if (result != 0) {
				std::cout << "Shader compilation terminated with exit code " << result << std::endl;
			}
		}
		//Open and close the timestamp file, a new last write time will be added to the file's metadata
		std::ofstream timestampWriteStream{ shaderDir + "\\spirv\\_timestamp", std::ios::binary };
		timestampWriteStream.close();
		std::cout << "Shader recompilation complete." << std::endl;
	}
	
}
