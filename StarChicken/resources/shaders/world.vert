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

layout (set = 1, binding = 1, std140) uniform VPMatrices {
	mat4 view;
	mat4 projection;
} vpMatrices;

layout (push_constant, std140) uniform SetOffset {
	//Limit this to 4 bytes, someone in the vulkan discord said 4 bytes was recommended for AMD GPUs (otherwise the driver will do a uniform buffer behind your back)
	uint setId;
} setOffset;


layout (location = 0) out vec3 worldPos;
layout (location = 1) out vec3 worldNormal;
layout (location = 2) out vec3 worldTangent;
layout (location = 3) out vec2 texCoord;


vec3 read_geo_vec3(uint offset){
	return vec3(geometry.geo[offset], geometry.geo[offset + 1], geometry.geo[offset + 2]);
};

vec2 read_geo_vec2(uint offset){
	return vec2(geometry.geo[offset], geometry.geo[offset + 1]);
};

void main(){
	uint instanceId = (gl_VertexIndex >> 16) & 0xFFFF;
	uint vertId = gl_VertexIndex & 0xFFFF;
	
	//First get the current geometry set from the push constant set id, then find out what model this is through the instance id packed into the index.
	//Next, look up the model with the model id and get the current mesh from that. The mesh contains offsets into the geometry buffer.
	//Finally, lookup the model matrix with the model id and the vertex attributes from the mesh offsets
	//That's a whole lot of memory read dependencies... might be an improvement to try to minimize this later.
	uint modelId = dispatchModelIds.ids[instanceId + geoSets.sets[setOffset.setId].setModelOffset];
	Model model = models.model[modelId];
	Mesh mesh = meshes.mesh[model.meshId];

	mat4 modelMatrix = objectMatrices.mat[modelId];
	uint posOffset;
	uint normOffset;
	uint tanOffset;
	//Position, normal, and tangent are skinned, so if this is a skinned mesh we'll read from the skinned vertices.
	if(model.skinVerticesOffset > 0){
		posOffset = geoOffsets.skinPosOffset + model.skinVerticesOffset * 3;
		normOffset = geoOffsets.skinNormOffset + model.skinVerticesOffset * 3;
		tanOffset = geoOffsets.skinTanOffset + model.skinVerticesOffset * 3;
	} else {
		posOffset = geoOffsets.posOffset + mesh.vertexOffset * 3;
		normOffset = geoOffsets.normOffset + mesh.vertexOffset * 3;
		tanOffset = geoOffsets.tanOffset + mesh.vertexOffset * 3;
	}
	vec3 in_pos = read_geo_vec3(posOffset + vertId * 3);
	vec3 in_normal = read_geo_vec3(normOffset + vertId * 3);
	vec3 in_tangent = read_geo_vec3(tanOffset + vertId * 3);
	vec2 in_texcoord = read_geo_vec2(geoOffsets.texOffset + (mesh.vertexOffset + vertId) * 2);

	mat3 normalMatrix = inverse(transpose(mat3(modelMatrix)));
	worldPos = (modelMatrix * vec4(in_pos, 1)).xyz;
	worldNormal = normalMatrix * in_normal;
	worldTangent = normalMatrix * in_tangent;
	texCoord = in_texcoord;

	gl_Position = vpMatrices.projection * vpMatrices.view * modelMatrix * vec4(in_pos, 1);
}