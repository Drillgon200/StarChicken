#version 460

layout (set = 0, binding = 1) uniform sampler bilinearSampler;
layout (set = 0, binding = 2) uniform texture2D textures[];

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec4 color;

void main(){
	FragColor = color;
}