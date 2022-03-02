#include "Engine.h"
#include <mutex>
#include <algorithm>
#include <processthreadsapi.h>
#include "util/DrillMath.h"
#include "graphics/VertexFormats.h"
#include "graphics/ShaderUniforms.h"
#include "graphics/GraphicsPipeline.h"
#include "graphics/RenderPass.h"
#include "graphics/StagingManager.h"
#include "graphics/TextureManager.h"
#include "graphics/geometry/Models.h"
#include "graphics/geometry/GeometryAllocator.h"
#include "util/FileDocument.h"
#include "util/Util.h"
#include "util/MSDFGenerator.h"
#include "util/FontRenderer.h"
#include "ui/Gui.h"
#include "Scene.h"

uint64_t iterations;

namespace engine {

	job::JobSystem jobSystem;

	int32_t WINDOW_WIDTH = 512;
	int32_t WINDOW_HEIGHT = 512;

	bool keyState[windowing::MAX_KEY_CODE];
	bool mouseState[windowing::MAX_MOUSE_CODE];
	vec2f deltaMouse;

	float deltaTime;

	ui::Gui* mainGui;
	vec2f lastMousePos{ 0.0F };

	scene::Scene scene;
	bool hasSelectedObjects{ false };

	windowing::Window* window;

	struct test {
		int bruh;
	};

	struct test2 {
		uint16_t arg;
		float mug;
	};

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

	//vku::UniformBuffer<MVPMatrices>* mvpMatricesBuffer = nullptr;
	vku::UniformBuffer<mat4f>* uiProjectionMatrixBuffer = nullptr;
	
	vku::UniformSampler2D* bilinearSampler = nullptr;
	vku::UniformSampler2D* nearestSampler = nullptr;
	vku::UniformSampler2D* maxSampler = nullptr;
	vku::UniformSampler2D* minSampler = nullptr;
	vku::UniformTexture2DArray* textureArray = nullptr;

	vku::GraphicsPipeline* uiPipeline;
	vku::GraphicsPipeline* uiLinePipeline;
	vku::GraphicsPipeline* textPipeline;
	vku::DescriptorSet* shaderSet;
	vku::DescriptorSet* uiSet;

	text::FontRenderer* fontRenderer = nullptr;

	vku::Texture* duckTex = nullptr;
	uint16_t missingTexIdx;
	uint16_t whiteTexIdx;
	uint16_t duckTexIdx;

	std::chrono::steady_clock::time_point lastTime;

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
			fontRenderer->draw_string("VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT", 12.0F, 0.075F, 50.0F, 20.0F, 0.0F);
			fontRenderer->draw_string("VK_IMAGE_LAYOUT_UNDEFINED", 12.0F, 0.075F, 50.0F, 250.0F, 0.0F);

			vku::begin_frame();
			VkCommandBuffer cmdBuf = vku::graphics_cmd_buf();

			vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_NONE_KHR, VK_ACCESS_TRANSFER_WRITE_BIT, vku::swapChainImages[vku::swapchainImageIndex], VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			vku::image_barrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, finalTexture->get_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

			vku::Texture::acquire_new_textures();


			//FRAME DRAW BEGIN//
			
			//Depth prepass, setup
			scene.prepare_render_world(depthPreRenderPass);
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
			shaderSet->bind(cmdBuf, *scene.geo_manager().get_render_pipeline(), 2);
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
			mouseSelectCopy.imageOffset = { clamp<int32_t>(mouseX, 0, vku::swapChainExtent.width-1), clamp<int32_t>(mouseY, 0, vku::swapChainExtent.height - 1), 0 };
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
			uiPipeline->bind(cmdBuf);
			uiSet->bind(cmdBuf, *uiPipeline, 0);
			ui::draw_gui(cmdBuf);
			fontRenderer->render_cached_strings(*textPipeline);
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

	void update() {
		vec2f mousePos = windowing::get_mouse();
		vec2f dMouse = mousePos - lastMousePos;
		if ((dMouse.x != 0) || (dMouse.y != 0)) {
			mainGui->mouse_drag(lastMousePos.x, lastMousePos.y, dMouse.x, dMouse.y);
		}
		mousePos = windowing::get_mouse();
		mainGui->mouse_hover(mousePos.x, mousePos.y);
		lastMousePos = mousePos;
	}

	void main_loop() {
		while (!windowing::window_should_close(window)) {
			/*if (iterations % 10000 == 0) {
				std::cout << iterations << "\n";
			}*/
			++iterations;
			job::JobDecl decls[2];
			decls[0] = job::JobDecl(update);
			decls[1] = job::JobDecl(render);
				
			jobSystem.start_jobs_and_wait_for_counter(decls, 2);
		}
	}

