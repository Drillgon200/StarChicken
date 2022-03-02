#pragma once
#include "VkUtil.h"
#include "DeviceMemoryAllocator.h"

namespace vku {
	template<typename Vertex>
	class DynamicBuffer {
	private:
		DeviceBuffer buffers[NUM_FRAME_DATA];
		uint32_t currentSize;
		uint32_t indexOffset;
		uint16_t currentVertex;
		uint16_t currentIndex;
	public:
		void init(uint32_t startSize) {
			currentSize = startSize;
			currentVertex = 0;
			currentIndex = 0;
			for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
				buffers[i] = generalAllocator->alloc_buffer(startSize * (sizeof(Vertex) + sizeof(uint16_t)), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			}
			indexOffset = sizeof(Vertex) * startSize;
		}
		void cleanup() {
			for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
				generalAllocator->free_buffer(buffers[i]);
			}
		}

		void draw(VkCommandBuffer cmdBuf) {
			if (currentIndex == 0) {
				return;
			}
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &buffers[currentFrame].buffer, &offset);
			vkCmdBindIndexBuffer(cmdBuf, buffers[currentFrame].buffer, static_cast<VkDeviceSize>(indexOffset), VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(cmdBuf, currentIndex, 1, 0, 0, 0);
			currentVertex = 0;
			currentIndex = 0;
		}

		void resize(uint32_t newSize) {
			uint32_t newIdxOffset = sizeof(Vertex) * newSize;
			for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
				DeviceBuffer newBuffer = generalAllocator->alloc_buffer(newSize * (sizeof(Vertex) + sizeof(uint16_t)), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				memcpy(newBuffer.allocation->map(), buffers[i].allocation->map(), currentSize * sizeof(Vertex));
				memcpy(reinterpret_cast<uint8_t*>(newBuffer.allocation->map()) + newIdxOffset, reinterpret_cast<uint8_t*>(buffers[i].allocation->map()) + indexOffset, currentSize * sizeof(uint16_t));
				generalAllocator->queue_free_buffer(i, buffers[i]);
				buffers[i] = newBuffer;
			}
			indexOffset = newIdxOffset;
			currentSize = newSize;
		}

		void add_vertex(Vertex& vertex) {
			if ((currentIndex + 1) > currentSize) {
				resize(static_cast<uint32_t>(currentSize * 1.5));
			}
			uint8_t* mapping = reinterpret_cast<uint8_t*>(buffers[currentFrame].allocation->map());
			*(mapping + currentVertex * sizeof(Vertex)) = vertex;
			*(mapping + indexOffset + currentIndex * sizeof(uint16_t)) = currentIndex;
			++currentVertex;
			++currentIndex;
		}

		void add_vertices_and_indices(uint16_t vertexCount, Vertex* vertices, uint16_t indexCount, uint16_t* indices) {
			while ((currentIndex + indexCount) > currentSize) {
				resize(static_cast<uint32_t>(currentSize * 1.5));
			}
			uint8_t* mapping = reinterpret_cast<uint8_t*>(buffers[currentFrame].allocation->map());
			memcpy(mapping + currentVertex * sizeof(Vertex), vertices, vertexCount * sizeof(Vertex));
			for (uint32_t i = 0; i < indexCount; i++) {
				indices[i] += currentVertex;
			}
			memcpy(mapping + indexOffset + currentIndex * sizeof(uint16_t), indices, indexCount * sizeof(uint16_t));
			currentVertex += vertexCount;
			currentIndex += indexCount;

		}
	};
}