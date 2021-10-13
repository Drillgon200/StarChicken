#pragma once

#include <iostream>
#include <thread>
#define NOMINMAX
#pragma comment(lib, "Ws2_32.lib")
#include <Windows.h>
#include "EntityComponentSystem.h"
#include "Profiling.h"
#include "JobSystem.h"

extern job::JobSystem jobSystem;

inline bool big_endian() {
	return htonl(1) == 1;
}