#pragma once
#include <vulkan/vulkan.h>
#include "Framebuffer.h"

namespace vku {
	class RenderPass {
	private:
		VkRenderPass renderPass;
	public:
		void create(VkFormat swapChainImageFormat);
		void destroy();
		VkRenderPass& get_pass();
		void begin_pass(VkCommandBuffer commandBuffer, Framebuffer& fbo);
		void end_pass(VkCommandBuffer commandBuffer);
	};
}
