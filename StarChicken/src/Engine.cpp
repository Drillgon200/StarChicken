#include "Engine.h"
#include <algorithm>

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

	job::SpinLock loggerLock{};

	void nestfunc() {
		FUNC_PROF();

		uint32_t count = 0;
		for (uint32_t i = 0; i < 100000000; i++) {
			count += i;
		}

		SPIN_LOCK(loggerLock);
		std::cout << "No";
		std::string str = std::to_string(count);
		std::cout << "Yes " << str;
		std::cout << "No";
		count += 10;
		str = std::to_string(count);
		std::cout << str << std::endl;
	}

	//#pragma optimize("", off)
	void func() {
		FUNC_PROF();

		/*job::JobDecl decls[3];
		decls[0] = job::JobDecl(nestfunc);
		decls[1] = job::JobDecl(nestfunc);
		decls[2] = job::JobDecl(nestfunc);
		jobSystem.start_jobs_and_wait_for_counter(decls, 3); */
	}
	//#pragma optimize("", on)

	void render() {
	}

	void update() {
	}

	void main_loop() {
		std::cout << "here" << std::endl;
		while (!glfwWindowShouldClose(window)) {
			bool shouldClose = glfwWindowShouldClose(window);
			 {
				//SCOPE_PROF("Main loop");
				job::JobDecl decls[2];
				decls[0] = job::JobDecl(update);
				decls[1] = job::JobDecl(render);
				
				jobSystem.start_jobs_and_wait_for_counter(decls, 2);
				
			}
		}

		//vkDeviceWaitIdle(device);
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
		std::cout << "here0 " << std::to_string(job::this_thread_id()) << std::endl;
		main_loop();
		cleanup();
		return;
		/*FUNC_PROF()

		job::JobDecl decls[2];
		decls[0] = job::JobDecl(func);
		decls[1] = job::JobDecl(func);
		jobSystem.start_jobs_and_wait_for_counter(decls, 2);*/
	}
}

int& test() {
	int bruh = 2;
	return bruh;
}

Context testctx{};

void brug() {
	load_registers(testctx);
	std::cout << "warn" << std::endl;
}

int main() {
	/*engine::init_window();
	while (!glfwWindowShouldClose(engine::window)) {
		glfwPollEvents();
	}
	engine::cleanup();
	return 0;*/
	engine::init_window();

	uint32_t threadCount = std::max(static_cast<uint32_t>(1), std::thread::hardware_concurrency()-1);
	engine::jobSystem.init_job_system(threadCount);
	profiling::init_profiler([](void) {return job::this_thread_id(); }, threadCount, 1024, "testlog");

	job::JobDecl decl{ engine::entry_point };
	engine::jobSystem.start_entry_point(decl);

	while (!engine::jobSystem.is_done()) {
		glfwPollEvents();
		bool shouldClose = glfwWindowShouldClose(engine::window);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	engine::jobSystem.end_job_system();

	/*engine::jobSystem.initSystem(threadCount);
	profiling::init_profiler([](void) {return job::thisThreadId(); }, threadCount, 1024, "testlog");

	jobtest::JobDecl decl{ engine::entry_point };
	engine::jobSystem.entryJob(decl);

	while (!engine::jobSystem.done()) {
		glfwPollEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	engine::jobSystem.end();

	profiling::dump_data();*/
	return 0;
}