#pragma once

#include <stdlib.h>
#include <string>
#include <algorithm>
#include "GPUModels.h"
#include "..\VkUtil.h"
#include "..\ShaderUniforms.h"
#include "..\DeviceMemoryAllocator.h"
#include "..\..\util\DrillMath.h"

namespace geom {
	class Mesh;
	class Model;
	class SkinnedMesh;
	class SelectableObject;
}
namespace scene {
	struct Camera;
}
namespace document {
	struct DocumentNode;
}
namespace vku {

	class WorldGeometryManager;
	class DescriptorSet;
	class GraphicsPipeline;
	class RenderPass;
	class Framebuffer;

#pragma pack(push, 1)
	//All of these offsets are in units of sizeof(uint32_t) because that makes it easier to index into uint arrays in the shader.
	struct WorldGeometryOffsets {
		//Offsets into uint buffer for each attribute (positions is always 0 now, but still here just in case I want to change it)
		uint32_t posOffset;
		uint32_t texOffset;
		uint32_t normOffset;
		uint32_t tanOffset;
		uint32_t skinDataOffset;
		uint32_t indicesOffset;
		//Skinning vertices have different offsets because they don't contain texcoords and I don't want to waste space
		uint32_t skinPosOffset;
		uint32_t skinNormOffset;
		uint32_t skinTanOffset;
		//Offset of the final index buffer. Each index here is 32 bits compared to the 16 bit model indices, and contains both the offset into the geometry set model buffer and vertex index.
		uint32_t finalIndicesOffset;
	};
	struct GpuCamera {
		mat4f view;
		mat4f projection;
		//x, y, width, height
		vec4f viewport;
		uint32_t flags;
	};
	struct GPUGeometrySet {
		uint32_t modelId[256];
		uint32_t setModelOffset;
		uint32_t setModelCount;
		uint32_t indexOffset;
	};
#pragma pack(pop)

	constexpr uint32_t OBJECT_FLAG_SELECTED = 1;
	constexpr uint32_t OBJECT_FLAG_ACTIVE = 2;

	//Pos tex norm tan
	constexpr uint32_t vertexSize = 4 * 3 + 4 * 2 + 4 * 3 + 4 * 3;
	//Indices weights
	constexpr uint32_t skinVertSize = sizeof(uint32_t) * 2;
	//Pos norm tan
	constexpr uint32_t skinnedVertexSize = 4 * 3 + 4 * 3 + 4 * 3;
	//16 bit index
	constexpr uint32_t localIndexSize = sizeof(uint16_t);
	//32 bit index
	constexpr uint32_t globalIndexSize = sizeof(uint32_t);

	enum WorldRenderPass {
		WORLD_RENDER_PASS_DEPTH = 0,
		WORLD_RENDER_PASS_ID = 1,
		WORLD_RENDER_PASS_COLOR = 2
	};

	struct GeometrySet {
		//The meshes in this set
		uint32_t modelId[256];
		//Set id used as index to access set related data
		uint32_t setId;
		//Offset into a buffer of integers representing the model id per triangle cull dispatch
		uint32_t setModelOffset;
		//How many slots this set will take up in the model indices array.
		uint32_t modelSlotCount;
		//Number of models in this set, there won't always be exactly 256 models per set
		uint32_t setModelCount;
		//Offset into final index buffer, each geometry set gets its own worst case allocation of index buffer space.
		uint32_t indexOffset;
		uint32_t indexCount;

		void add_model(geom::Model& model);
	};

	struct GeometrySetUniform {
		uint32_t setId;
		uint32_t setModelOffset;
		uint32_t setModelCount;
	};

	//Allocated per static model
	struct WorldGeometryAllocation {
		WorldGeometryManager* allocator;
		uint32_t vertexOffset;
		uint32_t vertexCount;
		uint32_t indexOffset;
		uint32_t indexCount;
	};

	//Allocated for models with skinning data
	struct WorldSkinnedGeometryAllocation {
		WorldGeometryManager* allocator;
		uint32_t vertexOffset;
		uint32_t vertexCount;
		uint32_t indexOffset;
		uint32_t indexCount;
		uint32_t skinningDataOffset;
	};

