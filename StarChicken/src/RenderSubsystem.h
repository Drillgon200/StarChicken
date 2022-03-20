#pragma once

#include "Engine.h"
#include "resources/DefaultResources.h"
#include "scene/Scene.h"
#include "ui/Gui.h"
#include "graphics/VkUtil.h"
#include "graphics/DeviceMemoryAllocator.h"
#include "graphics/RenderPass.h"
#include "graphics/ShaderUniforms.h"
#include "util/FontRenderer.h"
#include "util/Util.h"
#include "graphics/StagingManager.h"

//This file just exists to separate out the render part of the engine because it was getting really messy sticking all the random render stuff in the same file.

namespace engine {
	class RenderSubsystem {
	public:
		vku::DeviceBuffer mouseObjectSelection;

		vku::RenderPass mainRenderPass{};
		vku::RenderPass depthPreRenderPass{};
		vku::RenderPass outlineRenderPass{};
		vku::RenderPass uiRenderPass{};
		vku::Framebuffer mainFramebuffer;
		vku::Framebuffer mainDepthFramebuffer;
		vku::Framebuffer objectOutlineFramebuffer;
		vku::Framebuffer uiFramebuffer;
		vku::Texture* finalTexture = nullptr;

		vku::ComputePipeline* finalPostPipeline;
		vku::DescriptorSet* finalPostSet;
		vku::UniformStorageImage* sceneStorageImage;
		vku::UniformStorageImage* uiStorageImage;
		vku::UniformStorageImage* finalTexStorageImage;
		vku::UniformStorageImage* outlineStorageImage;

		vku::UniformBuffer<mat4f>* uiProjectionMatrixBuffer = nullptr;
		vku::UniformBuffer<mat4f>* worldViewProjectionBuffer = nullptr;

		vku::UniformTexture2DArray* textureArray = nullptr;

		text::FontRenderer* fontRenderer = nullptr;

		bool hasSelectedObjects{ false };

		void create_render_passes() {
			VkAttachmentDescription colorAttachment{};
			colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription idAttachment = colorAttachment;
			idAttachment.format = VK_FORMAT_R32_SINT;

			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = VK_FORMAT_D32_SFLOAT;
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkAttachmentDescription mainAttachments[] = { colorAttachment, idAttachment, depthAttachment };
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
			VkAttachmentDescription uiAttachments[] = { colorAttachment, depthAttachment };
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			VkAttachmentDescription outlineAttachments[] = { idAttachment, depthAttachment };
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			VkAttachmentDescription depthPreAttachments[] = { depthAttachment };


			VkAttachmentReference colorAttachmentRefs[2];
			colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachmentRefs[0].attachment = 0;
			colorAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachmentRefs[1].attachment = 1;

			VkAttachmentReference depthAttachmentRef{};
			depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthAttachmentRef.attachment = 2;

			VkSubpassDescription subpass{};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 2;
			subpass.pColorAttachments = colorAttachmentRefs;
			subpass.pDepthStencilAttachment = &depthAttachmentRef;

			mainRenderPass.create(3, 1, 0, mainAttachments, &subpass, nullptr);
			depthAttachmentRef.attachment = 0;
			subpass.colorAttachmentCount = 0;
			depthPreRenderPass.create(1, 1, 0, depthPreAttachments, &subpass, nullptr);
			depthAttachmentRef.attachment = 1;
			subpass.colorAttachmentCount = 1;
			outlineRenderPass.create(2, 1, 0, outlineAttachments, &subpass, nullptr);
			uiRenderPass.create(2, 1, 0, uiAttachments, &subpass, nullptr);
		}

