#version 450 core

layout (binding = 0) uniform UniformBufferObject {
	mat4 model;
	mat4 view;
	mat4 projection;
} ubo;

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec4 color;
layout (location = 1) out vec2 texCoord;

void main(){
	color = inColor;
	texCoord = inTexCoord;
	gl_Position = ubo.projection * ubo.view * ubo.model * vec4(inPosition, 1);
}