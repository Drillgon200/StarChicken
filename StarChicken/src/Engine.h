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
namespace ui {
	class Gui;
}
namespace scene {
	class Scene;
}
namespace engine {

	class RenderSubsystem;
	class InputSubsystem;

	//extern job::JobSystem jobSystem;
	extern job::JobSystem jobSystem;
	extern windowing::Window* window;

	extern float deltaTime;

	extern scene::Scene& scene;
	extern InputSubsystem& userInput;
	extern RenderSubsystem& rendering;
	extern ui::Gui* mainGui;;

	inline void log(std::string str) {
		std::cout << str << std::endl;
	}
}