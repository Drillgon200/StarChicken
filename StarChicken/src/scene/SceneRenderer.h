#pragma once
#include "..\graphics\geometry\GeometryAllocator.h"
#include "vulkan/vulkan.h"

namespace vku {
	class Framebuffer;
	class Texture;
	class UniformTexture2D;
	class UniformStorageImage;
	class DescriptorSet;
	class GraphicsPipeline;
	class ComputePipeline;
}
namespace engine {
	class RenderSubsystem;
}
namespace scene {
	class Scene;

	//Keep this renderer separate from the main scene. I'd want to have an entirely different render path for stuff like mobile (VR) GPUs, so this allows for easier expansion to more render paths in the future.
	class SceneRenderer {
	private:
		friend class engine::RenderSubsystem;
		Scene* scene;
		vku::WorldGeometryManager geometryManager;
		vku::Framebuffer* framebuffer;
		vku::Framebuffer* depthFramebuffer;

		vku::Texture* depthPyramid;
		//This uniform is a static object, not recreated on resize
		vku::UniformTexture2D* depthPyramidUniform{ nullptr };
		vku::UniformTexture2D** depthPyramidReadUniforms;
		vku::UniformStorageImage** depthPyramidWriteUniforms;
		vku::DescriptorSet** depthPyramidDownsampleSets;
		vku::ComputePipeline* depthPyramidDownsample;
		uint32_t depthPyramidLevels;
	public:
		SceneRenderer(Scene* scene);
		void prepare_render_world(vku::RenderPass& renderPass);
		void render_world(Camera& cam, vku::WorldRenderPass pass);
		void generate_depth_pyramid(VkCommandBuffer cmdBuf);
		void framebuffer_resized(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY);
		void init(VkCommandBuffer cmdBuf, vku::RenderPass* renderPass, vku::RenderPass* depthPass, vku::RenderPass* objectIdPass, vku::Framebuffer* mainFramebuffer, vku::Framebuffer* mainDepthFramebuffer);
		void cleanup();
		inline vku::WorldGeometryManager& geo_manager() {
			return geometryManager;
		}
		inline vku::Framebuffer* get_framebuffer() {
			return framebuffer;
		}
	};
}