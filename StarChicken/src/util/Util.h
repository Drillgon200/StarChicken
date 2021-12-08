#pragma once

#include <string>

namespace util {
	struct FileMapping {
		void* fileHandle;
		void* mappingHandle;
		void* mapping;
	};

	FileMapping map_file(std::wstring name);

	void unmap_file(FileMapping& mapping);
}