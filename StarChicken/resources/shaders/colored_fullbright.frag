#version 460

layout (push_constant) uniform DrawData {
	vec4 color;
	int id;
} drawData;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out int OutId;

void main(){
	FragColor = drawData.color;
	OutId = drawData.id;
}