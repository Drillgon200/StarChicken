#include "TextureManager.h"
#include "..\util\Util.h"
#include "StagingManager.h"

namespace vku {
	std::vector<Texture*> Texture::newTextures;

	Texture::Texture(VkImageView imageView, VkImageLayout imageLayout, VkImage image) :
		mipImageViews{ nullptr },
		imageLayout{ imageLayout },
		image{ image },
		memory{ nullptr },
		textureType{ TEXTURE_TYPE_NORMAL }
	{
		mipImageViews = new VkImageView[1];
		mipImageViews[0] = imageView;
	}
	Texture::Texture() :
		mipImageViews{ nullptr },
		imageLayout{},
		image{},
		memory{ nullptr },
		textureType{ TEXTURE_TYPE_NORMAL } 
	{
	}
	Texture::~Texture() {
		delete[] mipImageViews;
	}

	void Texture::create(VkCommandBuffer cmdBuf, uint32_t width, uint32_t height, uint32_t mipLevels, void* data, TextureType textureType, VkImageUsageFlags extraUsage) {
		assert(!memory);
		this->textureType = textureType;
		uint32_t imageSize = width * height;
		if (textureType != TEXTURE_TYPE_COMPRESSED) {
			imageSize *= 4;
		}
		if ((textureType == TEXTURE_TYPE_COLOR_ATTACHMENT_16F) || (textureType == TEXTURE_TYPE_DEPTH_MINMAX_MIP)) {
			imageSize *= 2;
		}
		//DeviceBuffer stagingBuffer{};
		VkCommandBuffer stagingCmdBuf{};
		VkBuffer stagingBuffer{};
		uint32_t stagingOffset{};

		if (data) {
			//stagingBuffer = generalAllocator->alloc_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			//void* mapping = stagingBuffer.allocation->map();
			void* mapping = transferStagingManager->stage(imageSize, 16, stagingCmdBuf, stagingBuffer, stagingOffset);
			memcpy(mapping, data, imageSize);
		}
		
		VkFormat imageFormat{};
		switch (textureType) {
		case TEXTURE_TYPE_NORMAL: imageFormat = VK_FORMAT_R8G8B8A8_UNORM; break;
		case TEXTURE_TYPE_COMPRESSED: imageFormat = VK_FORMAT_BC7_SRGB_BLOCK; break;
		case TEXTURE_TYPE_DEPTH_ATTACHMENT: imageFormat = VK_FORMAT_D32_SFLOAT; break;
		case TEXTURE_TYPE_COLOR_ATTACHMENT_16F: imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT; break;
		case TEXTURE_TYPE_COLOR_ATTACHMENT_8UNORM: imageFormat = VK_FORMAT_R8G8B8A8_UNORM; break;
		case TEXTURE_TYPE_DEPTH_MINMAX_MIP: imageFormat = VK_FORMAT_R32G32_SFLOAT; break;
		case TEXTURE_TYPE_ID_BUFFER: imageFormat = VK_FORMAT_R32_SINT; break;
		}
		VK_IMAGE_VIEW_TYPE_2D;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.arrayLayers = 1;
		imageInfo.extent = { width, height, 1 };
		imageInfo.format = imageFormat;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.mipLevels = mipLevels;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		} else if (textureType == TEXTURE_TYPE_COLOR_ATTACHMENT_16F || textureType == TEXTURE_TYPE_COLOR_ATTACHMENT_8UNORM || textureType == TEXTURE_TYPE_ID_BUFFER) {
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}
		if (data) {
			imageInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		imageInfo.usage |= extraUsage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.queueFamilyIndexCount = 0;
		imageInfo.pQueueFamilyIndices = nullptr;
		imageInfo.flags = 0;

		VKU_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &image), "Failed to create image!");

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(device, image, &memReq);

		memory = generalAllocator->alloc(memReq, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, DEVICE_ALLOCATION_TYPE_IMAGE);

		vkBindImageMemory(device, image, memory->memory->get_memory(), memory->offset);

		VkImageMemoryBarrier imageBarrier{};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.image = image;
		imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VkPipelineStageFlags dstStageMask{};
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		} else if (textureType == TEXTURE_TYPE_COLOR_ATTACHMENT_16F || textureType == TEXTURE_TYPE_COLOR_ATTACHMENT_8UNORM) {
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		} else if (textureType == TEXTURE_TYPE_DEPTH_MINMAX_MIP) {
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		} else if (textureType == TEXTURE_TYPE_ID_BUFFER) {
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
		} else {
			if (data) {
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			} else {
				imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			}
		}
		imageLayout = imageBarrier.newLayout;
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.srcAccessMask = 0;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.layerCount = 1;
		imageBarrier.subresourceRange.levelCount = mipLevels;
		vkCmdPipelineBarrier(data ? stagingCmdBuf : cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

		if (data) {
			VkBufferImageCopy region{};
			region.bufferOffset = stagingOffset;
			//Setting these to 0 means the data is tightly packed
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageOffset = { 0, 0, 0 };
			region.imageExtent.width = width;
			region.imageExtent.height = height;
			region.imageExtent.depth = 1;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageSubresource.mipLevel = 0;
			vkCmdCopyBufferToImage(stagingCmdBuf, stagingBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageLayout = imageBarrier.newLayout;
			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask = 0;
			imageBarrier.srcQueueFamilyIndex = deviceQueueFamilyIndices.transferFamily.value();
			imageBarrier.dstQueueFamilyIndex = deviceQueueFamilyIndices.graphicsFamily.value();
			vkCmdPipelineBarrier(stagingCmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		}

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.format = imageFormat;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		} else {
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		viewInfo.flags = 0;

		mipImageViews = new VkImageView[mipLevels];
		for (uint32_t i = 0; i < mipLevels; i++) {
			viewInfo.subresourceRange.baseMipLevel = i;
			viewInfo.subresourceRange.levelCount = mipLevels - i;
			VKU_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &mipImageViews[i]), "Failed to create image view!");
		}
		this->mipLevels = mipLevels;
	}

	void Texture::destroy() {
		if (memory) {
			for (uint32_t i = 0; i < mipLevels; i++) {
				vkDestroyImageView(device, mipImageViews[i], nullptr);
			}
			vkDestroyImage(device, image, nullptr);
			generalAllocator->free(memory);
			memory = nullptr;
			mipImageViews = nullptr;
		}
	}

	VkImageView Texture::get_image_view(uint32_t mipLevel) {
		return mipImageViews[mipLevel];
	}

	VkImageLayout Texture::get_image_layout() {
		return imageLayout;
	}

	VkImage Texture::get_image() {
		return image;
	}

	void Texture::acquire_new_textures() {
		for (Texture* tex : newTextures) {
			VkImageMemoryBarrier imageBarrier{};
			imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageBarrier.image = tex->image;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex = deviceQueueFamilyIndices.transferFamily.value();
			imageBarrier.dstQueueFamilyIndex = deviceQueueFamilyIndices.graphicsFamily.value();
			imageBarrier.srcAccessMask = 0;
			imageBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.baseMipLevel = 0;
			imageBarrier.subresourceRange.layerCount = 1;
			imageBarrier.subresourceRange.levelCount = 1;
			vkCmdPipelineBarrier(graphics_cmd_buf(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		}
		newTextures.clear();
	}

	Texture* load_texture(std::wstring textureName, VkImageUsageFlags extraUsage) {
		Texture* tex = new Texture();
		util::FileMapping mapping = util::map_file(L"resources/textures/" + textureName);
		if (textureName.compare(textureName.length() - 5, 5, L".msdf") == 0) {
			uint32_t* data = reinterpret_cast<uint32_t*>(mapping.mapping);
			uint32_t width = data[0];
			uint32_t height = data[1];
			tex->create(transferCommandBuffer, width, height, 1, data + 2, TEXTURE_TYPE_NORMAL, extraUsage | VK_IMAGE_USAGE_SAMPLED_BIT);
		} else {
			uint32_t* data = reinterpret_cast<uint32_t*>(mapping.mapping);
			uint32_t width = data[1];
			uint32_t height = data[2];
			tex->create(transferCommandBuffer, width, height, 1, data + 4, TEXTURE_TYPE_COMPRESSED, extraUsage | VK_IMAGE_USAGE_SAMPLED_BIT);
		}
		util::unmap_file(mapping);
		return tex;
	}

	void delete_texture(Texture* tex) {
		tex->destroy();
		delete tex;
	}

}
