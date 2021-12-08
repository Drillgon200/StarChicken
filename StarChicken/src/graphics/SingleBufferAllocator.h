#pragma once

#include <stdlib.h>
#include <string>
#include <algorithm>
#include "VkUtil.h"

namespace vku {

	class SingleBufferAllocator;

	struct SingleBufferAllocation {
		SingleBufferAllocator& allocator;
		uint32_t offset;
		uint32_t size;

		SingleBufferAllocation(SingleBufferAllocator& alloc, uint32_t offset, uint32_t size);
		~SingleBufferAllocation();

		void* map();
	};

	struct PendingDeallocation {
		VkDevice& device;
		VkBuffer buffer;
		VkDeviceMemory memory;
	};

	//A single buffer allocator allocates all memory out of a single buffer, unlike the regular memory allocator. It resizes the buffer when it gets too big.
	//Resizing requires a transfer command, so memory allocations out of this buffer type should only happen during startup when the transfer command buffer is active.
	class SingleBufferAllocator {
	private:
		static std::vector<PendingDeallocation> deallocs;

		std::vector<SingleBufferAllocation*> allAllocations;

		VkDevice& device;
		VkDeviceMemory deviceMemory;
		void* memoryMapping;
		uint32_t currentSize;
		uint32_t currentOffset;
		VkBufferUsageFlags bufferUsageFlags;
		VkMemoryPropertyFlags memoryPropertyFlags;
		VkBuffer buffer;

		void check_resize_buffer(uint32_t newSize);
	public:
		static void check_deallocate();

		SingleBufferAllocator(VkDevice& device, uint32_t defaultSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);

		~SingleBufferAllocator();

		void* map();

		void unmap();

		//Basic linear allocator for now, should move to a block or block/linear hybrid later
		SingleBufferAllocation* alloc(uint32_t size);
		void free(SingleBufferAllocation* allocation);

		//Only use if absolutely sure no one is using or will ever use the allocated space anymore
		void reset();

		VkBuffer get_buffer();
		VkDeviceMemory get_memory();
	};
}
