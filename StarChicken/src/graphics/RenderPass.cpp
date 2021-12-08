#include "RenderPass.h"
#include "VkUtil.h"

namespace vku {
	void RenderPass::create(VkFormat swapChainImageFormat) {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = VK_FORMAT_D32_SFLOAT;
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentRef.attachment = 0;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttachmentRef.attachment = 1;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 2;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		VKU_CHECK_RESULT(vkCreateRenderPass(vku::device, &renderPassInfo, nullptr, &renderPass), "Failed to create render pass!");
	}

	void RenderPass::destroy() {
		vkDestroyRenderPass(vku::device, renderPass, nullptr);
	}

	VkRenderPass& RenderPass::get_pass() {
		return renderPass;
	}

	void RenderPass::begin_pass(VkCommandBuffer commandBuffer, Framebuffer& fbo) {
		VkRenderPassBeginInfo renderBeginInfo{};
		renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginInfo.renderPass = renderPass;
		renderBeginInfo.framebuffer = fbo.get_framebuffer();
		renderBeginInfo.renderArea.extent = fbo.get_extent();
		float* color = fbo.get_clear_color().components;
		VkClearValue clearValues[2];
		clearValues[0].color = { color[0], color[1], color[2], color[3] };
		clearValues[1].depthStencil = { 0.0f, 0 };
		renderBeginInfo.clearValueCount = 2;
		renderBeginInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(commandBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void RenderPass::end_pass(VkCommandBuffer commandBuffer) {
		vkCmdEndRenderPass(commandBuffer);
	}
}
