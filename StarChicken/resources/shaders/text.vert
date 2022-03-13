#version 460

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_tex;

struct GlyphData {
	vec2 offset;
	float xStart;
	float xEnd;
	float cutoff;
	uint color;
	uint texId;
};
struct StringData {
	mat4 matrix;
	uint glyphOffset;
	uint flags;
};
layout (set = 0, binding = 2, std140) readonly buffer GlyphBuffer {
	GlyphData glyphs[];
} glyphData;
layout (set = 0, binding = 3, std140) readonly buffer StringBuffer {
	StringData strings[];
} stringData;

layout (location = 0) out vec2 texCoord;
layout (location = 1) out float cutoff;
layout (location = 2) out vec4 color;
layout (location = 3) flat out uint texIdx;
layout (location = 4) flat out uint flags;

void main() {
	uint glyphArrayOffset = stringData.strings[gl_DrawID].glyphOffset + gl_InstanceIndex;
	mat4 matrix = stringData.strings[gl_DrawID].matrix;

	vec2 glyphOffset = glyphData.glyphs[glyphArrayOffset].offset;
	float xStart = glyphData.glyphs[glyphArrayOffset].xStart;
	float glyphWidth = glyphData.glyphs[glyphArrayOffset].xEnd - xStart;
	cutoff = glyphData.glyphs[glyphArrayOffset].cutoff;
	flags = stringData.strings[gl_DrawID].flags;
	color = unpackUnorm4x8(glyphData.glyphs[glyphArrayOffset].color);
	
	texCoord = in_tex * vec2(glyphWidth, 1) + vec2(xStart, 0);
	texIdx = glyphData.glyphs[glyphArrayOffset].texId;
	gl_Position = matrix * vec4(in_pos.xy * vec2(glyphWidth, 1) + vec2(xStart, 0) + glyphOffset, in_pos.z, 1);
}