#include "ShaderUniforms.h"

namespace vku {
	std::vector<Uniform*> Uniform::uniformsToRecreate;

	void UniformTexture2DArray::update_texture(uint32_t index, Texture* tex) {
		imageInfos[index].imageView = tex->get_image_view();
		for (UniformBinding& binding : affectedDescriptorSets) {
			for (uint32_t i = 0; i < swapchainImageCount; i++) {
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
}