#version 460
#extension GL_EXT_nonuniform_qualifier : require

const uint VERTEX_FLAG_NO_CORNER_TOP_LEFT = 1 << 0;
const uint VERTEX_FLAG_NO_CORNER_TOP_RIGHT = 1 << 1;
const uint VERTEX_FLAG_NO_CORNER_BOTTOM_LEFT = 1 << 2;
const uint VERTEX_FLAG_NO_CORNER_BOTTOM_RIGHT = 1 << 3;
const uint VERTEX_FLAG_INVERSE = 1 << 4;
const uint VERTEX_FLAG_NO_DISCARD = 1 << 5;

const uint CORNER_INDICES[4] = uint[](VERTEX_FLAG_NO_CORNER_BOTTOM_LEFT, VERTEX_FLAG_NO_CORNER_TOP_LEFT, VERTEX_FLAG_NO_CORNER_BOTTOM_RIGHT, VERTEX_FLAG_NO_CORNER_TOP_RIGHT);

layout (set = 0, binding = 1) uniform sampler bilinearSampler;
layout (set = 0, binding = 2) uniform texture2D textures[];

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec2 texcoord;
layout (location = 1) in vec4 color;
layout (location = 2) flat in uint texidx;
layout (location = 3) flat in uint flags;

float box_distance(){
	const float radius = 0.2;
	vec2 sgn = sign(texcoord-vec2(0.5));
	uint idx = (uint(sgn.x * 0.5 + 1) << 1) | uint(sgn.y * 0.5 + 1);
	if((CORNER_INDICES[idx] & flags) > 0){
		return 0;
	}
	return length(max(abs(texcoord-vec2(0.5))-0.5+radius, 0.0)) - radius;
}

void main(){
	float dist = clamp(box_distance(), 0.0, 1.0);
	if(bool(flags & VERTEX_FLAG_INVERSE)){
		dist = 1.0 - dist;
	}
	if(!bool(flags & VERTEX_FLAG_NO_DISCARD) && box_distance() > 0){
		discard;
	}
	FragColor = texture(sampler2D(textures[texidx], bilinearSampler), texcoord) * color;
}