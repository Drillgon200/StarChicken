#pragma once

#include <iostream>
#include <thread>
#include <string>
#define NOMINMAX
#pragma comment(lib, "Ws2_32.lib")
#include <Windows.h>
#include "EntityComponentSystem.h"
#include "Profiling.h"
#include "JobSystem.h"
#include "graphics\VkUtil.h"

namespace engine {

	//extern job::JobSystem jobSystem;
	extern job::JobSystem jobSystem;

	extern GLFWwindow* window;
	extern VkInstance instance;
	extern VkPhysicalDevice physicalDevice;
	extern VkDevice device;
	extern VkDebugUtilsMessengerEXT debugMessenger;

	extern VkQueue graphicsQueue;
	extern VkQueue computeQueue;
	extern VkQueue transferQueue;
	extern VkQueue presentQueue;

	extern job::SpinLock loggerLock;

	inline void log(std::string str) {
		std::cout << str << std::endl;
	}
}

inline bool big_endian() {
	return htonl(1) == 1;
}