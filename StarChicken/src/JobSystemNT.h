#pragma once

#include "JobSystem.h"

#pragma optimize("", off)
namespace jobnt {

	alignas(64) job::SpinLock globalLock{};
	Job** jobPool;
	uint32_t jobPoolIdx = 0;
	Job** jobToComplete;
	uint32_t jobToCompleteIdx = 0;
	std::thread* threads;

	struct Counter;

	struct JobDecl    {
		Counter* counter = nullptr;
		void (*func)(void*);
	};
	struct Job {
		JobDecl* decl = nullptr;
		bool active = false;
		char* data = nullptr;
		char* stackPointer = nullptr;
		Context ctx{};

		Job() {
			data = reinterpret_cast<char*>(malloc(job::JOB_STACK_SIZE));
			std::cout << "\t" << std::hex << reinterpret_cast<uintptr_t>(data) << std::endl;
			//Stacks grow down, so start at the top
			stackPointer = data + job::JOB_STACK_SIZE;
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
		}

		static void run(Job* job) {

		}
	};

	struct Counter {
		Job* job;
		uint32_t count;

		void decrement() {
			if (--count == 0) {
				globalLock.lock();
				jobToComplete[jobToCompleteIdx++] = job;
				globalLock.unlock();
			}
		}
	};

	alignas(64) std::atomic<bool> shouldContinue;
	_declspec(thread) int32_t threadId = -5;
	Context* threadCtx = nullptr;

	Job* getJob();

	void threadFunc(uint32_t tid) {
		threadId = tid;
		while (!shouldContinue.load()) {
		}
		while (shouldContinue.load()) {
			save_registers(threadCtx[tid]);
			Job* job = getJob();
			if (job) {
				if (!job->active) {
					job->active = true;
					job->ctx.rsp = job->stackPointer;
					job->ctx.rip = job->run;
				}
				load_registers(job->ctx);
			}
		}
		std::cout << "Exited job thread " << tid << " successfully" << std::endl;
	}

	void init(uint32_t threadCount) {
		shouldContinue.store(false);

		jobPool = reinterpret_cast<Job**>(malloc(sizeof(uintptr_t)*256));
		jobToComplete = reinterpret_cast<Job**>(malloc(sizeof(uintptr_t) * 256));
		for (uint32_t i = 0; i < 256; i++) {
			jobPool[i] = new Job();
		}
		threads = reinterpret_cast<std::thread*>(malloc(sizeof(std::thread)*threadCount));
		threadCtx = reinterpret_cast<Context*>(malloc(sizeof(Context) * threadCount));
		for (uint32_t i = 0; i < threadCount; i++) {
			new (threads + i * sizeof(std::thread)) std::thread(threadFunc, i);
		}

		shouldContinue.store(true);
	}

	void startJob(JobDecl& decl) {
		globalLock.lock();
		Job* job = jobPool[jobPoolIdx];
		++jobPoolIdx;
		jobToComplete[jobToCompleteIdx] = job;
		++jobToCompleteIdx;
		globalLock.unlock();
	}

	Job* getJob() {
		globalLock.lock();
		if (jobToCompleteIdx == 0) {
			globalLock.unlock();
			return nullptr;
		}
		Job* job = jobToComplete[--jobToCompleteIdx];
		globalLock.unlock();
		return job;
	}

	void waitForJobs(JobDecl* jobs, uint32_t count) {
		for (uint32_t i = 0; i < count; i++) {
			startJob(jobs[i]);
		}
	}

	void cleanup() {
		free(jobPool);
		free(jobToComplete);
		free(threads);
		free(threadCtx);
	}
}
#pragma optimize("", on)