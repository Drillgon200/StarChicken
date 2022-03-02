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
	private:
	protected:
		void update_sets(uint32_t frame);
	public:
		//Uniforms that need to be recreated when the swapchain is invalidated
		static std::vector<Uniform*> uniformsToRecreate;
		static std::vector<Uniform*> descriptorSetsToUpdate;
		std::vector<UniformBinding> affectedDescriptorSets;
		//For double buffering in case that's needed
		uint32_t bufferCount{ 0 };
		VkDescriptorType descriptorType;
		VkShaderStageFlags stageFlags{ 0 };
		VkDescriptorBindingFlags bindingFlags{ 0 };
		uint32_t dynamicOffset{ 0 };

		static void update_pending_sets(uint32_t frame);

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
		VkDescriptorType get_descriptor_type() {
			return descriptorType;
		}
	};

	template<typename T>
	class UniformBuffer : public Uniform {
	private:
		DeviceBuffer* buffers;
		VkDescriptorBufferInfo* bufferInfo;
		uint32_t size;
		uint32_t alignedSizeBytes;
		bool updatedPerFrame{ true };
	public:
		UniformBuffer(bool updatedPerFrame, bool dynamic, VkShaderStageFlags stageFlags) {
			this->descriptorType = dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			this->stageFlags = stageFlags;
			this->updatedPerFrame = updatedPerFrame;
			this->size = 1;
			uint32_t minAlignment = deviceProperties.limits.minUniformBufferOffsetAlignment;
			this->alignedSizeBytes = (sizeof(T) + minAlignment - 1) & ~(minAlignment - 1);
			create();
			Uniform::uniformsToRecreate.push_back(this);
		}

		~UniformBuffer() {
			destroy();
			Uniform::uniformsToRecreate.erase(std::find(Uniform::uniformsToRecreate.begin(), Uniform::uniformsToRecreate.end(), this));
		}

		void resize(uint32_t newSize) {
			if (newSize <= size) {
				return;
			}
			size = newSize;
			uint32_t newSizeBytes = newSize * alignedSizeBytes;
			for (uint32_t i = 0; i < bufferCount; i++) {
				generalAllocator->queue_free_buffer(currentFrame, buffers[i]);
				buffers[i] = generalAllocator->alloc_buffer(newSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				bufferInfo[i].buffer = buffers[i].buffer;
				bufferInfo[i].offset = 0;
				bufferInfo[i].range = sizeof(T);
			}
			descriptorSetsToUpdate.push_back(this);
			update_sets(currentFrame);
		}

		void create() override {
			bufferCount = updatedPerFrame ? NUM_FRAME_DATA : 1;
			buffers = new DeviceBuffer[bufferCount];
			bufferInfo = new VkDescriptorBufferInfo[bufferCount];
			for (uint32_t i = 0; i < bufferCount; i++) {
				buffers[i] = generalAllocator->alloc_buffer(alignedSizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				bufferInfo[i].buffer = buffers[i].buffer;
				bufferInfo[i].offset = 0;
				bufferInfo[i].range = sizeof(T);
			}
		}

		void destroy() override {
			for (uint32_t i = 0; i < bufferCount; i++) {
				generalAllocator->free_buffer(buffers[i]);
			}
			delete[] buffers;
			delete[] bufferInfo;
		}

		void update(T& data, uint32_t index) {
			if (index >= size) {
				resize(index + 1);
			}
			void* memory = buffers[currentFrame].allocation->map();
			memcpy(static_cast<uint8_t*>(memory) + index * alignedSizeBytes, &data, sizeof(T));
		}

		void update(T& data) {
			update(data, 0);
		}

		void set_offset_index(uint32_t index) {
			dynamicOffset = alignedSizeBytes * index;
		}

		VkDescriptorBufferInfo* get_buffer_info(uint32_t index) override {
			return &bufferInfo[index];
		}
	};

	template<typename T>
	class UniformSSBO : public Uniform {
	private:
		DeviceBuffer* buffers;
		VkDescriptorBufferInfo* bufferInfo;
		uint32_t maxObjects;
		VkMemoryPropertyFlags memoryFlags;
		VkBufferUsageFlagBits extraUsageFlags;
		bool updatedPerFrame{ true };
		bool gpuSide{ false };
	public:
		UniformSSBO(uint32_t maxObjects, bool updatedPerFrame, bool gpuSide, VkShaderStageFlags stageFlags, VkBufferUsageFlagBits extraUsage, DeviceBuffer* buffer) : gpuSide{ gpuSide }, extraUsageFlags{ extraUsage } {
			this->maxObjects = maxObjects;
			this->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			this->stageFlags = stageFlags;
			this->memoryFlags = gpuSide ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			this->updatedPerFrame = updatedPerFrame;
			set_buffer(maxObjects, buffer);
			create();
			Uniform::uniformsToRecreate.push_back(this);
		}

		UniformSSBO(uint32_t maxObjects, bool updatedPerFrame, VkShaderStageFlags stageFlags, VkBufferUsageFlagBits extraUsage) : UniformSSBO(maxObjects, updatedPerFrame, false, stageFlags, extraUsage, nullptr) {
		}

		UniformSSBO(uint32_t maxObjects, bool updatedPerFrame, bool gpuSide, VkShaderStageFlags stageFlags, DeviceBuffer* buffer) : UniformSSBO(maxObjects, updatedPerFrame, gpuSide, stageFlags, static_cast<VkBufferUsageFlagBits>(0), buffer) {

		}

		UniformSSBO(uint32_t maxObjects, bool updatedPerFrame, VkShaderStageFlags stageFlags) : UniformSSBO(maxObjects, updatedPerFrame, false, stageFlags, static_cast<VkBufferUsageFlagBits>(0), nullptr) {
		}

		~UniformSSBO() {
			destroy();
			Uniform::uniformsToRecreate.erase(std::find(Uniform::uniformsToRecreate.begin(), Uniform::uniformsToRecreate.end(), this));
		}

		void set_buffer(uint32_t sizeBytes, DeviceBuffer* buffer) {
			if (buffers && buffer) {
				buffers = buffer;
				bufferInfo[0].buffer = buffers[0].buffer;
				bufferInfo[0].offset = 0;
				bufferInfo[0].range = sizeBytes;

				descriptorSetsToUpdate.push_back(this);
				update_sets(currentFrame);
			} else {
				buffers = buffer;
			}
		}

		VkBuffer get_buffer() {
			if (!gpuSide) {
				throw std::runtime_error("Not a gpu side buffer!");
			}
			return buffers[0].buffer;
		}

		void create() override {
			bufferCount = updatedPerFrame ? NUM_FRAME_DATA : 1;
			if (gpuSide) {
				bufferCount = 1;
			}
			bufferInfo = new VkDescriptorBufferInfo[bufferCount];
			if (buffers) {
				bufferInfo[0].buffer = buffers[0].buffer;
				bufferInfo[0].offset = 0;
				bufferInfo[0].range = maxObjects;
			} else {
				buffers = new DeviceBuffer[bufferCount];
				for (uint32_t i = 0; i < bufferCount; i++) {
					buffers[i] = generalAllocator->alloc_buffer(sizeof(T) * maxObjects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extraUsageFlags, memoryFlags);
					bufferInfo[i].buffer = buffers[i].buffer;
					bufferInfo[i].offset = 0;
					bufferInfo[i].range = sizeof(T) * maxObjects;
				}
			}
		}

		void resize(uint32_t newSize) {
			if (newSize <= maxObjects) {
				return;
			}
			maxObjects = newSize;
			for (uint32_t i = 0; i < bufferCount; i++) {
				generalAllocator->queue_free_buffer(currentFrame, buffers[i]);
				buffers[i] = generalAllocator->alloc_buffer(sizeof(T) * maxObjects, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | extraUsageFlags, memoryFlags);
				bufferInfo[i].buffer = buffers[i].buffer;
				bufferInfo[i].offset = 0;
				bufferInfo[i].range = sizeof(T) * maxObjects;
			}
			descriptorSetsToUpdate.push_back(this);
			update_sets(currentFrame);
		}

		inline uint32_t size() {
			return maxObjects;
		}

		void destroy() override {
			if (buffers) {
				for (uint32_t i = 0; i < bufferCount; i++) {
					generalAllocator->free_buffer(buffers[i]);
				}
				delete[] buffers;
			}
			delete[] bufferInfo;
		}

		void update(T& data, uint32_t index) {
			if (gpuSide) {
				throw std::runtime_error("Can't directly copy into a gpu side SSBO. Use your own staging buffer!");
			}
			if (index >= maxObjects) {
				resize(index + 1);
			}
			void* memory = reinterpret_cast<uint8_t*>(buffers[currentFrame].allocation->map()) + index * sizeof(T);
			memcpy(memory, &data, sizeof(T));
		}

		void update(T* data, uint32_t index, uint32_t count) {
			if (gpuSide) {
				throw std::runtime_error("Can't directly copy into a gpu side SSBO. Use your own staging buffer!");
			}
			if ((index + count) >= size) {
				resize(index + count + 1);
			}
			void* memory = reinterpret_cast<uint8_t*>(buffers[currentFrame].allocation->map()) + index * sizeof(T);
			memcpy(memory, &data, sizeof(T) * count);
		}

		void* map() {
			if (gpuSide) {
				throw std::runtime_error("Can't map device local buffer!");
			}
			return buffers[currentFrame].allocation->map();
		}

		VkDescriptorBufferInfo* get_buffer_info(uint32_t index) override {
			if (gpuSide) {
				return bufferInfo;
			}
			return &bufferInfo[index];
		}
	};

	class UniformSampler2D : public Uniform {
	private:
		VkDescriptorImageInfo samplerInfo{};
		VkSampler sampler{};
	public:
		UniformSampler2D(bool filter, bool mipmap, bool anisotropic = true, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, VkShaderStageFlags samplerStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, VkSamplerReductionMode reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE, VkBorderColor border = VK_BORDER_COLOR_INT_OPAQUE_BLACK);

		~UniformSampler2D();

		VkDescriptorImageInfo* get_image_info(uint32_t index) override;
	};

	class UniformTexture2D : public Uniform {
	private:
		friend class UniformStorageImage;
		VkDescriptorImageInfo imageInfo;
	public:
		UniformTexture2D(VkImageView imageView, VkShaderStageFlags shaderStageFlags, VkImageLayout overrideLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		void update_texture(uint32_t frame, Texture* tex);
		void update_texture(uint32_t frame, VkImageView imageView);
		VkDescriptorImageInfo* get_image_info(uint32_t index) override;
		uint32_t get_descriptor_count() override;
	};

	class UniformStorageImage : public UniformTexture2D {
	public:
		UniformStorageImage(VkImageView imageView);
		UniformStorageImage(Texture* tex);
	};

	class UniformTexture2DArray : public Uniform {
	private:
		VkDescriptorImageInfo* imageInfos;
		std::vector<uint32_t> freeSlots{};
		uint16_t descriptorCount;
		
		uint16_t arraySize;
	public:
		UniformTexture2DArray(uint16_t startArraySize);
		~UniformTexture2DArray();

		void create() override;

		uint16_t alloc_descriptor_slot();

		void free_descriptor_slot(uint16_t idx);

		void update_texture(uint32_t index, VkImageView imageView);
		void update_texture(uint32_t index, Texture* tex);
		uint16_t add_texture(Texture* tex);

		void destroy() override;

		VkDescriptorImageInfo* get_image_info(uint32_t index) override;
		uint32_t get_descriptor_count() override;
	};

	class DescriptorSet {
	private:
		friend class DescriptorAllocator;
		VkDescriptorSetLayout layout;
		VkDescriptorPool descriptorPool;
		VkDescriptorSet* descriptorSets;
		Uniform** uniforms;
		uint32_t numUniforms;
		uint32_t setCount;
	public:

		DescriptorSet();

		DescriptorSet(std::initializer_list<Uniform*> uniformList);

		DescriptorSet(const DescriptorSet& set);

		DescriptorSet& operator=(const DescriptorSet& set);

		~DescriptorSet();

		void recreate_set_layout();

		void create_descriptor_sets();

		void destroy_descriptor_sets();

		void bind(VkCommandBuffer cmdBuf, GraphicsPipeline& pipeline, uint32_t set);
		void bind(VkCommandBuffer cmdBuf, ComputePipeline& pipeline, uint32_t set);

		inline VkDescriptorSetLayout* get_layout() {
			return &layout;
		}

		inline VkDescriptorSet* get_sets() {
			return descriptorSets;
		}
		inline uint32_t get_set_count() {
			return setCount;
		}
		inline Uniform** get_uniforms() {
			return uniforms;
		}
		inline uint32_t get_uniform_count() {
			return numUniforms;
		}
	};

	class DescriptorAllocator {
	private:
		static constexpr uint32_t NUM_DEFAULT_DESCRIPTOR_SIZES = 11;
		static constexpr VkDescriptorPoolSize defaultPoolSizes[NUM_DEFAULT_DESCRIPTOR_SIZES] {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 500 } };
		std::vector<VkDescriptorPool> pools;
	public:
		void init();
		void cleanup();

		VkDescriptorPool alloc_new_pool(DescriptorSet* set);

		bool alloc_sets(DescriptorSet* set);
		void free_sets(DescriptorSet* set);
	};
}