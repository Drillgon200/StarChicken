#pragma once

#include "VkUtil.h"
#include "DeviceMemoryAllocator.h"

namespace vku {

	enum TextureType {
		TEXTURE_TYPE_NORMAL = 0,
		TEXTURE_TYPE_COMPRESSED = 1,
		TEXTURE_TYPE_DEPTH_ATTACHMENT = 2
	};

	class Texture {
	private:
		static std::vector<Texture*> newTextures;

		VkImageView imageView;
		VkImage image;
		DeviceAllocation* memory;
		TextureType textureType;
	public:
		Texture();
		~Texture();

		void create(VkCommandBuffer cmdBuf, uint32_t width, uint32_t height, uint32_t mipLevels, void* data, TextureType compressed = TEXTURE_TYPE_COMPRESSED);
		void destroy();

		VkImageView get_image_view();

		static void acquire_new_textures();
	};

	Texture* load_texture(std::wstring textureName);
	void delete_texture(Texture* tex);
}
