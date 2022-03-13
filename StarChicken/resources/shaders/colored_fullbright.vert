#version 460

layout (location = 0) in vec3 in_pos;

layout (set = 0, binding = 0) uniform Matrices {
	mat4 viewProjectionMatrix;
} matrices;

void main(){
	gl_Position = matrices.viewProjectionMatrix * vec4(in_pos, 1);
}