#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <array>
#include "DeviceMemoryAllocator.h"

namespace vku {

	class SingleBufferAllocation;

	struct VertexFormatElement {
		VkFormat format;
		uint16_t size;

		VertexFormatElement(VkFormat format, uint16_t size);
	};

	class VertexFormat {
	private:
		uint32_t elementCount;
		VkVertexInputBindingDescription bindingDesc;
		VkVertexInputAttributeDescription* attributeDescriptions;
	public:
		VertexFormat(uint32_t count, VertexFormatElement* elems);

		~VertexFormat();

		inline uint32_t get_element_count() {
			return elementCount;
		}

		inline uint32_t size_bytes() {
			return bindingDesc.stride;
		}

		inline VkVertexInputBindingDescription& get_binding_description() {
			return bindingDesc;
		}

		inline VkVertexInputAttributeDescription* get_attribute_descriptions() {
			return attributeDescriptions;
		}
	};

	extern VertexFormat POSITION;
	extern VertexFormat POSITION_COLOR;
	extern VertexFormat POSITION_TEX;
	extern VertexFormat POSITION_TEX_NORMAL;
	extern VertexFormat POSITION_TEX_NORMAL_TANGENT;

	class VertexBuffer {
	private:

		VertexFormat& format;
		SingleBufferAllocation* vertexAllocation;
		SingleBufferAllocation* indexAllocation;
		uint16_t drawCount;
		bool available;
	public:
		VertexBuffer(VertexFormat& format);

		void upload_data(uint16_t vertCount, void* vertexData, uint16_t indexCount = -1, void* indexData = nullptr);
		void free_data();

		void draw();
	};
}
