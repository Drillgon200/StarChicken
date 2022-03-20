#include "GeometryAllocator.h"
#include "..\StagingManager.h"
#include "..\VertexFormats.h"
#include "..\..\resources\Models.h"
#include "..\..\scene\Scene.h"
#include "..\..\resources\FileDocument.h"
#include "..\RenderPass.h"
#include "..\..\Engine.h"
#include "..\..\RenderSubsystem.h"

namespace vku {

	void GeometrySet::add_model(geom::Model& model) {
		modelId[setModelCount] = model.get_model_id();
		modelSlotCount += model.get_mesh()->get_model_slot_count();
		indexCount += model.get_mesh()->get_index_count();
		++setModelCount;
	}

	void WorldGeoSuballocator::resize(uint32_t newSize) {
		if (newSize <= size) {
			return;
		}
		if (freeBlocks.empty() || (freeBlocks[freeBlocks.size() - 1].end != size)) {
			freeBlocks.push_back(WorldGeometryBlock{ size, newSize });
		} else {
			freeBlocks[freeBlocks.size() - 1].end = newSize;
		}
		size = newSize;
	}

	bool WorldGeoSuballocator::alloc(uint32_t allocSize, WorldGeometryBlock* offset) {
		for (uint32_t i = 0; i < freeBlocks.size(); i++) {
			WorldGeometryBlock& freeBlock = freeBlocks[i];
			uint32_t freeBlockSize = (freeBlock.end - freeBlock.start);
			if (freeBlockSize >= allocSize) {
				offset->start = freeBlock.start;
				offset->end = freeBlock.start + allocSize;
				if (freeBlockSize == allocSize) {
					freeBlocks.erase(freeBlocks.begin() + i);
				} else {
					freeBlock.start += allocSize;
				}
				return true;
			}
		}
		return false;
	}

	void WorldGeoSuballocator::free(WorldGeometryBlock& block) {
		if (block.end >= size) {
			return;
		}
		for (uint32_t i = 0; i < freeBlocks.size(); i++) {
			WorldGeometryBlock& freeBlock = freeBlocks[i];
			if ((freeBlock.start <= block.start) && (freeBlock.end >= block.end)) {
				//Does this free block contain the block?
				if (freeBlock.end == block.start) {
					//Is this free block connected behind the block? If so, expand this free block to cover it
					freeBlock.end = block.end;
					if ((i < (freeBlocks.size() - 1)) && (freeBlocks[i + 1].end == freeBlock.end)) {
						//Is the block also connected to the next free block? If so, merge this free block and the next one.
						freeBlock.end = freeBlocks[i + 1].end;
						freeBlocks.erase(freeBlocks.begin() + (i + 1));
					}
					return;
				} else if (freeBlock.start == block.end) {
					//Is this free block connected in front of the block? If so, expand this free block to cover it.
					freeBlock.start = block.start;
				} else {
					//Not connected to any free blocks, insert a new one
					freeBlocks.insert(freeBlocks.begin() + i, block);
				}
			}
		}
	}

	void stage_attribute(StagingManager& staging, void* from, uint32_t size, VkBuffer dstBuffer, uint32_t dstOffset) {
		VkCommandBuffer cmdBuf;
		VkBuffer buf;
		uint32_t offset;
		void* data = staging.stage(size, 4, cmdBuf, buf, offset);
		memcpy(data, from, size);
		VkBufferCopy copy{};
		copy.size = size;
		copy.srcOffset = offset;
		copy.dstOffset = dstOffset;
		vkCmdCopyBuffer(cmdBuf, buf, dstBuffer, 1, &copy);
	}

