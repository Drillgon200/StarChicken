#include "Engine.h"
#include <mutex>
#include <algorithm>
#include <processthreadsapi.h>
#include "graphics/SingleBufferAllocator.h"
#include "util/DrillMath.h"
#include "graphics/VertexBuffer.h"
#include "graphics/ShaderUniforms.h"
#include "graphics/GraphicsPipeline.h"
#include "graphics/RenderPass.h"
#include "graphics/TextureManager.h"
#include "util/FileDocument.h"
#include "util/Util.h"

uint64_t iterations;

namespace engine {
	//job::JobSystem jobSystem;
	job::JobSystem jobSystem;

	int32_t WINDOW_WIDTH = 512;
	int32_t WINDOW_HEIGHT = 512;

	windowing::Window* window;

	struct test {
		int bruh;
	};

	struct test2 {
		uint16_t arg;
		float mug;
	};

	struct MVPMatrices {
		mat4f model{};
		mat4f view{};
		mat4f projection{};
	} mvpMatrices;

	vku::UniformBuffer<MVPMatrices>* mvpMatricesBuffer = nullptr;
	
	vku::UniformSampler2D* bilinearSampler = nullptr;
	vku::UniformTexture2DArray* textureArray = nullptr;

	vku::RenderPass renderPass{};
	vku::GraphicsPipeline mainPipeline{};
	vku::DescriptorSet* shaderSet;

	vku::Texture* duckTex = nullptr;

#pragma pack(push, 1)
	/*struct Vertex {
		vec3f pos;
		vec2f tex;
	};*/
	struct Vertex {
		vec3f pos;
		vec2f tex;
		vec4f norm;
		vec4f tan;
	};
	struct LightingData {
		vec3f lightPos;
		uint8_t padding[4];
		vec3f eyePos;
		uint8_t padding2[4];
		LightingData(vec3f pos, vec3f eye) {
			lightPos = pos;
			eyePos = eye;
		}
	}lightingData{ {0}, {0} };
#pragma pack(pop)

	vku::UniformBuffer<LightingData>* lightingBuffer = nullptr;

	/*Vertex vertices[]{
		{{-0.5, 0.5, 0}, {0.0F, 1.0F}},
		{{0.5, 0.5, 0}, {1.0F, 1.0F}},
		{{-0.5, -0.5, 0}, {0.0F, 0.0F}},
		{{0.5, -0.5, 0}, {1.0F, 0.0F}}
	};

	uint16_t indices[]{
		0, 1, 2, 1, 3, 2
	};*/

	vku::VertexBuffer testBuffer(vku::POSITION_TEX_NORMAL_TANGENT);

	std::chrono::steady_clock::time_point lastTime;

