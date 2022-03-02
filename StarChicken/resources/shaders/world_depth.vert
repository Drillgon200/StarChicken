#version 460

struct Object {
	uint selectionFlags;
	uint visible;
};
layout (set = 0, binding = 0, std430) restrict readonly buffer Objects {
	Object object[];
} objects;
struct Mesh {
	uint vertexOffset;
	uint indexOffset;
	//If 0, this is not a skinned mesh. We need this because not all meshes have skinning data, and the offset won't be the same as the vertex offset.
	uint skinDataOffset;
	uint vertCount;
	//Local space bounding box
	float minX;
	float minY;
	float minZ;
	float maxX;
	float maxY;
	float maxZ;
};
layout (set = 0, binding = 1, std430) restrict readonly buffer Meshes {
	Mesh mesh[];
} meshes;
struct Model {
	uint meshId;
	uint skinMatricesOffset;
	uint skinVerticesOffset;
	uint objectId;
};
layout (set = 0, binding = 2, std430) restrict readonly buffer Models {
	Model model[];
} models;
layout (set = 0, binding = 3) restrict readonly buffer ObjectMatrices {
	mat4 mat[];
} objectMatrices;
/*struct CompactedGeoSet {
	uint modelIds[256];
};
layout (set = 0, binding = 3, std430) restrict readonly buffer CompactedSets {
	CompactedGeoSet sets[];
} compactedSets;*/
layout (set = 0, binding = 4, std430) restrict readonly buffer DispatchModelIds {
	uint ids[];
} dispatchModelIds;
struct GeometrySet {
	uint modelId[256];
	uint setModelOffset;
	uint setModelCount;
	uint indexOffset;
};
layout (set = 0, binding = 5, std430) restrict readonly buffer GeometrySets {
	//uint modelIds[];
	GeometrySet sets[];
} geoSets;
layout (set = 0, binding = 6, std430) restrict readonly buffer Geometry {
	float geo[];
} geometry;

layout (set = 1, binding = 0, std140) uniform GeoOffset {
	uint posOffset;
	uint texOffset;
	uint normOffset;
	uint tanOffset;
	uint skinDataOffset;
	uint indicesOffset;
	uint skinPosOffset;
	uint skinNormOffset;
	uint skinTanOffset;
	uint finalIndicesOffset;
} geoOffsets;

layout (set = 1, binding = 1, std140) uniform Camera {
	mat4 view;
	mat4 projection;
	//x, y, width, height
	vec4 viewport;
	uint flags;
} camera;

layout (push_constant, std140) uniform SetOffset {
	//Limit this to 4 bytes, someone in the vulkan discord said 4 bytes was recommended for AMD GPUs (otherwise the driver will do a uniform buffer behind your back)
	//(some time layer) Ok, well I read the AMD gpuopen optimization guide today and it said you're supposed to keep the root descriptor set side under 13 DWORDs.
	//A descriptor set takes 1 DWORD, a dynamic buffer takes 2 DWORDs (4 if you have robust buffer access enabled), and push constants cost 1 DWORD per 4 bytes used.
	//So it's probably not actually 4 bytes that's recommended, I'm just supposed to stay under the 13 DWORD total limit so data doesn't spill to memory.
	uint setId;
} setOffset;

vec3 read_geo_vec3(uint offset){
	return vec3(geometry.geo[offset], geometry.geo[offset + 1], geometry.geo[offset + 2]);
};

void main(){
	uint instanceId = (gl_VertexIndex >> 16) & 0xFFFF;
	uint vertId = gl_VertexIndex & 0xFFFF;
	
	uint modelId = dispatchModelIds.ids[instanceId + geoSets.sets[setOffset.setId].setModelOffset] & 0xFFFFFF;
	Model model = models.model[modelId];
	Mesh mesh = meshes.mesh[model.meshId];

	mat4 modelMatrix = objectMatrices.mat[modelId];

	uint posOffset;
	if(model.skinVerticesOffset > 0){
		posOffset = geoOffsets.skinPosOffset + model.skinVerticesOffset * 3;
	} else {
		posOffset = geoOffsets.posOffset + mesh.vertexOffset * 3;
	}

	vec3 in_pos = read_geo_vec3(posOffset + vertId * 3);
	vec3 worldPos = (modelMatrix * vec4(in_pos, 1)).xyz;
	gl_Position = camera.projection * camera.view * vec4(worldPos, 1);
}