	uint32_t WorldGeometryManager::get_buffer_size_bytes() {
		return vertexSize * geoSize + localIndexSize * ((geoIndexSize + 1) & (~1)) + skinVertSize * skinDataSize + skinnedVertexSize * skinGeoSize + indexSize * globalIndexSize;
	}
	void WorldGeometryManager::recalc_geometry_offsets(WorldGeometryOffsets& newOffsets) {
		newOffsets.posOffset = 0;
		newOffsets.texOffset = geoSize * 3;
		newOffsets.normOffset = newOffsets.texOffset + geoSize * 2;
		newOffsets.tanOffset = newOffsets.normOffset + geoSize * 3;
		newOffsets.indicesOffset = newOffsets.tanOffset + geoSize * 3;
		newOffsets.skinDataOffset = newOffsets.indicesOffset + (geoIndexSize + 1) / 2;
		newOffsets.skinPosOffset = newOffsets.skinDataOffset + skinDataSize * 2;
		newOffsets.skinNormOffset = newOffsets.skinPosOffset + skinGeoSize * 3;
		newOffsets.skinTanOffset = newOffsets.skinNormOffset + skinGeoSize * 3;
		newOffsets.finalIndicesOffset = newOffsets.skinTanOffset + skinGeoSize * 3;
	}
	void WorldGeometryManager::resize_buffer(VkCommandBuffer cmdBuf, uint32_t oldGeoSize, uint32_t oldGeoIndexSize, uint32_t oldSkinDataSize, uint32_t oldSkinGeoSize, uint32_t oldIndexSize) {
		if ((oldGeoSize == geoSize) && (oldGeoIndexSize == geoIndexSize) && (oldSkinDataSize == skinDataSize) && (oldSkinGeoSize == skinGeoSize) && (oldIndexSize == indexSize)) {
			//If none of the sizes changed, do nothing
			return;
		}
		//Wait for pending transfers to complete before copying to new buffer. Could also try to handle it better by using a semaphore, but that would be more work and it's not like I'm resizing every frame anyway.
		transferStagingManager->flush();
		transferStagingManager->wait_all();

		uint32_t oldBufferSizeBytes = bufferSizeBytes;
		uint32_t newBufferSizeBytes = get_buffer_size_bytes();
		DeviceBuffer newBuffer = generalAllocator->alloc_buffer(newBufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		WorldGeometryOffsets newOffsets;
		recalc_geometry_offsets(newOffsets);

		//Ensure any writes to this buffer from previous resizes or model transfers are done before we copy to a new one
		//When I get to implementing streaming, I'll have to put a semaphore or something here as well.
		memory_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

		//4 vertex attribute arrays, index array, skin data array, skinned vertex 3 attribute arrays, final indices array
		constexpr uint32_t copyCount = 4 + 1 + 1 + 3 + 1;
		VkBufferCopy copy[copyCount];
		//Positions
		copy[0].srcOffset = offsets.posOffset * 4;
		copy[0].dstOffset = newOffsets.posOffset * 4;
		copy[0].size = oldGeoSize * sizeof(float) * 3;
		//Texcoords
		copy[1].srcOffset = offsets.texOffset * 4;
		copy[1].dstOffset = newOffsets.texOffset * 4;
		copy[1].size = oldGeoSize * sizeof(float) * 2;
		//Normals
		copy[2].srcOffset = offsets.normOffset * 4;
		copy[2].dstOffset = newOffsets.normOffset * 4;
		copy[2].size = oldGeoSize * sizeof(float) * 3;
		//Tangents
		copy[3].srcOffset = offsets.tanOffset * 4;
		copy[3].dstOffset = newOffsets.tanOffset * 4;
		copy[3].size = oldGeoSize * sizeof(float) * 3;
		//Local indices
		copy[4].srcOffset = offsets.indicesOffset * 4;
		copy[4].dstOffset = newOffsets.indicesOffset * 4;
		copy[4].size = oldGeoIndexSize * sizeof(uint16_t);
		//Skin data
		copy[5].srcOffset = offsets.skinDataOffset * 4;
		copy[5].dstOffset = newOffsets.skinDataOffset * 4;
		copy[5].size = oldSkinDataSize * sizeof(uint32_t) * 2;
		//Skinned positions
		copy[6].srcOffset = offsets.skinPosOffset * 4;
		copy[6].dstOffset = newOffsets.skinPosOffset * 4;
		copy[6].size = oldSkinGeoSize * sizeof(float) * 3;
		//Skinned normals
		copy[7].srcOffset = offsets.skinNormOffset * 4;
		copy[7].dstOffset = newOffsets.skinNormOffset * 4;
		copy[7].size = oldSkinGeoSize * sizeof(float) * 3;
		//Skinned tangents
		copy[8].srcOffset = offsets.skinTanOffset * 4;
		copy[8].dstOffset = newOffsets.skinTanOffset * 4;
		copy[8].size = oldSkinGeoSize * sizeof(float) * 3;
		//Global indices
		copy[9].srcOffset = offsets.finalIndicesOffset * 4;
		copy[9].dstOffset = newOffsets.finalIndicesOffset * 4;
		copy[9].size = oldIndexSize * sizeof(uint32_t);
		vkCmdCopyBuffer(cmdBuf, buffer.buffer, newBuffer.buffer, copyCount, copy);

		//Ensure this copy is done before anything tries to read from the new buffer
		buffer_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDEX_READ_BIT, newBuffer.buffer, 0, newBufferSizeBytes);

		offsets = newOffsets;
		generalAllocator->queue_free_buffer(currentFrame, buffer);
		buffer = newBuffer;
		bufferSizeBytes = newBufferSizeBytes;
		gpuGeometry->set_buffer(newBufferSizeBytes, &buffer);
	}

	uint32_t calc_resize_half_grow(uint32_t size, uint32_t accommodation) {
		while (accommodation >= size) {
			size = size + size / 2;
		}
		return size;
	}

	uint32_t WorldGeometryManager::check_resize_staging(uint32_t add) {
		if ((dataStagingOffset + add) >= dataStagingBuffer[currentFrame].allocation->size) {
			uint32_t newSize = calc_resize_half_grow(dataStagingBuffer[currentFrame].allocation->size, dataStagingOffset + add);
			for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
				DeviceBuffer oldBuffer = dataStagingBuffer[i];
				generalAllocator->queue_free_buffer(currentFrame, oldBuffer);
				dataStagingBuffer[i] = generalAllocator->alloc_buffer(newSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				if (i == currentFrame) {
					//Only need to copy this frame's data so future transfer operations referring to earlier in the buffer will still work.
					memcpy(dataStagingBuffer[i].allocation->map(), oldBuffer.allocation->map(), dataStagingOffset);
				}
			}
		}
		uint32_t oldOffset = dataStagingOffset;
		dataStagingOffset += add;
		return oldOffset;
	}

	WorldGeometryAllocation WorldGeometryManager::alloc_mesh(VkCommandBuffer cmdBuf, uint32_t vertCount, uint32_t indexCount) {
		WorldGeometryBlock vertAlloc;
		WorldGeometryBlock idxAlloc;
		uint32_t oldGeoSize = geoSize;
		uint32_t oldGeoIndexSize = geoIndexSize;
		while (!modelDataAllocator.alloc(vertCount, &vertAlloc)) {
			geoSize = geoSize + geoSize / 2;
			modelDataAllocator.resize(geoSize);
		}
		while (!modelIndexAllocator.alloc(indexCount, &idxAlloc)) {
			geoIndexSize = geoIndexSize + geoIndexSize / 2;
			modelIndexAllocator.resize(geoIndexSize);
		}
		resize_buffer(cmdBuf, oldGeoSize, oldGeoIndexSize, skinDataSize, skinGeoSize, indexSize);

		return WorldGeometryAllocation{ this, vertAlloc.start, vertCount, idxAlloc.start, indexCount };
	}

