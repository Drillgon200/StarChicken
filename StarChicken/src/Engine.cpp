#include "Engine.h"
#include <mutex>
#include <algorithm>
#include <processthreadsapi.h>

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

	static uint64_t iterations;

	void render() {
		//for (int i = 0; i < 10000000; i++) {
		//	jobSystem.yield_job();
		//}
	}

	void update() {
		//for (int i = 0; i < 10000000; i++) {
		//	jobSystem.yield_job();
		//}
	}

	/*Context* myThreadContext;
	job::Job** myCurrentJobs;
	thread_local uint32_t myThreadId = 5443214;
	job::SpinLock jobLock{};
	std::vector<job::Job*> jobs{};
	std::vector<job::Job*> newJobs{};

	void startAndSleep(job::JobDecl* decls, int32_t count) {
		uint32_t tid = myThreadId;
		job::JobCounter counter{ myCurrentJobs[tid], count };
		jobLock.lock();
		for (uint32_t i = 0; i < count; i++) {
			job::Job* j = new job::Job(nullptr);
			j->currentTask = &decls[i];
			j->currentTask->counter = &counter;
			newJobs.push_back(j);
		}
		jobLock.unlock();
		swap_registers(myCurrentJobs[tid]->ctx, myThreadContext[tid]);
	}

	void entry2() {
		while (true) {
			if (iterations % 1000 == 0) {
				std::cout << iterations << "\n";
			}
			++iterations;
			if (iterations > 10000000) {
				render();
			}
			job::JobDecl decls[2];
			decls[0] = job::JobDecl(update);
			decls[1] = job::JobDecl(render);

			startAndSleep(decls, 2);
		}
	}

	

	void jobFunc2(job::Job* job) {
		job->currentTask->func(job->currentTask->arg);
		uint32_t tid = myThreadId;
		job->active = false;
		if (job->currentTask->counter) {
			int32_t count = job->currentTask->counter->counter.fetch_add(-1, std::memory_order_relaxed);
			if (count == 1) {
				jobLock.lock();
				jobs.push_back(job->currentTask->counter->job);
				jobLock.unlock();
			}
		}
		job->currentTask = nullptr;
		swap_registers(job->ctx, myThreadContext[tid]);
	}

	void threadFunc2(uint32_t tid) {
		myThreadId = tid;
		while (true) {
			jobLock.lock();
			job::Job* job = nullptr;
			if (jobs.size() > 0) {
				job = jobs.back();
				jobs.pop_back();
			}
			jobLock.unlock();


			if (job) {
				if (!job->active) {
					job->active = true;
					job->ctx.rip = jobFunc2;
					job->ctx.rsp = job->stackPointer;
				}
				myCurrentJobs[tid] = job;
				swap_registers_arg(myThreadContext[tid], job->ctx, job);
				if (!job->active) {
					delete job;
				} else {
					jobLock.lock();
					while (newJobs.size() > 0) {
						job::Job* new_jb = newJobs.back();
						newJobs.pop_back();
						jobs.push_back(new_jb);
					}
					jobLock.unlock();
				}
			}

		}
	}*/

	void main_loop() {
		/*job::JobDecl testd[2];
		testd[0] = job::JobDecl(update);
		testd[1] = job::JobDecl(render);
		jobSystem.start_jobs_and_wait_for_counter(testd, 2);
		std::cout << "hereaaaaaaaa" << std::endl;
		return;*/
		while (!glfwWindowShouldClose(window)) {
			if (iterations % 1000 == 0) {
				std::cout << iterations << "\n";
			}
			++iterations;
			job::JobDecl decls[2];
			decls[0] = job::JobDecl(update);
			decls[1] = job::JobDecl(render);
				
			jobSystem.start_jobs_and_wait_for_counter(decls, 2);
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

char* data = nullptr;
char* stackPointer = nullptr;
Context testctx{};
Context testctx2{};

/*void tryAgain() {
	uint8_t mem2[1024 * 512];

	Context ctx{};

	save_registers(ctx);
	uint64_t base = reinterpret_cast<uint64_t>(ctx.stackBase);
	uint64_t limit = reinterpret_cast<uint64_t>(ctx.stackLimit);
	std::cout << "size2: " << (base - limit) << std::endl;
	std::cout << "guaranteed stack bytes 2: " << ctx.guaranteedStackBytes << std::endl;
}*/

void test() {
	Context agr{};
	save_registers(agr);
	std::cout << std::hex << std::to_string((reinterpret_cast<uintptr_t>(agr.rsp) % 16)) << std::endl;
	alignas(16) uint64_t te = 123;
	std::cout << te << "\n";
	load_registers(testctx2);
}

int main() {
	/*job::Job* entry2 = new job::Job(nullptr);
	job::JobDecl edecl{engine::entry2};
	entry2->currentTask = &edecl;
	engine::jobs.push_back(entry2);
	engine::myThreadContext = new Context[2];
	engine::myCurrentJobs = new job::Job * [2];
	std::atomic_thread_fence(std::memory_order_seq_cst);
	std::cout << "starting threads" << std::endl;
	std::thread threads[2]{ std::thread{ engine::threadFunc2, 0 }, std::thread{ engine::threadFunc2, 1 } };
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
	threads[0].join();
	threads[1].join();
	return 0;*/
	/*Context agr{};
	save_registers(agr);

	data = reinterpret_cast<char*>(malloc(1024 * 512));
	std::cout << std::hex << reinterpret_cast<uintptr_t>(data) << std::endl;
	//Stacks grow down, so start at the top
	stackPointer = data + 1024 * 512;
	//Align to 16 bytes
	stackPointer = (char*)((uintptr_t)stackPointer & -16L);
	//Red zone
	//From further research, this is not actually the red zone. The red zone is really below the stack, always the 128 bytes below RSP.
	//I'll leave it here anyway, just for some padding
	stackPointer -= 128;
	std::cout << std::hex << reinterpret_cast<uintptr_t>(stackPointer) << " " << (reinterpret_cast<uintptr_t>(stackPointer) % 16) << " " << (reinterpret_cast<uintptr_t>(agr.rsp) % 16) << std::endl;
	//stackPointer -= 8;

	testctx.stackBase = stackPointer;
	testctx.stackLimit = data;
	testctx.deallocationStack = data;
	testctx.guaranteedStackBytes = 0;
	testctx.rsp = stackPointer - 8;
	testctx.rip = test;
	swap_registers(testctx2, testctx);
	std::cout << "end" << std::endl;
	return 0;*/

	/*uint8_t mem[1024 * 32];
	ULONG guarantee = 32*1024;
	bool res = SetThreadStackGuarantee(&guarantee);
	DWORD err = GetLastError();
	std::cout << "guarantee " << guarantee << " err " << err << " res " << res << std::endl;
	Context ctx{};
	std::cout << "size " << sizeof(Context) << " offset " << offsetof(Context, stackBase);
	std::cout << std::endl;
	save_registers(ctx);
	std::cout << "rip: " << ctx.rip << std::endl;
	std::cout << "rsp: " << ctx.rsp << std::endl;
	std::cout << "rbp: " << ctx.rbp << std::endl;
	std::cout << "xmmcontrolword " << ctx.mxcsrControlWord << std::endl;
	std::cout << "x87 control word " << ctx.x87fpuControlWord << std::endl;
	std::cout << "limit: " << ctx.stackLimit << std::endl;
	std::cout << "base: " << ctx.stackBase << std::endl;
	uint64_t base = reinterpret_cast<uint64_t>(ctx.stackBase);
	uint64_t limit = reinterpret_cast<uint64_t>(ctx.stackLimit);
	std::cout << "size: " << (base - limit) << std::endl;
	std::cout << "deallocation: " << ctx.deallocationStack << std::endl;
	std::cout << "guaranteed stack bytes: " << ctx.guaranteedStackBytes << std::endl;

	tryAgain();

	
	guarantee = 32 * 1024;
	res = SetThreadStackGuarantee(&guarantee);
	err = GetLastError();
	std::cout << "guarantee " << guarantee << " err " << err << " res " << res << std::endl;

	return 0;*/

	/*uint32_t testcount = 2;
	engine::jobSystem.init_job_system(testcount);
	job::JobDecl ent{ engine::entry2 };
	engine::jobSystem.start_entry_point(ent);
	while (!engine::jobSystem.is_done()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	engine::jobSystem.end_job_system();
	return 0;*/

	engine::init_window();

	uint32_t threadCount = std::max(static_cast<uint32_t>(1), std::thread::hardware_concurrency()-1);
	threadCount = 1;
	engine::jobSystem.init_job_system(threadCount);
	profiling::init_profiler([](void) {return job::this_thread_id(); }, threadCount, 1024, "testlog");

	job::JobDecl decl{ engine::entry_point };
	engine::jobSystem.start_entry_point(decl);

	while (!engine::jobSystem.is_done()) {
		glfwPollEvents();
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