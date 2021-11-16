#pragma once

#include <emmintrin.h>
#include <vector>
#include <atomic>
#include <thread>
#include <iostream>

#pragma pack(push, 4)
struct alignas(16) Context {
	void* rip, * rsp;
	void* rbx, * rbp, * r12, * r13, * r14, * r15, * rdi, * rsi;
	__m128i xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
	uint32_t mxcsrControlWord, x87fpuControlWord;
	void* stackBase, * stackLimit, * deallocationStack;
	uint32_t guaranteedStackBytes;
	uint32_t padding;
	void* fiberData;
};
#pragma pack(pop)

#pragma optimize("", off)

extern "C" void save_registers(Context& c);
extern "C" void load_registers(Context& c);
extern "C" void swap_registers(Context& oldCtx, Context& newCtx);
extern "C" void load_registers_arg(Context& ctx, void* arg);
extern "C" void swap_registers_arg(Context & oldCtx, Context & newCtx, void* arg);

namespace job {

#define SUSPENDED 0
#define ACTIVE 1
#define ENDED 2

	//Lock a regular full spin lock
#define SPIN_LOCK(lock) job::RAIISpinLocker spin_locker##__LINE__(&lock);static_assert(true, "")
//Read lock a reader writer spin lock
#define RSPIN_LOCK(lock) job::RAIIRSpinLocker spin_locker##__LINE__(&lock);static_assert(true, "")
//Write lock a reader writer spin lock
#define WSPIN_LOCK(lock) job::RAIIWSpinLocker spin_locker##__LINE__(&lock);static_assert(true, "")

	class RWSpinLock {
	private:
		std::atomic<int32_t> lockCount;
	public:
		RWSpinLock() {
			lockCount = 0;
		}

		void lock_read() {
			while (true) {
				int32_t lock = lockCount.load(std::memory_order_relaxed);
				if (lock != -1 && lockCount.compare_exchange_strong(lock, lock + 1, std::memory_order_acquire, std::memory_order_relaxed)) {
					break;
				}
				while (lockCount.load(std::memory_order_relaxed) == -1);
			}
		}

		void lock_write() {
			while (true) {
				int32_t val = 0;
				if (lockCount.compare_exchange_strong(val, -1, std::memory_order_acquire, std::memory_order_relaxed)) {
					break;
				}
				while (lockCount.load(std::memory_order_relaxed) != 0);
			}
		}

		void unlock_read() {
			//Assumes you have already established a read lock
			lockCount.fetch_add(-1, std::memory_order_relaxed);
		}

		void unlock_write() {
			//Assumes you have already established a write lock
			lockCount.store(0, std::memory_order_release);
		}
	};

	class SpinLock {
	private:
		std::atomic<bool> locked;
	public:
		SpinLock() {
			locked.store(false, std::memory_order_relaxed);
		}
		~SpinLock() {
		}

		void lock() {
			while (true) {
				if (!locked.exchange(true, std::memory_order_seq_cst)) {
					break;
				}
				while (locked.load(std::memory_order_seq_cst));
			}
		}

		void unlock() {
			locked.store(false, std::memory_order_seq_cst);
		}
	};

	class RAIIRSpinLocker {
	private:
		RWSpinLock* lock;
	public:
		RAIIRSpinLocker(RWSpinLock* lock) {
			this->lock = lock;
			lock->lock_read();
		}
		~RAIIRSpinLocker() {
			this->lock->unlock_read();
		}
	};

	class RAIIWSpinLocker {
	private:
		RWSpinLock* lock;
	public:
		RAIIWSpinLocker(RWSpinLock* lock) {
			this->lock = lock;
			lock->lock_write();
		}
		~RAIIWSpinLocker() {
			this->lock->unlock_write();
		}
	};

	class RAIISpinLocker {
	private:
		SpinLock* lock;
	public:
		RAIISpinLocker(SpinLock* lock) {
			this->lock = lock;
			lock->lock();
		}
		~RAIISpinLocker() {
			this->lock->unlock();
		}
	};

	const uint32_t JOB_STACK_SIZE = 128 * 1024;
	//I like to keep sizes in powers of 2
	const uint32_t queueSize = 1 << 8;
	const uint32_t queueSizeMask = queueSize - 1;

	extern thread_local int32_t currentThreadId;
	extern Context* threadCtx;
	int32_t this_thread_id();

	class JobSystem;
	struct JobDecl;

	volatile class Job {
	public:
		friend class JobSystem;
		friend struct JobCounter;

		JobSystem* system;
		JobDecl* currentTask;
		bool active = false;
		char* data = nullptr;
		char* stackPointer = nullptr;
		uint32_t state = ENDED;
		Context ctx{};
	public:
		
		Job(JobSystem* sys);
		~Job();



		//This function handles releasing the job and context switching back after the task ends
		static void run(Job* job);
	};

	/*volatile struct JobQueue {
		std::thread::id threadId;
		uint32_t id;
		Job** jobs;
		std::atomic<int32_t> top;
		std::atomic<int32_t> bottom;
		uint32_t stealId;

		JobQueue(std::thread::id tid, uint32_t id);
		~JobQueue();
	};*/
	volatile struct JobCounter {
		Job* job;
		std::atomic<int32_t> counter;

		JobCounter(Job* job, int32_t count);

		void decrement();
	};

	volatile struct JobDecl {
		JobCounter* counter;
		void (*func)(void*);
		void* arg;

		JobDecl();

		JobDecl(void (*f)(void*), void* argument);

		JobDecl(void (*f)(void));
	};

	volatile class JobSystem {
	private:
		friend struct JobCounter;
		friend class Job;

		std::vector<Job*> jobPool;
		std::vector<std::vector<Job*>> jobQueues;
		Job*** newJobsToAdd = nullptr;
		uint32_t* newJobsCount = nullptr;
		std::vector<uint32_t> stealIds;

		//std::atomic<uint32_t> jobPoolReadIdx;
		//std::atomic<uint32_t> jobPoolMaxReadIdx;
		//std::atomic<uint32_t> jobPoolWriteIdx;
		//The queue of empty available jobs, implemented as a ring buffer
		//Job* jobPool[queueSize];

		//std::vector<JobQueue*> queues;
		Job** currentJobsByThread = nullptr;
		std::atomic<uint32_t> activeJobCount{ 0 };

		std::vector<std::thread> threadpool;
		std::atomic<bool> finished = false;

		SpinLock lock{};

		void push_job(std::vector<Job*>& queue, Job& job);
		Job* pop_job(std::vector<Job*>& queue);
		Job* acquire_job();
		void release_job(Job* job);
		Job* steal_job(std::vector<Job*>& queue, uint32_t& stealId);
		Job* getJob(uint32_t index);

		void threadFunc(uint32_t idx);

		void cleanup();
	public:
		Job* thisjob();
		void init_job_system(uint32_t threadCount);
		void start_entry_point(JobDecl& decl);
		void end_job_system();
		void start_jobs_and_wait_for_counter(JobDecl* jobs, uint32_t jobCount);
		void start_job(JobDecl& decl);
		void start_job(JobDecl& decl, uint32_t tid);
		void yield_job();
		uint16_t thread_count();
		bool is_done();
	};

}

#pragma optimize("", on)
