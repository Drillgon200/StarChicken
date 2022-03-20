#include "Engine.h"
#include <mutex>
#include <algorithm>
#include <processthreadsapi.h>
#include "util/DrillMath.h"
#include "graphics/VertexFormats.h"
#include "graphics/ShaderUniforms.h"
#include "graphics/GraphicsPipeline.h"
#include "graphics/RenderPass.h"
#include "graphics/StagingManager.h"
#include "graphics/TextureManager.h"
#include "resources/Models.h"
#include "graphics/geometry/GeometryAllocator.h"
#include "resources/FileDocument.h"
#include "util/Util.h"
#include "util/MSDFGenerator.h"
#include "util/FontRenderer.h"
#include "ui/Gui.h"
#include "InputSubsystem.h"
#include "scene/Scene.h"
#include "RenderSubsystem.h"
#include "ResourcesSubsystem.h"

uint64_t iterations;

namespace engine {

	int32_t WINDOW_WIDTH = 512;
	int32_t WINDOW_HEIGHT = 512;
	windowing::Window* window;
	
	job::JobSystem jobSystem;

	float deltaTime;
	
	scene::Scene sceneSystem;
	InputSubsystem inputSystem;
	RenderSubsystem renderSystem;
	ResourcesSubsystem resourceSubsystem;
	//Other subsystems go here. Should eventually include stuff like physics, animation, audio, enemy AI, streaming, etc.
	ui::Gui* mainGui;

	//For external subsystem access
	scene::Scene& scene{ sceneSystem };
	InputSubsystem& userInput{ inputSystem };
	RenderSubsystem& rendering{ renderSystem };
	ResourcesSubsystem& resourceManager{ resourceSubsystem };

	std::chrono::steady_clock::time_point lastTime;

	void render() {
		renderSystem.render();
	}

	void update() {
		if (inputSystem.get_scroll() != 0.0F) {
			mainGui->mouse_scroll(inputSystem.get_scroll());
		}
		vec2f lastMousePos = inputSystem.get_last_mouse_pos();
		vec2f dMouse = inputSystem.get_last_delta_mouse();
		if ((dMouse.x != 0) || (dMouse.y != 0)) {
			mainGui->mouse_drag(lastMousePos.x, lastMousePos.y, dMouse.x, dMouse.y);
		}
		vec2f mousePos = inputSystem.get_mouse_pos();
		mainGui->mouse_hover(mousePos.x, mousePos.y);
	}

	void main_loop() {
		while (!windowing::window_should_close(window)) {
			/*if (iterations % 10000 == 0) {
				std::cout << iterations << "\n";
			}*/
			++iterations;
			job::JobDecl decls[2];
			decls[0] = job::JobDecl(update);
			decls[1] = job::JobDecl(render);
				
			jobSystem.start_jobs_and_wait_for_counter(decls, 2);
		}
	}

	void framebuffer_resize_callback(windowing::Window* window, uint32_t width, uint32_t height) {
		bool* fboResized = reinterpret_cast<bool*>(windowing::get_user_ptr(window));
		*fboResized = true;
	}

	void key_callback(windowing::Window* window, uint32_t keyCode, uint32_t state) {
		inputSystem.on_key_pressed(window, keyCode, state);
		mainGui->on_key_typed(keyCode, state);
	}

	void mouse_callback(windowing::Window* window, uint32_t button, uint32_t state) {
		inputSystem.on_mouse_used(window, button, state);
		
		vec2f cursorPos = inputSystem.get_mouse_pos();
		bool guiDidAction = mainGui->on_click(cursorPos.x, cursorPos.y, button, (state == windowing::MOUSE_BUTTON_STATE_DOWN) ? false : true);
		if (!guiDidAction && (button == windowing::MOUSE_LEFT) && (state == windowing::MOUSE_BUTTON_STATE_DOWN)) {
			int32_t mouseOverId = renderSystem.get_select();
			scene.select_object(mouseOverId);
		}
	}

	//Must be called from main thread
	void init_window() {
		windowing::init();
		if (!windowing::query_vulkan_support()) {
			windowing::cleanup();
			std::cerr << "Failed to initialize game! Vulkan is not supported!" << std::endl;
			exit(1);
		}

		window = windowing::create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Star Chicken", true);
		windowing::set_user_ptr(window, &vku::frameBufferResized);
		windowing::set_window_resize_callback(window, framebuffer_resize_callback);
		windowing::set_keyboard_callback(window, key_callback);
		windowing::set_mouse_callback(window, mouse_callback);
	}

