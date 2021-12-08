#pragma once

#include "DeviceMemoryAllocator.h"
#include "GraphicsPipeline.h"
#include "VkUtil.h"
#include "TextureManager.h"
#include <initializer_list>

namespace vku {

	class DescriptorSet;

	struct UniformBinding {
		DescriptorSet* set;
		uint32_t binding;
	};

	class Uniform {
	public:
		//Uniforms that need to be recreated when the swapchain is invalidated
		static std::vector<Uniform*> uniformsToRecreate;
		std::vector<UniformBinding> affectedDescriptorSets;
		uint32_t swapchainImagesSize{ 0 };
		VkDescriptorType descriptorType;
		VkShaderStageFlags stageFlags;

		virtual void create() {
		}

		virtual void destroy() {
		}

		virtual VkDescriptorBufferInfo* get_buffer_info(uint32_t index) {
			return nullptr;
		}
		virtual VkDescriptorImageInfo* get_image_info(uint32_t index) {
			return nullptr;
		}
		virtual uint32_t get_descriptor_count() {
			return 1;
		}
	};

	template<typename T>
	class UniformBuffer : public Uniform {
	private:
		DeviceBuffer* buffers;
		VkDescriptorBufferInfo* bufferInfo;
		bool updatedPerFrame{ true };
	public:
		UniformBuffer(bool updatedPerFrame = true) {
			this->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			this->stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			this->updatedPerFrame = updatedPerFrame;
			create();
			Uniform::uniformsToRecreate.push_back(this);
		}

		~UniformBuffer() {
			destroy();
			Uniform::uniformsToRecreate.erase(std::find(Uniform::uniformsToRecreate.begin(), Uniform::uniformsToRecreate.end(), this));
		}

		void create() override {
			swapchainImagesSize = updatedPerFrame ? swapchainImageCount : 1;
			buffers = new DeviceBuffer[swapchainImagesSize];
			bufferInfo = new VkDescriptorBufferInfo[swapchainImagesSize];
			for (uint32_t i = 0; i < swapchainImagesSize; i++) {
				buffers[i] = generalAllocator->alloc_buffer(sizeof(T), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				bufferInfo[i].buffer = buffers[i].buffer;
				bufferInfo[i].offset = 0;
				bufferInfo[i].range = sizeof(T);
			}
		}

		void destroy() override {
			for (uint32_t i = 0; i < swapchainImagesSize; i++) {
				generalAllocator->free_buffer(buffers[i]);
			}
			delete[] buffers;
			delete[] bufferInfo;
		}

		void update(T& data) {
			void* memory = buffers[swapchainImageIndex].allocation->map();
			memcpy(memory, &data, sizeof(T));
		}

		VkDescriptorBufferInfo* get_buffer_info(uint32_t index) override {
			return &bufferInfo[index];
		}
	};

	class UniformSampler2D : public Uniform {
	private:
		VkDescriptorImageInfo samplerInfo{};
		VkSampler sampler{};
	public:
		UniformSampler2D(bool filter, bool mipmap, bool anisotropic = true, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, VkBorderColor border = VK_BORDER_COLOR_INT_OPAQUE_BLACK) {
			this->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			this->stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			VkSamplerCreateInfo info{};
			info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			info.addressModeU = addressMode;
			info.addressModeV = addressMode;
			info.addressModeW = addressMode;
			info.anisotropyEnable = anisotropic ? VK_TRUE : VK_FALSE;
			info.maxAnisotropy = anisotropic ? deviceProperties.limits.maxSamplerAnisotropy : 0;
			info.borderColor = border;
			info.compareEnable = VK_FALSE;
			info.compareOp + VK_COMPARE_OP_ALWAYS;
			info.minFilter = filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
			info.magFilter = filter ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
			info.mipmapMode = filter ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
			info.mipLodBias = 0.0F;
			info.minLod = 0.0F;
			info.maxLod = VK_LOD_CLAMP_NONE;
			info.flags = 0;

			VKU_CHECK_RESULT(vkCreateSampler(device, &info, nullptr, &sampler), "Failed to create sampler!");

			//Only sampler is accessed for this descriptor type
			samplerInfo.sampler = sampler;
		}

		~UniformSampler2D() {
			vkDestroySampler(device, sampler, nullptr);
		}

