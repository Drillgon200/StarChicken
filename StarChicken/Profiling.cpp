#include "Profiling.h"
#include <iostream>

namespace profiling {
#if PROFILING_ENABLE

	int32_t(*tidGetter)(void);
	uint32_t threadCount;
	std::vector<std::vector<ProfData>> data;
	char* filePath;

#endif

	void init_profiler(int32_t(*tid)(void), uint32_t tCount, size_t initialBufferSize, const char* name) {
#if PROFILING_ENABLE
		tidGetter = tid;
		threadCount = tCount;
		data.resize(threadCount);
		for (uint32_t i = 0; i < threadCount; i++) {
			data[i].reserve(initialBufferSize);
		}
		const char* mainPath = "F:\\vulkan_shooter\\logs\\";
		size_t length = strlen(mainPath) + strlen(name) + strlen(".json") + 1;
		filePath = new char[length];
		strcpy_s(filePath, length, mainPath);
		strcat_s(filePath, length, name);
		strcat_s(filePath, length, ".json");
#endif
	}

	void dump_data() {
#if PROFILING_ENABLE
		std::ofstream out;
		out.open(filePath);
		out << "[";
		bool first = true;
		for (uint32_t i = 0; i < threadCount; i++) {
			for (ProfData& dat : data[i]) {
				uint64_t micro = std::chrono::time_point_cast<std::chrono::microseconds>(dat.time).time_since_epoch().count();
				if (!first) {
					out << ",";
				}
				first = false;

				out << "\n\t{";
				out << "\"name\": \"" << dat.name << "\"";
				out << ", \"cat\": \"PERF\"";
				out << ", \"ph\": \"" << dat.type << "\"";
				out << ", \"pid\": " << 0;
				//Google's tracing apparently fails to display a 0 thread id, so we'll just add 1.
				out << ", \"tid\": " << (i + 1);
				out << ", \"ts\": " << micro;
				out << "}";
			}
			data[i].clear();
		}
		out << "\n]";
		data.clear();
		out.close();
#endif
	}


	void Profiler::begin() {
#if PROFILING_ENABLE
		tid = tidGetter();
		if (tid != -1) {
			data[tid].push_back({ std::chrono::high_resolution_clock::time_point(), 'B', m_name });
			data[tid].back().time = std::chrono::high_resolution_clock::now();
		}
#endif
	}

	void Profiler::end() {
#if PROFILING_ENABLE
		if (tid != -1) {
			data[tid].push_back({ std::chrono::high_resolution_clock::now(), 'E', m_name });
		}
#endif
	}
	
	Profiler::Profiler(std::string name) {
		m_name = name;
		begin();
	}

	Profiler::~Profiler() {
		end();
	}
}