#include "TextureManager.h"
#include "..\util\Util.h"

namespace vku {
	std::vector<Texture*> Texture::newTextures;

	Texture::Texture() {
	}
	Texture::~Texture() {
	}

	void Texture::create(VkCommandBuffer cmdBuf, uint32_t width, uint32_t height, uint32_t mipLevels, void* data, TextureType textureType) {
		assert(!memory);
		this->textureType = textureType;
		uint32_t imageSize = width * height;
		if (textureType != TEXTURE_TYPE_COMPRESSED) {
			imageSize *= 4;
		}
		DeviceBuffer stagingBuffer{};

		if (data) {
			stagingBuffer = generalAllocator->alloc_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			void* mapping = stagingBuffer.allocation->map();
			memcpy(mapping, data, imageSize);
		}
		
		VkFormat imageFormat{};
		switch (textureType) {
		case TEXTURE_TYPE_NORMAL: imageFormat = VK_FORMAT_R8G8B8A8_UNORM; break;
		case TEXTURE_TYPE_COMPRESSED: imageFormat = VK_FORMAT_BC7_SRGB_BLOCK; break;
		case TEXTURE_TYPE_DEPTH_ATTACHMENT: imageFormat = VK_FORMAT_D32_SFLOAT; break;
		}

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.arrayLayers = 1;
		imageInfo.extent = { width, height, 1 };
		imageInfo.format = imageFormat;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.mipLevels = 1;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		} else {
			imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
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
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		} else {
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		}
		imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageBarrier.srcAccessMask = 0;
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			imageBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		} else {
			imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		} else {
			imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.baseMipLevel = 0;
		imageBarrier.subresourceRange.layerCount = 1;
		imageBarrier.subresourceRange.levelCount = 1;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) ? VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT : VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

		if (data) {
			VkBufferImageCopy region{};
			region.bufferOffset = 0;
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
			vkCmdCopyBufferToImage(cmdBuf, stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask = 0;
			imageBarrier.srcQueueFamilyIndex = deviceQueueFamilyIndices.transferFamily.value();
			imageBarrier.dstQueueFamilyIndex = deviceQueueFamilyIndices.graphicsFamily.value();
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

			transferStagingBuffersToFree.push_back(stagingBuffer);
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
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.baseMipLevel = 0;
		if (textureType == TEXTURE_TYPE_DEPTH_ATTACHMENT) {
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		} else {
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		viewInfo.flags = 0;

		VKU_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &imageView), "Failed to create image view!");
	}

	void Texture::destroy() {
		vkDestroyImageView(device, imageView, nullptr);
		vkDestroyImage(device, image, nullptr);
		generalAllocator->free(memory);
	}

	VkImageView Texture::get_image_view() {
		return imageView;
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

	Texture* load_texture(std::wstring textureName) {
		Texture* tex = new Texture();
		util::FileMapping mapping = util::map_file(L"resources/textures/" + textureName);
		uint32_t* data = reinterpret_cast<uint32_t*>(mapping.mapping);
		uint32_t width = data[1];
		uint32_t height = data[2];
		tex->create(transferCommandBuffer, width, height, 1, data + 4);
		util::unmap_file(mapping);
		return tex;
	}

	void delete_texture(Texture* tex) {
		tex->destroy();
		delete tex;
	}

}
