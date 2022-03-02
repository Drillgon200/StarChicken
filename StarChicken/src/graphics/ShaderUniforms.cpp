#include "ShaderUniforms.h"

namespace vku {
	std::vector<Uniform*> Uniform::uniformsToRecreate;
	std::vector<Uniform*> Uniform::descriptorSetsToUpdate;

	void Uniform::update_sets(uint32_t frame) {
		for (UniformBinding& binding : affectedDescriptorSets) {
			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = descriptorType;
			write.dstArrayElement = 0;
			write.dstBinding = binding.binding;
			write.dstSet = binding.set->get_sets()[frame];
			write.descriptorCount = get_descriptor_count();
			write.pBufferInfo = get_buffer_info(frame);
			write.pImageInfo = get_image_info(frame);
			write.pTexelBufferView = nullptr;
			write.pNext = nullptr;
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
	}

	void Uniform::update_pending_sets(uint32_t frame) {
		for (Uniform* uni : descriptorSetsToUpdate) {
			uni->update_sets(frame);
		}
		descriptorSetsToUpdate.clear();
	}

	UniformSampler2D::UniformSampler2D(bool filter, bool mipmap, bool anisotropic, VkSamplerAddressMode addressMode, VkShaderStageFlags samplerStageFlags, VkSamplerReductionMode reductionMode, VkBorderColor border) {
		this->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		this->stageFlags = samplerStageFlags;
		VkSamplerCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.addressModeU = addressMode;
		info.addressModeV = addressMode;
		info.addressModeW = addressMode;
		info.anisotropyEnable = anisotropic ? VK_TRUE : VK_FALSE;
		info.maxAnisotropy = anisotropic ? deviceProperties.limits.maxSamplerAnisotropy : 0;
		info.borderColor = border;
		info.compareEnable = VK_FALSE;
		info.compareOp = VK_COMPARE_OP_ALWAYS;
		info.minFilter = filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		info.magFilter = filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		info.mipmapMode = filter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		info.mipLodBias = 0.0F;
		info.minLod = 0.0F;
		info.maxLod = VK_LOD_CLAMP_NONE;
		info.flags = 0;

		VkSamplerReductionModeCreateInfo reduction{};
		reduction.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
		reduction.reductionMode = reductionMode;
		info.pNext = &reduction;

		VKU_CHECK_RESULT(vkCreateSampler(device, &info, nullptr, &sampler), "Failed to create sampler!");

		//Only sampler is accessed for this descriptor type
		samplerInfo.sampler = sampler;
	}

	UniformSampler2D::~UniformSampler2D() {
		vkDestroySampler(device, sampler, nullptr);
	}

	VkDescriptorImageInfo* UniformSampler2D::get_image_info(uint32_t index) {
		return &samplerInfo;
	}

	UniformTexture2DArray::UniformTexture2DArray(uint16_t startArraySize) :
		imageInfos{ nullptr },
		arraySize{ startArraySize },
		descriptorCount{ 0 }
	{
		this->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		this->stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		this->bindingFlags = VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
		create();
		Uniform::uniformsToRecreate.push_back(this);
	}
	UniformTexture2DArray::~UniformTexture2DArray() {
		delete[] imageInfos;
		Uniform::uniformsToRecreate.erase(std::find(Uniform::uniformsToRecreate.begin(), Uniform::uniformsToRecreate.end(), this));
	}

	void UniformTexture2DArray::create() {
		if (!imageInfos) {
			imageInfos = new VkDescriptorImageInfo[arraySize];
			for (uint16_t i = 0; i < arraySize; i++) {
				imageInfos[i].sampler = VK_NULL_HANDLE;
				imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfos[i].imageView = missingTexture->get_image_view();
			}
		}
	}

	uint16_t UniformTexture2DArray::alloc_descriptor_slot() {
		if (!freeSlots.empty()) {
			uint16_t idx = freeSlots.back();
			freeSlots.pop_back();
			return idx;
		}
		uint16_t idx = descriptorCount++;
		if (idx == arraySize) {
			if (arraySize * 2 >= UINT16_MAX) {
				throw std::runtime_error("Ran out of descriptor slots for texture array");
			}
			arraySize *= 2;
			VkDescriptorImageInfo* oldInfos = imageInfos;
			imageInfos = new VkDescriptorImageInfo[arraySize];
			memcpy(imageInfos, oldInfos, idx * sizeof(VkDescriptorImageInfo));
			delete[] oldInfos;
			for (uint16_t i = idx; i < arraySize; i++) {
				imageInfos[i].sampler = nullptr;
				imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfos[i].imageView = missingTexture->get_image_view();
			}
			for (UniformBinding& binding : affectedDescriptorSets) {
				binding.set->destroy_descriptor_sets();
				binding.set->recreate_set_layout();
			}
			for (UniformBinding& binding : affectedDescriptorSets) {
				binding.set->create_descriptor_sets();
			}
			/*destroy_descriptor_sets();
			recreate_descriptor_set_layouts();
			recreate_graphics_pipelines();
			create_descriptor_sets();*/
		}
		return idx;
	}

	void UniformTexture2DArray::free_descriptor_slot(uint16_t idx) {
		freeSlots.push_back(idx);
		update_texture(idx, vku::missingTexture);
	}

	void UniformTexture2DArray::update_texture(uint32_t index, VkImageView imageView) {
		imageInfos[index].imageView = imageView;
		for (UniformBinding& binding : affectedDescriptorSets) {
			for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
				VkWriteDescriptorSet write{};
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.descriptorCount = 1;
				write.dstArrayElement = index;
				write.descriptorType = descriptorType;
				write.dstBinding = binding.binding;
				write.dstSet = binding.set->get_sets()[i];
				write.pBufferInfo = nullptr;
				write.pImageInfo = &imageInfos[index];
				write.pTexelBufferView = nullptr;
				vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
			}
		}
	}

