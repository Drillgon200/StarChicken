#version 450 core

layout (set = 0, binding = 1) uniform sampler bilinearSampler;
layout (set = 0, binding = 2) uniform texture2D textures[];

layout (location = 0) in vec2 texCoord;
layout (location = 1) in vec3 worldPos;

layout (location = 2) in vec3 lightPos;
layout (location = 3) in vec3 eyePos;
layout (location = 4) in vec3 normal;

layout (location = 0) out vec4 FragColor;

void main(){
	//FragColor = texture(sampler2D(textures[0], bilinearSampler), texCoord);
	vec3 V = normalize(eyePos - worldPos);
	vec3 L = normalize(lightPos - worldPos);
	FragColor = vec4(1) * (dot(normal, L) * 0.5 + 0.5);
}
