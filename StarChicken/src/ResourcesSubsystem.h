#pragma once

#include "Engine.h"
#include "RenderSubsystem.h"
#include "resources/DefaultResources.h"


namespace engine {
	class ResourcesSubsystem {
		//Streaming and stuff might go here eventually. Now it just loads some default resources into global variables.
	public:

		void load_uniforms() {
			resources::bilinearSampler = new vku::UniformSampler2D(true, true, true, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
			resources::nearestSampler = new vku::UniformSampler2D(false, true, true, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
			resources::maxSampler = new vku::UniformSampler2D(true, false, false, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_SAMPLER_REDUCTION_MODE_MAX);
			resources::minSampler = new vku::UniformSampler2D(true, false, false, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_SAMPLER_REDUCTION_MODE_MIN);
		}
		void load_textures() {
			resources::missingTexIdx = rendering.textureArray->add_texture(vku::missingTexture);
			resources::whiteTexIdx = rendering.textureArray->add_texture(vku::whiteTexture);
			resources::duckTex = vku::load_texture(L"orbus.dtf", 0);
			resources::duckTexIdx = rendering.textureArray->add_texture(resources::duckTex);
		}
		void load_descriptor_sets() {
			resources::shaderSet = vku::create_descriptor_set({ resources::bilinearSampler, engine::rendering.textureArray });
			resources::worldViewProjectionSet = vku::create_descriptor_set({ engine::rendering.worldViewProjectionBuffer });
			resources::uiSet = vku::create_descriptor_set({ engine::rendering.uiProjectionMatrixBuffer, resources::bilinearSampler, engine::rendering.textureArray });
		}
		void load_pipelines() {
			resources::uiPipeline = (new vku::GraphicsPipeline())->name("ui").pass(engine::rendering.uiRenderPass, 0).vertex_format(vku::POSITION_TEX_COLOR_TEXIDX_FLAGS).descriptor_set(*resources::uiSet).build();
			resources::uiLinePipeline = (new vku::GraphicsPipeline())->name("ui_line").pass(engine::rendering.uiRenderPass, 0).vertex_format(vku::POSITION_COLOR).descriptor_set(*resources::uiSet).build();
			resources::textPipeline = (new vku::GraphicsPipeline())->name("text").pass(engine::rendering.uiRenderPass, 0).vertex_format(vku::POSITION_TEX).descriptor_set(*text::textDescSet).build();
			resources::colorFullbrightPipeline = (new vku::GraphicsPipeline())->name("colored_fullbright").pass(engine::rendering.mainRenderPass, 0).vertex_format(vku::POSITION).descriptor_set(*resources::worldViewProjectionSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4 * sizeof(float) + sizeof(int32_t) }).build();
			resources::depthPipeline = (new vku::GraphicsPipeline())->name("depth").pass(engine::rendering.depthPreRenderPass, 0).vertex_format(vku::POSITION).descriptor_set(*resources::worldViewProjectionSet).build();
		}
		void free_resources() {
			delete resources::duckTex;

			delete resources::bilinearSampler;
			delete resources::nearestSampler;
			delete resources::maxSampler;
			delete resources::minSampler;

			delete resources::uiPipeline;
			delete resources::uiLinePipeline;
			delete resources::textPipeline;
			delete resources::colorFullbrightPipeline;
			delete resources::depthPipeline;
		}
	};
	
}