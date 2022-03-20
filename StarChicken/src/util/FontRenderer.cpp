#include "FontRenderer.h"
#include "../graphics/ShaderUniforms.h"
#include "../graphics/VertexFormats.h"
#include "../Engine.h"
#include "../RenderSubsystem.h"
#include "../util/Util.h"
#include "../graphics/StagingManager.h"
#include "../graphics/VkUtil.h"

namespace text {

	constexpr uint32_t MAX_STRING_RENDER_COUNT = 128;
	constexpr uint32_t MAX_CHAR_RENDER_COUNT = 1024;

#pragma pack(push, 1)
	struct TextVertex {
		vec3f pos;
		vec2f tex;
	};
	struct StringRenderData {
		mat4f matrix;
		uint32_t glyphArrayOffset;
		uint32_t flags;
		//Make sure tightly packed array is aligned to vec4
		uint8_t padding[8];
	};
	struct GlyphRenderData {
		vec2f offset;
		float xStart;
		float xEnd;
		float cutoff;
		uint32_t color;
		uint32_t texId;
		//Again, array stride is rounded up to vec4
		uint8_t padding[4];
	};
#pragma pack(pop)

	mat4f stringMatrix;

	VkDrawIndirectCommand indirectCommands[MAX_STRING_RENDER_COUNT];
	uint32_t stringRenderCount = 0;
	uint32_t charRenderCount = 0;

	vku::DescriptorSet* textDescSet;

	TextVertex textQuad[6] = {
		{{0, 0, 0}, {0, 0}},
		{{1, 1, 0}, {1, 1}},
		{{0, 1, 0}, {0, 1}},
		{{0, 0, 0}, {0, 0}},
		{{1, 0, 0}, {1, 0}},
		{{1, 1, 0}, {1, 1}},
	};

	vku::UniformSSBO<StringRenderData>* stringDataBuffer = nullptr;
	vku::UniformSSBO<GlyphRenderData>* textDataBuffer = nullptr;
	vku::DeviceBuffer vertexBuffer;
	vku::DeviceBuffer indirectDrawBuffer;

	void set_matrix(mat4f& mat) {
		stringMatrix.set(mat);
	}

