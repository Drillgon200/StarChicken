#include "Tessellator.h"

namespace vku {

	Tessellator::Tessellator() {
		//Position tex color is completely arbitrary, vertex format doesn't matter. I just think I'll be using that one more than the others.
		maxVertexBytes = 0;
		vertexOffset = maxIndices * sizeof(uint16_t);
		maxIndices = 0;
		resize(maxIndices * sizeof(uint16_t), 128 * POSITION_TEX_COLOR.size_bytes());
		currentVertexOffset = 0;
		currentIndexOffset = 0;
	}

	Tessellator::~Tessellator() {
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			generalAllocator->free_buffer(vertexBuffer[i]);
		}
	}

	void Tessellator::resize(uint32_t newIndexCount, uint32_t newVertexByteSize) {
		if (newIndexCount <= maxIndices && newVertexByteSize <= maxVertexBytes) {
			return;
		} else if (maxIndices == 0) {
			maxIndices = newIndexCount;
			maxVertexBytes = newVertexByteSize;
		} else {
			while (maxVertexBytes < newVertexByteSize) {
				maxVertexBytes = maxVertexBytes + maxVertexBytes / 2;
			}
			while (maxIndices < newIndexCount) {
				maxIndices = maxIndices + maxIndices / 2;
			}
		}
		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			if (maxIndices != 0) {
				generalAllocator->queue_free_buffer(currentFrame, vertexBuffer[i]);
			}
			vertexBuffer[i] = generalAllocator->alloc_buffer(maxIndices * sizeof(uint16_t) + maxVertexBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
	}

	void Tessellator::start_drawing(GraphicsPipeline* pipeline, uint32_t setCount, DescriptorSet** sets, VkPushConstantRange pushConstantRange, void* pushConstantData) {
		if (currentDrawCommand.pipeline) {
			throw std::runtime_error("Illegal state: already drawing!");
		}
		if (setCount > 4) {
			throw std::runtime_error("Don't use more than 4 descriptor sets!");
		}
		currentDrawCommand.pipeline = pipeline;
		currentDrawCommand.start = currentIndexOffset;
		currentDrawCommand.vertOffset = currentVertexOffset;
		for (uint32_t i = 0; i < MAX_BOUND_DESCRIPTOR_SETS; i++) {
			if (i < setCount) {
				currentDrawCommand.sets[i] = sets[i];
			} else {
				currentDrawCommand.sets[i] = nullptr;
			}
		}
		if (pushConstantData) {
			if (pushConstantRange.size > (12 * sizeof(uint32_t))) {
				throw std::runtime_error("push constant too large! Max DWORD count for AMD should be less than 13, use a uniform buffer");
			}
			currentDrawCommand.pushConstant = pushConstantRange;
			memcpy(currentDrawCommand.pushConstantData, pushConstantData, pushConstantRange.size);
		} else {
			currentDrawCommand.pushConstant.size = 0;
		}
	}

	void Tessellator::draw() {
		if (currentDrawCommand.pipeline == nullptr) {
			throw std::runtime_error("Illegal state: not drawing!");
		}
		currentDrawCommand.end = currentIndexOffset;
		drawCommands.push_back(currentDrawCommand);
		currentDrawCommand.pipeline = nullptr;
	}

	//Actually submit the rendering command
	void Tessellator::render(VkCommandBuffer cmdBuf) {
		currentVertexOffset = 0;
		currentIndexOffset = 0;
		GraphicsPipeline* currentPipeline = nullptr;
		DescriptorSet* currentSets[MAX_BOUND_DESCRIPTOR_SETS]{};
		VkDeviceSize offset = vertexOffset;
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBuffer[currentFrame].buffer, &offset);
		vkCmdBindIndexBuffer(cmdBuf, vertexBuffer[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT16);
		for (TessellatorDrawCmd& cmd : drawCommands) {
			//If the pipeline isn't the same, bind it. I don't know if rebinding a pipeline has a significant cost, but might as well be safe since it's only a couple lines of code
			if (currentPipeline != cmd.pipeline) {
				currentPipeline = cmd.pipeline;
				currentPipeline->bind(cmdBuf);
			}
			//Same thing here.
			for (uint32_t i = 0; i < MAX_BOUND_DESCRIPTOR_SETS; i++) {
				if (cmd.sets[i] != nullptr && cmd.sets[i] != currentSets[i]) {
					currentSets[i] = cmd.sets[i];
					currentSets[i]->bind(cmdBuf, *currentPipeline, i);
				}
			}
			vkCmdPushConstants(cmdBuf, currentPipeline->get_layout(), cmd.pushConstant.stageFlags, cmd.pushConstant.offset, cmd.pushConstant.size, cmd.pushConstantData);

			vkCmdDrawIndexed(cmdBuf, cmd.end - cmd.start, 1, cmd.start * 2, cmd.vertOffset, 0);
		}
		drawCommands.clear();
	}
}