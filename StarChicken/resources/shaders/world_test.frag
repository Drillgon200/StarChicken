#version 460

layout (set = 2, binding = 0) uniform sampler bilinearSampler;
layout (set = 2, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec3 worldPos;
layout (location = 1) in vec3 worldNormal;
layout (location = 2) in vec3 worldTangent;
layout (location = 3) in vec2 texCoord;
layout (location = 4) flat in int objectId;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out int outId;

void main(){
	FragColor = vec4(vec3(dot(worldNormal, normalize(vec3(1, 1, 1))) * 0.5 + 0.5), 1.0);
	outId = objectId;
}