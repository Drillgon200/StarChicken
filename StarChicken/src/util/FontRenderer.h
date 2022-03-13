#pragma once
#include <unordered_map>
#include "../util/DrillMath.h"

namespace vku {
	class Texture;
	class GraphicsPipeline;
	class DescriptorSet;
}

namespace text {

	enum StringFlags {
		STRING_FLAG_MULTISAMPLE = 1,
		STRING_FLAG_SUBPIXEL = 2
	};

	extern vku::DescriptorSet* textDescSet;

	struct CharPair {
		char left;
		char right;

		inline bool operator==(const CharPair& other) const {
			return left == other.left && right == other.right;
		}
	};
	struct CharPairHash {
		inline size_t operator()(const CharPair& pair) const {
			return (pair.left << 8) | pair.right;
		}
	};
	struct GlyphData {
		vku::Texture* texture;
		uint32_t textureArrayIdx;
		float minX;
		float maxX;
	};

	void set_matrix(mat4f& mat);
	void init();
	void create_text_quad();

	void cleanup();

	class FontRenderer {
	private:
		std::unordered_map<CharPair, float, CharPairHash> kerning{};
		std::vector<char> orderedChars{};
		GlyphData glyphData[0xFF];
	public:
		FontRenderer();

		void draw_string(const char* str, float fontScale, float characterSpacing, float x, float y, float z, float r, float g, float b, float a, uint32_t flags);
		void draw_string(const char* str, float fontScale, float characterSpacing, float x, float y, float z, uint32_t flags);
		void render_cached_strings(vku::GraphicsPipeline& pipeline);

		void load(std::wstring fileName);
		void destroy();
	};
}