#include "SingleBufferAllocator.h"

namespace vku {

	std::vector<PendingDeallocation> SingleBufferAllocator::deallocs;

	SingleBufferAllocation::SingleBufferAllocation(SingleBufferAllocator& alloc, uint32_t offset, uint32_t size) :
		allocator{ alloc },
		offset{ offset },
		size{ size }
	{
	}

	SingleBufferAllocation::~SingleBufferAllocation() {
		allocator.free(this);
	}

	void* SingleBufferAllocation::map() {
		return reinterpret_cast<char*>(allocator.map()) + offset;
	}

	void SingleBufferAllocator::check_resize_buffer(uint32_t newSize) {
		if (newSize >= currentSize) {
			uint32_t prevSize = currentSize;
			if (currentSize > 0) {
				while (currentSize < newSize) {
					currentSize *= 2;
				}
			} else {
				currentSize = newSize;
			}
			VkBuffer newBuffer;
			VkDeviceMemory newMemory;
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			bufferInfo.size = currentSize;
			bufferInfo.usage = bufferUsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			VKU_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &newBuffer), "Failed to create buffer in single buffer allocator!");

			VkMemoryRequirements memReq{};
			vkGetBufferMemoryRequirements(device, newBuffer, &memReq);

			VkMemoryAllocateInfo alloc{};
			alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits, memoryPropertyFlags);
			alloc.allocationSize = memReq.size;
			VKU_CHECK_RESULT(vkAllocateMemory(device, &alloc, nullptr, &newMemory), std::string("Failed to allocate memory in single buffer allocator! Size: ") + std::to_string(currentSize));

			VKU_CHECK_RESULT(vkBindBufferMemory(device, newBuffer, newMemory, 0), "Failed to bind buffer memory in single buffer allocator!");

			if (prevSize > 0) {
				//Barrier to make sure all writes to the previous buffer are complete before we copy it over to the new one
				VkBufferMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				barrier.buffer = buffer;
				barrier.offset = 0;
				barrier.size = prevSize;
				barrier.srcQueueFamilyIndex = deviceQueueFamilyIndices.transferFamily.value();
				barrier.dstQueueFamilyIndex = deviceQueueFamilyIndices.transferFamily.value();
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				if (memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
					barrier.srcAccessMask |= VK_ACCESS_HOST_WRITE_BIT;
				}
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
				vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

				unmap();
				VkBufferCopy cpy{};
				cpy.srcOffset = 0;
				cpy.dstOffset = 0;
				cpy.size = prevSize;
				vkCmdCopyBuffer(transferCommandBuffer, buffer, newBuffer, 1, &cpy);
				deallocs.push_back({ device, buffer, deviceMemory });
			}
			buffer = newBuffer;
			deviceMemory = newMemory;
		}
	}

	void SingleBufferAllocator::check_deallocate() {
		for (PendingDeallocation& dealloc : deallocs) {
			vkDestroyBuffer(dealloc.device, dealloc.buffer, nullptr);
			vkFreeMemory(dealloc.device, dealloc.memory, nullptr);
		}
		deallocs.clear();
	}

	SingleBufferAllocator::SingleBufferAllocator(VkDevice& device, uint32_t defaultSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) :
		buffer{},
		deviceMemory{},
		memoryMapping{ nullptr },
		device{ device },
		currentSize{ 0 },
		currentOffset{ 0 },
		bufferUsageFlags{ usage },
		memoryPropertyFlags{ props },
		allAllocations{}
	{
		assert(defaultSize > 0);
		check_resize_buffer(defaultSize);
	}

	SingleBufferAllocator::~SingleBufferAllocator() {
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, deviceMemory, nullptr);
	}

	void* SingleBufferAllocator::map() {
		if (!memoryMapping) {
			VKU_CHECK_RESULT(vkMapMemory(device, deviceMemory, 0, currentSize, 0, &memoryMapping), "Failed to map memory for single buffer allocator!");
		}
		return memoryMapping;
	}

	void SingleBufferAllocator::unmap() {
		if (memoryMapping) {
			vkUnmapMemory(device, deviceMemory);
		}
		memoryMapping = nullptr;
	}

	SingleBufferAllocation* SingleBufferAllocator::alloc(uint32_t size) {
		uint32_t newSize = currentOffset + size;
		check_resize_buffer(newSize);
		SingleBufferAllocation* allocation = new SingleBufferAllocation(*this, currentOffset, size);
		currentOffset += size;
		allAllocations.push_back(allocation);
		return allocation;
	}

	void SingleBufferAllocator::free(SingleBufferAllocation* allocation) {
		allAllocations.erase(std::find(allAllocations.begin(), allAllocations.end(), allocation));
	}

	void SingleBufferAllocator::reset() {
		for (SingleBufferAllocation* alloc : allAllocations) {
			delete alloc;
		}
		allAllocations.clear();
		currentOffset = 0;
	}

	VkBuffer SingleBufferAllocator::get_buffer() {
		return buffer;
	}

	VkDeviceMemory SingleBufferAllocator::get_memory() {
		return deviceMemory;
	}
}