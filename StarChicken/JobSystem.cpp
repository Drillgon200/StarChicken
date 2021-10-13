#include <iostream>
#include <stdint.h>
#include <atomic>
#include <thread>
#include "JobSystem.h"
#include "Profiling.h"
#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <pthread.h>
#endif

//Yeah we're just going to unoptimize this whole class. Stupid? Probably, but also have no idea what I'm doing otherwise
#pragma optimize("", off)

thread_local Context threadCtx{};

namespace job {

	thread_local int32_t currentThreadId = -5;


	Job::Job(JobSystem* sys) {
		this->system = sys;
		data = reinterpret_cast<char*>(malloc(JOB_STACK_SIZE));
		//Stacks grow down, so start at the top
		stackPointer = data + JOB_STACK_SIZE;
		//Align to 16 bytes
		stackPointer = (char*)((uintptr_t)stackPointer & -16L);
		//Red zone
		stackPointer -= 128;
	}

	Job::~Job() {
		free(data);
	}

	void Job::run(Job* job) {
		{
			SCOPE_PROF("Job func")
			job->currentTask->func(job->currentTask->arg);
			//void (*testf)(void) = reinterpret_cast<void (*)(void)>(job->currentTask->func);
			//testf();
		}
		{
			SCOPE_PROF("Job release")
			if (job->currentTask->counter) {
				job->currentTask->counter->decrement();
			}
			job->currentTask = nullptr;
			job->active = false;
			job->system->release_job(job);
			job->system->activeJobCount.fetch_add(-1, std::memory_order_relaxed);
		}
		swap_registers(job->ctx, threadCtx);
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
			job->system->push_job(*job->system->queues[currentThreadId], *job);
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
		queues.resize(threadpool.size());
		currentJobsByThread = new Job*[threadpool.size()];
		finished.store(true, std::memory_order_relaxed);

		for (uint32_t i = 0; i < threadpool.size(); i++) {
			threadpool[i] = std::thread(&JobSystem::threadFunc, this, i);
			queues[i] = new JobQueue(threadpool[i].get_id(), i);

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

	void JobSystem::push_job(JobQueue& queue, Job& job) {
		const int32_t bottom = queue.bottom.load(std::memory_order_relaxed);
		queue.jobs[bottom & queueSizeMask] = &job;
		//Make sure if something tries to read this job, it's in memory
		std::atomic_thread_fence(std::memory_order_release);
		queue.bottom.store(bottom + 1, std::memory_order_relaxed);
	}

	Job* JobSystem::pop_job(JobQueue& queue) {
		int32_t bottom = queue.bottom.fetch_add(-1, std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_seq_cst);
		int32_t top = queue.top.load(std::memory_order_relaxed);
		Job* job = nullptr;
		if (bottom <= top) {
			queue.bottom.store(bottom, std::memory_order_relaxed);
		} else {
			job = queue.jobs[(bottom - 1) & queueSizeMask];
		}
		return job;
	}


	Job* JobSystem::steal_job(JobQueue& queue) {
		uint32_t queueSize = static_cast<uint32_t>(queues.size());
		queue.stealId = (queue.stealId + 1) % queueSize;
		if (queue.stealId == queue.id) {
			queue.stealId = (queue.stealId + 1) % queueSize;
		}
		JobQueue& stealQueue = *queues[queue.stealId];

		int32_t top = stealQueue.top.fetch_add(1, std::memory_order_relaxed);
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
		bool b = job && job->active;
		return job;
	}


	Job* JobSystem::getJob(uint32_t index) {
		JobQueue& queue = *queues[index];
		Job* job = pop_job(queue);
		bool b = job && job->active;
		if (!job) {
			job = steal_job(queue);
			b = job && job->active;
		}
		return job;
	}

	Job* JobSystem::acquire_job() {
		while (true) {
			uint32_t read = jobPoolReadIdx.load(std::memory_order_relaxed);
			uint32_t maxRead = jobPoolMaxReadIdx.load(std::memory_order_relaxed);
			if (read == maxRead) {
				continue;
			}
			if (jobPoolReadIdx.compare_exchange_weak(read, (read + 1) & queueSizeMask, std::memory_order_acquire, std::memory_order_relaxed)) {
				return jobPool[read];
			}
		}
	}

	void JobSystem::release_job(Job* job) {
		while (true) {
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
	}

	void JobSystem::start_jobs_and_wait_for_counter(JobDecl* jobs, uint32_t jobCount) {
		Job* job = currentJobsByThread[currentThreadId];
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
		load_registers(threadCtx);
	}

	void JobSystem::start_job(JobDecl& decl) {
		start_job(decl, currentThreadId);
	}

	void JobSystem::yield_job() {
		Job* job = currentJobsByThread[currentThreadId];
		volatile bool exit = true;
		save_registers(job->ctx);
		//Behold, some stupid logic to not run the rest of the code when someone resumes this job because my brain is too small to think of a cleaner solution
		if (!exit)
			return;
		exit = false;
		push_job(*queues[currentThreadId], *job);
		load_registers(threadCtx);
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