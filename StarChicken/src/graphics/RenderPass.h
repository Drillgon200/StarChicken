#pragma once

#include <vulkan/vulkan.h>
#include "Framebuffer.h"

namespace vku {
	class RenderPass {
	private:
		VkRenderPass renderPass;
		VkSubpassDescription subpass;
	public:
		void create(uint32_t attachmentCount, uint32_t subpassCount, uint32_t dependencyCount, VkAttachmentDescription* attachments, VkSubpassDescription* subpassDesc, VkSubpassDependency* subpassDependencies);
		void destroy();
		VkRenderPass& get_pass();
		void begin_pass(VkCommandBuffer commandBuffer, Framebuffer& fbo);
		void end_pass(VkCommandBuffer commandBuffer);
		inline uint32_t get_color_attachment_count() {
			return subpass.colorAttachmentCount;
		}
	};
}
