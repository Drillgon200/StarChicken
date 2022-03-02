#pragma once

#include <stdint.h>
#include <string>

namespace util {
	namespace msdf {
		void generate_MSDF(std::wstring srcFile, std::wstring dstFile, uint32_t sizeX, uint32_t sizeY, bool font);
	}
}