	void render() {
		std::chrono::steady_clock::time_point currentTime = std::chrono::high_resolution_clock::now();
		float dt = std::chrono::duration<float, std::chrono::seconds::period>(
			
		currentTime - lastTime).count();
		bool shouldContinue = vku::setup_next_frame();
		if (shouldContinue) {
			mvpMatrices.projection.project_perspective(70.0F, static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT), 0.05F/*, 100.0F */ );
			mvpMatrices.model.rotate(dt*90, {0, 1, 0});
			mvpMatricesBuffer->update(mvpMatrices);
			lightingBuffer->update(lightingData);

			vku::begin_frame();

			vku::Texture::acquire_new_textures();

			renderPass.begin_pass(vku::graphics_cmd_buf(), vku::current_swapchain_framebuffer());
			mainPipeline.bind(vku::graphics_cmd_buf());
			shaderSet->bind(vku::graphics_cmd_buf(), mainPipeline);
			VkBuffer vertexBuffers[] = { vku::vertexBufferAllocator->get_buffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(vku::graphics_cmd_buf(), 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(vku::graphics_cmd_buf(), vku::indexBufferAllocator->get_buffer(), 0, VK_INDEX_TYPE_UINT16);

			testBuffer.draw();
			renderPass.end_pass(vku::graphics_cmd_buf());
			vku::end_frame();
			vkWaitForFences(vku::device, 1, &vku::transferFence, VK_TRUE, UINT64_MAX);
			vku::cleanup_staging_buffers();
			vku::SingleBufferAllocator::check_deallocate();
		}
		lastTime = currentTime;
	}

	void update() {
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

	//Must be called from main thread
	void init_window() {
		windowing::init();
		if (!windowing::query_vulkan_support()) {
			windowing::cleanup();
			std::cerr << "Failed to initialize game! Vulkan is not supported!" << std::endl;
			exit(1);
		}

		window = windowing::create_window(WINDOW_WIDTH, WINDOW_HEIGHT, "Star Chicken", true);
		windowing::set_user_ptr(window, &vku::frameBufferResized);
		windowing::set_window_resize_callback(window, framebuffer_resize_callback);
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

	void load_model(vku::VertexBuffer& buf, std::wstring name) {
		util::FileMapping map = util::map_file(L"resources/models/" + name);

		document::DocumentNode* model = document::parse_document(map.mapping);
		document::DocumentNode* child0 = model->get_child("geometry")->children[0];

		document::DocumentData* vertices = child0->get_data("vertices");
		document::DocumentData* indices = child0->get_data("indices");
		buf.upload_data(vertices->numBytes/vku::POSITION_TEX_NORMAL_TANGENT.size_bytes(), vertices->data, indices->numBytes/sizeof(uint16_t), indices->data);

		delete model;
		util::unmap_file(map);
	}

	void init_engine() {
		vku::init_vulkan();
		//print_memory_info();

		mvpMatricesBuffer = new vku::UniformBuffer<MVPMatrices>();
		lightingBuffer = new vku::UniformBuffer<LightingData>();
		mvpMatrices.projection.project_perspective(70.0F, static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT), 0.05F/*, 100.0F */);
		mvpMatrices.view.set_identity().translate({ 0, -0.5F, -2.0F }).rotate(20, { 1, 0, 0 });
		mvpMatrices.model.set_identity().scale(0.65);
		lightingData = { {5.0F, 5.0F, 0.0F}, { 0, 0.5F, 2.0F } };
		bilinearSampler = new vku::UniformSampler2D(true, true);
		textureArray = new vku::UniformTexture2DArray(1);
		shaderSet = &vku::create_descriptor_set({ mvpMatricesBuffer, bilinearSampler, textureArray, lightingBuffer });

		renderPass.create(vku::swapChainImageFormat);
		mainPipeline.name("triangle").pass(renderPass).vertex_format(vku::POSITION_TEX_NORMAL_TANGENT).descriptor_set(*shaderSet).build();

		vku::create_descriptor_sets();

		vkWaitForFences(vku::device, 1, &vku::transferFence, VK_TRUE, UINT64_MAX);
		vkResetFences(vku::device, 1, &vku::transferFence);

		vkResetCommandPool(vku::device, vku::transferCommandPool, 0);
		vku::begin_command_buffer(vku::transferCommandBuffer);
		duckTex = vku::load_texture(L"orbus.dtf");
		textureArray->update_texture(0, duckTex);
		load_model(testBuffer, L"testmodel.dmf");
		vku::end_command_buffer(vku::transferCommandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &vku::transferCommandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &vku::transferSemaphore;
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pWaitSemaphores = nullptr;
		submitInfo.pWaitDstStageMask = nullptr;
		vkQueueSubmit(vku::transferQueue, 1, &submitInfo, vku::transferFence);
		vku::mark_transfer_dirty();

		lastTime = std::chrono::high_resolution_clock::now();
	}

	void cleanup() {
		delete mvpMatricesBuffer;
		delete bilinearSampler;
		delete textureArray;
		delete lightingBuffer;

		vku::delete_texture(duckTex);

		mainPipeline.destroy();
		renderPass.destroy();
		vku::cleanup_staging_buffers();
		vku::SingleBufferAllocator::check_deallocate();
		vku::cleanup_vulkan();
	}

	//We're going to go with snake_case names to match the standard library. Feels a little weird coming from java, but I'll get over it.
	//I wonder if I should have made this camelCase actually, to match glfw and vulkan
	//Variable names should be camelCase to differentiate them from functions
	//Class and struct names should be PascalCase
	void entry_point() {
		init_engine();
		std::cout << "here0 " << job::threadData->threadId << std::endl;
		main_loop();
		cleanup();
	}
}

int main() {
	engine::init_window();
	engine::init_engine();
	while (!windowing::window_should_close(engine::window)) {
		windowing::poll_events(engine::window);
		engine::WINDOW_WIDTH = windowing::width;
		engine::WINDOW_HEIGHT = windowing::height;
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