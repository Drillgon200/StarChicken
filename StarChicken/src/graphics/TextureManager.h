#pragma once

#include "VkUtil.h"
#include "DeviceMemoryAllocator.h"

namespace vku {

	enum TextureType {
		TEXTURE_TYPE_NORMAL = 0,
		TEXTURE_TYPE_COMPRESSED = 1,
		TEXTURE_TYPE_DEPTH_ATTACHMENT = 2,
		TEXTURE_TYPE_COLOR_ATTACHMENT_8UNORM = 3,
		TEXTURE_TYPE_COLOR_ATTACHMENT_16F = 4,
		TEXTURE_TYPE_DEPTH_MINMAX_MIP = 5,
		TEXTURE_TYPE_ID_BUFFER = 6
	};

	class Texture {
	private:
		static std::vector<Texture*> newTextures;

		VkImageView* mipImageViews;
		uint32_t mipLevels;
		VkImageLayout imageLayout;
		VkImage image;
		DeviceAllocation* memory;
		TextureType textureType;
	public:
		Texture();
		//Constructor for a dummy Texture object that acts as a container for an existing image
		Texture(VkImageView imageView, VkImageLayout imageLayout, VkImage image);
		~Texture();

		void create(VkCommandBuffer cmdBuf, uint32_t width, uint32_t height, uint32_t mipLevels, void* data, TextureType textureType, VkImageUsageFlags extraUsage);
		void destroy();

		VkImage get_image();
		inline uint32_t get_mip_levels() {
			return mipLevels;
		}
		VkImageView get_image_view(uint32_t mipLevel = 0);
		VkImageLayout get_image_layout();

		static void acquire_new_textures();
	};

	Texture* load_texture(std::wstring textureName, VkImageUsageFlags extraUsage);
}
