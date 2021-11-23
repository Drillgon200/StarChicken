#pragma once
#include <string>
#include "RenderPass.h"

namespace vku {
	class GraphicsPipeline {
	private:
		std::string shaderName;
		RenderPass* renderPass;

		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	public:
		GraphicsPipeline() :
			shaderName{ "" },
			renderPass{ nullptr }
		{}

		GraphicsPipeline& name(const char* name) {
			shaderName = name;
			return *this;
		}

		GraphicsPipeline& pass(RenderPass& pass) {
			renderPass = &pass;
			return *this;
		}

		void build();
		void destroy();
		void bind(VkCommandBuffer commandBuffer);
	};
}
