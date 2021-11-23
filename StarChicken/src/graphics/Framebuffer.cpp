#include "Framebuffer.h"

namespace vku {
	void Framebuffer::create() {
		assert(fboExtent.width != 0 && fboExtent.height != 0);
		assert(renderPass != nullptr);
		VkImageView attachments[] = {
			*imageView
		};
		VkFramebufferCreateInfo fboInfo{};
		fboInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fboInfo.renderPass = *renderPass;
		fboInfo.attachmentCount = 1;
		fboInfo.pAttachments = attachments;
		fboInfo.width = fboExtent.width;
		fboInfo.height = fboExtent.height;
		fboInfo.layers = 1;

		VKU_CHECK_RESULT(vkCreateFramebuffer(vku::device, &fboInfo, nullptr, &fbo), "Failed to create framebuffer!");
	}

	void Framebuffer::destroy() {
		vkDestroyFramebuffer(vku::device, fbo, nullptr);
	}

	VkFramebuffer Framebuffer::get_framebuffer() {
		return fbo;
	}
	VkExtent2D Framebuffer::get_extent() {
		return fboExtent;
	}
	vec4f Framebuffer::get_clear_color() {
		return clearColor;
	}
	VkRenderPass Framebuffer::get_render_pass() {
		return *renderPass;
	}
}