	//Allocated per skinned model
	struct WorldSkinnedAllocation {
		WorldGeometryManager* allocator;
		uint32_t vertexOffset;
		uint32_t vertexCount;
		uint32_t matrixOffset;
		uint32_t matrixCount;
	};

	struct WorldGeometryBlock {
		uint32_t start;
		uint32_t end;
	};

	//Small block allocator class for allocating ranges of numbers
	class WorldGeoSuballocator {
	private:
		std::vector<WorldGeometryBlock> freeBlocks{};
		uint32_t size{ 0 };
	public:
		void resize(uint32_t newSize);
		bool alloc(uint32_t allocSize, WorldGeometryBlock* offset);
		void free(WorldGeometryBlock& block);
	};

	//The world geometry manager handles all world geometry. It has a single buffer split into four parts: the model data part, the skin data part, the skinned vertices part, and the index part.
	//The model part stores vertex data like position and normals in flat non interleaved arrays. It also stores local 16 bit indices.
	//The skin data part is just like the model data part, but separate because not every model needs skinning data. It stores 8 bytes per vetex, packing 4 bone indices and 4 weights in 8 bits each.
	//The skinned vertices part stores vertex data that needs skinning (pos, norm, tan). This is written to in compute.
	//The index part stores 32 bit global indices. The first 16 bits are an offset into an array of model ids offset with the geometry set. The other 16 bits store the local vertex index.
	//
	//
	class WorldGeometryManager {
	private:
		WorldGeoSuballocator modelDataAllocator{};
		WorldGeoSuballocator modelIndexAllocator{};
		WorldGeoSuballocator skinModelDataAllocator{};
		WorldGeoSuballocator skinnedVerticesAllocator{};
		WorldGeometryOffsets offsets;
		DeviceBuffer buffer;
		uint32_t bufferSizeBytes;
		uint32_t geoSize;
		uint32_t geoIndexSize;
		uint32_t skinDataSize;
		uint32_t skinGeoSize;
		uint32_t indexSize;
		//These indices use a simple linear allocator, reset at the beginning of each frame
		uint32_t currentIndexOffset;

		WorldGeoSuballocator skinMatricesAllocator{};

		//set 0
		UniformSSBO<geom::GPUMesh>* gpuMeshes;
		UniformSSBO<geom::GPUModel>* gpuModels;
		UniformSSBO<mat4f>* modelTransforms;
		UniformSSBO<uint32_t>* dispatchModelIds;
		UniformSSBO<GPUGeometrySet>* gpuGeometrySets;
		UniformSSBO<VkDispatchIndirectCommand>* triangleCullArgs;
		UniformSSBO<uint32_t>* gpuGeometry;
		UniformSSBO<VkDrawIndexedIndirectCommand>* drawArgs;
		//Objects contain data for generic objects (right now just visiblity and whether it's selected)
		std::vector<uint32_t> freeObjectIds{};
		uint32_t objectCount;
		UniformSSBO<geom::GPUObject>* gpuObjects;
		std::vector<geom::SelectableObject*> objects{};

		uint32_t maxSkinMatrices;
		UniformSSBO<mat4f>* skinMatrices;
		uint32_t maxSkinWorkgroups;
		UniformSSBO<uint32_t>* skinModelIdsByWorkgroup;

		//set 1
		UniformBuffer<WorldGeometryOffsets>* geoOffsetBuffer;
		//Holds the view/projection matrices for all cameras in the scene, bound at different offsets
		UniformBuffer<GpuCamera>* cameraBuffer;

		uint32_t maxMeshCount;
		std::vector<uint32_t> freeMeshIds{};
		std::vector<geom::Mesh*> meshes{};
		uint32_t maxModelCount;
		std::vector<uint32_t> freeModelIds{};
		std::vector<geom::Model*> models{};
		//New meshes and models that must be uploaded
		std::vector<uint32_t> newMeshes;
		std::vector<uint32_t> newModels;
		
		uint32_t maxDispatchModelIds;
		uint32_t maxGeoSetCount;
		uint32_t currentGeoSetCount;
		std::vector<uint32_t> freeGeoSetList{};

		uint32_t maxTriangleCullDispatches;


