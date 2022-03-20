#pragma once

#include <stdint.h>

namespace vku {
	class UniformSampler2D;
	class GraphicsPipeline;
	class ComputePipeline;
	class DescriptorSet;
	class Texture;
}

//Contains default resources built in to the engine (pipelines, textures, models, etc), anything necessary for the engine and not specific to a scene.
namespace resources {
	extern vku::UniformSampler2D* bilinearSampler;
	extern vku::UniformSampler2D* nearestSampler;
	extern vku::UniformSampler2D* maxSampler;
	extern vku::UniformSampler2D* minSampler;

	extern vku::GraphicsPipeline* uiPipeline;
	extern vku::GraphicsPipeline* uiLinePipeline;
	extern vku::GraphicsPipeline* textPipeline;
	extern vku::GraphicsPipeline* colorFullbrightPipeline;
	extern vku::GraphicsPipeline* depthPipeline;
	extern vku::DescriptorSet* shaderSet;
	extern vku::DescriptorSet* worldViewProjectionSet;
	extern vku::DescriptorSet* uiSet;

	extern vku::Texture* duckTex;
	extern uint16_t missingTexIdx;
	extern uint16_t whiteTexIdx;
	extern uint16_t duckTexIdx;
}