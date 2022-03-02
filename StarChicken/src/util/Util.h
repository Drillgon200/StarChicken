#pragma once

#include <string>
#include <vector>

namespace util {
	struct FileMapping {
		void* fileHandle;
		void* mappingHandle;
		void* mapping;
	};

	FileMapping map_file(std::wstring name);

	void unmap_file(FileMapping& mapping);

	int32_t run_program(const char* prog);

	template<typename T>
	bool vector_contains(std::vector<T> vec, T& value) {
		for (uint32_t i = 0; i < vec.size(); i++) {
			if (vec[i] == value) {
				return true;
			}
		}
		return false;
	}

	inline bool string_ends_with(const char* str, const char* end) {
		uint32_t len1 = strlen(str);
		uint32_t len2 = strlen(end);
		if (len2 > len1) {
			return false;
		}
		for (uint32_t i = 0; i < len2; i++) {
			if (str[(len1 - len2) + i] != end[i]) {
				return false;
			}
		}
		return true;
	}
}