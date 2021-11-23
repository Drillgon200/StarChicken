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
//Actually looks like it works now that I've actually fixed my code, so cool.
//#pragma optimize("", off)

//Don't use, I can't get lock free programming to work for the life of me. Lock free programming is cursed.
//#define LOCK_FREE

namespace job {

	thread_local JobThreadData* threadData;

	JobThreadData::JobThreadData() :
		threadCtx{},
		currentJob{ nullptr },
		newJobsToAdd{ nullptr },
		newJobsToAddCount{ 0 },
		stealId{ UINT32_MAX },
		threadId{ UINT32_MAX }
	{}

	Job* JobSystem::thisjob() {
		return threadData->currentJob;
	}

	Job::Job(job::JobSystem* sys) {
		this->system = sys;
		data = reinterpret_cast<char*>(malloc(JOB_STACK_SIZE));
		//std::cout << "\t" << std::hex << reinterpret_cast<uintptr_t>(data) << std::endl;
		//Stacks grow down, so start at the top
		stackPointer = data + JOB_STACK_SIZE;
		//Align to 16 bytes
		stackPointer = (char*)((uintptr_t)stackPointer & -16L);
		//Red zone
		//From further research, this is not actually the red zone. The red zone is really below the stack, always the 128 bytes below RSP.
		//I'll leave it here anyway, just for some padding
		stackPointer -= 128;

		ctx.stackBase = stackPointer;
		ctx.stackLimit = data;
		ctx.deallocationStack = data;
		ctx.guaranteedStackBytes = 0;
		ctx.fiberData = 0;
	}

	Job::~Job() {
		//std::cout << "\t" << std::hex << reinterpret_cast<uintptr_t>(data) << std::endl;
		free(data);
	}

	void Job::run(Job* job) {
		job->currentTask->func(job->currentTask->arg);
		if (job->currentTask->counter) {
			job->currentTask->counter->decrement();
		}
		job->currentTask = nullptr;
		job->active = false;
		job->state = ENDED;
		job->system->activeJobCount.fetch_add(-1, std::memory_order_relaxed);
		/*Context ctx{};
		save_registers(ctx);
		int32_t tid = this_thread_id();
		if (tid == 0) {
			std::cout << "0" << std::endl;
		} else if (tid == 1) {
			std::cout << "1" << std::endl;
		}*/
		
		//Context ctx{};
		//swap_registers(ctx, threadCtx[this_thread_id()]);
		load_registers(threadData->threadCtx);
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
			job->system->push_job(*job->system->queues[threadData->threadId], *job);
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
		queues.resize(threadCount);
		//queues.resize(threadpool.size());
		finished.store(true, std::memory_order_relaxed);

		for (uint32_t i = 0; i < threadCount; i++) {
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
		for (int i = 0; i < queueSize; i++) {
			delete jobPool[i];
		}
		for (uint16_t i = 0; i < queues.size(); i++) {
			delete queues[i];
		}
	}

	void JobSystem::threadFunc(uint32_t idx) {
		threadData = new JobThreadData{};
		JobThreadData& localData = *threadData;
		localData.threadId = idx;
		localData.stealId = idx;
		while (finished.load(std::memory_order_relaxed)) {
		}
		std::atomic_thread_fence(std::memory_order_acquire);
		uint32_t x87fpuControlWord = 0;
		uint32_t mxcsrControlWord = 0;
		{
			Context ctx{};
			save_registers(ctx);
			x87fpuControlWord = ctx.x87fpuControlWord;
			mxcsrControlWord = ctx.mxcsrControlWord;
		}
		uint8_t count = 0;
		while (!finished.load(std::memory_order_relaxed)) {
			Job* job = getJob(idx);
			if (job) {
				count = 0;
				if (!job->currentTask) {
					std::cout << "Failed, no current task in tf!" << std::endl;
				}
				if (!job->active) {
					job->active = true;
					job->ctx.rip = (void*)job->run;
					//Subtract from stack pointer to pretend we pushed a return address, otherwise the stack will be aligned wrong
					job->ctx.rsp = (void*)(job->stackPointer-sizeof(uintptr_t));

					job->ctx.x87fpuControlWord = x87fpuControlWord;
					job->ctx.mxcsrControlWord = mxcsrControlWord;
				}
				localData.currentJob = job;

				if (job->state == ACTIVE) {
					std::cout << "Wrong state before: " << job->state << "\n";
				}

				job->state = ACTIVE;
				swap_registers_arg(localData.threadCtx, job->ctx, job);
				if (job->state == ACTIVE) {
					std::cout << "Wrong state after: " << job->state << "\n";
				}

				if (job->state == ENDED) {
					job->system->release_job(job);
				}

				if (localData.newJobsToAddCount > 0) {
					for (uint32_t i = 0; i < localData.newJobsToAddCount; i++) {
						push_job(*queues[idx], *localData.newJobsToAdd[i]);
					}
				}
				localData.newJobsToAddCount = 0;
				localData.newJobsToAdd = nullptr;

				continue;
			}
			if (++count > 16) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				count = 0;
			}
		}
		delete threadData;
		threadData = nullptr;
		std::cout << "Exited job thread " << idx << " successfully" << std::endl;
	}

