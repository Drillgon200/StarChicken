#include "DefaultResources.h"
#include "..\graphics\VkUtil.h"
#include "..\graphics\GraphicsPipeline.h"
#include "..\graphics\ShaderUniforms.h"
#include "..\graphics\TextureManager.h"

namespace resources {
	vku::UniformSampler2D* bilinearSampler{};
	vku::UniformSampler2D* nearestSampler{};
	vku::UniformSampler2D* maxSampler{};
	vku::UniformSampler2D* minSampler{};

	vku::GraphicsPipeline* uiPipeline{};
	vku::GraphicsPipeline* uiLinePipeline{};
	vku::GraphicsPipeline* textPipeline{};
	vku::GraphicsPipeline* colorFullbrightPipeline{};
	vku::GraphicsPipeline* depthPipeline{};
	vku::DescriptorSet* shaderSet{};
	vku::DescriptorSet* worldViewProjectionSet{};
	vku::DescriptorSet* uiSet{};

	vku::Texture* duckTex{};
	uint16_t missingTexIdx{};
	uint16_t whiteTexIdx{};
	uint16_t duckTexIdx{};
}