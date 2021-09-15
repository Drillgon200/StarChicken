#include <iostream>
#include <emmintrin.h>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
//#include <winbase.h>

#pragma pack(8)
struct Context {
	void* rip, * rsp;
	void* rbx, * rbp, * r12, * r13, * r14, * r15, * rdi, * rsi;
	__m128i xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
};
thread_local Context threadCtx{};

extern "C" void save_registers(Context & c);
extern "C" void load_registers(Context & c);
extern "C" void swap_registers(Context & oldCtx, Context & newCtx);
extern "C" void load_registers_arg(Context & ctx, void* arg);

const int JOB_STACK_SIZE = 64 * 1024;
class Job;
struct JobQueue;
struct JobDecl;
void startJob(JobDecl& decl);
void pushJob(JobQueue& queue, Job& job);
void releaseJob(Job* job);
std::vector<JobQueue*> queues;
thread_local int currentThreadId;
Job** currentJobsByThread;
std::atomic<uint32_t> activeJobCount{ 0 };

struct JobCounter {
	Job* job;
	std::atomic<uint32_t> counter;

	JobCounter(Job* job, uint32_t count) {
		this->job = job;
		this->counter = count;
	}

	void decrement() {
		if (--counter == 0) {
			pushJob(*queues[currentThreadId], *job);
		}
	}
};

struct JobDecl {
	JobCounter* counter;
	void (*func)(void);

	JobDecl() {
	}

	JobDecl(void (*f)(void)) {
		func = f;
		counter = nullptr;
	}
};

class Job {

public:
	JobDecl* currentTask;
	bool active = false;
	char* data = nullptr;
	char* stackPointer = nullptr;
	Context ctx{};

	Job() {
		data = reinterpret_cast<char*>(malloc(JOB_STACK_SIZE));
		//Stacks grow down, so start at the top
		stackPointer = data + JOB_STACK_SIZE;
		//Align to 16 bytes
		stackPointer = (char*)((uintptr_t)stackPointer & -16L);
		//Red zone
		stackPointer -= 128;
	}
	~Job() {
		free(data);
	}

	//This function handles releasing the job and context switching back after the task ends
	static void run(Job* job) {
		std::cout << "test job run called" << std::endl;
		job->currentTask->func();
		if (job->currentTask->counter) {
			job->currentTask->counter->decrement();
		}
		job->currentTask = nullptr;
		job->active = false;
		releaseJob(job);
		activeJobCount.fetch_add(-1, std::memory_order_relaxed);
		swap_registers(job->ctx, threadCtx);
	}

	static void yield_job() {
		Job* job = currentJobsByThread[currentThreadId];
		volatile bool exit = true;
		save_registers(job->ctx);
		//Behold, some stupid logic to not run the rest of the code when someone resumes this job because my brain is too small to think of a cleaner solution
		if (!exit)
			return;
		exit = false;
		pushJob(*queues[currentThreadId], *job);
		load_registers(threadCtx);
	}

	static void start_jobs_and_wait_for_counter(JobDecl* jobs, uint32_t jobCount) {
		Job* job = currentJobsByThread[currentThreadId];
		volatile bool exit = true;
		save_registers(job->ctx);
		if (!exit)
			return;
		exit = false;
		JobCounter count(job, jobCount);
		for (uint32_t i = 0; i < jobCount; i++) {
			jobs[i].counter = &count;
			startJob(jobs[i]);
		}
		load_registers(threadCtx);
	}

};

//I like to keep sizes in powers of 2
const uint32_t queueSize = 1 << 8;
const uint32_t queueSizeMask = queueSize - 1;
std::atomic<int> jobPoolReadIdx;
std::atomic<int> jobPoolMaxReadIdx;
std::atomic<int> jobPoolWriteIdx;
//The queue of empty available jobs, implemented as a ring buffer
Job* jobPool[queueSize];

