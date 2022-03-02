#pragma once

#include "VkUtil.h"
#include "../util/DrillMath.h"
#include "TextureManager.h"

namespace vku {
	class Framebuffer {
	private:
		VkFramebuffer fbo;
		VkImageView* colorImageView;
		TextureType depthType;
		std::vector<TextureType> colorTypes;
		std::vector<Texture*> colorTextures;
		Texture* depthTexture;
		VkExtent2D fboExtent;
		VkRenderPass* renderPass;
		std::vector<VkClearValue> clearValues;
		float clearDepth;
		bool hasDepthBuffer;
	public:
		Framebuffer() :
			clearValues{},
			clearDepth{ 0.0F },
			hasDepthBuffer{ false },
			colorImageView{ nullptr },
			colorTypes{},
			colorTextures{},
			depthType{ TEXTURE_TYPE_DEPTH_ATTACHMENT },
			depthTexture{ nullptr },
			fboExtent{ 0, 0 },
			renderPass{ nullptr },
			fbo{ VK_NULL_HANDLE }
		{
		}

		inline Framebuffer& render_pass(VkRenderPass& renderPass) {
			this->renderPass = &renderPass;
			return *this;
		}

		inline Framebuffer& size(VkExtent2D ext) {
			fboExtent = ext;
			return *this;
		}

		inline Framebuffer& custom_image(VkImageView& imageView) {
			this->colorImageView = &imageView;
			return *this;
		}

		inline Framebuffer& add_color_attachment(TextureType type) {
			colorTypes.push_back(type);
			return *this;
		}

		inline Texture* get_color_attachment(uint32_t index) {
			return colorTextures[index];
		}

		inline Framebuffer& clear_color_float(vec4f clearColor) {
			VkClearValue value;
			value.color.float32[0] = clearColor.r;
			value.color.float32[1] = clearColor.g;
			value.color.float32[2] = clearColor.b;
			value.color.float32[3] = clearColor.a;
			clearValues.push_back(value);
			return *this;
		}

		inline Framebuffer& clear_color_int(vec4i32 clearColor) {
			VkClearValue value;
			value.color.int32[0] = clearColor.r;
			value.color.int32[1] = clearColor.g;
			value.color.int32[2] = clearColor.b;
			value.color.int32[3] = clearColor.a;
			clearValues.push_back(value);
			return *this;
		}

		inline Framebuffer& clear_depth(float clearDepth, uint32_t clearStencil = 0) {
			VkClearValue value;
			value.depthStencil.depth = clearDepth;
			value.depthStencil.stencil = clearStencil;
			clearValues.push_back(value);
			return *this;
		}
		inline uint32_t get_attachment_count() {
			return colorTextures.size() + (hasDepthBuffer ? 1 : 0);
		}

		inline Framebuffer& depth(bool depth) {
			hasDepthBuffer = depth;
			return *this;
		}

		inline Framebuffer& custom_depth(vku::Texture* tex) {
			hasDepthBuffer = true;
			depthTexture = tex;
			return *this;
		}

		inline Texture* get_depth_texture() {
			return depthTexture;
		}

		void create(VkCommandBuffer cmdBuf);
		void destroy();
		VkFramebuffer get_framebuffer();
		VkExtent2D get_extent();
		VkClearValue* get_clear_values();
		VkRenderPass get_render_pass();
	};
}