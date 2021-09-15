#version 450 core

layout (location = 0) in vec4 color;
layout (location = 1) in vec2 texCoord;

layout (binding = 1) uniform sampler2D texSampler;

layout (location = 0) out vec4 FragColor;

void main(){
	FragColor = texture(texSampler, texCoord);
}