	void init() {
		stringMatrix.set_identity();
		textDataBuffer = new vku::UniformSSBO<GlyphRenderData>(MAX_CHAR_RENDER_COUNT, true, VK_SHADER_STAGE_VERTEX_BIT);
		stringDataBuffer = new vku::UniformSSBO<StringRenderData>(MAX_STRING_RENDER_COUNT, true, VK_SHADER_STAGE_VERTEX_BIT);
		textDescSet = vku::create_descriptor_set({ resources::bilinearSampler, engine::rendering.textureArray, textDataBuffer, stringDataBuffer });

		indirectDrawBuffer = vku::generalAllocator->alloc_buffer(sizeof(VkDrawIndirectCommand) * MAX_STRING_RENDER_COUNT, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	void create_text_quad() {
		vertexBuffer = vku::generalAllocator->alloc_buffer(6 * sizeof(TextVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VkCommandBuffer stagingCmdBuf;
		VkBuffer stagingBuffer;
		uint32_t stagingOffset;
		void* data = vku::transferStagingManager->stage(6 * sizeof(TextVertex), 0, stagingCmdBuf, stagingBuffer, stagingOffset);
		memcpy(data, textQuad, 6 * sizeof(TextVertex));
		
		VkBufferCopy region{};
		region.srcOffset = stagingOffset;
		region.dstOffset = 0;
		region.size = 6 * sizeof(TextVertex);
		vkCmdCopyBuffer(stagingCmdBuf, stagingBuffer, vertexBuffer.buffer, 1, &region);
	}

	void cleanup() {
		vku::generalAllocator->free_buffer(vertexBuffer);
		vku::generalAllocator->free_buffer(indirectDrawBuffer);
		delete textDataBuffer;
		delete stringDataBuffer;
	}

	FontRenderer::FontRenderer() {
		for (uint32_t i = 0; i < 0xFF; i++) {
			glyphData[i] = GlyphData{ nullptr, 0, 0, 0 };
		}
	}

	FontRenderer::~FontRenderer() {
		destroy();
	}

	bool is_whitespace(char c) {
		return (c == ' ') || (c == '\t') || (c == '\n');
	}

	float count_whitespace(const char* str, uint32_t& index, uint32_t max, float spaceWidth) {
		float whitespace = 0.0F;
		while ((is_whitespace(str[index])) && (index < max)) {
			if (str[index] == ' ') {
				whitespace += spaceWidth;
			} else if (str[index] == '\t') {
				whitespace += spaceWidth * 2;
			} else if (str[index] == '\n') {
				return 0.0F;
			}
			++index;
		}
		return whitespace;
	}

	void FontRenderer::draw_string(const char* str, float fontScale, float characterSpacing, float x, float y, float z, float r, float g, float b, float a, uint32_t flags) {
		if (stringRenderCount >= MAX_STRING_RENDER_COUNT) {
			throw std::runtime_error("Too many rendered strings!");
		}
		uint32_t charCount = strlen(str);
		if (charCount == 0) {
			return;
		}
		if ((charCount + charRenderCount) > MAX_CHAR_RENDER_COUNT) {
			throw std::runtime_error("Too many rendered characters!");
		}

		StringRenderData strData{};
		strData.matrix.set_identity().translate_global({x, y, z}).scale(fontScale);
		stringMatrix.mul(strData.matrix, strData.matrix);
		strData.glyphArrayOffset = charRenderCount;
		strData.flags = flags;
		stringDataBuffer->update(strData, stringRenderCount);

		vec2f currentOffset{ 0, 0 };
		uint32_t instanceCount = 0;
		char prev = '\0';
		for (uint32_t i = 0; i < charCount; i++) {
			float whitespace = count_whitespace(str, i, charCount, characterSpacing * 4);
			char c = str[i];
			GlyphData charData = glyphData[c];
			/*if ((c == ' ') || (c == '\t')) {
				//currentOffset.x += characterSpacing * ((c == ' ') ? 4 : 16);
				if ((i > 0) && (!is_whitespace(str[i - 1]))) {
				}
				if ((i < (charCount - 1)) && (!is_whitespace(str[i + 1]))) {
					charData = glyphData[str[i - 1]];
					//currentOffset.x += (charData.maxX - charData.minX);
				}
				continue;
			}*/
			if (c == '\n') {
				currentOffset.x = 0;
				currentOffset.y += 1;
				prev = '\0';
				continue;
			}
			if ((i == 0) || (str[i-1] == '\n')) {
				currentOffset.x -= charData.minX;
			}
			if (prev != '\0') {
				currentOffset.x += (1-kerning[CharPair{ prev, c }]) + characterSpacing + whitespace;
			}
			
			GlyphRenderData glyphRenderData{};
			glyphRenderData.xStart = charData.minX;
			glyphRenderData.xEnd = charData.maxX;
			glyphRenderData.offset = currentOffset;
			glyphRenderData.cutoff = 0.5F;
			uint32_t color = static_cast<uint32_t>(r * 255.0F) | (static_cast<uint32_t>(g * 255.0F) << 8) | (static_cast<uint32_t>(b * 255.0F) << 16) | (static_cast<uint32_t>(a * 255.0F) << 24);
			glyphRenderData.color = color;
			glyphRenderData.texId = charData.textureArrayIdx;
			textDataBuffer->update(glyphRenderData, charRenderCount + instanceCount);
			++instanceCount;
			prev = c;
		}

		VkDrawIndirectCommand strDraw{};
		strDraw.firstVertex = 0;
		strDraw.firstInstance = 0;
		strDraw.vertexCount = 6;
		strDraw.instanceCount = instanceCount;
		indirectCommands[stringRenderCount] = strDraw;

		stringRenderCount += 1;
		charRenderCount += instanceCount;
	}

	void FontRenderer::draw_string(const char* str, float fontScale, float characterSpacing, float x, float y, float z, uint32_t flags) {
		draw_string(str, fontScale, characterSpacing, x, y, z, 0.5F, 0.5F, 0.5F, 1.0F, flags);
	}

	void FontRenderer::render_cached_strings(vku::GraphicsPipeline& pipeline) {
		if (stringRenderCount == 0) {
			return;
		}
		memcpy(indirectDrawBuffer.allocation->map(), indirectCommands, sizeof(VkDrawIndirectCommand) * stringRenderCount);
		pipeline.bind(vku::graphics_cmd_buf());
		textDescSet->bind(vku::graphics_cmd_buf(), pipeline, 0);
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(vku::graphics_cmd_buf(), 0, 1, &vertexBuffer.buffer, offsets);
		vkCmdDrawIndirect(vku::graphics_cmd_buf(), indirectDrawBuffer.buffer, 0, stringRenderCount, sizeof(VkDrawIndirectCommand));
		stringRenderCount = 0;
		charRenderCount = 0;
	}

	void FontRenderer::load(std::wstring fileName) {
		util::FileMapping mapping = util::map_file(L"resources/textures/" + fileName);
		uint32_t* data = reinterpret_cast<uint32_t*>(mapping.mapping);

		uint32_t width = data[0];
		uint32_t height = data[1];
		uint32_t imageSize = width * height;
		uint32_t numGlyphs = data[2];
		data += 3;
		kerning.reserve(numGlyphs*numGlyphs);
		
		orderedChars.reserve(numGlyphs);
		for (uint32_t i = 0; i < numGlyphs; i++) {
			char* pchar = reinterpret_cast<char*>(&data[0]);
			float* sizes = reinterpret_cast<float*>(&data[1]);
			uint16_t texArrayIdx = engine::rendering.textureArray->alloc_descriptor_slot();
			vku::Texture* tex = new vku::Texture();
			glyphData[*pchar] = GlyphData{ tex, texArrayIdx, sizes[0], sizes[1] };
			orderedChars.push_back(*pchar);
			data += 3;
		}
		for (uint32_t i = 0; i < numGlyphs*numGlyphs; i++) {
			char* pchar = reinterpret_cast<char*>(&data[0]);
			float* kern = reinterpret_cast<float*>(&data[1]);
			kerning[CharPair{ pchar[0], pchar[1] }] = *kern;
			data += 2;
		}
		for (uint32_t i = 0; i < numGlyphs; i++) {
			char glyph = orderedChars[i];
			GlyphData& gdata = glyphData[glyph];
			gdata.texture->create(vku::transferCommandBuffer, width, height, 1, data, vku::TEXTURE_TYPE_NORMAL, VK_IMAGE_USAGE_SAMPLED_BIT);
			engine::rendering.textureArray->update_texture(gdata.textureArrayIdx, gdata.texture);
			data += imageSize;
		}
	}

	void FontRenderer::destroy() {
		for (uint32_t i = 0; i < orderedChars.size(); i++) {
			char glyph = orderedChars[i];
			GlyphData& gdata = glyphData[glyph];
			delete gdata.texture;
			engine::rendering.textureArray->free_descriptor_slot(gdata.textureArrayIdx);
		}
		orderedChars.clear();
	}
}