	WorldSkinnedGeometryAllocation WorldGeometryManager::alloc_skinned_mesh(VkCommandBuffer cmdBuf, uint32_t vertCount, uint32_t indexCount) {
		WorldGeometryBlock vertAlloc;
		WorldGeometryBlock idxAlloc;
		WorldGeometryBlock skinDataAlloc;
		uint32_t oldGeoSize = geoSize;
		uint32_t oldGeoIndexSize = geoIndexSize;
		uint32_t oldSkinDataSize = skinDataSize;
		while (!modelDataAllocator.alloc(vertCount, &vertAlloc)) {
			geoSize = geoSize + geoSize / 2;
			modelDataAllocator.resize(geoSize);
		}
		while (!modelIndexAllocator.alloc(indexCount, &idxAlloc)) {
			geoIndexSize = geoIndexSize + geoIndexSize / 2;
			modelIndexAllocator.resize(geoIndexSize);
		}
		while (!skinModelDataAllocator.alloc(vertCount, &skinDataAlloc)) {
			skinDataSize = skinDataSize + skinDataSize / 2;
			skinModelDataAllocator.resize(skinDataSize);
		}
		resize_buffer(cmdBuf, oldGeoSize, oldGeoIndexSize, oldSkinDataSize, skinGeoSize, indexSize);

		return WorldSkinnedGeometryAllocation{ this, vertAlloc.start, vertCount, idxAlloc.start, indexCount, skinDataAlloc.start };
	}
	WorldSkinnedAllocation WorldGeometryManager::alloc_skinned(VkCommandBuffer cmdBuf, geom::SkinnedMesh& mesh) {
		WorldGeometryBlock skinVertices;
		uint32_t oldSkinGeoSize = skinGeoSize;
		while (!skinnedVerticesAllocator.alloc(mesh.get_vert_count(), &skinVertices)) {
			skinGeoSize = skinGeoSize + skinGeoSize / 2;
			skinnedVerticesAllocator.resize(skinGeoSize);
		}
		resize_buffer(cmdBuf, geoSize, geoIndexSize, skinDataSize, oldSkinGeoSize, indexSize);

		return WorldSkinnedAllocation{ this, skinVertices.start, mesh.get_vert_count(), 0, 0 };
	}
	geom::Mesh* WorldGeometryManager::create_mesh(document::DocumentNode* geometry) {
		geom::Mesh* mesh = new geom::Mesh(geometry);
		mesh->set_memory(alloc_mesh(transferCommandBuffer, mesh->get_vert_count(), mesh->get_index_count()));
		upload_mesh(*transferStagingManager, *mesh);
		return mesh;
	}
	void WorldGeometryManager::delete_mesh(geom::Mesh* mesh) {
		WorldGeometryAllocation allocation = mesh->vertexMemory;
		WorldGeometryBlock vertAlloc{ allocation.vertexOffset, allocation.vertexOffset + allocation.vertexCount };
		WorldGeometryBlock idxAlloc{ allocation.indexOffset, allocation.indexOffset + allocation.indexCount };
		modelDataAllocator.free(vertAlloc);
		modelIndexAllocator.free(idxAlloc);
		if (mesh->isSkinned) {
			WorldSkinnedGeometryAllocation& skinAllocation = static_cast<geom::SkinnedMesh*>(mesh)->get_skinned_memory();
			WorldGeometryBlock skinDataAlloc{ skinAllocation.skinningDataOffset, skinAllocation.vertexCount };
			skinModelDataAllocator.free(skinDataAlloc);
		}
		meshes.erase(std::find(meshes.begin(), meshes.end(), mesh));
		delete mesh;
	}
	void WorldGeometryManager::free_skinned_mesh(WorldSkinnedGeometryAllocation& allocation) {
		WorldGeometryBlock vertAlloc{ allocation.vertexOffset, allocation.vertexOffset + allocation.vertexCount };
		WorldGeometryBlock idxAlloc{ allocation.indexOffset, allocation.indexOffset + allocation.indexCount };
		WorldGeometryBlock skinDataAlloc{ allocation.skinningDataOffset, allocation.vertexCount };
		modelDataAllocator.free(vertAlloc);
		modelIndexAllocator.free(idxAlloc);
		skinModelDataAllocator.free(skinDataAlloc);
	}
	void WorldGeometryManager::free_skinned(WorldSkinnedAllocation& allocation) {
		WorldGeometryBlock skinVertices{ allocation.vertexOffset, allocation.vertexOffset + allocation.vertexCount };
		skinnedVerticesAllocator.free(skinVertices);
	}

