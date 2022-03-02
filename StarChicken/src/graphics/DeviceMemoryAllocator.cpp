#include "DeviceMemoryAllocator.h"
#include "VkUtil.h"
#include <assert.h>

namespace vku {

	void* DeviceAllocation::map() {
		void* mapping = reinterpret_cast<uint8_t*>(memory->map()) + offset;

		return mapping;
	}

	DeviceMemoryBlock::DeviceMemoryBlock(uint32_t size, uint32_t memIndex, uint32_t blockIndex) : freeBlocks{}, size{ size }, memoryTypeIndex{ memIndex }, blockIndex{ blockIndex }, memoryMapping{ nullptr } {
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = size;
		allocInfo.memoryTypeIndex = memIndex;
		VKU_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &memory), "Failed to allocate memory in device memory block!");
		uint32_t cap = freeBlocks.capacity();
		freeBlocks.push_back({ 0, DEVICE_ALLOCATION_BLOCK_SIZE, NONE, NONE });
	}

	DeviceMemoryBlock::DeviceMemoryBlock(const DeviceMemoryBlock& other) {
		memory = other.memory;
		size = other.size;
		memoryTypeIndex = other.memoryTypeIndex;
		memoryMapping = other.memoryMapping;
		blockIndex = other.blockIndex;
		for (const DeviceFreeMemoryBlob& blob : other.freeBlocks) {
			freeBlocks.push_back(blob);
		}
	}

	DeviceMemoryBlock::~DeviceMemoryBlock() {
		vkFreeMemory(device, memory, nullptr);
	}

	DeviceAllocation* DeviceMemoryBlock::alloc(uint32_t size, uint32_t alignment, VkDeviceSize bufferImageGranularity, DeviceAllocationType type) {
		DeviceAllocation* allocation = nullptr;
		if (this->size != DEVICE_ALLOCATION_BLOCK_SIZE) {
			//This is a block that is bigger than the default size, AKA meant to have exactly one large allocation in it.
			if (freeBlocks.empty()) {
				return nullptr;
			} else {
				freeBlocks.clear();
				allocation = new DeviceAllocation{ this, memoryTypeIndex, blockIndex, 0, size };
				return allocation;
			}
		}
		for (uint32_t i = 0; i < freeBlocks.size(); i++) {
			DeviceFreeMemoryBlob& blob = freeBlocks[i];
			//Trivial check, if it isn't big enough, continue
			if (blob.endIdx - blob.beginIdx < size) {
				continue;
			}

			uint32_t allocStart = (blob.beginIdx + (alignment - 1)) & (~(alignment - 1));
			uint32_t granularity = bufferImageGranularity - (allocStart - blob.beginIdx);
			if (granularity > 0 && ((blob.previousType | type) == (DEVICE_ALLOCATION_TYPE_BUFFER | DEVICE_ALLOCATION_TYPE_IMAGE))) {
				allocStart += granularity;
			}
			allocStart = (blob.beginIdx + (alignment - 1)) & (~(alignment - 1));
			uint32_t allocEnd = blob.endIdx;
			if ((blob.nextType | type) == (DEVICE_ALLOCATION_TYPE_BUFFER | DEVICE_ALLOCATION_TYPE_IMAGE)) {
				allocEnd -= bufferImageGranularity;
			}
			//Second check, is the size modified to fit alignment and buffer image granularity also big enough?
			if ((allocEnd - allocStart) < size) {
				continue;
			}
			allocEnd = allocStart + size;

			allocation = new DeviceAllocation{ this, memoryTypeIndex, blockIndex, allocStart, allocEnd - allocStart };
			if (allocEnd < blob.endIdx) {
				freeBlocks.insert(freeBlocks.begin() + i + 1, { allocEnd, blob.endIdx, type, blob.nextType });
			}
			if (allocStart > freeBlocks[i].beginIdx) {
				freeBlocks[i].endIdx = allocStart;
				freeBlocks[i].nextType = type;
			} else {
				freeBlocks.erase(freeBlocks.begin() + i);
			}
			break;

		}
		return allocation;
	}

	void DeviceMemoryBlock::free(DeviceAllocation* allocation) {
		int32_t prevIdx = -1;
		int32_t nextIdx = -1;
		uint32_t addIdx = 0;
		for (uint32_t i = 0; i < freeBlocks.size(); i++) {
			DeviceFreeMemoryBlob& blob = freeBlocks[i];
			if (blob.endIdx < allocation->offset) {
				addIdx = i + 1;
			}
			if (blob.beginIdx > (allocation->offset + allocation->size)) {
				break;
			}
			if (blob.endIdx == allocation->offset) {
				prevIdx = i;
			} else if (blob.beginIdx == (allocation->offset + allocation->size)) {
				nextIdx = i;
				break;
			}
		}
		if (prevIdx >= 0 && nextIdx >= 0) {
			//If there are free blocks both free and behind, expand the previous one to encompass both and remove the next one
			DeviceFreeMemoryBlob& prev = freeBlocks[prevIdx];
			DeviceFreeMemoryBlob& next = freeBlocks[nextIdx];
			prev.endIdx = next.endIdx;
			prev.nextType = next.nextType;
			freeBlocks.erase(freeBlocks.begin() + nextIdx);
		} else if (prevIdx >= 0) {
			//If there's only a previous free block, expand it to reclaim the newly freed memory and set its next type to no type
			DeviceFreeMemoryBlob& prev = freeBlocks[prevIdx];
			prev.endIdx = allocation->offset + allocation->size;
			prev.nextType = NONE;
		} else if (nextIdx >= 0) {
			//If there's only a next free block, expand it to reclaim the newly freed memory and set its previous type to no type
			DeviceFreeMemoryBlob& next = freeBlocks[nextIdx];
			next.beginIdx = allocation->offset;
			next.previousType = NONE;
		} else {
			//If there are no adjacent free blocks, add this as a free block in sorted order
			freeBlocks.insert(freeBlocks.begin() + addIdx, { allocation->offset, allocation->offset + allocation->size, NONE, NONE });
		}
		delete allocation;
	}

	void* DeviceMemoryBlock::map() {
		if (memoryMapping) {
			return memoryMapping;
		}
		VKU_CHECK_RESULT(vkMapMemory(device, memory, 0, DEVICE_ALLOCATION_BLOCK_SIZE, 0, &memoryMapping), "Failed to map memory!");
		return memoryMapping;
	}

	VkDeviceMemory DeviceMemoryBlock::get_memory() {
		return memory;
	}

	DeviceMemoryPool::DeviceMemoryPool(uint32_t memIndex) : memoryTypeIndex{ memIndex } {}
	DeviceMemoryPool::DeviceMemoryPool(const DeviceMemoryPool& other) {
		memoryTypeIndex = other.memoryTypeIndex;
	}
	DeviceMemoryPool::~DeviceMemoryPool() {
		for (DeviceMemoryBlock* block : blocks) {
			delete block;
		}
		blocks.clear();
	}

	DeviceAllocation* DeviceMemoryPool::alloc(uint32_t size, uint32_t alignment, VkDeviceSize bufferImageGranularity, DeviceAllocationType type) {
		DeviceAllocation* allocation = nullptr;
		for (uint32_t i = 0; i < blocks.size(); i++) {
			allocation = blocks[i]->alloc(size, alignment, bufferImageGranularity, type);
		}
		if (!allocation) {
			blocks.push_back(new DeviceMemoryBlock(std::max(static_cast<uint32_t>(DEVICE_ALLOCATION_BLOCK_SIZE), size), memoryTypeIndex, blocks.size()));
			allocation = blocks[blocks.size()-1]->alloc(size, alignment, bufferImageGranularity, type);
		}
		assert(allocation);
		return allocation;
	}

	void DeviceMemoryPool::free(DeviceAllocation* allocation) {
		blocks[allocation->blockIndex]->free(allocation);
	}

	DeviceMemoryAllocator::DeviceMemoryAllocator() : memoryPools{}, totalAllocatedBytes{ 0 }, totalAllocations{ 0 } {
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
		memoryPools.reserve(memProps.memoryTypeCount);
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
			memoryPools.push_back(DeviceMemoryPool(i));
		}
		bufferImageGranularity = deviceProperties.limits.bufferImageGranularity;
	}

	DeviceMemoryAllocator::~DeviceMemoryAllocator() {
		memoryPools.clear();
	}

	DeviceBuffer DeviceMemoryAllocator::alloc_buffer(uint32_t size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memProps) {
		VkBuffer buffer;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usageFlags;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;
		bufferInfo.flags = 0;
		VKU_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "Failed to create buffer!");

		VkMemoryRequirements memReq;
		vkGetBufferMemoryRequirements(device, buffer, &memReq);

		DeviceAllocation* memory = alloc(memReq, memProps, DEVICE_ALLOCATION_TYPE_BUFFER);

		VKU_CHECK_RESULT(vkBindBufferMemory(device, buffer, memory->memory->get_memory(), memory->offset), "Failed to bind buffer memory!");
		
		return DeviceBuffer{ memory, buffer };
	}

	DeviceAllocation* DeviceMemoryAllocator::alloc(VkMemoryRequirements memReq, VkMemoryPropertyFlags propertyFlags, DeviceAllocationType type) {
		return alloc(memReq.size, memReq.alignment, find_memory_type(memReq.memoryTypeBits, propertyFlags), type);
	}

	DeviceAllocation* DeviceMemoryAllocator::alloc(uint32_t size, uint32_t alignment, uint32_t memoryType, DeviceAllocationType type) {
		DeviceMemoryPool& pool = memoryPools[memoryType];
		DeviceAllocation* allocation = pool.alloc(size, alignment, bufferImageGranularity, type);
		totalAllocations++;
		totalAllocatedBytes += allocation->size;
		return allocation;
	}

	void DeviceMemoryAllocator::free_buffer(DeviceBuffer& buffer) {
		//std::cout << "Free buffer! " << buffer.buffer << std::endl;
		vkDestroyBuffer(device, buffer.buffer, nullptr);
		free(buffer.allocation);
	}

	void DeviceMemoryAllocator::queue_free_buffer(uint32_t frame, DeviceBuffer buffer) {
		deletionQueues[frame].push_back(buffer);
	}
	void DeviceMemoryAllocator::free_old_data(uint32_t frame) {
		std::vector<DeviceBuffer>& queue = deletionQueues[frame];
		for (uint32_t i = 0; i < queue.size(); i++) {
			free_buffer(queue[i]);
		}
		queue.clear();
	}

	void DeviceMemoryAllocator::free(DeviceAllocation* allocation) {
		memoryPools[allocation->memoryTypeIndex].free(allocation);
		totalAllocations--;
		totalAllocatedBytes -= allocation->size;
	}

	uint64_t DeviceMemoryAllocator::total_allocated_memory() {
		return totalAllocatedBytes;
	}

	uint32_t DeviceMemoryAllocator::total_allocations() {
		return totalAllocations;
	}
}