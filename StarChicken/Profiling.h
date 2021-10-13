#pragma once

#include <string>
#include <chrono>
#include <fstream>
#include <vector>

#define PROFILING_ENABLE 1
#if PROFILING_ENABLE
#define SCOPE_PROF(name) profiling::Profiler scope_profiler##__LINE__(name);
#else
#define SCOPE_PROF(name)
#endif
#define FUNC_PROF() SCOPE_PROF(__FUNCTION__)

namespace profiling {
#if PROFILING_ENABLE
	struct ProfData {
		std::chrono::high_resolution_clock::time_point time;
		char type;
		std::string name;
	};

	extern int32_t(*tidGetter)(void);
	extern uint32_t threadCount;
	extern std::vector<std::vector<ProfData>> data;
	extern char* filePath;

#endif

	void init_profiler(int32_t(*tid)(void), uint32_t tCount, size_t initialBufferSize, const char* name);

	void dump_data();

	class Profiler {
	private:
		std::string m_name;
		int32_t tid;

		void begin();

		void end();

	public:

		Profiler(std::string name);

		~Profiler();
	};
}