	void WorldGeometryManager::upload_mesh(StagingManager& staging, geom::Mesh& mesh) {
		uint32_t size = 3 * sizeof(float);
		stage_attribute(staging, mesh.get_positions(), mesh.get_memory().vertexCount * size, buffer.buffer, offsets.posOffset * 4 + mesh.get_memory().vertexOffset * size);
		size = 2 * sizeof(float);
		stage_attribute(staging, mesh.get_texcoords(), mesh.get_memory().vertexCount * size, buffer.buffer, offsets.texOffset * 4 + mesh.get_memory().vertexOffset * size);
		size = 3 * sizeof(float);
		stage_attribute(staging, mesh.get_normals(), mesh.get_memory().vertexCount * size, buffer.buffer, offsets.normOffset * 4 + mesh.get_memory().vertexOffset * size);
		size = 3 * sizeof(float);
		stage_attribute(staging, mesh.get_tangents(), mesh.get_memory().vertexCount * size, buffer.buffer, offsets.tanOffset * 4 + mesh.get_memory().vertexOffset * size);
		size = sizeof(uint16_t);
		stage_attribute(staging, mesh.get_indices(), mesh.get_memory().indexCount * size, buffer.buffer, offsets.indicesOffset * 4 + mesh.get_memory().indexOffset * size);

		if (!freeMeshIds.empty()) {
			uint32_t id = freeMeshIds.back();
			freeMeshIds.pop_back();
			mesh.set_mesh_id(id);
			meshes[id] = &mesh;
		} else {
			mesh.set_mesh_id(meshes.size());
			meshes.push_back(&mesh);
		}
		newMeshes.push_back(mesh.get_mesh_id());
	}

	void WorldGeometryManager::upload_skinned_mesh(StagingManager& staging, geom::SkinnedMesh& mesh) {
		upload_mesh(staging, mesh);
		uint32_t size = 2 * sizeof(uint32_t);
		stage_attribute(staging, mesh.get_indices_and_weights(), mesh.get_memory().vertexCount * size, buffer.buffer, offsets.skinDataOffset * 4 + mesh.get_skinned_memory().skinningDataOffset * size);
	}

	void WorldGeometryManager::add_model(geom::Model& model) {
		if (!freeModelIds.empty()) {
			uint32_t id = freeModelIds.back();
			freeModelIds.pop_back();
			model.set_model_id(id);
			models[id] = &model;
		} else {
			uint32_t id = models.size();
			model.set_model_id(id);
			models.push_back(&model);
		}
		model.set_needs_object_udpate(true);
		newModels.push_back(model.get_model_id());
	}

	void WorldGeometryManager::set_render_pass(RenderPass* renderPass, RenderPass* depthPass, RenderPass* objectIdPass) {
		worldRenderPass = renderPass;
		depthRenderPass = depthPass;
		objectIdRenderPass = objectIdPass;
	}

	void WorldGeometryManager::set_render_desc_set(DescriptorSet* descSet) {
		renderSet = descSet;
	}