		VkDescriptorImageInfo* get_image_info(uint32_t index) override {
			return &samplerInfo;
		}
	};

	class UniformTexture2DArray : public Uniform {
	private:
		VkDescriptorImageInfo* imageInfos;
		uint32_t arraySize;
	public:
		UniformTexture2DArray(uint32_t arraySize) :
			imageInfos{ nullptr },
			arraySize{ arraySize }
		{
			this->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			this->stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			create();
			Uniform::uniformsToRecreate.push_back(this);
		}
		~UniformTexture2DArray() {
			Uniform::uniformsToRecreate.erase(std::find(Uniform::uniformsToRecreate.begin(), Uniform::uniformsToRecreate.end(), this));
		}

		void create() override {
			if (!imageInfos) {
				imageInfos = new VkDescriptorImageInfo[arraySize];
				for (uint32_t i = 0; i < arraySize; i++) {
					imageInfos[i].sampler = nullptr;
					imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageInfos[i].imageView = missingTexture->get_image_view();
				}
			}
		}

		void update_texture(uint32_t index, Texture* tex);

		void destroy() override {

		}

		VkDescriptorImageInfo* get_image_info(uint32_t index) override {
			return imageInfos;
		}

		uint32_t get_descriptor_count() override {
			return arraySize;
		}
	};

	class DescriptorSet {
	private:
		VkDescriptorSetLayout layout;
		VkDescriptorSet* descriptorSets;
		Uniform** uniforms;
		uint32_t numUniforms;
		uint32_t setCount;
	public:

		DescriptorSet() {
		}

		DescriptorSet(std::initializer_list<Uniform*> uniformList) {
			VkDescriptorSetLayoutBinding* bindings = reinterpret_cast<VkDescriptorSetLayoutBinding*>(alloca(uniformList.size() * sizeof(VkDescriptorSetLayoutBinding)));
			uint32_t bindingIdx = 0;
			uniforms = new Uniform* [uniformList.size()];
			numUniforms = uniformList.size();
			for (Uniform* const uni : uniformList) {
				bindings[bindingIdx].binding = bindingIdx;
				bindings[bindingIdx].descriptorCount = 1;
				bindings[bindingIdx].descriptorType = uni->descriptorType;
				bindings[bindingIdx].pImmutableSamplers = nullptr;
				bindings[bindingIdx].stageFlags = uni->stageFlags;

				uniforms[bindingIdx] = uni;
				uni->affectedDescriptorSets.push_back({ this, bindingIdx });

				bindingIdx++;
			}
			VkDescriptorSetLayoutCreateInfo setInfo{};
			setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			setInfo.bindingCount = uniformList.size();
			setInfo.pBindings = bindings;
			setInfo.flags = 0;
			VKU_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &layout), "Failed to create descriptor set layout!");
		}

		DescriptorSet(const DescriptorSet& set) {
			this->layout = set.layout;
			this->descriptorSets = set.descriptorSets;
			this->uniforms = set.uniforms;
			this->numUniforms = set.numUniforms;
			this->setCount = set.setCount;
		}

		DescriptorSet& operator=(const DescriptorSet& set) {
			this->layout = set.layout;
			this->descriptorSets = set.descriptorSets;
			this->uniforms = set.uniforms;
			this->numUniforms = set.numUniforms;
			this->setCount = set.setCount;
			return *this;
		}

		~DescriptorSet() {
			vkDestroyDescriptorSetLayout(device, layout, nullptr);
		}

		void create_descriptor_sets() {
			std::vector<VkDescriptorSetLayout> layouts(swapchainImageCount, layout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = swapchainImageCount;
			allocInfo.pSetLayouts = layouts.data();
			descriptorSets = new VkDescriptorSet[swapchainImageCount];
			VKU_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets), "Failed to allocate descriptor sets!");
			setCount = swapchainImageCount;

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

		void destroy_descriptor_sets() {
			delete[] descriptorSets;
		}

		void bind(VkCommandBuffer cmdBuf, GraphicsPipeline& pipeline) {
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get_layout() , 0, 1, &descriptorSets[swapchainImageIndex], 0, nullptr);
		}

		VkDescriptorSetLayout* get_layout() {
			return &layout;
		}

		VkDescriptorSet* get_sets() {
			return descriptorSets;
		}
	};
}