		void render() {
			bool shouldContinue = vku::setup_next_frame();
			if (shouldContinue) {
				mat4f guiProjection;
				guiProjection.project_ortho(vku::swapChainExtent.width, 0, vku::swapChainExtent.height, 0, 1000, 1);
				uiProjectionMatrixBuffer->update(guiProjection);

				vec2f mousePos = windowing::get_mouse();
				mainGui->handle_input(mousePos.x, mousePos.y);
				mainGui->render();

				text::set_matrix(guiProjection);
				fontRenderer->draw_string("VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT", 12.0F, 0.075F, 50.0F, 20.0F, 0.0F, 0);
				fontRenderer->draw_string("VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT", 12.0F, 0.075F, 50.0F, 40.0F, 0.0F, text::STRING_FLAG_MULTISAMPLE);
				fontRenderer->draw_string("VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT", 12.0F, 0.075F, 50.0F, 60.0F, 0.0F, text::STRING_FLAG_SUBPIXEL);
				fontRenderer->draw_string("VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT", 12.0F, 0.075F, 50.0F, 80.0F, 0.0F, text::STRING_FLAG_SUBPIXEL | text::STRING_FLAG_MULTISAMPLE);
				fontRenderer->draw_string("VK_ACCESS_TRANSFER_WRITE_BIT", 8.0F, 0.075F, 50.0F, 100.0F, 0.0F, 0);
				fontRenderer->draw_string("VK_ACCESS_TRANSFER_WRITE_BIT", 8.0F, 0.075F, 50.0F, 115.0F, 0.0F, text::STRING_FLAG_MULTISAMPLE);
				fontRenderer->draw_string("VK_ACCESS_TRANSFER_WRITE_BIT", 8.0F, 0.075F, 50.0F, 130.0F, 0.0F, text::STRING_FLAG_SUBPIXEL);
				fontRenderer->draw_string("VK_ACCESS_TRANSFER_WRITE_BIT", 8.0F, 0.075F, 50.0F, 145.0F, 0.0F, text::STRING_FLAG_SUBPIXEL | text::STRING_FLAG_MULTISAMPLE);
				fontRenderer->draw_string("VK_IMAGE_LAYOUT_UNDEFINED", 14.0F, 0.075F, 50.0F, 250.0F, 0.0F, 0);
				fontRenderer->draw_string("VK_IMAGE_LAYOUT_UNDEFINED", 14.0F, 0.075F, 50.0F, 270.0F, 0.0F, text::STRING_FLAG_MULTISAMPLE);
				fontRenderer->draw_string("VK_IMAGE_LAYOUT_UNDEFINED", 14.0F, 0.075F, 50.0F, 290.0F, 0.0F, text::STRING_FLAG_SUBPIXEL);
				fontRenderer->draw_string("VK_IMAGE_LAYOUT_UNDEFINED", 14.0F, 0.075F, 50.0F, 310.0F, 0.0F, text::STRING_FLAG_SUBPIXEL | text::STRING_FLAG_MULTISAMPLE);

				vku::begin_frame();
				VkCommandBuffer cmdBuf = vku::graphics_cmd_buf();

				vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, vku::swapChainImages[vku::swapchainImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, finalTexture->get_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

				vku::Texture::acquire_new_textures();


				//FRAME DRAW BEGIN//

				//Depth prepass, setup
				scene.get_renderer().prepare_render_world(depthPreRenderPass);
				//Render selected objects to id buffer
				if (!scene.get_selected_objects().empty()) {
					outlineRenderPass.begin_pass(cmdBuf, objectOutlineFramebuffer);
					mainGui->render_cameras(vku::WORLD_RENDER_PASS_ID);
					outlineRenderPass.end_pass(cmdBuf);
					hasSelectedObjects = true;
				} else if (hasSelectedObjects) {
					outlineRenderPass.begin_pass(cmdBuf, objectOutlineFramebuffer);
					outlineRenderPass.end_pass(cmdBuf);
				}
				//Main color pass
				mainRenderPass.begin_pass(cmdBuf, mainFramebuffer);
				resources::shaderSet->bind(cmdBuf, *scene.get_renderer().geo_manager().get_render_pipeline(), 2);
				mainGui->render_cameras(vku::WORLD_RENDER_PASS_COLOR);
				mainRenderPass.end_pass(cmdBuf);

				//Transfer mouse over object id to CPU
				vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, mainFramebuffer.get_color_attachment(1)->get_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
				int32_t mouseX;
				int32_t mouseY;
				windowing::get_mouse(&mouseX, &mouseY);
				VkBufferImageCopy mouseSelectCopy{};
				mouseSelectCopy.bufferRowLength = 0;
				mouseSelectCopy.bufferImageHeight = 0;
				mouseSelectCopy.bufferOffset = vku::currentFrame * sizeof(int32_t);
				mouseSelectCopy.imageOffset = { clamp<int32_t>(mouseX, 0, vku::swapChainExtent.width - 1), clamp<int32_t>(mouseY, 0, vku::swapChainExtent.height - 1), 0 };
				mouseSelectCopy.imageExtent = { 1, 1, 1 };
				mouseSelectCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				mouseSelectCopy.imageSubresource.baseArrayLayer = 0;
				mouseSelectCopy.imageSubresource.layerCount = 1;
				mouseSelectCopy.imageSubresource.mipLevel = 0;
				vkCmdCopyImageToBuffer(cmdBuf, mainFramebuffer.get_color_attachment(1)->get_image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mouseObjectSelection.buffer, 1, &mouseSelectCopy);

				//UI pass
				VkViewport viewport;
				viewport.x = 0;
				viewport.y = 0;
				viewport.width = vku::swapChainExtent.width;
				viewport.height = vku::swapChainExtent.height;
				viewport.minDepth = 0.0F;
				viewport.maxDepth = 1.0F;
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
				uiRenderPass.begin_pass(cmdBuf, uiFramebuffer);
				resources::uiPipeline->bind(cmdBuf);
				resources::uiSet->bind(cmdBuf, *resources::uiPipeline, 0);
				ui::draw_gui(cmdBuf);
				fontRenderer->render_cached_strings(*resources::textPipeline);
				uiRenderPass.end_pass(cmdBuf);

				//FRAME DRAW END//


				VkImageMemoryBarrier transitionToFinal[3]{
					vku::get_image_barrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, mainFramebuffer.get_color_attachment(0)->get_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
					vku::get_image_barrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, uiFramebuffer.get_color_attachment(0)->get_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL),
					vku::get_image_barrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, objectOutlineFramebuffer.get_color_attachment(0)->get_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL)
				};
				vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, hasSelectedObjects ? 3 : 2, transitionToFinal);

