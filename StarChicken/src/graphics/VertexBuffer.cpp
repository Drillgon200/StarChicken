#include "VertexBuffer.h"
#include "SingleBufferAllocator.h"
#include "VkUtil.h"

namespace vku {
	VertexFormatElement POS3F{ VK_FORMAT_R32G32B32_SFLOAT, 12 };
	VertexFormatElement TEX2F{ VK_FORMAT_R32G32_SFLOAT, 8 };
	VertexFormatElement NORM3F{ VK_FORMAT_R32G32B32_SFLOAT, 12 };
	VertexFormatElement NORM1010102{ VK_FORMAT_A2R10G10B10_SNORM_PACK32, 4 };
	VertexFormatElement TAN3F{ NORM3F };
	VertexFormatElement TAN1010102{ NORM1010102 };
	VertexFormatElement COLOR4UI8{ VK_FORMAT_R8G8B8A8_UNORM, 4 };

	//I cannot figure out how to allocate an array on the stack in a constructor call, so new it is and delete in the constructor.
	//Not good code design, but it matters less for this because all VertexFormats should be declared here, not dynamically.
	VertexFormat POSITION{ 1, new VertexFormatElement[]{POS3F} };
	VertexFormat POSITION_COLOR{ 2, new VertexFormatElement[]{POS3F, COLOR4UI8} };
	VertexFormat POSITION_TEX{ 2, new VertexFormatElement[]{POS3F, TEX2F} };
	VertexFormat POSITION_TEX_NORMAL{ 3, new VertexFormatElement[]{POS3F, TEX2F, NORM3F} };
	VertexFormat POSITION_TEX_NORMAL_TANGENT{ 4, new VertexFormatElement[] {POS3F, TEX2F, NORM3F, TAN3F} };

	VertexFormatElement::VertexFormatElement(VkFormat format, uint16_t size) :
		format{ format },
		size{ size }
	{
	}

	VertexFormat::VertexFormat(uint32_t count, VertexFormatElement* elems) {
		elementCount = count;
		attributeDescriptions = new VkVertexInputAttributeDescription[elementCount];
		uint32_t offset = 0;
		for (uint32_t i = 0; i < elementCount; i++) {
			attributeDescriptions[i].binding = 0;
			attributeDescriptions[i].format = elems[i].format;
			attributeDescriptions[i].location = i;
			attributeDescriptions[i].offset = offset;

			offset += elems[i].size;
		}

		bindingDesc.binding = 0;
		//Instance data will be dealt with separately by a compute shader using a different buffer
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bindingDesc.stride = offset;

		delete[] elems;
	}

	VertexFormat::~VertexFormat() {
		delete[] attributeDescriptions;
	}

	void VertexBuffer::upload_data(uint16_t vertCount, void* vertexData, uint16_t indexCount, void* indexData) {
		assert(!vertexAllocation);
		assert(!indexAllocation);
		if (!indexData) {
			indexCount = vertCount;
		}
		drawCount = indexCount;
		vertexAllocation = vertexBufferAllocator->alloc(vertCount * format.size_bytes());
		indexAllocation = indexBufferAllocator->alloc(indexCount * sizeof(uint16_t));
		DeviceBuffer vertexStaging = generalAllocator->alloc_buffer(vertCount * format.size_bytes(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		DeviceBuffer indexStaging = generalAllocator->alloc_buffer(indexCount * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		void* vertData = vertexStaging.allocation->map();
		void* idxData = indexStaging.allocation->map();
		memcpy(vertData, vertexData, static_cast<size_t>(vertCount) * format.size_bytes());
		if (indexData) {
			memcpy(idxData, indexData, indexCount * sizeof(uint16_t));
		} else {
			uint16_t* indices = reinterpret_cast<uint16_t*>(idxData);
			for (uint32_t i = 0; i < indexCount; i++) {
				indices[i] = i;
			}
		}
		VkBufferCopy copy{};
		copy.srcOffset = 0;
		copy.dstOffset = vertexAllocation->offset;
		copy.size = static_cast<VkDeviceSize>(vertCount) * format.size_bytes();
		vkCmdCopyBuffer(transferCommandBuffer, vertexStaging.buffer, vertexBufferAllocator->get_buffer(), 1, &copy);

		copy.srcOffset = 0;
		copy.dstOffset = indexAllocation->offset;
		copy.size = indexCount * sizeof(uint16_t);
		vkCmdCopyBuffer(transferCommandBuffer, indexStaging.buffer, indexBufferAllocator->get_buffer(), 1, &copy);

		transferStagingBuffersToFree.push_back(vertexStaging);
		transferStagingBuffersToFree.push_back(indexStaging);
	}

	void VertexBuffer::free_data() {
		vertexBufferAllocator->free(vertexAllocation);
		indexBufferAllocator->free(indexAllocation);
		vertexAllocation = nullptr;
		indexAllocation = nullptr;
	}

	void VertexBuffer::draw() {
		vkCmdDrawIndexed(vku::graphics_cmd_buf(), drawCount, 1, indexAllocation->offset, vertexAllocation->offset, 0);
	}

	VertexBuffer::VertexBuffer(VertexFormat& format) :
		format{ format },
		vertexAllocation{ nullptr },
		indexAllocation{ nullptr },
		available{ false },
		drawCount{ 0 }
	{
	}

}