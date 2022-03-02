#pragma once

#include <vulkan/vulkan.h>
#include "VkUtil.h"
#include <vector>

#define DEVICE_ALLOCATION_BLOCK_SIZE (64 * 1024 * 1024)

namespace vku {

	//Drillgon200 2021-12-02: Basic alloctor. Lots of improvements could be made here, but it'll work for now.

	class DeviceMemoryBlock;

	struct DeviceAllocation {
		DeviceMemoryBlock*  memory;
		uint32_t memoryTypeIndex;
		uint32_t blockIndex;
		uint32_t offset;
		uint32_t size;

		void* map();
	};

	struct DeviceBuffer {
		DeviceAllocation* allocation;
		VkBuffer buffer;
	};

	enum DeviceAllocationType {
		NONE = 0,
		DEVICE_ALLOCATION_TYPE_BUFFER = 1,
		DEVICE_ALLOCATION_TYPE_IMAGE = 2
	};

	struct DeviceFreeMemoryBlob {
		uint32_t beginIdx;
		uint32_t endIdx;
		DeviceAllocationType previousType;
		DeviceAllocationType nextType;
	};

	class DeviceMemoryBlock {
	private:
		VkDeviceMemory memory;
		void* memoryMapping;
		std::vector<DeviceFreeMemoryBlob> freeBlocks{};
		uint32_t size;
		uint32_t memoryTypeIndex;
		uint32_t blockIndex;
	public:
		DeviceMemoryBlock(uint32_t size, uint32_t memIndex, uint32_t blockIndex);
		DeviceMemoryBlock(const DeviceMemoryBlock& other);
		~DeviceMemoryBlock();

		DeviceAllocation* alloc(uint32_t size, uint32_t alignment, VkDeviceSize bufferImageGranularity, DeviceAllocationType type);

		void free(DeviceAllocation* allocation);

		void* map();

		VkDeviceMemory get_memory();
	};

	class DeviceMemoryPool {
	private:
		std::vector<DeviceMemoryBlock*> blocks{};
		uint32_t memoryTypeIndex;
	public:
		DeviceMemoryPool(uint32_t memIndex);
		DeviceMemoryPool(const DeviceMemoryPool& other);
		~DeviceMemoryPool();

		DeviceAllocation* alloc(uint32_t size, uint32_t alignment, VkDeviceSize bufferImageGranularity, DeviceAllocationType type);

		void free(DeviceAllocation* allocation);
	};

	class DeviceMemoryAllocator {
	private:
		std::vector<DeviceMemoryPool> memoryPools{};
		std::vector<DeviceBuffer> deletionQueues[NUM_FRAME_DATA];
		VkDeviceSize bufferImageGranularity;
		uint64_t totalAllocatedBytes{0};
		uint32_t totalAllocations{0};
	public:
		DeviceMemoryAllocator();
		~DeviceMemoryAllocator();

		DeviceBuffer alloc_buffer(uint32_t size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memProps);
		DeviceAllocation* alloc(VkMemoryRequirements memReq, VkMemoryPropertyFlags propertyFlags, DeviceAllocationType type);
		DeviceAllocation* alloc(uint32_t size, uint32_t alignment, uint32_t memoryType, DeviceAllocationType type);

		void free_buffer(DeviceBuffer& buffer);
		void queue_free_buffer(uint32_t frame, DeviceBuffer buffer);
		void free_old_data(uint32_t frame);
		void free(DeviceAllocation* allocation);

		uint64_t total_allocated_memory();

		uint32_t total_allocations();
	};
}