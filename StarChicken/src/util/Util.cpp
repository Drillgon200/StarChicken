#include "Util.h"
#ifdef _WIN32
#include <Windows.h>
#endif

namespace util {
	FileMapping map_file(std::wstring name) {
		FileMapping map{};
#ifdef _WIN32
		HANDLE hFile = CreateFile(name.c_str(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
		HANDLE mapping = CreateFileMapping(hFile, 0, PAGE_READONLY, 0, 0, (name + L"_mapping").c_str());
		LPVOID fileMap = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
		map = { hFile, mapping, fileMap };
#endif
		return map;
	}

	void unmap_file(FileMapping& mapping) {
#ifdef _WIN32
		UnmapViewOfFile(mapping.mapping);
		CloseHandle(mapping.mappingHandle);
		CloseHandle(mapping.fileHandle);
#endif
	}
}