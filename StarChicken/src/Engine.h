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

	extern job::SpinLock loggerLock;

	inline void log(std::string str) {
		std::cout << str << std::endl;
	}
}