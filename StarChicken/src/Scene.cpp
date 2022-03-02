#include "Scene.h"
#include "graphics/geometry/Models.h"
#include "Engine.h"
#include "graphics/Framebuffer.h"
#include "graphics/RenderPass.h"
#include "util/Util.h"

namespace scene {

	void Scene::init(VkCommandBuffer cmdBuf, vku::DescriptorSet* mainShaderSet, vku::RenderPass* renderPass, vku::RenderPass* depthPass, vku::RenderPass* objectIdPass, vku::Framebuffer* mainFramebuffer, vku::Framebuffer* mainDepthFramebuffer) {
		framebuffer = mainFramebuffer;
		depthFramebuffer = mainDepthFramebuffer;
		depthPyramidLevels = 0;
		depthPyramid = new vku::Texture();
		framebuffer_resized(cmdBuf, vku::swapChainExtent.width, vku::swapChainExtent.height);
		depthPyramidUniform = new vku::UniformTexture2D(depthPyramid->get_image_view(), VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL);

		geometryManager.set_render_pass(renderPass, depthPass, objectIdPass);
		geometryManager.set_render_desc_set(mainShaderSet);
		geometryManager.init(1024, 1024, 32, 32, 2048, depthPyramidUniform);
		
		depthPyramidDownsample = new vku::ComputePipeline();
		depthPyramidDownsample->name("min_max_downsample").descriptor_set(*depthPyramidDownsampleSets[0]).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) * 3 }).build();
		activeObject = nullptr;
	}

	void Scene::cleanup() {
		geometryManager.cleanup();
		for (geom::Model* model : cleanupModels) {
			delete model;
		}
		delete depthPyramidUniform;
		depthPyramid->destroy();
		delete depthPyramid;
		depthPyramidDownsample->destroy();
		delete depthPyramidDownsample;
	}

	void Scene::framebuffer_resized(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY) {
		if (!depthPyramid) {
			return;
		}
		if (depthPyramidLevels > 0) {
			for (uint32_t i = 0; i < depthPyramidLevels; i++) {
				delete depthPyramidReadUniforms[i];
				delete depthPyramidWriteUniforms[i];
				vku::destroy_descriptor_set(depthPyramidDownsampleSets[i]);
			}
			delete[] depthPyramidReadUniforms;
			delete[] depthPyramidWriteUniforms;
			delete[] depthPyramidDownsampleSets;

			depthPyramid->destroy();
		}
		depthPyramidLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(newX, newY))));
		depthPyramid->create(cmdBuf, newX/2, newY/2, depthPyramidLevels, nullptr, vku::TEXTURE_TYPE_DEPTH_MINMAX_MIP, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		
		depthPyramidReadUniforms = new vku::UniformTexture2D*[depthPyramidLevels];
		depthPyramidWriteUniforms = new vku::UniformStorageImage*[depthPyramidLevels];
		depthPyramidDownsampleSets = new vku::DescriptorSet*[depthPyramidLevels];
		for (uint32_t i = 0; i < depthPyramidLevels; i++) {
			if (i == 0) {
				depthPyramidReadUniforms[i] = new vku::UniformTexture2D(framebuffer->get_depth_texture()->get_image_view(0), VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			} else {
				depthPyramidReadUniforms[i] = new vku::UniformTexture2D(depthPyramid->get_image_view(i - 1), VK_SHADER_STAGE_COMPUTE_BIT, VK_IMAGE_LAYOUT_GENERAL);
			}
			depthPyramidWriteUniforms[i] = new vku::UniformStorageImage(depthPyramid->get_image_view(i));
			depthPyramidDownsampleSets[i] = vku::create_descriptor_set({ depthPyramidReadUniforms[i], depthPyramidWriteUniforms[i], engine::minSampler, engine::maxSampler });
		}
		if (depthPyramidUniform) {
			for (uint32_t i = 0; i < NUM_FRAME_DATA; i++) {
				depthPyramidUniform->update_texture(i, depthPyramid->get_image_view());
			}
		}
	}

	void Scene::generate_depth_pyramid(VkCommandBuffer cmdBuf) {
		vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, depthFramebuffer->get_depth_texture()->get_image(),
			VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		depthPyramidDownsample->bind(cmdBuf);
		int32_t imageDimension[3]{ vku::swapChainExtent.width, vku::swapChainExtent.height, 1 };
		for (uint32_t i = 0; i < depthPyramidLevels; i++) {
			imageDimension[0] = std::max(1, imageDimension[0] / 2);
			imageDimension[1] = std::max(1, imageDimension[1] / 2);
			depthPyramidDownsampleSets[i]->bind(cmdBuf, *depthPyramidDownsample, 0);
			vkCmdPushConstants(cmdBuf, depthPyramidDownsample->get_layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, 3 * sizeof(uint32_t), imageDimension);
			imageDimension[2] = 0;
			vkCmdDispatch(cmdBuf, (imageDimension[0] + 15)/16, (imageDimension[1] + 15) / 16, 1);

			vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, depthPyramid->get_image(),
				VK_IMAGE_ASPECT_COLOR_BIT, 0, depthPyramid->get_mip_levels(), 0, 1, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	void Scene::prepare_render_world(vku::RenderPass& renderPass) {
		VkCommandBuffer cmdBuf = vku::graphics_cmd_buf();
		geometryManager.begin_frame(cmdBuf, cameras);
		for (Camera* cam : cameras) {
			for (geom::Model* model : sceneModels) {
				//TODO replace with real scene traversal with CPU culling and stuff
				cam->add_render_model(*model);
			}
		}
		geometryManager.update_cam_sets(cmdBuf, cameras);
		geometryManager.send_data(cmdBuf, cameras);

		geometryManager.cull_depth_prepass(cmdBuf, cameras, renderPass, *depthFramebuffer);

		vku::memory_barrier(cmdBuf, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		generate_depth_pyramid(cmdBuf);
		geometryManager.cull(cmdBuf, cameras);

		vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT, depthFramebuffer->get_depth_texture()->get_image(),
			VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		renderPass.begin_pass(cmdBuf, *depthFramebuffer);
		for (Camera* cam : cameras) {
			vkCmdSetViewport(cmdBuf, 0, 1, &cam->viewport);
			geometryManager.draw(cmdBuf, *cameras[0], vku::WORLD_RENDER_PASS_DEPTH, cam->cameraIndex);
		}
		renderPass.end_pass(cmdBuf);

		vku::memory_barrier(cmdBuf, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		//generate_depth_pyramid(cmdBuf);
	}

	void Scene::render_world(Camera& cam, vku::WorldRenderPass pass) {
		VkCommandBuffer cmdBuf = vku::graphics_cmd_buf();
		vkCmdSetViewport(cmdBuf, 0, 1, &cam.viewport);
		geometryManager.draw(cmdBuf, *cameras[0], pass, cam.cameraIndex);
	}

	void Scene::add_model(geom::Model* model) {
		sceneModels.push_back(model);
	}

	geom::Model* Scene::new_instance(geom::Mesh* mesh) {
		geom::Model* model = new geom::Model(mesh);
		geometryManager.add_model(*model);
		sceneModels.push_back(model);
		cleanupModels.push_back(model);
		return model;
	}

	void Scene::add_camera(Camera* cam) {
		cameras.push_back(cam);
	}

	void Scene::select_object(int32_t id) {
		if (id != -1) {
			if (activeObject != nullptr) {
				activeObject->set_selection(vku::OBJECT_FLAG_SELECTED);
			}
			activeObject = geometryManager.get_object_list()[id];
			activeObject->set_selection(vku::OBJECT_FLAG_ACTIVE | vku::OBJECT_FLAG_SELECTED);
			if (!util::vector_contains(selectedObjects, activeObject)) {
				selectedObjects.push_back(activeObject);
			}
		} else {
			for (geom::SelectableObject* obj : selectedObjects) {
				obj->set_selection(0);
			}
			selectedObjects.clear();
			activeObject = nullptr;
		}
	}
}