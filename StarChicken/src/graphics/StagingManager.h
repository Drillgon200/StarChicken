#pragma once
#include "VkUtil.h"
#include "DeviceMemoryAllocator.h"

namespace vku {
	constexpr uint32_t MAX_STAGING_BUFFER_SIZE = 24 * 1024 * 1024;

	struct StagingBuffer {
		DeviceBuffer buffer;
		VkCommandBuffer commandBuffer;
		VkFence fence;
		uint32_t offset;
		bool submitted;
	};

	//Idea from VkDoom3. I like this approach much better than whatever I was doing with staging deferred delete lists.
	class StagingManager {
	private:
		StagingBuffer stagingBuffers[NUM_FRAME_DATA];
		VkCommandPool commandPool;
		VkQueue queue;
		uint32_t currentBuffer;
	public:
		void init(VkQueue queue, uint32_t queueFamilyIndex);
		void destroy();

		void* stage(uint32_t size, uint32_t alignment, VkCommandBuffer& commandBuffer, VkBuffer& buffer, uint32_t& bufferOffset);
		void flush();
		void wait_for_buffer(StagingBuffer& buffer);
		void wait_all();
	};
}