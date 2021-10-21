#include <iostream>
#include <stdint.h>
#include <atomic>
#include <thread>
#include <signal.h>
#include "JobSystem.h"
#include "Profiling.h"
#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <pthread.h>
#endif

//Yeah we're just going to unoptimize this whole class. Stupid? Probably, but also have no idea what I'm doing otherwise
#pragma optimize("", off)

namespace job {

	thread_local int32_t currentThreadId = -5;
	Context* threadCtx = nullptr;

	int32_t this_thread_id() {
		return currentThreadId;
	}
	Job* JobSystem::thisjob() {
		return currentJobsByThread[this_thread_id()];
	}

	Job::Job(JobSystem* sys) {
		this->system = sys;
		data = reinterpret_cast<char*>(malloc(JOB_STACK_SIZE));
		//Stacks grow down, so start at the top
		stackPointer = data + JOB_STACK_SIZE;
		//Align to 16 bytes
		stackPointer = (char*)((uintptr_t)stackPointer & -16L);
		//Red zone
		//From further research, this is not actually the red zone. The red zone is really below the stack, always the 128 bytes below RSP.
		//I'll leave it here anyway, just for some padding
		stackPointer -= 128;
	}

	Job::~Job() {
		free(data);
	}

	void dummy() {
		std::cerr << "Should not be here!" << std::endl;
	};
	static Context testctx;

	void Job::run(Job* job) {
		job->currentTask->func(job->currentTask->arg);
		if (job->currentTask->counter) {
			job->currentTask->counter->decrement();
		}
		job->currentTask = nullptr;
		job->active = false;
		job->system->release_job(job);
		job->system->activeJobCount.fetch_add(-1, std::memory_order_relaxed);
		testctx = threadCtx[this_thread_id()];
		swap_registers(job->ctx, testctx);
		dummy();
	}



	JobDecl::JobDecl() {
		func = nullptr;
		counter = nullptr;
		arg = nullptr;
	}

	JobDecl::JobDecl(void (*f)(void*), void* argument) {
		func = f;
		counter = nullptr;
		arg = argument;
	}

	JobDecl::JobDecl(void (*f)(void)) {
		func = reinterpret_cast<void (*)(void*)>(f);
		counter = nullptr;
		arg = nullptr;
	}



	JobQueue::JobQueue(std::thread::id tid, uint32_t id) {
		threadId = tid;
		this->id = id;
		stealId = id;
		jobs = reinterpret_cast<Job**>(malloc(sizeof(uintptr_t) * queueSize));
#ifdef _DEBUG
		for (uint32_t i = 0; i < queueSize; i++) {
			jobs[i] = reinterpret_cast<Job*>(0xDEADBEEF);
		}
#endif
		top.store(0, std::memory_order_relaxed);
		bottom.store(0, std::memory_order_relaxed);
	}

	JobQueue::~JobQueue() {
		free(jobs);
	}



	JobCounter::JobCounter(Job* job, int32_t count) {
		this->job = job;
		this->counter = count;
	}

	void JobCounter::decrement() {
		int32_t count = counter.fetch_add(-1, std::memory_order_relaxed);
		if (count == 1) {
			job->system->push_job(*job->system->queues[this_thread_id()], *job);
		}
	}



	void JobSystem::init_job_system(uint32_t threadCount) {
		for (int i = 0; i < queueSize; i++) {
			jobPool[i] = new Job(this);
		}
		jobPoolReadIdx.store(0, std::memory_order_relaxed);
		jobPoolMaxReadIdx.store(queueSizeMask, std::memory_order_relaxed);
		jobPoolWriteIdx.store(queueSizeMask, std::memory_order_relaxed);

		threadpool.resize(threadCount);
		threadCtx = new Context[threadCount];
		queues.resize(threadpool.size());
		currentJobsByThread = new Job*[threadpool.size()];
		finished.store(true, std::memory_order_relaxed);

		for (uint32_t i = 0; i < threadpool.size(); i++) {
			threadpool[i] = std::thread(&JobSystem::threadFunc, this, i);
			queues[i] = new JobQueue(threadpool[i].get_id(), i);
			threadCtx[i] = {};
			#ifdef _WIN32
				SetThreadAffinityMask(threadpool[i].native_handle(), static_cast<DWORD_PTR>(1)<<i);
			#elif __linux__
				cpu_set_t cputset;
				CPU_ZERO(&cpuset);
				CPU_SET(i, &cpuset);
				pthread_setaffinity_np(threadpool[i].native_handle(), sizeof(cpu_set_t), &cpuset);
			#endif
			
		}
	}

