#pragma once

namespace geom {
#pragma pack(push, 1)
	struct GPUMesh {
		uint32_t vertexOffset;
		uint32_t indexOffset;
		//If 0, this is not a skinned mesh. We need this because not all meshes have skinning data, and the offset won't be the same as the vertex offset.
		uint32_t skinDataOffset;
		uint32_t vertCount;
		//Local space bounding box
		float minX;
		float minY;
		float minZ;
		float maxX;
		float maxY;
		float maxZ;
	};
	struct GPUModel {
		uint32_t meshId;
		uint32_t skinMatricesOffset;
		uint32_t skinVerticesOffset;
		uint32_t objectId;
	};
	struct GPUObject {
		uint32_t selectionFlags;
		uint32_t visible;
	};
#pragma pack(pop)
}