				finalPostPipeline->bind(cmdBuf);
				finalPostSet->bind(cmdBuf, *finalPostPipeline, 0);
				int32_t swapchainSize[2]{ vku::swapChainExtent.width, vku::swapChainExtent.height };
				vkCmdPushConstants(cmdBuf, finalPostPipeline->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(int32_t), swapchainSize);
				vkCmdDispatch(cmdBuf, (vku::swapChainExtent.width + 15) / 16, (vku::swapChainExtent.height + 15) / 16, 1);

				vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, finalTexture->get_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				VkImageCopy finalCopyRegion{};
				finalCopyRegion.srcOffset = { 0, 0, 0 };
				finalCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				finalCopyRegion.srcSubresource.mipLevel = 0;
				finalCopyRegion.srcSubresource.baseArrayLayer = 0;
				finalCopyRegion.srcSubresource.layerCount = 1;
				finalCopyRegion.dstOffset = { 0, 0, 0 };
				finalCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				finalCopyRegion.dstSubresource.mipLevel = 0;
				finalCopyRegion.dstSubresource.baseArrayLayer = 0;
				finalCopyRegion.dstSubresource.layerCount = 1;
				finalCopyRegion.extent.width = vku::swapChainExtent.width;
				finalCopyRegion.extent.height = vku::swapChainExtent.height;
				finalCopyRegion.extent.depth = 1;
				vkCmdCopyImage(cmdBuf, finalTexture->get_image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vku::swapChainImages[vku::swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &finalCopyRegion);

				vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE_KHR, vku::swapChainImages[vku::swapchainImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

				if (hasSelectedObjects && scene.get_selected_objects().empty()) {
					hasSelectedObjects = false;
				}

				vku::end_frame();
			}
		}

