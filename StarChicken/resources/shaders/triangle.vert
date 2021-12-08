#version 450 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_tex;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec3 in_tangent;

layout (set = 0, binding = 0) uniform MVPMatrices {
	mat4 model;
	mat4 view;
	mat4 projection;
} mvpMatrices;

layout (set = 0, binding = 3) uniform LightingData {
	vec3 lightPos;
	vec3 eyePos;
} lightingData;

layout (location = 0) out vec2 texCoord;
layout (location = 1) out vec3 worldPos;
layout (location = 2) out vec3 lightPos;
layout (location = 3) out vec3 eyePos;
layout (location = 4) out vec3 normal;

void main(){
	texCoord = in_tex;
	vec4 world = mvpMatrices.model * vec4(in_pos, 1);;
	worldPos = world.xyz;
	lightPos = lightingData.lightPos;
	eyePos = lightingData.eyePos;
	normal = in_normal;
	gl_Position = mvpMatrices.projection * mvpMatrices.view * world;
}