	void JobSystem::push_job(JobQueue& queue, Job& job) {
#ifndef LOCK_FREE
		queue.queueLock.lock();
		int64_t bottom = queue.bottom.fetch_add(1, std::memory_order_relaxed);
		queue.jobs[bottom] = &job;
		queue.queueLock.unlock();
#else
		int64_t bottom = queue.bottom.fetch_add(1, std::memory_order_relaxed);
		queue.jobs[bottom & queueSizeMask] = &job;
		//Make sure if something tries to read this job, it's in memory
		std::atomic_thread_fence(std::memory_order_seq_cst);
		queue.bottom.store(bottom + 1, std::memory_order_relaxed);
#endif
	}

	Job* JobSystem::pop_job(JobQueue& queue) {
#ifndef LOCK_FREE
		Job* job = nullptr;
		queue.queueLock.lock();
		int64_t bottom = queue.bottom.load(std::memory_order_relaxed);
		if (bottom > 0) {
			queue.bottom.store(bottom - 1, std::memory_order_relaxed);
			job = queue.jobs[(bottom - 1) & queueSizeMask];
		}
		queue.queueLock.unlock();
		return job;
#else
		int64_t bottom = queue.bottom.fetch_add(-1, std::memory_order_relaxed) - 1;
		std::atomic_thread_fence(std::memory_order_seq_cst);
		int64_t top = queue.top.load(std::memory_order_relaxed);

		Job* job = nullptr;
		if (top <= bottom) {
			job = queue.jobs[bottom & queueSizeMask];
			if (top == bottom) {
				if (!queue.top.compare_exchange_strong(top, top + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
					job = nullptr;
				}
				queue.bottom.store(top + 1, std::memory_order_relaxed);
			}
		} else {
			queue.bottom.store(top, std::memory_order_relaxed);
			job = nullptr;
		}
		return job;
#endif
	}

	Job* JobSystem::steal_job(JobQueue& queue, uint32_t& stealId) {
		uint32_t queueSize = static_cast<uint32_t>(queues.size());
		uint8_t tries = 0;
		//Pick a new steal queue until we have a valid one or we ran out of other queues to steal from.
		//A queue is invalid if it's the last one we stole from (tries == 0), or it's this thread's queue
		while (tries == 0 || &queue == queues[stealId]) {
			stealId = (stealId + 1) % queueSize;
			++tries;
			if (tries > queueSize) {
				return nullptr;
			}
		}
		JobQueue& stealQueue = *queues[stealId];
		
#ifndef LOCK_FREE
		Job* job = nullptr;
		stealQueue.queueLock.lock();
		int64_t bottom = stealQueue.bottom.load(std::memory_order_relaxed);
		if (bottom > 0) {
			stealQueue.bottom.store(bottom - 1, std::memory_order_relaxed);
			job = stealQueue.jobs[(bottom - 1) & queueSizeMask];
		}
		stealQueue.queueLock.unlock();
		return job;
#else
		int64_t top = stealQueue.top.load(std::memory_order_relaxed);
		std::atomic_thread_fence(std::memory_order_seq_cst);
		int64_t bottom = stealQueue.bottom.load(std::memory_order_acquire);
		Job* job = nullptr;
		if (top < bottom) {
			job = stealQueue.jobs[top & queueSizeMask];
			if (!stealQueue.top.compare_exchange_strong(top, top+1, std::memory_order_relaxed, std::memory_order_relaxed)) {
				job = nullptr;
			}
		}
		return job;
#endif
	}


	Job* JobSystem::getJob(uint32_t index) {
		JobQueue& queue = *queues[index];
		Job* job = pop_job(queue);
		if (job && !job->currentTask) {
			std::cout << "Failed, no current task in gj 1!" << std::endl;
		}
		if (!job) {
			job = steal_job(queue, threadData->stealId);
			if (job && !job->currentTask) {
				std::cout << "Failed, no current task in gj 2!" << std::endl;
			}
		}
		return job;
	}

	Job* JobSystem::acquire_job() {
#ifndef LOCK_FREE
		Job* job = nullptr;
		jobPoolLock.lock();
		uint32_t read = jobPoolReadIdx.fetch_add(1, std::memory_order_relaxed);
		job = jobPool[read];
		jobPoolLock.unlock();
		return job;
#else
		while (true) {
			uint32_t read = jobPoolReadIdx.load(std::memory_order_relaxed);
			//Make sure these reads are ordered correctly. There's a slight chance for read to be greater than maxRead otherwise.
			std::atomic_thread_fence(std::memory_order_seq_cst);
			uint32_t maxRead = jobPoolMaxReadIdx.load(std::memory_order_relaxed);
			if (read == maxRead) {
				continue;
			}
			if (jobPoolReadIdx.compare_exchange_weak(read, (read + 1) & queueSizeMask, std::memory_order_acquire, std::memory_order_relaxed)) {
				return jobPool[read];
			}
		}
#endif
	}

	void JobSystem::release_job(Job* job) {
#ifndef LOCK_FREE
		jobPoolLock.lock();
		uint32_t read = jobPoolReadIdx.fetch_add(-1, std::memory_order_relaxed)-1;
		jobPool[read] = job;
		jobPoolLock.unlock();
#else
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
#endif
	}

	

	void JobSystem::start_jobs_and_wait_for_counter(JobDecl* jobs, uint32_t jobCount) {
		JobThreadData& localData = *threadData;
		Job* job = localData.currentJob;
		JobCounter count(job, jobCount);

		Job** jobArray = reinterpret_cast<Job**>(alloca(jobCount * sizeof(Job*)));

		for (uint32_t i = 0; i < jobCount; i++) {
			Job* newJob = acquire_job();
			newJob->currentTask = &jobs[i];
			jobs[i].counter = &count;
			jobArray[i] = newJob;
			activeJobCount.fetch_add(1, std::memory_order_relaxed);
		}
		localData.newJobsToAdd = jobArray;
		localData.newJobsToAddCount = jobCount;
		job->state = SUSPENDED;
		swap_registers(job->ctx, localData.threadCtx);
	}

	void JobSystem::yield_job() {
		JobThreadData& localData = *threadData;
		Job* job = localData.currentJob;
		//Spinning on a load store should be ok here since this lightweight lock is only held for a tiny amount of time
		push_job(*queues[localData.threadId], *job);
		localData.newJobsToAdd = &job;
		localData.newJobsToAddCount = 1;
		job->state = SUSPENDED;
		swap_registers(job->ctx, localData.threadCtx);
	}

	void JobSystem::start_job(JobDecl& decl) {
		start_job(decl, threadData->threadId);
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

//#pragma optimize("", on)