	void framebuffer_resize_callback(windowing::Window* window, uint32_t width, uint32_t height) {
		bool* fboResized = reinterpret_cast<bool*>(windowing::get_user_ptr(window));
		*fboResized = true;
	}

	void key_callback(windowing::Window* window, uint32_t keyCode, uint32_t state) {
		if (state == windowing::KEY_STATE_DOWN) {
			keyState[keyCode] = true;
			if (keyCode == windowing::KEY_ESC) {
				windowing::set_mouse_captured(!windowing::mouse_captured());
			}
		} else if (state == windowing::KEY_STATE_UP) {
			keyState[keyCode] = false;
		}
		mainGui->on_key_typed(keyCode, state);
	}

	void mouse_callback(windowing::Window* window, uint32_t button, uint32_t state) {
		if (button == windowing::MOUSE_WHEEL) {
			float scrollAmount = static_cast<float>(static_cast<int16_t>(state));
			mainGui->mouse_scroll(scrollAmount);
			return;
		}
		if (state == windowing::MOUSE_BUTTON_STATE_DOWN) {
			mouseState[button] = true;
		} else if (state == windowing::MOUSE_BUTTON_STATE_UP) {
			mouseState[button] = false;
		}
		vec2f cursorPos = windowing::get_mouse();
		bool guiDidAction = mainGui->on_click(cursorPos.x, cursorPos.y, button, (state == windowing::MOUSE_BUTTON_STATE_DOWN) ? false : true);

		if (!guiDidAction && (button == windowing::MOUSE_LEFT) && (state == windowing::MOUSE_BUTTON_STATE_DOWN)) {
			int32_t mouseOverId = reinterpret_cast<int32_t*>(mouseObjectSelection.allocation->map())[vku::currentFrame];
			scene.select_object(mouseOverId);
		}
	}

	//Must be called from main thread
	void init_window() {
		windowing::init();
		if (!windowing::query_vulkan_support()) {
			windowing::cleanup();
			std::cerr << "Failed to initialize game! Vulkan is not supported!" << std::endl;
			exit(1);
		}

		for (uint32_t i = 0; i < windowing::MAX_KEY_CODE; i++) {
			keyState[i] = false;
		}

		window = windowing::create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Star Chicken", true);
		windowing::set_user_ptr(window, &vku::frameBufferResized);
		windowing::set_window_resize_callback(window, framebuffer_resize_callback);
		windowing::set_keyboard_callback(window, key_callback);
		windowing::set_mouse_callback(window, mouse_callback);
	}

	void cleanup_window() {
		windowing::cleanup();
	}

