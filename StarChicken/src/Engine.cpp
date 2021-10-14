#include "Engine.h"

job::JobSystem jobSystem;

struct test {
	int bruh;
};

struct test2 {
	uint16_t arg;
	float mug;
};

job::SpinLock sLock{};

void nestfunc() {
	FUNC_PROF()

	uint32_t count = 0;
	for (uint32_t i = 0; i < 100000000; i++) {
		count += i;
	}

	SPIN_LOCK(sLock)
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
	FUNC_PROF()

	job::JobDecl decls[3];
	decls[0] = job::JobDecl(nestfunc);
	decls[1] = job::JobDecl(nestfunc);
	decls[2] = job::JobDecl(nestfunc);
	jobSystem.start_jobs_and_wait_for_counter(decls, 3);
}
//#pragma optimize("", on)

void entry_point() {
	FUNC_PROF()

	job::JobDecl decls[2];
	decls[0] = job::JobDecl(func);
	decls[1] = job::JobDecl(func);
	jobSystem.start_jobs_and_wait_for_counter(decls, 2);
}

int main() {
	uint32_t threadCount = std::thread::hardware_concurrency();
	jobSystem.init_job_system(threadCount);
	profiling::init_profiler([](void) {return job::currentThreadId; }, threadCount, 1024, "testlog");
	job::JobDecl decl{ entry_point };
	jobSystem.start_entry_point(decl);

	while (!jobSystem.is_done()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	jobSystem.end_job_system();

	profiling::dump_data();
	return 0;
}