#pragma once

#include <iostream>
#include <string>
#define NOMINMAX
#pragma comment(lib, "Ws2_32.lib")
#include <Windows.h>
#include "EntityComponentSystem.h"
#include "Profiling.h"
#include "JobSystem.h"
#include "graphics/VkUtil.h"
#include "util/Windowing.h"

namespace vku {
	class UniformTexture2DArray;
	class UniformSampler2D;
}
namespace engine {

	//extern job::JobSystem jobSystem;
	extern job::JobSystem jobSystem;
	extern windowing::Window* window;

	extern bool keyState[windowing::MAX_KEY_CODE];
	extern bool mouseState[windowing::MAX_MOUSE_CODE];
	extern vec2f deltaMouse;

	extern float deltaTime;

	extern job::SpinLock loggerLock;

	extern vku::UniformSampler2D* bilinearSampler;
	extern vku::UniformSampler2D* nearestSampler;
	extern vku::UniformSampler2D* maxSampler;
	extern vku::UniformSampler2D* minSampler;

	extern vku::SingleBufferAllocator* vertexBufferAllocator;
	extern vku::SingleBufferAllocator* indexBufferAllocator;
	extern vku::SingleBufferAllocator* stagingBufferAllocator;

	extern vku::UniformTexture2DArray* textureArray;

	extern uint16_t missingTexIdx;
	extern uint16_t whiteTexIdx;
	extern uint16_t duckTexIdx;

	inline void log(std::string str) {
		std::cout << str << std::endl;
	}
}