	void UniformTexture2DArray::update_texture(uint32_t index, Texture* tex) {
		update_texture(index, tex->get_image_view());
	}
	uint16_t UniformTexture2DArray::add_texture(Texture* tex) {
		uint16_t slot = alloc_descriptor_slot();
		update_texture(slot, tex);
		return slot;
	}

	void UniformTexture2DArray::destroy() {

	}
	VkDescriptorImageInfo* UniformTexture2DArray::get_image_info(uint32_t index) {
		return imageInfos;
	}
	uint32_t UniformTexture2DArray::get_descriptor_count() {
		return arraySize;
	}

	UniformTexture2D::UniformTexture2D(VkImageView imageView, VkShaderStageFlags shaderStageFlags, VkImageLayout overrideLayout) {
		this->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		this->stageFlags = shaderStageFlags;
		imageInfo.imageLayout = overrideLayout;
		imageInfo.imageView = imageView;
		imageInfo.sampler = VK_NULL_HANDLE;
	}

	UniformStorageImage::UniformStorageImage(VkImageView imageView) : UniformTexture2D(imageView, VK_SHADER_STAGE_COMPUTE_BIT){
		this->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		this->imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	UniformStorageImage::UniformStorageImage(Texture* tex) : UniformStorageImage(tex->get_image_view()) {
	}

	void UniformTexture2D::update_texture(uint32_t frame, Texture* tex) {
		update_texture(frame, tex->get_image_view());
	}

	void UniformTexture2D::update_texture(uint32_t frame, VkImageView imageView) {
		imageInfo.imageView = imageView;
		for (UniformBinding& binding : affectedDescriptorSets) {
			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorCount = 1;
			write.dstArrayElement = 0;
			write.descriptorType = descriptorType;
			write.dstBinding = binding.binding;
			write.dstSet = binding.set->get_sets()[frame];
			write.pBufferInfo = nullptr;
			write.pImageInfo = &imageInfo;
			write.pTexelBufferView = nullptr;
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
	}

	VkDescriptorImageInfo* UniformTexture2D::get_image_info(uint32_t index) {
		return &imageInfo;
	}

	uint32_t UniformTexture2D::get_descriptor_count() {
		return 1;
	}

	DescriptorSet::DescriptorSet() {
	}

	DescriptorSet::DescriptorSet(std::initializer_list<Uniform*> uniformList) {
		uniforms = new Uniform * [uniformList.size()];
		numUniforms = uniformList.size();

		uint32_t bindingIdx = 0;
		VkDescriptorSetLayoutBinding* bindings = reinterpret_cast<VkDescriptorSetLayoutBinding*>(alloca(uniformList.size() * sizeof(VkDescriptorSetLayoutBinding)));
		VkDescriptorSetLayoutBindingFlagsCreateInfo flags{};
		flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		flags.bindingCount = numUniforms;
		VkDescriptorBindingFlags* bindingFlags = reinterpret_cast<VkDescriptorBindingFlags*>(alloca(uniformList.size() * sizeof(VkDescriptorBindingFlags)));
		flags.pBindingFlags = bindingFlags;
		for (Uniform* const uni : uniformList) {
			bindings[bindingIdx].binding = bindingIdx;
			bindings[bindingIdx].descriptorCount = uni->get_descriptor_count();
			bindings[bindingIdx].descriptorType = uni->descriptorType;
			bindings[bindingIdx].pImmutableSamplers = nullptr;
			bindings[bindingIdx].stageFlags = uni->stageFlags;
			bindingFlags[bindingIdx] = uni->bindingFlags;

			uniforms[bindingIdx] = uni;
			uni->affectedDescriptorSets.push_back({ this, bindingIdx });

			bindingIdx++;
		}
		VkDescriptorSetLayoutCreateInfo setInfo{};
		setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setInfo.bindingCount = uniformList.size();
		setInfo.pBindings = bindings;
		setInfo.flags = 0;
		setInfo.pNext = &flags;
		VKU_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &layout), "Failed to create descriptor set layout!");
	}

