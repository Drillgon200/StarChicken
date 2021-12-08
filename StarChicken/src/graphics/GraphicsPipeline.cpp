#include <string>
#include <fstream>
#include <vector>
#include <vulkan/vulkan.h>
#include "VkUtil.h"
#include "GraphicsPipeline.h"
#include "RenderPass.h"
#include "VertexBuffer.h"
#include "ShaderUniforms.h"

namespace vku {
	std::vector<uint8_t> read_file_to_buffer(std::string& fileName) {
		std::ifstream file(fileName, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file!");
		}
		size_t fileSize = static_cast<size_t>(file.tellg());
		file.seekg(0);

		std::vector<uint8_t> buffer(fileSize);
		file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

		file.close();

		return buffer;
	}

	VkShaderModule createShaderModule(std::vector<uint8_t>& shaderData) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderData.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderData.data());

		VkShaderModule shaderModule;
		VKU_CHECK_RESULT(vkCreateShaderModule(vku::device, &createInfo, nullptr, &shaderModule), "Failed to create shader module!");

		return shaderModule;
	}

	GraphicsPipeline::GraphicsPipeline() :
		shaderName{ "" },
		renderPass{ nullptr },
		vertexFormat{ nullptr }
	{}

	void GraphicsPipeline::build() {
		assert(renderPass != nullptr);

		std::string fileLoc = "resources/shaders/" + shaderName + ".vspv";
		std::vector<uint8_t> vertShaderData = read_file_to_buffer(fileLoc);
		fileLoc = "resources/shaders/" + shaderName + ".fspv";
		std::vector<uint8_t> fragShaderData = read_file_to_buffer(fileLoc);

		VkShaderModule vertShaderModule = createShaderModule(vertShaderData);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderData);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo stages[] = { vertShaderStageInfo, fragShaderStageInfo };


		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		if (vertexFormat) {
			vertexInputInfo.vertexBindingDescriptionCount = 1;
			vertexInputInfo.pVertexBindingDescriptions = &vertexFormat->get_binding_description();
			vertexInputInfo.vertexAttributeDescriptionCount = vertexFormat->get_element_count();
			vertexInputInfo.pVertexAttributeDescriptions = vertexFormat->get_attribute_descriptions();
		} else {
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.pVertexBindingDescriptions = nullptr;
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
			vertexInputInfo.pVertexAttributeDescriptions = nullptr;
		}

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
		inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyInfo.primitiveRestartEnable = false;
		inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		//Irrelevant, this is a dynamic state
		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = static_cast<float>(vku::swapChainExtent.width);
		viewport.height = static_cast<float>(vku::swapChainExtent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;

		//Irrelevant, this is a dynamic state
		VkRect2D scissor{};
		scissor.extent = vku::swapChainExtent;
		scissor.offset = { 0, 0 };

		VkPipelineViewportStateCreateInfo viewportStateInfo{};
		viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateInfo.viewportCount = 1;
		viewportStateInfo.pViewports = &viewport;
		viewportStateInfo.scissorCount = 1;
		viewportStateInfo.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
		rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationInfo.depthClampEnable = VK_FALSE;
		rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
		rasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationInfo.lineWidth = 1.0F;
		rasterizationInfo.depthBiasEnable = VK_FALSE;
		rasterizationInfo.depthBiasConstantFactor = 0.0F;
		rasterizationInfo.depthBiasSlopeFactor = 0.0F;
		rasterizationInfo.depthBiasClamp = 0.0F;

		VkPipelineMultisampleStateCreateInfo multisampleInfo{};
		multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleInfo.sampleShadingEnable = VK_FALSE;
		multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleInfo.minSampleShading = 1.0F;
		multisampleInfo.pSampleMask = nullptr;
		multisampleInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleInfo.alphaToOneEnable = VK_FALSE;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.minDepthBounds = 0.0F;
		depthStencil.maxDepthBounds = 1.0F;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthTestEnable = VK_TRUE;
		//No stencil buffering right now.
		depthStencil.front = {};
		depthStencil.back = {};
		depthStencil.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo blendStateInfo{};
		blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blendStateInfo.logicOpEnable = VK_FALSE;
		blendStateInfo.logicOp = VK_LOGIC_OP_COPY;
		blendStateInfo.attachmentCount = 1;
		blendStateInfo.pAttachments = &colorBlendAttachment;
		blendStateInfo.blendConstants[0] = 0.0F;
		blendStateInfo.blendConstants[1] = 0.0F;
		blendStateInfo.blendConstants[2] = 0.0F;
		blendStateInfo.blendConstants[3] = 0.0F;

		std::array<VkDynamicState, 2> dynamicStates{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR 
		};

		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = dynamicStates.size();
		dynamicStateInfo.pDynamicStates = dynamicStates.data();

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
		if (descriptorSet == nullptr) {
			pipelineLayoutInfo.setLayoutCount = 0;
			pipelineLayoutInfo.pSetLayouts = nullptr;
		} else {
			pipelineLayoutInfo.setLayoutCount = 1;
			pipelineLayoutInfo.pSetLayouts = descriptorSet->get_layout();
		}
		

		VKU_CHECK_RESULT(vkCreatePipelineLayout(vku::device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout!");

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
		pipelineInfo.pViewportState = &viewportStateInfo;
		pipelineInfo.pRasterizationState = &rasterizationInfo;
		pipelineInfo.pMultisampleState = &multisampleInfo;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &blendStateInfo;
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass->get_pass();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		VKU_CHECK_RESULT(vkCreateGraphicsPipelines(vku::device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "Failed to create graphics pipeline!");

		vkDestroyShaderModule(vku::device, vertShaderModule, nullptr);
		vkDestroyShaderModule(vku::device, fragShaderModule, nullptr);
	}

	void GraphicsPipeline::destroy() {
		vkDestroyPipeline(vku::device, pipeline, nullptr);
		vkDestroyPipelineLayout(vku::device, pipelineLayout, nullptr);
	}

	void GraphicsPipeline::bind(VkCommandBuffer commandBuffer) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	}
}
