#include "RenderPass.h"
#include "VkUtil.h"

namespace vku {
	void RenderPass::create(uint32_t attachmentCount, uint32_t subpassCount, uint32_t dependencyCount, VkAttachmentDescription* attachments, VkSubpassDescription* subpassDesc, VkSubpassDependency* subpassDependencies) {
		subpass = *subpassDesc;
		if (subpass.colorAttachmentCount > 0) {
			VkAttachmentReference* colorRefs = new VkAttachmentReference[subpass.colorAttachmentCount];
			memcpy(colorRefs, subpass.pColorAttachments, subpass.colorAttachmentCount * sizeof(VkAttachmentReference));
			subpass.pColorAttachments = colorRefs;
		}
		if (subpass.pDepthStencilAttachment != nullptr) {
			VkAttachmentReference* depthRef = new VkAttachmentReference();
			*depthRef = *subpass.pDepthStencilAttachment;
			subpass.pDepthStencilAttachment = depthRef;
		}
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = attachmentCount;
		renderPassInfo.pAttachments = attachments;
		renderPassInfo.subpassCount = subpassCount;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = dependencyCount;
		renderPassInfo.pDependencies = subpassDependencies;
		

		VKU_CHECK_RESULT(vkCreateRenderPass(vku::device, &renderPassInfo, nullptr, &renderPass), "Failed to create render pass!");
	}

	void RenderPass::destroy() {
		if (subpass.colorAttachmentCount > 0) {
			delete[] subpass.pColorAttachments;
		}
		if (subpass.pDepthStencilAttachment) {
			delete subpass.pDepthStencilAttachment;
		}
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
		VkClearValue* clearValues = fbo.get_clear_values();
		renderBeginInfo.clearValueCount = fbo.get_attachment_count();
		renderBeginInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(commandBuffer, &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void RenderPass::end_pass(VkCommandBuffer commandBuffer) {
		vkCmdEndRenderPass(commandBuffer);
	}
}
