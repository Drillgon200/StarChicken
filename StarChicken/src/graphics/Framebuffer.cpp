#include "Framebuffer.h"
#include "TextureManager.h"
#include <assert.h>

namespace vku {
	void Framebuffer::create() {
		assert(fboExtent.width != 0 && fboExtent.height != 0);
		assert(renderPass != nullptr);
		std::vector<VkImageView> imageViews{};
		imageViews.push_back(*imageView);
		if (hasDepthBuffer) {
			depthTexture = new Texture();

			vkQueueWaitIdle(graphicsQueue);
			vkResetCommandBuffer(graphicsCommandBuffers[0], 0);
			begin_command_buffer(graphicsCommandBuffers[0]);

			depthTexture->create(graphicsCommandBuffers[0], fboExtent.width, fboExtent.height, 1, nullptr, TEXTURE_TYPE_DEPTH_ATTACHMENT);

			end_command_buffer(graphicsCommandBuffers[0]);

			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &graphicsCommandBuffers[0];
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(graphicsQueue);

			imageViews.push_back(depthTexture->get_image_view());
		}
		VkFramebufferCreateInfo fboInfo{};
		fboInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fboInfo.renderPass = *renderPass;
		fboInfo.attachmentCount = imageViews.size();
		fboInfo.pAttachments = imageViews.data();
		fboInfo.width = fboExtent.width;
		fboInfo.height = fboExtent.height;
		fboInfo.layers = 1;

		VKU_CHECK_RESULT(vkCreateFramebuffer(device, &fboInfo, nullptr, &fbo), "Failed to create framebuffer!");
	}

	void Framebuffer::destroy() {
		if (depthTexture) {
			depthTexture->destroy();
			delete depthTexture;
		}
		vkDestroyFramebuffer(device, fbo, nullptr);
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