	void cleanup_window() {
		windowing::cleanup();
	}

	void print_memory_info() {
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(vku::physicalDevice, &memProps);
		std::cout << "\n\nMemory types: " << memProps.memoryTypeCount << "\n";
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
			std::cout << "Type index: " << i << ", Type heap index: " << memProps.memoryTypes[i].heapIndex << ", Type flags: " << memProps.memoryTypes[i].propertyFlags << "\n";
		}
		std::cout << "\nMemory heaps: " << memProps.memoryHeapCount << "\n";
		for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
			std::cout << "Heap index: " << i << ", Heap size: " << memProps.memoryHeaps[i].size << ", Heap flags: " << memProps.memoryHeaps[i].flags << "\n";
		}
		std::cout << "Buffer image granularity: " << vku::deviceProperties.limits.bufferImageGranularity << std::endl;
		std::cout << "Max texture size: " << vku::deviceProperties.limits.maxImageDimension2D << std::endl;
		exit(0);
	}

	void resize_callback(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY) {
		renderSystem.on_window_resize(cmdBuf, newX, newY);
		
		mainGui->resize(static_cast<float>(newX), static_cast<float>(newY));
		scene.framebuffer_resized(cmdBuf, newX, newY);
	}

	void init_engine() {
		vku::init_vulkan(resize_callback);

		scene.init();
		resourceManager.load_uniforms();
		renderSystem.init();
		resourceManager.load_descriptor_sets();
		renderSystem.create_descriptor_sets();
		resourceManager.load_pipelines();
		renderSystem.create_pipelines();
		resourceManager.load_textures();

		//print_memory_info();
		ui::init();
		mainGui = new ui::Gui(&scene);
		mainGui->resize(static_cast<float>(vku::swapChainExtent.width), static_cast<float>(vku::swapChainExtent.height));

		lastTime = std::chrono::high_resolution_clock::now();
	}

	void cleanup() {
		ui::cleanup();
		delete mainGui;
		scene.cleanup();

		resourceManager.free_resources();
		renderSystem.cleanup();

		vku::cleanup_vulkan();
	}

	//We're going to go with snake_case names to match the standard library. Feels a little weird coming from java, but I'll get over it.
	//I wonder if I should have made this camelCase actually, to match glfw and vulkan
	//Variable names should be camelCase to differentiate them from functions
	//Class and struct names should be PascalCase
	//Avoid auto most of the time. I think it seriously affects the readability of some code when I can't just look at it and see the type.
	void entry_point() {
		init_engine();
		std::cout << "here0 " << job::threadData->threadId << std::endl;
		main_loop();
		cleanup();
	}
}

int main() {
	//util::msdf::generate_MSDF(L"resources/textures/startchickenfont2.svg", L"resources/textures/starchickenfont.msdf", 64, 64, true);
	//return 0;

	vku::recompile_modified_shaders(".\\resources\\shaders");
	engine::init_window();
	engine::init_engine();
	while (!windowing::window_should_close(engine::window)) {
		engine::inputSystem.handle_input();
		engine::WINDOW_WIDTH = windowing::width;
		engine::WINDOW_HEIGHT = windowing::height;

		std::chrono::steady_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
		engine::deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - engine::lastTime).count();
		engine::lastTime = currentTime;
		
		engine::update();
		engine::renderSystem.render();
	}
	VKU_CHECK_RESULT(vkDeviceWaitIdle(vku::device), "Failed to wait idle!");
	engine::cleanup();
	engine::cleanup_window();
	return 0;



	engine::init_window();

	uint32_t threadCount = std::max(static_cast<uint32_t>(1), std::thread::hardware_concurrency()-1);
	threadCount = 2;
	engine::jobSystem.init_job_system(threadCount);
	profiling::init_profiler([](void) {return static_cast<int32_t>(job::threadData->threadId); }, threadCount, 1024, "testlog");

	job::JobDecl decl{ engine::entry_point };
	engine::jobSystem.start_entry_point(decl);

	while (!engine::jobSystem.is_done()) {
		windowing::poll_events(engine::window);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	engine::jobSystem.end_job_system();
	engine::cleanup_window();

	return 0;
}