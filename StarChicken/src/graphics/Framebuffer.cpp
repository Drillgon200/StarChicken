#include "Framebuffer.h"
#include "VkUtil.h"
#include "TextureManager.h"
#include <assert.h>

namespace vku {
	void Framebuffer::create(VkCommandBuffer cmdBuf) {
		assert(fboExtent.width != 0 && fboExtent.height != 0);
		assert(renderPass != nullptr);
		std::vector<VkImageView> imageViews{};
		if (!colorImageView) {
			for (uint32_t i = 0; i < colorTypes.size(); i++) {
				Texture* colorTexture = new Texture();
				uint32_t usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
				if (colorTypes[i] == TEXTURE_TYPE_ID_BUFFER) {
					usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				}
				usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				colorTexture->create(cmdBuf, fboExtent.width, fboExtent.height, 1, nullptr, colorTypes[i], usage);
				imageViews.push_back(colorTexture->get_image_view());
				colorTextures.push_back(colorTexture);
			}
		} else {
			imageViews.push_back(*colorImageView);
		}
		if (hasDepthBuffer) {
			if (depthTexture == nullptr) {
				depthTexture = new Texture();
				depthTexture->create(cmdBuf, fboExtent.width, fboExtent.height, 1, nullptr, depthType, VK_IMAGE_USAGE_SAMPLED_BIT);
			}
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
		fboInfo.flags = 0;

		VKU_CHECK_RESULT(vkCreateFramebuffer(device, &fboInfo, nullptr, &fbo), "Failed to create framebuffer!");
	}

	void Framebuffer::destroy() {
		for (Texture* colorTex : colorTextures) {
			colorTex->destroy();
			delete colorTex;
		}
		colorTextures.clear();
		colorTypes.clear();
		if (depthTexture) {
			depthTexture->destroy();
			delete depthTexture;
			depthTexture = nullptr;
		}
		vkDestroyFramebuffer(device, fbo, nullptr);
	}

	VkFramebuffer Framebuffer::get_framebuffer() {
		return fbo;
	}
	VkExtent2D Framebuffer::get_extent() {
		return fboExtent;
	}
	VkClearValue* Framebuffer::get_clear_values() {
		return clearValues.data();
	}
	VkRenderPass Framebuffer::get_render_pass() {
		return *renderPass;
	}
}