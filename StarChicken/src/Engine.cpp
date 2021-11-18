#include "Engine.h"
#include <mutex>
#include <algorithm>
#include <processthreadsapi.h>

uint64_t iterations;

namespace engine {
	//job::JobSystem jobSystem;
	job::JobSystem jobSystem;

	int WIDTH = 512;
	int HEIGHT = 512;

	GLFWwindow* window;
	VkInstance instance;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	VkDebugUtilsMessengerEXT debugMessenger;

	VkSurfaceKHR surface;

	VkQueue graphicsQueue;
	VkQueue computeQueue;
	VkQueue transferQueue;
	VkQueue presentQueue;

	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	bool frameBufferResized = false;

	struct test {
		int bruh;
	};

	struct test2 {
		uint16_t arg;
		float mug;
	};

	void render() {
	}

	void update() {
	}

	void main_loop() {
		while (!glfwWindowShouldClose(window)) {
			if (iterations % 10000 == 0) {
				std::cout << iterations << "\n";
			}
			++iterations;
			job::JobDecl decls[2];
			decls[0] = job::JobDecl(update);
			decls[1] = job::JobDecl(render);
				
			jobSystem.start_jobs_and_wait_for_counter(decls, 2);
		}
	}

	void framebuffer_resize_callback(GLFWwindow* window, int width, int height) {
		bool* fboResized = reinterpret_cast<bool*>(glfwGetWindowUserPointer(window));
		*fboResized = true;
	}

	//Must be called from main thread
	void init_window() {
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Star Chicken", nullptr, nullptr);
		glfwSetWindowUserPointer(window, &frameBufferResized);
		glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback);
	}

	void init_engine() {
		//vku::init_vulkan();
	}

	void cleanup() {
		glfwDestroyWindow(window);
	}

	//We're going to go with snake_case names to match the standard library. Feels a little weird coming from java, but I'll get over it.
	//I wonder if I should have made this camelCase actually, to match glfw and vulkan
	//Variable names should be camelCase to differentiate them from functions
	//Class and struct names should be PascalCase
	void entry_point() {
		init_engine();
		std::cout << "here0 " << job::threadData->threadId << std::endl;
		main_loop();
		cleanup();
	}
}

int main() {

	engine::init_window();

	uint32_t threadCount = std::max(static_cast<uint32_t>(1), std::thread::hardware_concurrency()-1);
	threadCount = 2;
	engine::jobSystem.init_job_system(threadCount);
	profiling::init_profiler([](void) {return static_cast<int32_t>(job::threadData->threadId); }, threadCount, 1024, "testlog");

	job::JobDecl decl{ engine::entry_point };
	engine::jobSystem.start_entry_point(decl);

	while (!engine::jobSystem.is_done()) {
		glfwPollEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	engine::jobSystem.end_job_system();

	return 0;
}