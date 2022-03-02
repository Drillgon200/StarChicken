#version 460

layout (set = 2, binding = 0) uniform sampler bilinearSampler;
layout (set = 2, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec3 worldPos;
layout (location = 1) in vec3 worldNormal;
layout (location = 2) in vec3 worldTangent;
layout (location = 3) in vec2 texCoord;

layout (location = 0) out vec4 FragColor;

void main(){
	FragColor = vec4(texCoord, 1.0, 1.0);
}