	void WorldGeometryManager::init(uint32_t startVertSize, uint32_t startIdxSize, uint32_t startSkinDataSize, uint32_t startSkinnedVertSize, uint32_t startFinalIdxSize) {
		geoSize = startVertSize;
		geoIndexSize = startIdxSize;
		skinDataSize = startSkinDataSize;
		skinGeoSize = startSkinnedVertSize;
		indexSize = startFinalIdxSize;
		recalc_geometry_offsets(offsets);
		bufferSizeBytes = get_buffer_size_bytes();
		buffer = generalAllocator->alloc_buffer(bufferSizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		modelDataAllocator.resize(geoSize);
		modelIndexAllocator.resize(geoIndexSize);
		skinModelDataAllocator.resize(skinDataSize);
		skinnedVerticesAllocator.resize(skinGeoSize);

		maxMeshCount = 256;
		maxModelCount = 256;
		maxGeoSetCount = 16;
		maxDispatchModelIds = maxGeoSetCount * 256;
		maxTriangleCullDispatches = maxDispatchModelIds;
		gpuMeshes = new UniformSSBO<geom::GPUMesh>(maxMeshCount, false, true, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT, nullptr);
		gpuModels = new UniformSSBO<geom::GPUModel>(maxModelCount, false, true, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT, nullptr);
		modelTransforms = new UniformSSBO<mat4f>(maxModelCount, false, true, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT, nullptr);
		dispatchModelIds = new UniformSSBO<uint32_t>(maxDispatchModelIds, false, true, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, nullptr);
		gpuGeometrySets = new UniformSSBO<GPUGeometrySet>(maxGeoSetCount, false, true, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT, nullptr);
		triangleCullArgs = new UniformSSBO<VkDispatchIndirectCommand>(maxTriangleCullDispatches, false, true, VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, nullptr);
		gpuGeometry = new UniformSSBO<uint32_t>(bufferSizeBytes, false, true, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, &buffer);
		drawArgs = new UniformSSBO<VkDrawIndexedIndirectCommand>(maxGeoSetCount, false, true, VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, nullptr);
		objectCount = 0;
		gpuObjects = new UniformSSBO<geom::GPUObject>(1024, false, true, VK_SHADER_STAGE_COMPUTE_BIT, static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT), nullptr);
		objects.resize(gpuObjects->size(), nullptr);
		maxSkinMatrices = 256 * 10;
		maxSkinWorkgroups = 256 * 10;
		skinMatrices = new UniformSSBO<mat4f>(maxSkinMatrices, false, true, VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT, nullptr);
		skinModelIdsByWorkgroup = new UniformSSBO<uint32_t>(maxSkinWorkgroups, false, true, VK_SHADER_STAGE_COMPUTE_BIT, VK_BUFFER_USAGE_TRANSFER_DST_BIT, nullptr);

		geoOffsetBuffer = new UniformBuffer<WorldGeometryOffsets>(true, false, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
		cameraBuffer = new UniformBuffer<GpuCamera>(true, true, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);

		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			constexpr uint32_t defaultStagingSize = 128 * 1024;
			dataStagingBuffer[i] = generalAllocator->alloc_buffer(defaultStagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
	}

	void WorldGeometryManager::create_descriptor_sets(UniformTexture2D* depthPyramidUniform) {
		cullingSet = create_descriptor_set({ gpuObjects, gpuMeshes, gpuModels, modelTransforms, dispatchModelIds, gpuGeometrySets, triangleCullArgs, gpuGeometry, drawArgs, resources::minSampler, depthPyramidUniform });
		drawSet = create_descriptor_set({ gpuObjects, gpuMeshes, gpuModels, modelTransforms, dispatchModelIds, gpuGeometrySets, gpuGeometry });
		worldGeoDataSet = create_descriptor_set({ geoOffsetBuffer, cameraBuffer });
		skinningSet = create_descriptor_set({ gpuObjects, gpuMeshes, gpuModels, gpuGeometry, skinMatrices, skinModelIdsByWorkgroup });
	}

	void WorldGeometryManager::create_pipelines() {
		depthPipeline = (new GraphicsPipeline())->name("world_depth").pass(*depthRenderPass, 0).vertex_format(VERTEX_FORMAT_NONE).color_writes(0).descriptor_set(*drawSet).descriptor_set(*worldGeoDataSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t) }).build();
		renderPipeline = (new GraphicsPipeline())->name("world_test").pass(*worldRenderPass, 0).vertex_format(VERTEX_FORMAT_NONE).depth_compare(VK_COMPARE_OP_EQUAL).descriptor_set(*drawSet).descriptor_set(*worldGeoDataSet).descriptor_set(*renderSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t) }).build();
		objectIdPipeline = (new GraphicsPipeline())->name("world_id").pass(*objectIdRenderPass, 0).vertex_format(VERTEX_FORMAT_NONE).descriptor_set(*drawSet).descriptor_set(*worldGeoDataSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t) }).build();
		modelCullPipeline = (new ComputePipeline())->name("mesh_cull").descriptor_set(*cullingSet).descriptor_set(*worldGeoDataSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, 3 * sizeof(uint32_t) }).build();
		triangleCullPipeline = (new ComputePipeline())->name("triangle_cull").descriptor_set(*cullingSet).descriptor_set(*worldGeoDataSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, 3 * sizeof(uint32_t) }).build();
		skinningPipeline = (new ComputePipeline())->name("mesh_skin").descriptor_set(*skinningSet).descriptor_set(*worldGeoDataSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) }).build();
	}

	void WorldGeometryManager::cleanup() {
		for (geom::Mesh* mesh : meshes) {
			delete mesh;
		}
		meshes.clear();

		delete gpuModels;
		delete gpuMeshes;
		delete modelTransforms;
		delete dispatchModelIds;
		delete gpuGeometrySets;
		delete triangleCullArgs;
		gpuGeometry->set_buffer(0, nullptr);
		delete gpuGeometry;
		delete drawArgs;
		delete gpuObjects;
		delete skinMatrices;
		delete skinModelIdsByWorkgroup;

		delete geoOffsetBuffer;
		delete cameraBuffer;

		generalAllocator->free_buffer(buffer);

		delete renderPipeline;
		delete depthPipeline;
		delete objectIdPipeline;
		delete modelCullPipeline;
		delete triangleCullPipeline;
		delete skinningPipeline;

		for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
			generalAllocator->free_buffer(dataStagingBuffer[i]);
		}
	}

	VkBuffer WorldGeometryManager::get_buffer() {
		return buffer.buffer;
	}

	GraphicsPipeline* WorldGeometryManager::get_render_pipeline() {
		return renderPipeline;
	}

	void WorldGeometryManager::begin_frame(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras) {
		currentIndexOffset = 0;
		dataStagingOffset = 0;
		//currentGeoSetCount = 0;

		cameraBuffer->resize(cameras.size());

		for (int32_t i = (cameras.size()-1); i >= 0; i--) {
			scene::Camera* cam = cameras[i];
			GpuCamera gpucam{ cam->viewMatrix, cam->projectionMatrix, vec4f{ cam->viewport.x, cam->viewport.y, cam->viewport.width, cam->viewport.height }, 0 };

			cameraBuffer->update(gpucam, i);
			std::swap(cam->geometrySets, cam->prevGeometrySets);
			for (int32_t j = (cam->selectedSets.size() - 1); j >= 0; j--) {
				freeGeoSetList.push_back(cam->selectedSets[j].setId);
			}
			cam->selectedSets.clear();
			for (int32_t j = (cam->geometrySets.size()-1); j >= 0; j--) {
				freeGeoSetList.push_back(cam->geometrySets[j].setId);
			}
			cam->geometrySets.clear();
			cam->cameraIndex = i;
		}
	}

	void WorldGeometryManager::alloc_geo_set(GeometrySet* set) {
		uint32_t setId;
		if (!freeGeoSetList.empty()) {
			setId = freeGeoSetList.back();
			freeGeoSetList.pop_back();
		} else {
			setId = currentGeoSetCount;
			++currentGeoSetCount;
		}
		set->setId = setId;
	}

	template<typename T>
	void resize_and_copy_ssbo(VkCommandBuffer cmdBuf, UniformSSBO<T>* ssbo, uint32_t newSize) {
		uint32_t oldSize = ssbo->size();
		VkBuffer oldBuffer = ssbo->get_buffer();
		ssbo->resize(newSize);
		VkBuffer newBuffer = ssbo->get_buffer();
		VkBufferCopy copy;
		copy.srcOffset = 0;
		copy.dstOffset = 0;
		copy.size = oldSize * sizeof(T);
		vkCmdCopyBuffer(cmdBuf, oldBuffer, newBuffer, 1, &copy);
		buffer_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, newBuffer, 0, oldSize * sizeof(T));
	}

	int32_t WorldGeometryManager::get_object_id(VkCommandBuffer cmdBuf) {
		if (!freeObjectIds.empty()) {
			uint32_t objId = freeObjectIds.back();
			freeObjectIds.pop_back();
		} else {
			uint32_t objId = objectCount;
			++objectCount;
			if (objectCount > gpuObjects->size()) {
				resize_and_copy_ssbo(cmdBuf, gpuObjects, gpuObjects->size() + gpuObjects->size()/2);
				objects.resize(gpuObjects->size(), nullptr);
			}
			return objId;
		}
	}
	void WorldGeometryManager::free_object_id(int32_t id) {
		freeObjectIds.push_back(id);
	}

	void WorldGeometryManager::update_cam_sets(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras) {
		uint32_t currentSetOffset = 0;
		uint32_t currentSetIndexOffset = 0;
		for (uint32_t i = 0; i < cameras.size(); i++) {
			scene::Camera* cam = cameras[i];
			for (GeometrySet& set : cam->geometrySets) {
				set.setModelOffset = currentSetOffset;
				set.indexOffset = currentSetIndexOffset;
				currentSetOffset += set.modelSlotCount;
				currentSetIndexOffset += set.indexCount;
			}
			//There's probably some sort of vector union in C++ but I can't be bothered to find it right now. Duplicated code it is.
			for (GeometrySet& set : cam->selectedSets) {
				set.setModelOffset = currentSetOffset;
				set.indexOffset = currentSetIndexOffset;
				currentSetOffset += set.modelSlotCount;
				currentSetIndexOffset += set.indexCount;
			}
		}
		if (currentSetOffset > maxDispatchModelIds) {
			maxDispatchModelIds = calc_resize_half_grow(maxDispatchModelIds, currentSetOffset);
			maxTriangleCullDispatches = maxDispatchModelIds;
			dispatchModelIds->resize(maxDispatchModelIds);
			triangleCullArgs->resize(maxTriangleCullDispatches);
		}
		if (currentSetIndexOffset > indexSize) {
			uint32_t oldIndexSize = indexSize;
			indexSize = calc_resize_half_grow(indexSize, currentSetIndexOffset);
			resize_buffer(cmdBuf, geoSize, geoIndexSize, skinDataSize, skinGeoSize, oldIndexSize);
		}
		geoOffsetBuffer->update(offsets);
	}

	void WorldGeometryManager::send_data(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras) {
		static std::vector<GeometrySet*> geoSets{};
		for (scene::Camera* cam : cameras) {
			for (GeometrySet& set : cam->geometrySets) {
				geoSets.push_back(&set);
			}
			for (GeometrySet& set : cam->selectedSets) {
				geoSets.push_back(&set);
			}
		}
		//std::sort(geoSets.begin(), geoSets.end(), [](const GeometrySet* a, const GeometrySet* b) -> bool {
		//	return a->setId < b->setId;
		//});
		//Transfer geometry sets to gpu
		std::vector<VkBufferCopy> copyRanges{};
		uint32_t setStagingStart = dataStagingOffset;
		if (geoSets.size() > 0) {
			uint32_t newSize = calc_resize_half_grow(gpuGeometrySets->size(), currentGeoSetCount);
			gpuGeometrySets->resize(newSize);
			//Accumulate gpu struct geometry sets into staging buffer
			for (uint32_t i = 0; i < geoSets.size(); i++) {
				GeometrySet* geoSet = geoSets[i];
				uint32_t stagingOffset = check_resize_staging(sizeof(GPUGeometrySet));
				if (copyRanges.empty() || (geoSets[i - 1]->setId != (geoSet->setId - 1))) {
					copyRanges.push_back(VkBufferCopy{ stagingOffset, geoSet->setId * sizeof(GPUGeometrySet), 0 });
				}
				copyRanges.back().size += sizeof(GPUGeometrySet);
				GPUGeometrySet* copyToSet = reinterpret_cast<GPUGeometrySet*>(reinterpret_cast<uint8_t*>(dataStagingBuffer[currentFrame].allocation->map()) + stagingOffset);
				memcpy(copyToSet, geoSet->modelId, geoSet->setModelCount * 4);
				copyToSet->setModelOffset = geoSet->setModelOffset;
				copyToSet->setModelCount = geoSet->setModelCount;
				copyToSet->indexOffset = geoSet->indexOffset;
			}
			//Command copy staging buffer to device
			vkCmdCopyBuffer(cmdBuf, dataStagingBuffer[currentFrame].buffer, gpuGeometrySets->get_buffer(), copyRanges.size(), copyRanges.data());
			//Barrier to make sure this transfer is complete by the time we read a geometry set
			buffer_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, gpuGeometrySets->get_buffer(), 0, gpuGeometrySets->size() * sizeof(GPUGeometrySet));
			//dataStagingOffset += geoSets.size() * sizeof(GPUGeometrySet);
			geoSets.clear();
		}
		
		
		//Update mesh count
		if (meshes.size() > maxMeshCount) {
			maxMeshCount = calc_resize_half_grow(maxMeshCount, meshes.size());
			resize_and_copy_ssbo(cmdBuf, gpuMeshes, maxMeshCount);
		}

		//Update model count
		if (models.size() > maxModelCount) {
			maxModelCount = calc_resize_half_grow(maxModelCount, models.size());
			resize_and_copy_ssbo(cmdBuf, gpuModels, maxModelCount);
			resize_and_copy_ssbo(cmdBuf, modelTransforms, maxModelCount);
		}

		//Transfer model matrices to gpu
		copyRanges.clear();
		uint32_t matrixStagingStart = dataStagingOffset;
		for (uint32_t i = 0; i < models.size(); i++) {
			geom::Model* model = models[i];
			if (!model->transform_changed()) {
				continue;
			}
			uint32_t stagingOffset = check_resize_staging(sizeof(mat4f));
			if (copyRanges.empty() || !models[i-1]->transform_changed()) {
				copyRanges.push_back(VkBufferCopy{ stagingOffset, i * sizeof(mat4f), 0 });
			}
			memcpy(reinterpret_cast<uint8_t*>(dataStagingBuffer[currentFrame].allocation->map()) + stagingOffset, &model->get_transform(), sizeof(mat4f));
			
			copyRanges.back().size += sizeof(mat4f);
		}
		if (!copyRanges.empty()) {
			vkCmdCopyBuffer(cmdBuf, dataStagingBuffer[currentFrame].buffer, modelTransforms->get_buffer(), copyRanges.size(), copyRanges.data());
			buffer_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, modelTransforms->get_buffer(), 0, modelTransforms->size() * sizeof(mat4f));
		}

		//Transfer new meshes to gpu
		if (newMeshes.size() > 0) {
			copyRanges.clear();
			uint32_t meshStagingOffset = dataStagingOffset;
			//First is never the same as previous
			uint32_t previousMeshId = meshes.size();
			for (uint32_t i = 0; i < newMeshes.size(); i++) {
				uint32_t meshId = newMeshes[i];
				geom::Mesh* mesh = meshes[meshId];
				//We'll try to batch continuous transfers into the same copy range. Might make it more efficient, I don't know.
				uint32_t stagingOffset = check_resize_staging(sizeof(geom::GPUMesh));
				if (meshId != (previousMeshId + 1)) {
					copyRanges.push_back(VkBufferCopy{ stagingOffset, meshId * sizeof(geom::GPUMesh), 0 });
				}
				previousMeshId = meshId;
				geom::GPUMesh gpuMesh;
				mesh->pack(&gpuMesh);
				memcpy(reinterpret_cast<uint8_t*>(dataStagingBuffer[currentFrame].allocation->map()) + stagingOffset, &gpuMesh, sizeof(geom::GPUMesh));
				copyRanges.back().size += sizeof(geom::GPUMesh);
			}
			vkCmdCopyBuffer(cmdBuf, dataStagingBuffer[currentFrame].buffer, gpuMeshes->get_buffer(), copyRanges.size(), copyRanges.data());
			buffer_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, gpuMeshes->get_buffer(), 0, gpuMeshes->size() * sizeof(geom::GPUMesh));
			newMeshes.clear();
		}


		//Transfer new models to gpu
		if (newModels.size() > 0) {
			copyRanges.clear();
			uint32_t modelStagingOffset = dataStagingOffset;
			//First is never the same as previous
			uint32_t previousModelId = models.size();
			for (uint32_t i = 0; i < newModels.size(); i++) {
				uint32_t modelId = newModels[i];
				geom::Model* model = models[modelId];

				model->set_object_id(get_object_id(cmdBuf));
				objects[model->get_object_id()] = model;


				//We'll try to batch continuous transfers into the same copy range. Might make it more efficient, I don't know.
				uint32_t stagingOffset = check_resize_staging(sizeof(geom::GPUModel));
				if (modelId != (previousModelId + 1)) {
					copyRanges.push_back(VkBufferCopy{ stagingOffset, modelId * sizeof(geom::GPUModel), 0 });
				}
				previousModelId = modelId;
				geom::GPUModel gpuModel;
				model->pack(&gpuModel);
				memcpy(reinterpret_cast<uint8_t*>(dataStagingBuffer[currentFrame].allocation->map()) + stagingOffset, &gpuModel, sizeof(geom::GPUModel));
				copyRanges.back().size += sizeof(geom::GPUModel);
			}
			vkCmdCopyBuffer(cmdBuf, dataStagingBuffer[currentFrame].buffer, gpuModels->get_buffer(), copyRanges.size(), copyRanges.data());
			buffer_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, gpuModels->get_buffer(), 0, gpuModels->size() * sizeof(geom::GPUModel));
			newModels.clear();
		}

		//Transfer object updates to gpu
		copyRanges.clear();
		uint32_t objectStagingStart = dataStagingOffset;
		for (uint32_t i = 0; i < models.size(); i++) {
			geom::Model* model = models[i];
			if (!model->needs_object_update()) {
				continue;
			}
			model->set_needs_object_udpate(false);
			uint32_t stagingOffset = check_resize_staging(sizeof(geom::GPUObject));
			copyRanges.push_back(VkBufferCopy{ stagingOffset, model->get_object_id() * sizeof(geom::GPUObject), sizeof(geom::GPUObject) });
			geom::GPUObject obj;
			model->pack_object(&obj);
			memcpy(reinterpret_cast<uint8_t*>(dataStagingBuffer[currentFrame].allocation->map()) + stagingOffset, &obj, sizeof(geom::GPUObject));
		}
		if (!copyRanges.empty()) {
			vkCmdCopyBuffer(cmdBuf, dataStagingBuffer[currentFrame].buffer, gpuObjects->get_buffer(), copyRanges.size(), copyRanges.data());
			buffer_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, gpuObjects->get_buffer(), 0, gpuObjects->size() * sizeof(geom::GPUObject));
		}
	}

	void WorldGeometryManager::skin(VkCommandBuffer cmdBuf) {
		skinningSet->bind(cmdBuf, *skinningPipeline, 0);
	}

	void WorldGeometryManager::cull_depth_prepass(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras, RenderPass& renderPass, Framebuffer& framebuffer) {
		//Triangle culling pass
		triangleCullPipeline->bind(cmdBuf);
		uint32_t pushConstant[2] = { 0, 0 };
		vkCmdPushConstants(cmdBuf, triangleCullPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), 2 * sizeof(uint32_t), pushConstant);
		cullingSet->bind(cmdBuf, *triangleCullPipeline, 0);
		for (scene::Camera* cam : cameras) {
			cameraBuffer->set_offset_index(cam->cameraIndex);
			worldGeoDataSet->bind(cmdBuf, *triangleCullPipeline, 1);
			for (GeometrySet& set : cam->prevGeometrySets) {
				vkCmdPushConstants(cmdBuf, triangleCullPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDispatchIndirect(cmdBuf, triangleCullArgs->get_buffer(), set.setId * sizeof(VkDispatchIndirectCommand));
			}
		}

		memory_barrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT);
		
		renderPass.begin_pass(cmdBuf, framebuffer);
		depthPipeline->bind(cmdBuf);
		drawSet->bind(cmdBuf, *depthPipeline, 0);
		vkCmdBindIndexBuffer(cmdBuf, buffer.buffer, offsets.finalIndicesOffset * 4, VK_INDEX_TYPE_UINT32);

		for (scene::Camera* cam : cameras) {
			vkCmdSetViewport(cmdBuf, 0, 1, &cam->viewport);
			cameraBuffer->set_offset_index(cam->cameraIndex);
			worldGeoDataSet->bind(cmdBuf, *depthPipeline, 1);

			for (GeometrySet& set : cam->prevGeometrySets) {
				vkCmdPushConstants(cmdBuf, depthPipeline->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDrawIndexedIndirect(cmdBuf, drawArgs->get_buffer(), set.setId, 1, sizeof(VkDrawIndexedIndirectCommand));
			}
		}
		renderPass.end_pass(cmdBuf);
	}

	void WorldGeometryManager::cull(VkCommandBuffer cmdBuf, std::vector<scene::Camera*>& cameras) {
		memory_barrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT);

		uint32_t pushConstant[2] = { swapChainExtent.width/2, swapChainExtent.height/2 };
		vkCmdPushConstants(cmdBuf, triangleCullPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), 2 * sizeof(uint32_t), pushConstant);

		//Mesh culling pass
		modelCullPipeline->bind(cmdBuf);
		cullingSet->bind(cmdBuf, *modelCullPipeline, 0);
		for (scene::Camera* cam : cameras) {
			cameraBuffer->set_offset_index(cam->cameraIndex);
			worldGeoDataSet->bind(cmdBuf, *modelCullPipeline, 1);
			
			for (GeometrySet& set : cam->geometrySets) {
				vkCmdPushConstants(cmdBuf, modelCullPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDispatch(cmdBuf, 1, 1, 1);
			}
			for (GeometrySet& set : cam->selectedSets) {
				vkCmdPushConstants(cmdBuf, modelCullPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDispatch(cmdBuf, 1, 1, 1);
			}
		}

		memory_barrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
		
		//Triangle culling pass
		triangleCullPipeline->bind(cmdBuf);
		for (scene::Camera* cam : cameras) {
			cameraBuffer->set_offset_index(cam->cameraIndex);
			worldGeoDataSet->bind(cmdBuf, *triangleCullPipeline, 1);
			for (GeometrySet& set : cam->geometrySets) {
				vkCmdPushConstants(cmdBuf, triangleCullPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDispatchIndirect(cmdBuf, triangleCullArgs->get_buffer(), set.setId * sizeof(VkDispatchIndirectCommand));
			}
			for (GeometrySet& set : cam->selectedSets) {
				vkCmdPushConstants(cmdBuf, triangleCullPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDispatchIndirect(cmdBuf, triangleCullArgs->get_buffer(), set.setId * sizeof(VkDispatchIndirectCommand));
			}
		}

		memory_barrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT);
	}
	
	void WorldGeometryManager::draw(VkCommandBuffer cmdBuf, scene::Camera& cam, WorldRenderPass pass, int32_t overrideIndex) {
		GraphicsPipeline* pipeline = depthPipeline;
		switch (pass) {
		case WORLD_RENDER_PASS_DEPTH: pipeline = depthPipeline; break;
		case WORLD_RENDER_PASS_ID: pipeline = objectIdPipeline; break;
		case WORLD_RENDER_PASS_COLOR: pipeline = renderPipeline; break;
		}

		pipeline->bind(cmdBuf);
		cameraBuffer->set_offset_index((overrideIndex == -1) ? cam.cameraIndex : overrideIndex);
		worldGeoDataSet->bind(cmdBuf, *pipeline, 1);
		drawSet->bind(cmdBuf, *pipeline, 0);

		vkCmdBindIndexBuffer(cmdBuf, buffer.buffer, offsets.finalIndicesOffset * 4, VK_INDEX_TYPE_UINT32);
		if (pass == WORLD_RENDER_PASS_ID) {
			for (GeometrySet& set : cam.selectedSets) {
				vkCmdPushConstants(cmdBuf, pipeline->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDrawIndexedIndirect(cmdBuf, drawArgs->get_buffer(), set.setId * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
			}
		} else {
			for (GeometrySet& set : cam.geometrySets) {
				vkCmdPushConstants(cmdBuf, pipeline->get_layout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &set.setId);
				vkCmdDrawIndexedIndirect(cmdBuf, drawArgs->get_buffer(), set.setId * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
			}
			//vkCmdBindIndexBuffer(cmdBuf, buffer.buffer, offsets.indicesOffset * 4, VK_INDEX_TYPE_UINT16);
			//vkCmdDrawIndexed(cmdBuf, meshes[0]->get_index_count(), 1, 0, 0, 0);
		}
	}
}