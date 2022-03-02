#include "VertexFormats.h"
#include "VkUtil.h"
#include "StagingManager.h"

namespace vku {
	VertexFormatElement POS3F{ VK_FORMAT_R32G32B32_SFLOAT, 12 };
	VertexFormatElement TEX2F{ VK_FORMAT_R32G32_SFLOAT, 8 };
	VertexFormatElement NORM3F{ VK_FORMAT_R32G32B32_SFLOAT, 12 };
	VertexFormatElement NORM1010102{ VK_FORMAT_A2R10G10B10_SNORM_PACK32, 4 };
	VertexFormatElement TAN3F{ NORM3F };
	VertexFormatElement TAN1010102{ NORM1010102 };
	VertexFormatElement COLOR4UI8{ VK_FORMAT_R8G8B8A8_UNORM, 4 };
	VertexFormatElement UINT32{ VK_FORMAT_R32_UINT, 4 };
	VertexFormatElement UINT16{ VK_FORMAT_R16_UINT, 2 };

	//I cannot figure out how to allocate an array on the stack in a constructor call, so new it is and delete in the constructor.
	//Not good code design, but it matters less for this because all VertexFormats should be declared here, not dynamically.
	VertexFormat POSITION{ 1, new VertexFormatElement[]{POS3F} };
	VertexFormat POSITION_COLOR{ 2, new VertexFormatElement[]{POS3F, COLOR4UI8} };
	VertexFormat POSITION_TEX{ 2, new VertexFormatElement[]{POS3F, TEX2F} };
	VertexFormat POSITION_TEX_COLOR{ 3, new VertexFormatElement[]{POS3F, TEX2F, COLOR4UI8} };
	VertexFormat POSITION_TEX_TEXIDX{ 3, new VertexFormatElement[]{POS3F, TEX2F, UINT32} };
	VertexFormat POSITION_TEX_COLOR_TEXIDX_FLAGS{ 5, new VertexFormatElement[]{POS3F, TEX2F, COLOR4UI8, UINT16, UINT16} };
	VertexFormat POSITION_TEX_NORMAL{ 3, new VertexFormatElement[]{POS3F, TEX2F, NORM3F} };
	VertexFormat POSITION_TEX_NORMAL_TANGENT{ 4, new VertexFormatElement[] {POS3F, TEX2F, NORM3F, TAN3F} };
	VertexFormat POSITION_TEX_INST_OFFSET_CHARWIDTH_TEXIDX{ 2, new VertexFormatElement[] { POS3F, TEX2F}, 3, new VertexFormatElement[] { TEX2F, TEX2F, UINT32 } };
	VertexFormat VERTEX_FORMAT_NONE{ 0, nullptr };

	VertexFormatElement::VertexFormatElement(VkFormat format, uint16_t size) :
		format{ format },
		size{ size }
	{
	}

	VertexFormat::VertexFormat(uint32_t count, VertexFormatElement* elems, uint32_t instanceCount, VertexFormatElement* instanceElems) {
		elementCount = count;
		instanceElementCount = instanceCount;
		attributeDescriptions = new VkVertexInputAttributeDescription[elementCount + instanceElementCount];
		uint32_t vOffset = 0;
		for (uint32_t i = 0; i < elementCount; i++) {
			attributeDescriptions[i].binding = 0;
			attributeDescriptions[i].format = elems[i].format;
			attributeDescriptions[i].location = i;
			attributeDescriptions[i].offset = vOffset;

			vOffset += elems[i].size;
		}
		uint32_t iOffset = 0;
		for (uint32_t i = elementCount; i < instanceElementCount + elementCount; i++) {
			attributeDescriptions[i].binding = 1;
			attributeDescriptions[i].format = instanceElems[i - elementCount].format;
			attributeDescriptions[i].location = i;
			attributeDescriptions[i].offset = iOffset;

			iOffset += instanceElems[i - elementCount].size;
		}

		bindingDesc[0].binding = 0;
		bindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bindingDesc[0].stride = vOffset;
		bindingDesc[1].binding = 1;
		bindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		bindingDesc[1].stride = iOffset;

		delete[] elems;
		delete[] instanceElems;
	}

	VertexFormat::~VertexFormat() {
		delete[] attributeDescriptions;
	}

}