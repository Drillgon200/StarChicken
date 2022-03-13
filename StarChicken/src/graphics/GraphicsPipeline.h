#pragma once

#include <string>
#include <vulkan/vulkan.h>
#include <vector>

namespace vku {
	class RenderPass;
	class VertexFormat;
	class DescriptorSet;

	class ComputePipeline {
	private:
		std::string shaderName;
		std::vector<DescriptorSet*> descriptorSets{};
		VkPushConstantRange pushConstant{};

		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	public:
		ComputePipeline();

		inline ComputePipeline& name(const char* name) {
			shaderName = name;
			return *this;
		}

		inline ComputePipeline& descriptor_set(DescriptorSet& set) {
			descriptorSets.push_back(&set);
			return *this;
		}

		inline ComputePipeline& push_constant(VkPushConstantRange range) {
			pushConstant = range;
			return *this;
		}

		inline VkPipelineLayout get_layout() {
			return pipelineLayout;
		}

		void build();
		void destroy();
		void bind(VkCommandBuffer commandBuffer);
	};

	class GraphicsPipeline {
	private:
		std::string shaderName;
		RenderPass* renderPass;
		VkRenderPass* vkRenderPass;
		uint32_t subpass;
		VertexFormat* vertexFormat;
		std::vector<DescriptorSet*> descriptorSets;
		VkPushConstantRange pushConstant;
		VkCullModeFlagBits cullMode{ VK_CULL_MODE_NONE };
		VkBool32 depthWrites{ true };
		VkBool32 depthTest{ true };
		VkCompareOp depthCompareOp{ VK_COMPARE_OP_GREATER_OR_EQUAL };
		uint32_t colorWriteMask{ VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };
		VkBool32 blend{ false };
		VkBlendFactor srcBlend{ VK_BLEND_FACTOR_ONE };
		VkBlendFactor dstBlend{ VK_BLEND_FACTOR_ZERO };
		VkBlendOp blendOp{ VK_BLEND_OP_ADD };

		VkPipelineLayout pipelineLayout;
		VkPipeline pipeline;
	public:
		GraphicsPipeline();

		inline GraphicsPipeline& name(const char* name) {
			shaderName = name;
			return *this;
		}

		inline GraphicsPipeline& pass(RenderPass& pass, uint32_t subpass) {
			renderPass = &pass;
			this->subpass = subpass;
			return *this;
		}

		inline GraphicsPipeline& pass(VkRenderPass* pass) {
			vkRenderPass = pass;
			return *this;
		}

		inline GraphicsPipeline& vertex_format(VertexFormat& format) {
			vertexFormat = &format;
			return *this;
		}

		inline GraphicsPipeline& cull_mode(VkCullModeFlagBits mode) {
			cullMode = mode;
			return *this;
		}

		inline GraphicsPipeline& color_writes(uint32_t colorWrite) {
			colorWriteMask = colorWrite;
			return *this;
		}

		inline GraphicsPipeline& depth_writes(VkBool32 write) {
			depthWrites = write;
			return *this;
		}

		inline GraphicsPipeline& depth_test(VkBool32 test) {
			depthTest = test;
			return *this;
		}

		inline GraphicsPipeline& depth_compare(VkCompareOp compare) {
			depthCompareOp = compare;
			return *this;
		}

		inline GraphicsPipeline& enable_blend(VkBool32 blendEnable) {
			blend = blendEnable;
			return *this;
		}

		inline GraphicsPipeline& blend_func(VkBlendFactor src, VkBlendFactor dst, VkBlendOp op) {
			srcBlend = src;
			dstBlend = dst;
			blendOp = op;
			return *this;
		}

		inline GraphicsPipeline& descriptor_set(DescriptorSet& set) {
			descriptorSets.push_back(&set);
			return *this;
		}

		inline GraphicsPipeline& push_constant(VkPushConstantRange range) {
			pushConstant = range;
			return *this;
		}
		
		inline VkPipelineLayout get_layout() {
			return pipelineLayout;
		}

		void build();
		void destroy();
		
		void bind(VkCommandBuffer commandBuffer);
	};
}
