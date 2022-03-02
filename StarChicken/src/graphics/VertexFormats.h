#pragma once

#include <stdint.h>
#include <vulkan/vulkan.h>
#include <array>
#include "DeviceMemoryAllocator.h"

namespace vku {

	struct VertexFormatElement {
		VkFormat format;
		uint16_t size;

		VertexFormatElement(VkFormat format, uint16_t size);
	};

	class VertexFormat {
	private:
		uint32_t elementCount;
		uint32_t instanceElementCount;
		VkVertexInputBindingDescription bindingDesc[2];
		VkVertexInputAttributeDescription* attributeDescriptions;
	public:
		VertexFormat(uint32_t count, VertexFormatElement* elems, uint32_t instanceCount = 0, VertexFormatElement* instanceElems = nullptr);

		~VertexFormat();

		inline uint32_t get_element_count() {
			return elementCount + instanceElementCount;
		}

		inline uint32_t size_bytes() {
			return bindingDesc[0].stride;
		}

		inline uint32_t instance_size_bytes() {
			return bindingDesc[1].stride;
		}

		inline VkVertexInputBindingDescription* get_binding_descriptions() {
			return bindingDesc;
		}

		inline uint32_t get_binding_desc_count() {
			uint32_t descCount = 0;
			if (instanceElementCount > 0) {
				descCount += 1;
			}
			if (elementCount > 0) {
				descCount += 1;
			}
			return descCount;
		}

		inline VkVertexInputAttributeDescription* get_attribute_descriptions() {
			return attributeDescriptions;
		}

	};

	extern VertexFormat POSITION;
	extern VertexFormat POSITION_COLOR;
	extern VertexFormat POSITION_TEX;
	extern VertexFormat POSITION_TEX_COLOR;
	extern VertexFormat POSITION_TEX_TEXIDX;
	extern VertexFormat POSITION_TEX_COLOR_TEXIDX_FLAGS;
	extern VertexFormat POSITION_TEX_NORMAL;
	extern VertexFormat POSITION_TEX_NORMAL_TANGENT;
	extern VertexFormat VERTEX_FORMAT_NONE;

	extern VertexFormat POSITION_TEX_INST_OFFSET_CHARWIDTH_TEXIDX;
}
