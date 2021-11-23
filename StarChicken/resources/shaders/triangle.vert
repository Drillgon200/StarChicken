#version 450 core

vec4 vertices[3] = vec4[3](vec4(-0.5, -0.5, 0, 1), vec4(0.5, -0.5, 0, 1), vec4(0, 0.5, 0, 1));

void main(){
	gl_Position = vertices[gl_VertexIndex];
}