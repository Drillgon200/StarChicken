#pragma once

#include <string>
#include <vulkan/vulkan.h>

namespace vku {
	class RenderPass;
	class VertexFormat;
	class DescriptorSet;

	class GraphicsPipeline {
	private:
		std::string shaderName;
		RenderPass* renderPass;
		VertexFormat* vertexFormat;
		DescriptorSet* descriptorSet;

		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	public:
		GraphicsPipeline();

		GraphicsPipeline& name(const char* name) {
			shaderName = name;
			return *this;
		}

		GraphicsPipeline& pass(RenderPass& pass) {
			renderPass = &pass;
			return *this;
		}

		GraphicsPipeline& vertex_format(VertexFormat& format) {
			vertexFormat = &format;
			return *this;
		}

		GraphicsPipeline& descriptor_set(DescriptorSet& set) {
			descriptorSet = &set;
			return *this;
		}

		void build();
		void destroy();
		inline VkPipelineLayout get_layout() {
			return pipelineLayout;
		}
		void bind(VkCommandBuffer commandBuffer);
	};
}
