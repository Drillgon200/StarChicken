#version 460

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec4 in_color;

layout (set = 0, binding = 0) uniform MVPMatrices {
	mat4 model;
	mat4 view;
	mat4 projection;
} mvpMatrices;

layout (location = 0) out vec4 color;

void main(){
	color = in_color;
	gl_Position = mvpMatrices.projection * mvpMatrices.view * mvpMatrices.projection * vec4(in_pos, 1);
}