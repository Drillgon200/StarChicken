#pragma once
#include "GraphicsPipeline.h"
#include "VertexFormats.h"
#include "DeviceMemoryAllocator.h"
#include "ShaderUniforms.h"
#define MAX_BOUND_DESCRIPTOR_SETS 4

namespace vku {
	struct TessellatorDrawCmd {
		GraphicsPipeline* pipeline;
		DescriptorSet* sets[MAX_BOUND_DESCRIPTOR_SETS];
		uint32_t start;
		uint32_t end;
		uint32_t vertOffset;
		VkPushConstantRange pushConstant;
		uint32_t pushConstantData[12];
	};
	class Tessellator {
		std::vector<TessellatorDrawCmd> drawCommands{};
		TessellatorDrawCmd currentDrawCommand{};
		DeviceBuffer vertexBuffer[NUM_FRAME_DATA];
		uint32_t maxVertexBytes;
		uint32_t maxIndices;
		uint32_t vertexOffset;

		uint32_t currentVertexOffset;
		uint32_t currentIndexOffset;

		void resize(uint32_t newIndexCount, uint32_t newVertexByteSize);
	public:
		Tessellator();
		~Tessellator();

		void start_drawing(GraphicsPipeline* pipeline, uint32_t setCount, DescriptorSet** sets, VkPushConstantRange pushConstantRange, void* pushConstantData);

		//Add the draw command to the queue
		void draw();

		//Actually submit the rendering command
		void render(VkCommandBuffer cmdBuf);

		template<typename Vertex>
		void put_vertex_data(Vertex* vertices, uint32_t vertCount, uint16_t* indices, uint32_t idxCount) {
			uint32_t vertOffset = currentVertexOffset;
			uint32_t idxOffset = currentIndexOffset * sizeof(uint16_t);
			currentVertexOffset += vertCount * sizeof(Vertex);
			currentIndexOffset += idxCount;
			resize(currentIndexOffset, currentVertexOffset);
			memcpy(reinterpret_cast<uint8_t*>(vertexBuffer[currentFrame].allocation->map()) + (vertOffset + vertexOffset), vertices, vertCount * sizeof(Vertex));
			memcpy(reinterpret_cast<uint8_t*>(vertexBuffer[currentFrame].allocation->map()) + idxOffset, indices, idxCount * sizeof(uint16_t));
		}
		template<typename Vertex>
		void put_vertex_data(Vertex* vertices, uint32_t vertCount) {
			uint16_t* indices = reinterpret_cast<uint16_t*>(_malloca(vertCount * sizeof(uint16_t)));
			for (uint32_t i = 0; i < vertCount; i++) {
				indices[i] = i;
			}
			put_vertex_data(vertices, vertCount, indices, vertCount);
			_freea(indices);
		}
	};
}