#version 460

layout (location = 0) flat in int passId;

layout (location = 0) out int ObjectId;

void main(){
	ObjectId = passId;
}