		void on_window_resize(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY) {
			bool firstTime = false;
			if (!finalTexture) {
				firstTime = true;
			}
			if (firstTime) {
				finalTexture = new vku::Texture();
			} else {
				mainFramebuffer.destroy();
				mainDepthFramebuffer.custom_depth(nullptr);
				mainDepthFramebuffer.destroy();
				objectOutlineFramebuffer.destroy();
				uiFramebuffer.destroy();
				finalTexture->destroy();
			}
			mainFramebuffer.render_pass(mainRenderPass.get_pass()).size({ newX, newY }).add_color_attachment(vku::TEXTURE_TYPE_COLOR_ATTACHMENT_16F).add_color_attachment(vku::TEXTURE_TYPE_ID_BUFFER).depth(true).clear_color_float(vec4f{ 0.0F }).clear_color_int(vec4i32{ -1 }).clear_depth(0.0F).create(cmdBuf);
			mainDepthFramebuffer.render_pass(depthPreRenderPass.get_pass()).size({ newX, newY }).custom_depth(mainFramebuffer.get_depth_texture()).clear_depth(0.0F).create(cmdBuf);
			objectOutlineFramebuffer.render_pass(outlineRenderPass.get_pass()).size({ newX, newY }).add_color_attachment(vku::TEXTURE_TYPE_ID_BUFFER).depth(true).clear_color_float(vec4{ 0.0F }).clear_depth(0.0F).create(cmdBuf);
			uiFramebuffer.render_pass(uiRenderPass.get_pass()).size({ newX, newY }).add_color_attachment(vku::TEXTURE_TYPE_COLOR_ATTACHMENT_8UNORM).depth(true).clear_color_float(vec4f{ 0.12F, 0.12F, 0.12F, 1.0F }).clear_depth(0.0F).create(cmdBuf);
			finalTexture->create(cmdBuf, newX, newY, 1, nullptr, vku::TEXTURE_TYPE_NORMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			if (firstTime) {
				sceneStorageImage = new vku::UniformStorageImage(mainFramebuffer.get_color_attachment(0));
				uiStorageImage = new vku::UniformStorageImage(uiFramebuffer.get_color_attachment(0));
				finalTexStorageImage = new vku::UniformStorageImage(finalTexture);
				outlineStorageImage = new vku::UniformStorageImage(objectOutlineFramebuffer.get_color_attachment(0));
			} else {
				for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
					sceneStorageImage->update_texture(i, mainFramebuffer.get_color_attachment(0));
					uiStorageImage->update_texture(i, uiFramebuffer.get_color_attachment(0));
					finalTexStorageImage->update_texture(i, finalTexture);
					outlineStorageImage->update_texture(i, objectOutlineFramebuffer.get_color_attachment(0));
				}
			}
			//Make sure the selection buffer gets cleared after the resize.
			hasSelectedObjects = true;
		}

		int32_t get_select() {
			return reinterpret_cast<int32_t*>(mouseObjectSelection.allocation->map())[vku::currentFrame];
		}

		geom::Mesh* load_mesh(std::wstring name) {
			util::FileMapping map = util::map_file(L"resources/models/" + name);
			document::DocumentNode* meshdata = document::parse_document(map.mapping);
			document::DocumentNode* geometry = meshdata->get_child("geometry")->children[0];
			return scene.get_renderer().geo_manager().create_mesh(geometry);
		}

		void create_descriptor_sets() {
			scene.get_renderer().geo_manager().create_descriptor_sets(scene.get_renderer().depthPyramidUniform);
			scene.get_renderer().geo_manager().set_render_desc_set(resources::shaderSet);

			finalPostSet = vku::create_descriptor_set({ sceneStorageImage, uiStorageImage, finalTexStorageImage, outlineStorageImage, scene.get_renderer().geo_manager().get_object_buffer() });
		}

