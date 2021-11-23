#pragma once

#include "VkUtil.h"
#include "../util/DrillMath.h"

namespace vku {
	class Framebuffer {
	private:
		VkFramebuffer fbo;
		VkImageView* imageView;
		VkExtent2D fboExtent;
		VkRenderPass* renderPass;
		vec4f clearColor;
		bool hasDepthBuffer;
	public:
		Framebuffer() :
			clearColor{ 0.0F },
			hasDepthBuffer{ false },
			imageView{ nullptr },
			fboExtent{ 0, 0 },
			renderPass{ nullptr }
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
			this->imageView = &imageView;
			return *this;
		}

		inline Framebuffer& clear_color(vec4f clearColor) {
			this->clearColor = clearColor;
			return *this;
		}

		inline Framebuffer& depth(bool depth) {
			hasDepthBuffer = depth;
			return *this;
		}

		void create();
		void destroy();
		VkFramebuffer get_framebuffer();
		VkExtent2D get_extent();
		vec4f get_clear_color();
		VkRenderPass get_render_pass();
	};
}