	void print_memory_info() {
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(vku::physicalDevice, &memProps);
		std::cout << "\n\nMemory types: " << memProps.memoryTypeCount << "\n";
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
			std::cout << "Type index: " << i << ", Type heap index: " << memProps.memoryTypes[i].heapIndex << ", Type flags: " << memProps.memoryTypes[i].propertyFlags << "\n";
		}
		std::cout << "\nMemory heaps: " << memProps.memoryHeapCount << "\n";
		for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
			std::cout << "Heap index: " << i << ", Heap size: " << memProps.memoryHeaps[i].size << ", Heap flags: " << memProps.memoryHeaps[i].flags << "\n";
		}
		std::cout << "Buffer image granularity: " << vku::deviceProperties.limits.bufferImageGranularity << std::endl;
		std::cout << "Max texture size: " << vku::deviceProperties.limits.maxImageDimension2D << std::endl;
		exit(0);
	}

	/*void load_model(vku::VertexBuffer& buf, std::wstring name) {
		util::FileMapping map = util::map_file(L"resources/models/" + name);

		document::DocumentNode* model = document::parse_document(map.mapping);
		document::DocumentNode* child0 = model->get_child("geometry")->children[0];

		document::DocumentData* vertices = child0->get_data("vertices");
		document::DocumentData* indices = child0->get_data("indices");
		buf.upload_data(vertices->numBytes/vku::POSITION_TEX_NORMAL_TANGENT.size_bytes(), vertices->data, indices->numBytes/sizeof(uint16_t), indices->data);

		delete model;
		util::unmap_file(map);
	}*/

	geom::Mesh* load_mesh(std::wstring name) {
		util::FileMapping map = util::map_file(L"resources/models/" + name);
		document::DocumentNode* meshdata = document::parse_document(map.mapping);
		document::DocumentNode* geometry = meshdata->get_child("geometry")->children[0];
		return scene.geo_manager().create_mesh(geometry);
	}

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

	void resize_callback(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY) {
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
		mainFramebuffer.render_pass(mainRenderPass.get_pass()).size({ newX, newY }).add_color_attachment(vku::TEXTURE_TYPE_COLOR_ATTACHMENT_16F).add_color_attachment(vku::TEXTURE_TYPE_ID_BUFFER).depth(true).clear_color_float(vec4f{ 0.0F }).clear_color_int(vec4i32{-1}) .clear_depth(0.0F).create(cmdBuf);
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
		mainGui->resize(static_cast<float>(newX), static_cast<float>(newY));
		scene.framebuffer_resized(cmdBuf, newX, newY);
		//Make sure the selected object buffer gets cleared after resize.
		hasSelectedObjects = true;
	}

	void init_engine() {
		vku::init_vulkan(resize_callback);

		mouseObjectSelection = vku::generalAllocator->alloc_buffer(NUM_FRAME_DATA * sizeof(int32_t), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		
		//print_memory_info();
		ui::init();
		mainGui = new ui::Gui(&scene);

		//mvpMatricesBuffer = new vku::UniformBuffer<MVPMatrices>(true, true, VK_SHADER_STAGE_VERTEX_BIT);
		uiProjectionMatrixBuffer = new vku::UniformBuffer<mat4f>(true, false, VK_SHADER_STAGE_VERTEX_BIT);
		//lightingBuffer = new vku::UniformBuffer<LightingData>(true, false, VK_SHADER_STAGE_VERTEX_BIT);
		
		//mvpMatrices.projection.project_perspective(70.0F, static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT), 0.05F/*, 100.0F */);
		//mvpMatrices.view.set_identity().translate({ 0, -0.5F, -2.0F }).rotate(20, { 1, 0, 0 });
		//mvpMatrices.model.set_identity().scale(0.65);
		//lightingData = { {5.0F, 5.0F, 0.0F}, { 0, 0.5F, 2.0F } };
		bilinearSampler = new vku::UniformSampler2D(true, true, true, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
		nearestSampler = new vku::UniformSampler2D(false, true, true, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
		maxSampler = new vku::UniformSampler2D(true, false, false, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_SAMPLER_REDUCTION_MODE_MAX);
		minSampler = new vku::UniformSampler2D(true, false, false, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, VK_SAMPLER_REDUCTION_MODE_MIN);
		textureArray = new vku::UniformTexture2DArray(128);
		
		create_render_passes();

		vkQueueWaitIdle(vku::graphicsQueue);
		VkCommandBuffer cmdBuf = vku::graphics_cmd_buf();
		vkResetCommandPool(vku::device, vku::graphicsCommandPools[vku::currentFrame], 0);
		vku::begin_command_buffer(cmdBuf);

		resize_callback(cmdBuf, vku::swapChainExtent.width, vku::swapChainExtent.height);

		shaderSet = vku::create_descriptor_set({ bilinearSampler, textureArray });
		uiSet = vku::create_descriptor_set({ uiProjectionMatrixBuffer, bilinearSampler, textureArray });
		scene.init(cmdBuf, shaderSet, &mainRenderPass, &depthPreRenderPass, &outlineRenderPass, &mainFramebuffer, &mainDepthFramebuffer);
		finalPostSet = vku::create_descriptor_set({ sceneStorageImage, uiStorageImage, finalTexStorageImage, outlineStorageImage, scene.geo_manager().get_object_buffer() });

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
		uiPipeline = new vku::GraphicsPipeline();
		uiPipeline->name("ui").pass(uiRenderPass, 0).vertex_format(vku::POSITION_TEX_COLOR_TEXIDX_FLAGS).descriptor_set(*uiSet).build();
		uiLinePipeline = new vku::GraphicsPipeline();
		uiLinePipeline->name("ui_line").pass(uiRenderPass, 0).vertex_format(vku::POSITION_COLOR).descriptor_set(*uiSet).build();
		textPipeline = new vku::GraphicsPipeline();
		textPipeline->name("text").pass(uiRenderPass, 0).vertex_format(vku::POSITION_TEX).descriptor_set(*text::textDescSet).build();
		finalPostPipeline = new vku::ComputePipeline();
		finalPostPipeline->name("final_post").descriptor_set(*finalPostSet).push_constant(VkPushConstantRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, 2 * sizeof(int32_t)}).build();

		//vku::create_descriptor_sets();

		//vkWaitForFences(vku::device, 1, &vku::transferFence, VK_TRUE, UINT64_MAX);
		//vkResetFences(vku::device, 1, &vku::transferFence);

		vkResetCommandPool(vku::device, vku::transferCommandPool, 0);
		vku::begin_command_buffer(vku::transferCommandBuffer);

		geom::Mesh* testMesh = load_mesh(L"testmodel2_9.dmf");
		geom::Model* testModel = scene.new_instance(testMesh);

		text::creat_text_quad();
		fontRenderer = new text::FontRenderer();
		fontRenderer->load(L"starchickenfont.msdf");
		
		missingTexIdx = textureArray->add_texture(vku::missingTexture);
		whiteTexIdx = textureArray->add_texture(vku::whiteTexture);
		duckTex = vku::load_texture(L"orbus.dtf", 0);
		duckTexIdx = textureArray->add_texture(duckTex);

		//load_model(testBuffer, L"testmodel.dmf");
		
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

		lastTime = std::chrono::high_resolution_clock::now();
	}

	void cleanup() {
		ui::cleanup();
		delete mainGui;
		scene.cleanup();

		vku::delete_texture(duckTex);
		uiFramebuffer.destroy();
		mainFramebuffer.destroy();
		mainDepthFramebuffer.custom_depth(nullptr);
		mainDepthFramebuffer.destroy();
		objectOutlineFramebuffer.destroy();
		finalTexture->destroy();
		delete finalTexture;
		fontRenderer->destroy();
		delete fontRenderer;

		//delete mvpMatricesBuffer;
		delete uiProjectionMatrixBuffer;
		delete bilinearSampler;
		delete nearestSampler;
		delete maxSampler;
		delete minSampler;
		delete textureArray;
		//delete lightingBuffer;
		delete sceneStorageImage;
		delete uiStorageImage;
		delete finalTexStorageImage;
		delete outlineStorageImage;

		uiPipeline->destroy();
		uiLinePipeline->destroy();
		textPipeline->destroy();
		finalPostPipeline->destroy();
		delete uiPipeline;
		delete uiLinePipeline;
		delete textPipeline;
		delete finalPostPipeline;

		text::cleanup();

		vku::delete_descriptor_sets();
		mainRenderPass.destroy();
		uiRenderPass.destroy();
		depthPreRenderPass.destroy();
		outlineRenderPass.destroy();

		vku::generalAllocator->free_buffer(mouseObjectSelection);

		vku::cleanup_vulkan();
	}

	//We're going to go with snake_case names to match the standard library. Feels a little weird coming from java, but I'll get over it.
	//I wonder if I should have made this camelCase actually, to match glfw and vulkan
	//Variable names should be camelCase to differentiate them from functions
	//Class and struct names should be PascalCase
	//Avoid auto most of the time. I think it seriously affects the readability of some code when I can't just look at it and see the type.
	void entry_point() {
		init_engine();
		std::cout << "here0 " << job::threadData->threadId << std::endl;
		main_loop();
		cleanup();
	}
}

int main() {
	//util::msdf::generate_MSDF(L"resources/textures/startchickenfont2.svg", L"resources/textures/starchickenfont.msdf", 64, 64, true);
	//return 0;

	/*vec3f rot{90, 0, 0};
	vec3f pos{ 0, 0, 0 };
	mat4f mat;
	mat.set_identity().rotate(rot.x, {0, 1, 0});
	std::cout << "regular " << mat << std::endl << std::endl;
	mat.inverse();
	std::cout << "inverse " << mat << std::endl << std::endl;
	mat.set_identity().rotate(-rot.x, { 0, 1, 0 });
	std::cout << "reverse transform " << mat << std::endl;
	return 0;*/
	vku::recompile_modified_shaders(".\\resources\\shaders");
	engine::init_window();
	engine::init_engine();
	while (!windowing::window_should_close(engine::window)) {
		windowing::poll_events(engine::window);
		std::chrono::steady_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
		engine::deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - engine::lastTime).count();
		engine::lastTime = currentTime;
		int32_t mx;
		int32_t my;
		windowing::get_delta_mouse(&mx, &my);
		engine::deltaMouse.x = static_cast<float>(mx);
		engine::deltaMouse.y = static_cast<float>(my);

		engine::WINDOW_WIDTH = windowing::width;
		engine::WINDOW_HEIGHT = windowing::height;
		engine::update();
		engine::render();
	}
	VKU_CHECK_RESULT(vkDeviceWaitIdle(vku::device), "Failed to wait idle!");
	engine::cleanup();
	engine::cleanup_window();
	return 0;



	engine::init_window();

	uint32_t threadCount = std::max(static_cast<uint32_t>(1), std::thread::hardware_concurrency()-1);
	threadCount = 2;
	engine::jobSystem.init_job_system(threadCount);
	profiling::init_profiler([](void) {return static_cast<int32_t>(job::threadData->threadId); }, threadCount, 1024, "testlog");

	job::JobDecl decl{ engine::entry_point };
	engine::jobSystem.start_entry_point(decl);

	while (!engine::jobSystem.is_done()) {
		windowing::poll_events(engine::window);
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	engine::jobSystem.end_job_system();
	engine::cleanup_window();

	return 0;
}