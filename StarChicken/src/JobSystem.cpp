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

	void dummy() {
		std::cerr << "Should not be here!" << std::endl;
	};

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
		load_registers(threadCtx[this_thread_id()]);
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



	/*JobQueue::JobQueue(std::thread::id tid, uint32_t id) {
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
	}*/



	JobCounter::JobCounter(Job* job, int32_t count) {
		this->job = job;
		this->counter = count;
	}

	void JobCounter::decrement() {
		int32_t count = counter.fetch_add(-1, std::memory_order_relaxed);
		if (count == 1) {
			job->system->push_job(job->system->jobQueues[this_thread_id()], *job);
			//job->system->push_job(*job->system->queues[this_thread_id()], *job);
		}
	}



	void JobSystem::init_job_system(uint32_t threadCount) {
		jobPool.reserve(queueSize);
		for (int i = 0; i < queueSize; i++) {
			jobPool.push_back(new Job(this));
			//std::cout << jobPool[i] << std::endl;
		}
		//jobPoolReadIdx.store(0, std::memory_order_relaxed);
		//jobPoolMaxReadIdx.store(queueSizeMask, std::memory_order_relaxed);
		//jobPoolWriteIdx.store(queueSizeMask, std::memory_order_relaxed);

		threadpool.resize(threadCount);
		threadCtx = new Context[threadCount];
		newJobsToAdd = new Job**[threadCount];
		newJobsCount = new uint32_t[threadCount];
		jobQueues.reserve(threadCount);
		stealIds.reserve(threadCount);
		//queues.resize(threadpool.size());
		currentJobsByThread = new Job*[threadCount];
		finished.store(true, std::memory_order_seq_cst);

		for (uint32_t i = 0; i < threadCount; i++) {
			threadpool[i] = std::thread(&JobSystem::threadFunc, this, i);
			//queues[i] = new JobQueue(threadpool[i].get_id(), i);
			jobQueues.push_back(std::vector<Job*>());
			jobQueues[i].reserve(queueSize);
			stealIds.push_back(i);
			threadCtx[i] = {};
			newJobsToAdd[i] = nullptr;
			newJobsCount[i] = 0;
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
		finished.store(false, std::memory_order_seq_cst);
	}

	void JobSystem::end_job_system() {
		finished.store(true, std::memory_order_seq_cst);
		for (int i = 0; i < threadpool.size(); i++) {
			threadpool[i].join();
		}

		cleanup();
	}

	void JobSystem::cleanup() {
		//SPIN_LOCK(lock);
		lock.lock();
		delete currentJobsByThread;
		delete threadCtx;
		for (int i = 0; i < queueSize; i++) {
			//std::cout << jobPool[i] << std::endl;
			delete jobPool[i];
		}
		//for (int i = 0; i < queues.size(); i++) {
		//	delete queues[i];
		//}
		lock.unlock();
	}

	void JobSystem::threadFunc(uint32_t idx) {
		currentThreadId = idx;
		while (finished.load(std::memory_order_seq_cst)) {
		}
		std::atomic_thread_fence(std::memory_order_seq_cst);
		uint8_t count = 0;
		//if (idx == 1) {
			//std::this_thread::sleep_for(std::chrono::milliseconds(500));
		//}
		while (!finished.load(std::memory_order_seq_cst)) {
			Job* job = getJob(idx);
			if (job) {
				count = 0;

				if (!job->active) {
					job->active = true;
					job->ctx.rip = (void*)job->run;
					//Subtract from stack pointer to pretend we pushed a return address, otherwise the stack will be aligned wrong
					job->ctx.rsp = (void*)(job->stackPointer-sizeof(uintptr_t));
				}
				currentJobsByThread[idx] = job;

				if (job->state == ACTIVE) {
					std::cout << "Wrong state\n";
				}

				job->state = ACTIVE;
				swap_registers_arg(threadCtx[idx], job->ctx, job);
				if (job->state == ACTIVE) {
					std::cout << "Wrong state\n";
				}

				if (job->state == ENDED) {
					job->system->release_job(job);
				}

				if (newJobsCount[idx] > 0) {
					for (uint32_t i = 0; i < newJobsCount[idx]; i++) {
						push_job(jobQueues[idx], *newJobsToAdd[idx][i]);
					}
				}
				newJobsCount[idx] = 0;
				newJobsToAdd[idx] = nullptr;

				continue;
			}
			if (++count > 16) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				count = 0;
			}
		}
		std::cout << "Exited job thread " << idx << " successfully" << std::endl;
	}

	void JobSystem::push_job(std::vector<Job*>& queue, Job& job) {
		//SPIN_LOCK(lock);
		if (!job.currentTask) {
			std::cout << "Pushed job without task" << std::endl;
		}
		lock.lock();
		queue.push_back(&job);
		lock.unlock();
		/*int32_t bottom = queue.bottom.load(std::memory_order_relaxed);
		queue.jobs[bottom] = &job;
		queue.bottom.store(bottom+1, std::memory_order_relaxed);*/

		/*const int32_t bottom = queue.bottom.load(std::memory_order_relaxed);
		queue.jobs[bottom & queueSizeMask] = &job;
		//Make sure if something tries to read this job, it's in memory
		std::atomic_thread_fence(std::memory_order_release);
		queue.bottom.store(bottom + 1, std::memory_order_relaxed);*/
	}

	Job* JobSystem::pop_job(std::vector<Job*>& queue) {
		if (queue.size() <= 0) {
			return nullptr;
		}
		Job* j = queue.back();
		queue.pop_back();
		return j;
		/*int32_t bottom = queue.bottom.load(std::memory_order_relaxed);
		int32_t top = queue.top.load(std::memory_order_relaxed);
		if (bottom == top) {
			return nullptr;
		}
		queue.bottom.store(bottom - 1, std::memory_order_relaxed);
		uint32_t idx = (bottom - 1) & queueSizeMask;
		Job* job = queue.jobs[idx];
		if (job) {
			bool b = job->active;
		}
		queue.jobs[idx] = reinterpret_cast<Job*>(0xDEADBEEF);
		return job;*/

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

	Job* JobSystem::steal_job(std::vector<Job*>& queue, uint32_t& stealId) {
		uint32_t queueSize = static_cast<uint32_t>(jobQueues.size());
		uint8_t tries = 0;
		//Pick a new steal queue until we have a valid one or we ran out of other queues to steal from.
		//A queue is invalid if it's the last one we stole from (tries == 0), it's this thread's queue
		while (tries == 0 || queue == jobQueues[stealId]) {
			stealId = (stealId + 1) % queueSize;
			++tries;
			//Check a lightweight manual lock to remove a possible race condition caused by a fiber yielding or waiting
			if (tries > queueSize) {
				return nullptr;
			}
		}
		std::vector<Job*>& stealQueue = jobQueues[stealId];
		
		if (stealQueue.size() <= 0) {
			return nullptr;
		}
		Job* j = stealQueue.back();
		stealQueue.pop_back();
		return j;
		/*int32_t top = stealQueue.top.load(std::memory_order_relaxed);
		int32_t bottom = stealQueue.bottom.load(std::memory_order_relaxed);
		if (bottom == top) {
			return nullptr;
		}
		stealQueue.top.store(top + 1, std::memory_order_relaxed);
		Job* job = stealQueue.jobs[top & queueSizeMask];
		stealQueue.jobs[top & queueSizeMask] = reinterpret_cast<Job*>(0xDEADBEEF);
		return job;*/

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
		lock.lock();
		std::vector<Job*>& queue = jobQueues[index];
		Job* job = pop_job(queue);
		if (!job) {
			job = steal_job(queue, stealIds[index]);
		}
		lock.unlock();
		return job;
	}

	Job* JobSystem::acquire_job() {
		//SPIN_LOCK(lock);
		lock.lock();
		Job* j = jobPool.back();
		jobPool.pop_back();
		lock.unlock();
		return j;
		/*uint32_t read = jobPoolReadIdx.load(std::memory_order_relaxed);
		uint32_t maxRead = jobPoolMaxReadIdx.load(std::memory_order_relaxed);
		if (read == maxRead) {
			return nullptr;
		}
		jobPoolReadIdx.store((read+1)&queueSizeMask, std::memory_order_relaxed);
		if (!jobPool[read] || jobPool[read]->active) {
			int i = 1 + 1;
		}
		Job* job = jobPool[read];
		jobPool[read] = reinterpret_cast<Job*>(0xDEADBEEF);
		return job;*/
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
		//SPIN_LOCK(lock);
		lock.lock();
		jobPool.push_back(job);
		lock.unlock();
		/*uint32_t idx = jobPoolWriteIdx.load(std::memory_order_relaxed);
		uint32_t newidx = (idx + 1) & queueSizeMask;
		jobPoolWriteIdx.store(newidx, std::memory_order_relaxed);
		jobPoolMaxReadIdx.store(newidx, std::memory_order_relaxed);
		job->ctx.rip = reinterpret_cast<void*>(0xDEADBEEF);
		if (jobPool[newidx] != reinterpret_cast<Job*>(0xDEADBEEF)) {
			std::cout << jobPool[newidx] << std::endl;
			dummy();
		}
		jobPool[newidx] = job;*/
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
		JobCounter count(job, jobCount);

		Job** jobArray = reinterpret_cast<Job**>(alloca(jobCount * sizeof(Job*)));

		for (uint32_t i = 0; i < jobCount; i++) {
			Job* newJob = acquire_job();
			newJob->currentTask = &jobs[i];
			jobs[i].counter = &count;
			jobArray[i] = newJob;
			activeJobCount.fetch_add(1, std::memory_order_relaxed);
		}
		newJobsToAdd[tid] = jobArray;
		newJobsCount[tid] = jobCount;
		job->state = SUSPENDED;
		swap_registers(job->ctx, threadCtx[tid]);
	}

	void JobSystem::yield_job() {
		int32_t tid = this_thread_id();
		Job* job = currentJobsByThread[tid];
		//Spinning on a load store should be ok here since this lightweight lock is only held for a tiny amount of time
		push_job(jobQueues[tid], *job);
		newJobsToAdd[tid] = &job;
		newJobsCount[tid] = 1;
		job->state = SUSPENDED;
		swap_registers(job->ctx, threadCtx[tid]);
	}

	void JobSystem::start_job(JobDecl& decl) {
		start_job(decl, this_thread_id());
	}

	void JobSystem::start_job(JobDecl& decl, uint32_t tid) {
		Job* job = acquire_job();
		job->currentTask = &decl;
		push_job(jobQueues[tid], *job);
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