	void JobSystem::start_entry_point(JobDecl& decl) {
		start_job(decl, 0);
		finished.store(false, std::memory_order_release);
	}

	void JobSystem::end_job_system() {
		finished.store(true, std::memory_order_relaxed);
		for (int i = 0; i < threadpool.size(); i++) {
			threadpool[i].join();
		}

		cleanup();
	}

	void JobSystem::cleanup() {
		delete currentJobsByThread;
		for (int i = 0; i < queueSize; i++) {
			delete jobPool[i];
		}
		for (int i = 0; i < queues.size(); i++) {
			delete queues[i];
		}
	}

	void JobSystem::threadFunc(uint32_t idx) {
		currentThreadId = idx;
		while (finished.load(std::memory_order_relaxed)) {
		}
		std::atomic_thread_fence(std::memory_order_seq_cst);
		uint8_t count = 0;
		while (!finished.load(std::memory_order_relaxed)) {
			save_registers(threadCtx[idx]);
			Job* job = getJob(idx);
			if (job) {
				count = 0;
				if (!job->active) {
					job->ctx.rip = (void*)job->run;
					job->ctx.rsp = (void*)job->stackPointer;
					job->active = true;
				}
				currentJobsByThread[idx] = job;
				load_registers_arg(job->ctx, job);
			}
			if (++count > 16) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				count = 0;
			}
		}
		std::cout << "Exited job thread " << idx << " successfully" << std::endl;
	}

	void JobSystem::push_job(JobQueue& queue, Job& job) {
		SPIN_LOCK(lock);
		int32_t bottom = queue.bottom.load(std::memory_order_relaxed);
		queue.jobs[bottom] = &job;
		queue.bottom.store(bottom+1, std::memory_order_relaxed);
		/*const int32_t bottom = queue.bottom.load(std::memory_order_relaxed);
		queue.jobs[bottom & queueSizeMask] = &job;
		//Make sure if something tries to read this job, it's in memory
		std::atomic_thread_fence(std::memory_order_release);
		queue.bottom.store(bottom + 1, std::memory_order_relaxed);*/
	}

	Job* JobSystem::pop_job(JobQueue& queue) {
		SPIN_LOCK(lock);
		int32_t bottom = queue.bottom.load(std::memory_order_relaxed);
		int32_t top = queue.top.load(std::memory_order_relaxed);
		if (bottom == top) {
			return nullptr;
		}
		queue.bottom.store(bottom - 1, std::memory_order_relaxed);
		return queue.jobs[(bottom - 1) & queueSizeMask];
		/*int32_t bottom = queue.bottom.fetch_add(-1, std::memory_order_relaxed);
		//Bottom and top reads can't be reordered here, that would cause race conditions with steal_job
		std::atomic_thread_fence(std::memory_order_seq_cst);
		int32_t top = queue.top.load(std::memory_order_relaxed);
		Job* job = nullptr;
		if (bottom <= top) {
			queue.bottom.store(bottom, std::memory_order_relaxed);
		} else {
			job = queue.jobs[(bottom - 1) & queueSizeMask];
		}
		return job;*/
	}

	Job* JobSystem::steal_job(JobQueue& queue) {
		SPIN_LOCK(lock);
		uint32_t queueSize = static_cast<uint32_t>(queues.size());
		queue.stealId = (queue.stealId + 1) % queueSize;
		if (queue.stealId == queue.id) {
			queue.stealId = (queue.stealId + 1) % queueSize;
		}
		JobQueue& stealQueue = *queues[queue.stealId];

		int32_t top = stealQueue.top.load(std::memory_order_relaxed);
		int32_t bottom = stealQueue.bottom.load(std::memory_order_relaxed);
		if (bottom == top) {
			return nullptr;
		}
		stealQueue.top.store(top + 1, std::memory_order_relaxed);
		return stealQueue.jobs[top & queueSizeMask];

		/*int32_t top = stealQueue.top.fetch_add(1, std::memory_order_relaxed);
		//Same as pop_job, these reads can't be reordered. I wonder if I could use a relaxed memory order here, would that still stop a compiler reorder?
		std::atomic_thread_fence(std::memory_order_seq_cst);
		int32_t bottom = stealQueue.bottom.load(std::memory_order_acquire);
		Job* job = nullptr;
		if (bottom > top) {
			//if (queue.top.compare_exchange_strong(top, top+1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			job = stealQueue.jobs[top & queueSizeMask];
			//}
		} else {
			stealQueue.top.fetch_add(-1, std::memory_order_relaxed);
		}
		return job;*/
	}


	Job* JobSystem::getJob(uint32_t index) {
		JobQueue& queue = *queues[index];
		Job* job = pop_job(queue);
		if (!job) {
			job = steal_job(queue);
		}
		return job;
	}

	Job* JobSystem::acquire_job() {
		SPIN_LOCK(lock);
		uint32_t read = jobPoolReadIdx.load(std::memory_order_relaxed);
		uint32_t maxRead = jobPoolMaxReadIdx.load(std::memory_order_relaxed);
		if (read == maxRead) {
			return nullptr;
		}
		jobPoolReadIdx.store((read+1)&queueSizeMask, std::memory_order_relaxed);
		return jobPool[read];
		/*while (true) {
			uint32_t read = jobPoolReadIdx.load(std::memory_order_relaxed);
			//Make sure these reads are ordered correctly. There's a slight chance for read to be greater than maxRead otherwise.
			std::atomic_thread_fence(std::memory_order_acquire);
			uint32_t maxRead = jobPoolMaxReadIdx.load(std::memory_order_relaxed);
			if (read == maxRead) {
				continue;
			}
			if (jobPoolReadIdx.compare_exchange_weak(read, (read + 1) & queueSizeMask, std::memory_order_acquire, std::memory_order_relaxed)) {
				return jobPool[read];
			}
		}*/
	}

	void JobSystem::release_job(Job* job) {
		SPIN_LOCK(lock);
		uint32_t idx = jobPoolWriteIdx.load(std::memory_order_relaxed);
		uint32_t newidx = (idx + 1) & queueSizeMask;
		jobPoolWriteIdx.store(newidx, std::memory_order_relaxed);
		jobPoolMaxReadIdx.store(newidx, std::memory_order_relaxed);
		jobPool[idx] = job;
		/*while (true) {
			uint32_t write = jobPoolWriteIdx.load(std::memory_order_relaxed);
			uint32_t desired = (write + 1) & queueSizeMask;
			if (jobPoolWriteIdx.compare_exchange_weak(write, desired, std::memory_order_relaxed, std::memory_order_relaxed)) {
				jobPool[desired] = job;
				std::atomic_thread_fence(std::memory_order_release);
				while (!jobPoolMaxReadIdx.compare_exchange_weak(write, (write + 1) & queueSizeMask, std::memory_order_relaxed, std::memory_order_relaxed)) {
				}
				return;
			}
		}
		dummy();*/
	}

	

	void JobSystem::start_jobs_and_wait_for_counter(JobDecl* jobs, uint32_t jobCount) {
		int32_t tid = this_thread_id();
		Job* job = currentJobsByThread[tid];
		volatile bool exit = true;
		save_registers(job->ctx);
		if (!exit)
			return;
		exit = false;
		JobCounter count(job, jobCount);
		for (uint32_t i = 0; i < jobCount; i++) {
			jobs[i].counter = &count;
			start_job(jobs[i]);
		}
		testctx = threadCtx[tid];
		load_registers(testctx);
		dummy();
	}

	void JobSystem::start_job(JobDecl& decl) {
		start_job(decl, this_thread_id());
	}

	void JobSystem::yield_job() {
		int32_t tid = this_thread_id();
		Job* job = currentJobsByThread[tid];
		volatile bool exit = true;
		save_registers(job->ctx);
		if (!exit)
			return;
		exit = false;
		push_job(*queues[tid], *job);
		load_registers(threadCtx[tid]);
	}

	void JobSystem::start_job(JobDecl& decl, uint32_t tid) {
		Job* job = acquire_job();
		job->currentTask = &decl;
		push_job(*queues[tid], *job);
		activeJobCount.fetch_add(1, std::memory_order_relaxed);
	}

	uint16_t JobSystem::thread_count() {
		return static_cast<uint16_t>(threadpool.size());
	}

	bool JobSystem::is_done() {
		return activeJobCount.load(std::memory_order_relaxed) == 0;
	}
}

#pragma optimize("", on)