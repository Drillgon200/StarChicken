#pragma once

#include <string>
#include <chrono>
#include <fstream>
#include <vector>

#define PROFILING_ENABLE 0
#if PROFILING_ENABLE
//the static assert simply requires a semicolon, which is to make visual studio not auto indent after a macro call. Also makes it more consistent I think.
#define SCOPE_PROF(name) profiling::Profiler scope_profiler##__LINE__(name);static_assert(true, "")
#else
#define SCOPE_PROF(name)static_assert(true, "")
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