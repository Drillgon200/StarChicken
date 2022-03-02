#include "StagingManager.h"

namespace vku {
	void StagingManager::init(VkQueue queue, uint32_t queueFamilyIndex) {
		currentBuffer = 0;
		this->queue = queue;

		VkCommandPoolCreateInfo commandPoolInfo{};
		commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		commandPoolInfo.queueFamilyIndex = queueFamilyIndex;
		commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VKU_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool), "Failed to create command pool in staging manager init!");

		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			stagingBuffers[i].buffer = generalAllocator->alloc_buffer(MAX_STAGING_BUFFER_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			stagingBuffers[i].offset = 0;
			stagingBuffers[i].submitted = false;

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = 0;
			VKU_CHECK_RESULT(vkCreateFence(device, &fenceInfo, nullptr, &stagingBuffers[i].fence), "Failed to create fence in staging manager init!");

			VkCommandBufferAllocateInfo cmdBufInfo{};
			cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			cmdBufInfo.commandPool = commandPool;
			cmdBufInfo.commandBufferCount = 1;
			cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

			VKU_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufInfo, &stagingBuffers[i].commandBuffer), "Failed to allocate command buffer in staging manager init!");
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			VKU_CHECK_RESULT(vkBeginCommandBuffer(stagingBuffers[i].commandBuffer, &beginInfo), "Failed to begin command buffer in staging manager init!");
		}
	}

	void StagingManager::destroy() {
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			generalAllocator->free_buffer(stagingBuffers[i].buffer);
			vkDestroyFence(device, stagingBuffers[i].fence, nullptr);
		}
		vkDestroyCommandPool(device, commandPool, nullptr);
	}

	void* StagingManager::stage(uint32_t size, uint32_t alignment, VkCommandBuffer& commandBuffer, VkBuffer& buffer, uint32_t& bufferOffset) {
		if (size > MAX_STAGING_BUFFER_SIZE) {
			throw std::runtime_error("Attempted stage bigger than max staging buffer size!");
		}
		if (alignment == 0) {
			//0 signifies we don't care, so make it lowest
			alignment = 1;
		}
		StagingBuffer& stagingBuf = stagingBuffers[currentBuffer];
		uint32_t offset = (stagingBuf.offset + (alignment - 1)) & (~(alignment - 1));
		if (((offset + size) >= MAX_STAGING_BUFFER_SIZE) && !stagingBuf.submitted) {
			flush();
			stagingBuf = stagingBuffers[currentBuffer];
		} else {
			stagingBuf.offset = offset;
		}
		wait_for_buffer(stagingBuf);
		commandBuffer = stagingBuf.commandBuffer;
		buffer = stagingBuf.buffer.buffer;
		bufferOffset = stagingBuf.offset;
		void* data = reinterpret_cast<uint8_t*>(stagingBuf.buffer.allocation->map()) + stagingBuf.offset;
		stagingBuf.offset += size;
		return data;
	}
	void StagingManager::flush() {
		StagingBuffer& buf = stagingBuffers[currentBuffer];
		if (buf.submitted || (buf.offset == 0)) {
			return;
		}
		VkBufferMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;
		barrier.buffer = buf.buffer.buffer;
		barrier.offset = 0;
		barrier.size = buf.offset;
		barrier.srcQueueFamilyIndex = 0;
		barrier.dstQueueFamilyIndex = 0;
		vkCmdPipelineBarrier(buf.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

		VKU_CHECK_RESULT(vkEndCommandBuffer(buf.commandBuffer), "Failed to end command buffer for staging flush!");

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &buf.commandBuffer;
		submitInfo.signalSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = nullptr;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		VKU_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, buf.fence), "Failed to submit to queue in staging flush!");

		buf.submitted = true;

		currentBuffer = (currentBuffer + 1) % NUM_FRAME_DATA;
	}
	void StagingManager::wait_for_buffer(StagingBuffer& buffer) {
		if (!buffer.submitted) {
			return;
		}
		VKU_CHECK_RESULT(vkWaitForFences(device, 1, &buffer.fence, VK_TRUE, UINT64_MAX), "Failed to wait for fence in staging wait!");
		VKU_CHECK_RESULT(vkResetFences(device, 1, &buffer.fence), "Failed to reset fence in staging wait!");
		buffer.submitted = false;
		buffer.offset = 0;

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		VKU_CHECK_RESULT(vkBeginCommandBuffer(buffer.commandBuffer, &beginInfo), "Failed to begin command buffer after waiting for staging buffer!");
	}

	void StagingManager::wait_all() {
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			StagingBuffer& buffer = stagingBuffers[i];
			wait_for_buffer(buffer);
		}
	}
}