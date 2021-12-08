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
#include "graphics/SingleBufferAllocator.h"
#include "util/Windowing.h"

namespace engine {

	//extern job::JobSystem jobSystem;
	extern job::JobSystem jobSystem;
	extern windowing::Window* window;

	extern job::SpinLock loggerLock;

	extern vku::SingleBufferAllocator* vertexBufferAllocator;
	extern vku::SingleBufferAllocator* indexBufferAllocator;
	extern vku::SingleBufferAllocator* stagingBufferAllocator;

	inline void log(std::string str) {
		std::cout << str << std::endl;
	}
}