	DescriptorSet::DescriptorSet(const DescriptorSet& set) {
		this->layout = set.layout;
		this->descriptorSets = set.descriptorSets;
		this->uniforms = set.uniforms;
		this->numUniforms = set.numUniforms;
		this->setCount = set.setCount;
	}

	DescriptorSet& DescriptorSet::operator=(const DescriptorSet& set) {
		this->layout = set.layout;
		this->descriptorSets = set.descriptorSets;
		this->uniforms = set.uniforms;
		this->numUniforms = set.numUniforms;
		this->setCount = set.setCount;
		return *this;
	}

	DescriptorSet::~DescriptorSet() {
		vkDestroyDescriptorSetLayout(device, layout, nullptr);
	}

	void DescriptorSet::recreate_set_layout() {
		vkDestroyDescriptorSetLayout(device, layout, nullptr);

		VkDescriptorSetLayoutBinding* bindings = reinterpret_cast<VkDescriptorSetLayoutBinding*>(alloca(numUniforms * sizeof(VkDescriptorSetLayoutBinding)));
		VkDescriptorSetLayoutBindingFlagsCreateInfo flags{};
		flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		flags.bindingCount = numUniforms;
		VkDescriptorBindingFlags* bindingFlags = reinterpret_cast<VkDescriptorBindingFlags*>(alloca(numUniforms * sizeof(VkDescriptorBindingFlags)));
		flags.pBindingFlags = bindingFlags;
		for (uint32_t bindingIdx = 0; bindingIdx < numUniforms; bindingIdx++) {
			bindings[bindingIdx].binding = bindingIdx;
			bindings[bindingIdx].descriptorCount = uniforms[bindingIdx]->get_descriptor_count();
			bindings[bindingIdx].descriptorType = uniforms[bindingIdx]->descriptorType;
			bindings[bindingIdx].pImmutableSamplers = nullptr;
			bindings[bindingIdx].stageFlags = uniforms[bindingIdx]->stageFlags;
			bindingFlags[bindingIdx] = uniforms[bindingIdx]->bindingFlags;
		}
		VkDescriptorSetLayoutCreateInfo setInfo{};
		setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setInfo.bindingCount = numUniforms;
		setInfo.pBindings = bindings;
		setInfo.flags = 0;
		setInfo.pNext = &flags;
		VKU_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &layout), "Failed to recreate descriptor set layout!");
	}

	void DescriptorSet::create_descriptor_sets() {
		setCount = NUM_FRAME_DATA;
		descriptorSets = new VkDescriptorSet[setCount];
		descriptorSetAllocator->alloc_sets(this);

		VkWriteDescriptorSet* descriptorWrites = reinterpret_cast<VkWriteDescriptorSet*>(alloca(numUniforms * sizeof(VkWriteDescriptorSet)));
		for (uint32_t set = 0; set < setCount; set++) {
			for (uint32_t i = 0; i < numUniforms; i++) {
				descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrites[i].descriptorType = uniforms[i]->descriptorType;
				descriptorWrites[i].dstArrayElement = 0;
				descriptorWrites[i].dstBinding = i;
				descriptorWrites[i].dstSet = descriptorSets[set];
				descriptorWrites[i].descriptorCount = uniforms[i]->get_descriptor_count();
				descriptorWrites[i].pBufferInfo = uniforms[i]->get_buffer_info(set);
				descriptorWrites[i].pImageInfo = uniforms[i]->get_image_info(set);
				descriptorWrites[i].pTexelBufferView = nullptr;
				descriptorWrites[i].pNext = nullptr;
			}
			vkUpdateDescriptorSets(device, numUniforms, descriptorWrites, 0, nullptr);
		}
	}

	void DescriptorSet::destroy_descriptor_sets() {
		descriptorSetAllocator->free_sets(this);
		delete[] descriptorSets;
	}

	void DescriptorSet::bind(VkCommandBuffer cmdBuf, GraphicsPipeline& pipeline, uint32_t set) {
		uint32_t* dynamicOffsets = reinterpret_cast<uint32_t*>(alloca(numUniforms * sizeof(uint32_t)));
		uint32_t dynamicCount = 0;
		for (uint32_t i = 0; i < numUniforms; i++) {
			if ((uniforms[i]->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) || (uniforms[i]->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)) {
				dynamicOffsets[dynamicCount] = uniforms[i]->dynamicOffset;
				++dynamicCount;
			}
		}
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get_layout(), set, 1, &descriptorSets[currentFrame], dynamicCount, dynamicOffsets);
	}
	void DescriptorSet::bind(VkCommandBuffer cmdBuf, ComputePipeline& pipeline, uint32_t set) {
		uint32_t* dynamicOffsets = reinterpret_cast<uint32_t*>(alloca(numUniforms * sizeof(uint32_t)));
		uint32_t dynamicCount = 0;
		for (uint32_t i = 0; i < numUniforms; i++) {
			if ((uniforms[i]->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) || (uniforms[i]->descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)) {
				dynamicOffsets[dynamicCount] = uniforms[i]->dynamicOffset;
				++dynamicCount;
			}
		}
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.get_layout(), set, 1, &descriptorSets[currentFrame], dynamicCount, dynamicOffsets);
	}

	void DescriptorAllocator::init() {

	}
	void DescriptorAllocator::cleanup() {
		for (VkDescriptorPool pool : pools) {
			vkDestroyDescriptorPool(device, pool, nullptr);
		}
	}

	void get_descriptor_pool_sizes(DescriptorSet* set, std::vector<VkDescriptorPoolSize>& sizes) {
		Uniform** uniforms = set->get_uniforms();
		for (uint32_t i = 0; i < set->get_uniform_count(); i++) {
			for (VkDescriptorPoolSize& size : sizes) {
				if (size.type == uniforms[i]->get_descriptor_type()) {
					size.descriptorCount += uniforms[i]->get_descriptor_count() * NUM_FRAME_DATA;
					goto nextUniform;
				}
			}
			sizes.push_back(VkDescriptorPoolSize{ uniforms[i]->get_descriptor_type(), uniforms[i]->get_descriptor_count() * NUM_FRAME_DATA });
		nextUniform:;
		}
	}

	VkDescriptorPool DescriptorAllocator::alloc_new_pool(DescriptorSet* set) {
		std::vector<VkDescriptorPoolSize> sizes{};
		get_descriptor_pool_sizes(set, sizes);

		VkDescriptorPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.maxSets = 1000;
		createInfo.poolSizeCount = NUM_DEFAULT_DESCRIPTOR_SIZES;
		VkDescriptorPoolSize poolSizes[NUM_DEFAULT_DESCRIPTOR_SIZES];
		memcpy(poolSizes, defaultPoolSizes, NUM_DEFAULT_DESCRIPTOR_SIZES * sizeof(VkDescriptorPoolSize));
		for (VkDescriptorPoolSize& size : sizes) {
			poolSizes[size.type].descriptorCount = std::max(poolSizes[size.type].descriptorCount, size.descriptorCount);
		}
		createInfo.pPoolSizes = poolSizes;
		createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

		VkDescriptorPool pool;
		VKU_CHECK_RESULT(vkCreateDescriptorPool(device, &createInfo, nullptr, &pool), "Failed to allocate descriptor pool!");
		return pool;
	}

	bool DescriptorAllocator::alloc_sets(DescriptorSet* set) {
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		std::vector<VkDescriptorSetLayout> layouts(set->get_set_count(), *set->get_layout());
		allocInfo.pSetLayouts = layouts.data();
		allocInfo.descriptorSetCount = set->get_set_count();
		for (uint32_t i = 0; i < pools.size(); i++) {
			allocInfo.descriptorPool = pools[i];
			VkResult allocateResult = vkAllocateDescriptorSets(device, &allocInfo, set->get_sets());
			switch (allocateResult) {
			case VK_SUCCESS:
				set->descriptorPool = pools[i];
				return true;
			case VK_ERROR_FRAGMENTED_POOL:
			case VK_ERROR_OUT_OF_POOL_MEMORY:
				break;
			default:
				return false;
			}
		}
		VkDescriptorPool pool = alloc_new_pool(set);
		pools.push_back(pool);
		allocInfo.descriptorPool = pool;
		VkResult allocateResult = vkAllocateDescriptorSets(device, &allocInfo, set->get_sets());
		if (allocateResult == VK_SUCCESS) {
			set->descriptorPool = pool;
			return true;
		}
		return false;
	}

	void DescriptorAllocator::free_sets(DescriptorSet* set) {
		vkFreeDescriptorSets(device, set->descriptorPool, set->get_set_count(), set->get_sets());
	}
}