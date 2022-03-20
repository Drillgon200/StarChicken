#pragma once
#include "..\graphics\geometry\GeometryAllocator.h"
#include "..\graphics\geometry\GPUModels.h"
#include "..\util\DrillMath.h"
#include "..\resources\FileDocument.h"

namespace geom {
	class SelectableObject {
	public:
		virtual bool needs_object_update() = 0;
		virtual void pack_object(GPUObject* object) = 0;
		virtual void set_needs_object_udpate(bool needsUpdate) = 0;
		virtual void set_selection(uint32_t flags) = 0;
		virtual uint32_t get_selection() = 0;
		virtual int32_t get_object_id() = 0;
	};
	//A mesh represents a collection of vertex data.
	class Mesh {
	private:
		friend class vku::WorldGeometryManager;
		friend class SkinnedMesh;

		vku::WorldGeometryAllocation vertexMemory;
		vec3f* positions;
		vec2f* texcoords;
		vec3f* normals;
		vec3f* tangents;
		uint16_t* indices;
		uint32_t meshId;
		uint32_t vertCount;
		uint32_t indexCount;
		AxisAlignedBB3Df boundingBox;
		bool isSkinned;

		Mesh(document::DocumentNode* geo);
		~Mesh();
	public:

		inline void pack(GPUMesh* mesh) {
			mesh->vertexOffset = vertexMemory.vertexOffset;
			mesh->indexOffset = vertexMemory.indexOffset;
			mesh->skinDataOffset = 0;
			mesh->vertCount = indexCount;
			mesh->minX = boundingBox.minX;
			mesh->minY = boundingBox.minY;
			mesh->minZ = boundingBox.minZ;
			mesh->maxX = boundingBox.maxX;
			mesh->maxY = boundingBox.maxY;
			mesh->maxZ = boundingBox.maxZ;
		}

		inline uint32_t get_mesh_id() {
			return meshId;
		}
		inline void set_mesh_id(uint32_t id) {
			meshId = id;
		}
		inline vec3f* get_positions() {
			return positions;
		}
		inline vec2f* get_texcoords() {
			return texcoords;
		}
		inline vec3f* get_normals() {
			return normals;
		}
		inline vec3f* get_tangents() {
			return tangents;
		}
		inline uint16_t* get_indices() {
			return indices;
		}
		inline uint32_t get_vert_count() {
			return vertCount;
		}
		inline uint32_t get_index_count() {
			return indexCount;
		}
		inline void set_memory(vku::WorldGeometryAllocation mem) {
			//memcpy(&vertexMemory, &mem, sizeof(vku::WorldGeometryAllocation));
			vertexMemory = mem;
		}
		inline vku::WorldGeometryAllocation get_memory() {
			return vertexMemory;
		}
		inline uint32_t get_model_slot_count() {
			return (indexCount + 255) / 256;
		}
	};

	//A skinned mesh is just like a mesh, but also has weights and indices to define how its bone set affects each vertex.
	class SkinnedMesh : public Mesh {
	private:
		vku::WorldSkinnedGeometryAllocation skinnedVertexMemory;
		vec4ui8* boneIndicesAndWeights;
		uint32_t boneCount;

		SkinnedMesh(document::DocumentNode* geo);
		~SkinnedMesh();
	public:
		inline void set_skinned_memory(vku::WorldSkinnedGeometryAllocation mem) {
			//Fill in struct for parent as well
			set_memory(vku::WorldGeometryAllocation{ mem.allocator, mem.vertexOffset, mem.vertexCount, mem.indexOffset, mem.indexCount });
			//memcpy(&skinnedVertexMemory, &mem, sizeof(vku::WorldSkinnedGeometryAllocation));
			skinnedVertexMemory = mem;
		}
		inline vku::WorldSkinnedGeometryAllocation& get_skinned_memory() {
			return skinnedVertexMemory;
		}
		inline vec4ui8* get_indices_and_weights() {
			return boneIndicesAndWeights;
		}
	};

	//A model represents an instance of a mesh. It has a unique matrix transform, might have a shader material attached to it in the future.
	class Model : public SelectableObject {
	private:
		Mesh* mesh;
		mat4f* boneMatrices;
		mat4f modelMatrix;
		uint32_t skinVerticesOffset;
		uint32_t skinMatricesOffset;
		uint32_t modelId;
		int32_t objectId{-1};
		bool selected;
		bool active;
		bool transformChanged;
		bool needsObjectUpdate;
	public:

		Model(Mesh* mesh);

		inline void pack(GPUModel* model) {
			model->meshId = mesh->get_mesh_id();
			model->skinMatricesOffset = skinMatricesOffset;
			model->skinVerticesOffset = skinVerticesOffset;
			model->objectId = objectId;
		}

		void pack_object(GPUObject* object) override {
			object->selectionFlags = selected ? vku::OBJECT_FLAG_SELECTED : 0;
			object->selectionFlags |= active ? vku::OBJECT_FLAG_ACTIVE : 0;
			object->visible = 1;
		}
		bool needs_object_update() override {
			return needsObjectUpdate;
		}
		void set_needs_object_udpate(bool needsUpdate) override {
			needsObjectUpdate = needsUpdate;
		}
		void set_selection(uint32_t flags) override {
			selected = (flags & vku::OBJECT_FLAG_SELECTED) > 0;
			active = (flags & vku::OBJECT_FLAG_ACTIVE) > 0;
			needsObjectUpdate = true;
		}
		uint32_t get_selection() override {
			uint32_t flags = selected ? vku::OBJECT_FLAG_SELECTED : 0;
			if (active) {
				flags |= vku::OBJECT_FLAG_ACTIVE;
			}
			return flags;
		}

		inline uint32_t get_model_id() {
			return modelId;
		}

		inline void set_model_id(uint32_t id) {
			modelId = id;
		}

		int32_t get_object_id() override {
			return objectId;
		}

		inline void set_object_id(int32_t id) {
			objectId = id;
		}

		inline Mesh* get_mesh() {
			return mesh;
		}

		inline bool transform_changed() {
			return transformChanged;
		}

		inline mat4f& get_transform() {
			return modelMatrix;
		}
	};
}