struct JobQueue {
	std::thread::id threadId;
	uint32_t id;
	Job** jobs;
	std::atomic<int> top;
	std::atomic<int> bottom;
	uint32_t stealId;

	JobQueue(std::thread::id tid, uint32_t id) {
		threadId = tid;
		this->id = id;
		stealId = id;
		jobs = reinterpret_cast<Job**>(malloc(sizeof(uintptr_t) * queueSize));
	}
	~JobQueue() {
		free(jobs);
	}
};

std::vector<std::thread> threadpool;
std::atomic<bool> finished = false;

void pushJob(JobQueue& queue, Job& job) {
	const int bottom = queue.bottom.load(std::memory_order_relaxed);
	queue.jobs[bottom & queueSizeMask] = &job;
	//Make sure if something tries to read this job, it's in memory
	std::atomic_thread_fence(std::memory_order_release);
	queue.bottom.store(bottom+1, std::memory_order_relaxed);
}

Job* popJob(JobQueue& queue) {
	int bottom = queue.bottom.load(std::memory_order_relaxed);
	queue.bottom.store(bottom-1, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_seq_cst);
	int top = queue.top.load(std::memory_order_relaxed);
	Job* job = nullptr;
	if (bottom <= top) {
		queue.bottom.store(bottom, std::memory_order_relaxed);
		//job = queue.jobs[bottom & queueSizeMask];
	} else {
		std::cout << "aaa " << bottom << std::endl;
		job = queue.jobs[(bottom-1) & queueSizeMask];
	}
	return job;
}

Job* stealJob(JobQueue& queue) {
	queue.stealId = (queue.stealId + 1) % queues.size();
	if (queue.stealId == queue.id) {
		queue.stealId = (queue.stealId + 1) % queues.size();
	}
	JobQueue& stealQueue = *queues[queue.stealId];

	//int top = stealQueue.top.load(std::memory_order_acquire);
	int top = stealQueue.top.fetch_add(1, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_seq_cst);
	int bottom = stealQueue.bottom.load(std::memory_order_acquire);
	Job* job = nullptr;
	if (bottom > top) {
		//if (queue.top.compare_exchange_strong(top, top+1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
		job = stealQueue.jobs[top & queueSizeMask];
		//}
	} else {
		--stealQueue.top;
	}
	return job;
}

Job* getJob(int index) {
	JobQueue& queue = *queues[index];
	Job* job = popJob(queue);
	bool stolen = false;
	if (!job) {
		stolen = true;
		job = stealJob(queue);
	}
	if (job) {
		std::cout << "job ok" << std::endl;
		std::cout << "was stolen " << stolen << std::endl;
	}
	return job;
}

Job* acquireJob() {
	while (true) {
		int read = jobPoolReadIdx.load(std::memory_order_relaxed);
		int maxRead = jobPoolMaxReadIdx.load(std::memory_order_relaxed);
		if (read == maxRead) {
			continue;
		}
		if (jobPoolReadIdx.compare_exchange_weak(read, (read + 1) & queueSizeMask, std::memory_order_acquire, std::memory_order_relaxed)) {
			return jobPool[read];
		}
	}
}

void releaseJob(Job* job) {
	while (true) {
		int write = jobPoolWriteIdx.load(std::memory_order_relaxed);
		int desired = (write + 1) & queueSizeMask;
		if (jobPoolWriteIdx.compare_exchange_weak(write, desired, std::memory_order_relaxed, std::memory_order_relaxed)) {
			jobPool[desired] = job;
			std::atomic_thread_fence(std::memory_order_release);
			while (!jobPoolMaxReadIdx.compare_exchange_weak(write, (write + 1) & queueSizeMask, std::memory_order_relaxed, std::memory_order_relaxed)) {
			}
			return;
		}
	}
}

void startJob(JobDecl& decl, int tid) {
	Job* job = acquireJob();
	job->currentTask = &decl;
	pushJob(*queues[tid], *job);
	activeJobCount.fetch_add(1, std::memory_order_relaxed);
}