		void create_pipelines() {
			scene.get_renderer().geo_manager().create_pipelines();
			finalPostPipeline = new vku::ComputePipeline();
			finalPostPipeline->name("final_post").descriptor_set(*finalPostSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(int32_t) }).build();
		}

		void init() {
			mouseObjectSelection = vku::generalAllocator->alloc_buffer(NUM_FRAME_DATA * sizeof(int32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			uiProjectionMatrixBuffer = new vku::UniformBuffer<mat4f>(true, false, VK_SHADER_STAGE_VERTEX_BIT);
			worldViewProjectionBuffer = new vku::UniformBuffer<mat4f>(true, true, VK_SHADER_STAGE_VERTEX_BIT);

			textureArray = new vku::UniformTexture2DArray(128);

			create_render_passes();

			vkQueueWaitIdle(vku::graphicsQueue);
			VkCommandBuffer cmdBuf = vku::graphics_cmd_buf();
			vkResetCommandPool(vku::device, vku::graphicsCommandPools[vku::currentFrame], 0);
			vku::begin_command_buffer(cmdBuf);

			on_window_resize(cmdBuf, vku::swapChainExtent.width, vku::swapChainExtent.height);

			scene.get_renderer().init(cmdBuf, &mainRenderPass, &depthPreRenderPass, &outlineRenderPass, &mainFramebuffer, &mainDepthFramebuffer);

			vku::end_command_buffer(cmdBuf);
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &cmdBuf;
			submitInfo.signalSemaphoreCount = 0;
			submitInfo.pSignalSemaphores = nullptr;
			submitInfo.waitSemaphoreCount = 0;
			submitInfo.pWaitSemaphores = nullptr;
			submitInfo.pWaitDstStageMask = nullptr;
			vkQueueSubmit(vku::graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(vku::graphicsQueue);

			text::init();

			//vku::create_descriptor_sets();

			//vkWaitForFences(vku::device, 1, &vku::transferFence, VK_TRUE, UINT64_MAX);
			//vkResetFences(vku::device, 1, &vku::transferFence);

			vkResetCommandPool(vku::device, vku::transferCommandPool, 0);
			vku::begin_command_buffer(vku::transferCommandBuffer);

			geom::Mesh* testMesh = load_mesh(L"testmodel2_9.dmf");
			geom::Model* testModel = scene.new_instance(testMesh);

			text::create_text_quad();             
			fontRenderer = new text::FontRenderer();
			fontRenderer->load(L"starchickenfont.msdf");

			vku::end_command_buffer(vku::transferCommandBuffer);

			submitInfo.pCommandBuffers = &vku::transferCommandBuffer;
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = &vku::transferSemaphore;
			vkQueueSubmit(vku::transferQueue, 1, &submitInfo, vku::transferFence);
			vkQueueWaitIdle(vku::transferQueue);
			vku::mark_transfer_dirty();
			vku::transferStagingManager->flush();

			vkDeviceWaitIdle(vku::device);
			vku::Uniform::update_pending_sets((vku::currentFrame + 1) % NUM_FRAME_DATA);
			for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
				vku::generalAllocator->free_old_data(i);
			}

		}
		void finish_init() {

		}
		void cleanup() {
			uiFramebuffer.destroy();
			mainFramebuffer.destroy();
			mainDepthFramebuffer.custom_depth(nullptr);
			mainDepthFramebuffer.destroy();
			objectOutlineFramebuffer.destroy();
			delete finalTexture;
			delete fontRenderer;

			//delete mvpMatricesBuffer;
			delete uiProjectionMatrixBuffer;
			delete worldViewProjectionBuffer;
			delete textureArray;
			//delete lightingBuffer;
			delete sceneStorageImage;
			delete uiStorageImage;
			delete finalTexStorageImage;
			delete outlineStorageImage;

			delete finalPostPipeline;

			text::cleanup();

			vku::delete_descriptor_sets();
			mainRenderPass.destroy();
			uiRenderPass.destroy();
			depthPreRenderPass.destroy();
			outlineRenderPass.destroy();

			vku::generalAllocator->free_buffer(mouseObjectSelection);
		}
	};
}