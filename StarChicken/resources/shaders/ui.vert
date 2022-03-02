#version 460

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_texcoord;
layout (location = 2) in vec4 in_color;
layout (location = 3) in uint in_texidx;
layout (location = 4) in uint in_flags;

layout (set = 0, binding = 0) uniform ProjectionMatrix {
	mat4 projection;
} projection;

layout (location = 0) out vec2 texcoord;
layout (location = 1) out vec4 color;
layout (location = 2) flat out uint texidx;
layout (location = 3) flat out uint flags;

void main(){
	texcoord = in_texcoord;
	color = in_color;
	texidx = in_texidx;
	flags = in_flags;
	gl_Position = projection.projection * vec4(in_pos, 1);
}