void startJob(JobDecl& decl) {
	startJob(decl, currentThreadId);
}

void threadFunc(int idx) {
	currentThreadId = idx;
	while (!finished.load(std::memory_order_relaxed)) {
		save_registers(threadCtx);
		Job* job = getJob(idx);
		if (job) {
			if (!job->active) {
				job->ctx.rip = (void*)job->run;
				job->ctx.rsp = (void*)job->stackPointer;
				job->active = true;
			}
			currentJobsByThread[idx] = job;
			load_registers_arg(job->ctx, job);
		}
	}
}

std::atomic<int> testInc{0};

void testIncer() {
	std::cout << "testIncer " << testInc.load(std::memory_order_relaxed) << " " << currentThreadId << std::endl;
	testInc.fetch_add(1, std::memory_order_relaxed);
}

void test2() {
	std::cout << "\nTest 4 " << currentThreadId << "\n" << std::endl;
	JobDecl decls[100];
	for (int i = 0; i < 100; i++) {
		decls[i] = JobDecl(testIncer);
	}
	Job::start_jobs_and_wait_for_counter(decls, 100);
	std::cout << "Finished test 2 " << testInc.load(std::memory_order_relaxed) << std::endl;
}

void jobFunc() {
	std::cout << "Ran job func" << std::endl;
}

void cleanup() {
	std::cout << "cleanup" << std::endl;
	delete currentJobsByThread;
	for (int i = 0; i < queueSize; i++) {
		delete jobPool[i];
	}
	std::cout << "cleanup 2" << std::endl;
	for (int i = 0; i < queues.size(); i++) {
		delete queues[i];
	}
	std::cout << "cleanup done" << std::endl;
}

void bruh2(uint32_t test) {
	std::cout << "got here" << test << std::endl;
}

//Cool
template<typename... Args>
void bruh(void (*func)(Args...), Args... arg) {
	func(arg...);
}

//int main() {
//	bruh(bruh2, static_cast<uint32_t>(32));
//	return 0;
//}

void testjobfunc() {
	std::cout << "Hello World!" << std::endl;
}

/*int main() {
	for (int i = 0; i < queueSize; i++) {
		jobPool[i] = new Job();
	}
	jobPoolReadIdx.store(0, std::memory_order_relaxed);
	jobPoolMaxReadIdx.store(queueSizeMask, std::memory_order_relaxed);
	jobPoolWriteIdx.store(queueSizeMask, std::memory_order_relaxed);

	threadpool.resize(std::thread::hardware_concurrency());
	queues.resize(threadpool.size());
	currentJobsByThread = new Job * [threadpool.size()];
	for (int i = 0; i < threadpool.size(); i++) {
		threadpool[i] = std::thread(threadFunc, i);
		queues[i] = new JobQueue(threadpool[i].get_id(), i);
		//SetThreadAffinityMask(threadpool[i].native_handle(), static_cast<DWORD_PTR>(1)<<i);
		queues[i]->threadId = threadpool[i].get_id();
		queues[i]->id = i;
		queues[i]->stealId = i;
		queues[i]->jobs = reinterpret_cast<Job**>(malloc(sizeof(uintptr_t) * queueSize));
		queues[i]->top.store(0, std::memory_order_relaxed);
		queues[i]->bottom.store(0, std::memory_order_relaxed);
	}
	
	JobDecl d{ test2 };
	startJob(d, 0);

	while (activeJobCount.load(std::memory_order_relaxed) != 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	finished.store(true, std::memory_order_relaxed);
	for (int i = 0; i < threadpool.size(); i++) {
		threadpool[i].join();
	}

	cleanup();
}*/

/*char data[4096];

	char* sp = (char*)(data + sizeof(data));
	sp = (char*)((uintptr_t)sp & -16L);
	sp -= 128;

	Context ctx{};
	ctx.rip = (void*)test;
	ctx.rsp = (void*)sp;

	load_registers(ctx);*/