		//For updates that must arrive this frame (transform matrices, new instances, etc). Larger updates for mesh data go through a staging manager at load time or streaming.
		uint32_t dataStagingOffset;
		DeviceBuffer dataStagingBuffer[NUM_FRAME_DATA];

		RenderPass* worldRenderPass;
		RenderPass* depthRenderPass;
		RenderPass* objectIdRenderPass;
		GraphicsPipeline* depthPipeline;
		GraphicsPipeline* objectIdPipeline;
		GraphicsPipeline* renderPipeline;
		ComputePipeline* modelCullPipeline;
		ComputePipeline* triangleCullPipeline;
		ComputePipeline* skinningPipeline;

		DescriptorSet* cullingSet;
		DescriptorSet* drawSet;
		DescriptorSet* skinningSet;
		//Contains geometry offsets and view/projection matrices
		DescriptorSet* worldGeoDataSet;

		DescriptorSet* renderSet;

		uint32_t get_buffer_size_bytes();
		void recalc_geometry_offsets(WorldGeometryOffsets& newOffsets);
		void resize_buffer(VkCommandBuffer cmdBuf, uint32_t oldGeoSize, uint32_t oldGeoIndexSize, uint32_t oldSkinDataSize, uint32_t oldSkinGeoSize, uint32_t oldIndexSize);
		uint32_t check_resize_staging(uint32_t add);

		WorldGeometryAllocation alloc_mesh(VkCommandBuffer cmdBuf, uint32_t vertCount, uint32_t indexCount);
		WorldSkinnedGeometryAllocation alloc_skinned_mesh(VkCommandBuffer cmdBuf, uint32_t vertCount, uint32_t indexCount);
		void free_skinned_mesh(WorldSkinnedGeometryAllocation& allocation);
	public:

		int32_t get_object_id(VkCommandBuffer cmdBuf);
		void free_object_id(int32_t id);
		inline std::vector<geom::SelectableObject*> get_object_list() {
			return objects;
		}
		inline UniformSSBO<geom::GPUObject>* get_object_buffer() {
			return gpuObjects;
		}

		void alloc_geo_set(GeometrySet* set);
		void update_cam_sets(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras);

		geom::Mesh* create_mesh(document::DocumentNode* geometry);
		void delete_mesh(geom::Mesh* mesh);
		
		WorldSkinnedAllocation alloc_skinned(VkCommandBuffer cmdBuf, geom::SkinnedMesh& mesh);
		void free_skinned(WorldSkinnedAllocation& allocation);

		//Uploads mesh information and gives it an id
		void upload_mesh(StagingManager& staging, geom::Mesh& mesh);
		void upload_skinned_mesh(StagingManager& staging, geom::SkinnedMesh& mesh);
		void add_model(geom::Model& model);

		void set_render_pass(RenderPass* renderPass, RenderPass* depthRenderPass, RenderPass* objectIdPass);
		void set_render_desc_set(DescriptorSet* descSet);
		void init(uint32_t startVertSize, uint32_t startIdxSize, uint32_t startSkinDataSize, uint32_t startSkinnedVertSize, uint32_t startFinalIdxSize);
		void create_descriptor_sets(UniformTexture2D* depthPyramidUniform);
		void create_pipelines();
		void cleanup();

		VkBuffer get_buffer();
		GraphicsPipeline* get_render_pipeline();

		//Sets up this frame, including stuff like resetting counters and updating camera matrices
		void begin_frame(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras);
		//void begin_depth_pass(VkCommandBuffer cmdBuf, Framebuffer& fbo);
		//void end_depth_pass(VkCommandBuffer cmdBuf);
		//Should be called once per frame, will send all new data to the GPU and resize necessary buffers
		void send_data(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras);

		void skin(VkCommandBuffer cmdBuf);
		void cull_depth_prepass(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras, RenderPass& renderPass, Framebuffer& framebuffer);
		//void depth_prepass(VkCommandBuffer cmdBuf, scene::Camera& cam, int32_t overrideIndex = -1);
		void cull(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras);
		void draw(VkCommandBuffer cmdBuf, scene::Camera& cam, WorldRenderPass pass, int32_t overrideIndex = -1);
	};
}
