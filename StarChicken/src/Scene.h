#pragma once

#include "graphics/VkUtil.h"
#include "util/DrillMath.h"
#include "graphics/VertexFormats.h"
#include "graphics/geometry/GeometryAllocator.h"
#include "EntityComponentSystem.h"
#include "graphics/geometry/Models.h"

namespace vku {
	class Framebuffer;
	class RenderPass;
}
namespace geom {
	class SelectableObject;
}
namespace scene {

	class Scene {
	private:
		friend class Camera;
		std::vector<Camera*> cameras;
		//Contains every model present in this scene
		std::vector<geom::Model*> sceneModels;
		//Models that this scene owns (the sceneModels vector might hold models created externally)
		std::vector<geom::Model*> cleanupModels;

		geom::SelectableObject* activeObject;
		std::vector<geom::SelectableObject*> selectedObjects{};

		ecs::ComponentSystem entityComponentSystem;
		vku::WorldGeometryManager geometryManager;
		vku::Framebuffer* framebuffer;
		vku::Framebuffer* depthFramebuffer;

		vku::Texture* depthPyramid;
		//This uniform is a static object, not recreated on resize
		vku::UniformTexture2D* depthPyramidUniform{ nullptr };
		vku::UniformTexture2D** depthPyramidReadUniforms;
		vku::UniformStorageImage** depthPyramidWriteUniforms;
		vku::DescriptorSet** depthPyramidDownsampleSets;
		vku::ComputePipeline* depthPyramidDownsample;
		uint32_t depthPyramidLevels;
	public:

		void init(VkCommandBuffer cmdBuf, vku::DescriptorSet* mainShaderSet, vku::RenderPass* renderPass, vku::RenderPass* depthPass, vku::RenderPass* objectIdPass, vku::Framebuffer* mainFramebuffer, vku::Framebuffer* mainDepthFramebuffer);
		void cleanup();
		void add_model(geom::Model* model);
		geom::Model* new_instance(geom::Mesh* mesh);
		void add_camera(Camera* cam);

		void select_object(int32_t id);

		inline std::vector<geom::SelectableObject*>& get_selected_objects() {
			return selectedObjects;
		}

		inline geom::SelectableObject* get_active_object() {
			return activeObject;
		}

		inline vku::WorldGeometryManager& geo_manager() {
			return geometryManager;
		}

		inline vku::Framebuffer* get_framebuffer() {
			return framebuffer;
		}

		void framebuffer_resized(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY);

		void generate_depth_pyramid(VkCommandBuffer cmdBuf);
		void prepare_render_world(vku::RenderPass& renderPass);
		void render_world(Camera& cam, vku::WorldRenderPass pass);
	};

	enum CameraProjectionType {
		CAMERA_PROJECTION_ORTHOGRAPHIC,
		CAMERA_PROJECTION_PERSPECTIVE
	};

	struct Camera {
		Scene* scene;
		vku::Framebuffer* framebuffer;
		VkViewport viewport;
		mat4f cameraMatrix;
		//Inverse of cameraMatrix
		mat4f viewMatrix;
		mat4f projectionMatrix;
		mat4f invProjectionMatrix;
		mat4f viewProjectionMatrix;
		mat4f invViewProjectionMatrix;
		vec3f forwardVector;
		vec3f rightVector;
		vec3f upVector;
		vec3f position;
		vec3f rotation;
		CameraProjectionType projectionType;
		float fov;
		float orbitOffset;
		std::vector<vku::GeometrySet> geometrySets{};
		std::vector<vku::GeometrySet> selectedSets{};
		std::vector<vku::GeometrySet> prevGeometrySets{};
		//Index used for the view/projection matrix array and stuff
		uint32_t cameraIndex;

		Camera(Scene* scene, vku::Framebuffer* fbo) : scene{ scene }, framebuffer{ fbo }, cameraIndex{ 0 }, orbitOffset{ 0.0F }{
			cameraMatrix.set_identity();
			viewMatrix.set_identity();
			projectionMatrix.set_identity();
			invProjectionMatrix.set_identity();
			viewport = {};
			position = { 0.0F, 0.0F, 0.0F };
			rotation = { 0.0F, 0.0F, 0.0F };
			projectionType = CAMERA_PROJECTION_PERSPECTIVE;
			fov = 90.0F;
		}

		void set(Camera* cam) {
			scene = cam->scene;
			framebuffer = cam->framebuffer;
			viewport = cam->viewport;
			cameraMatrix = cam->cameraMatrix;
			viewMatrix = cam->viewMatrix;
			projectionMatrix = cam->projectionMatrix;
			invProjectionMatrix = cam->invProjectionMatrix;
			forwardVector = cam->forwardVector;
			rightVector = cam->rightVector;
			position = cam->position;
			orbitOffset = cam->orbitOffset;
			rotation = cam->rotation;
			projectionType = cam->projectionType;
			fov = cam->fov;
		}

		Camera& set_viewport(VkViewport& viewport) {
			this->viewport = viewport;
			float width = viewport.width;
			switch (projectionType) {
			case CAMERA_PROJECTION_ORTHOGRAPHIC:
				projectionMatrix.project_ortho(viewport.width, 0, viewport.height, 0, 5000.0F, 0.05F);
				break;
			case CAMERA_PROJECTION_PERSPECTIVE:
				projectionMatrix.project_perspective(fov, viewport.width / viewport.height, 0.05F);
				break;
			}
			invProjectionMatrix.set(projectionMatrix).inverse();
			viewProjectionMatrix = projectionMatrix * viewMatrix;
			invViewProjectionMatrix.set(viewProjectionMatrix).inverse();
			return *this;
		}

		Camera& set_viewport(float x, float y, float width, float height) {
			VkViewport viewport;
			viewport.x = x;
			viewport.y = y;
			viewport.width = width;
			viewport.height = height;
			viewport.minDepth = 0.0F;
			viewport.maxDepth = 1.0F;
			return set_viewport(viewport);
		}

		Camera& set_projection_type(CameraProjectionType type) {
			projectionType = type;
			set_viewport(viewport);
			return *this;
		}

		Camera& set_fov(float f) {
			fov = f;
			set_viewport(viewport);
			return *this;
		}

		Camera& set_orbit_offset(float f) {
			orbitOffset = f;
			return *this;
		}

		Camera& set_position(vec3f pos) {
			this->position = pos;
			return *this;
		}

		Camera& set_rotation(vec3f rotation) {
			this->rotation = rotation;
			return *this;
		}

		Camera& update_cam_transform() {
			cameraMatrix.set_identity();
			cameraMatrix.translate(position);
			if (rotation.y != 0.0F) {
				cameraMatrix.rotate(rotation.y, { 0, 1, 0 });
			}
			if (rotation.x != 0.0F) {
				cameraMatrix.rotate(rotation.x, { 1, 0, 0 });
			}
			if (rotation.z != 0.0F) {
				cameraMatrix.rotate(rotation.z, { 0, 0, 1 });
			}
			if (orbitOffset != 0.0F) {
				cameraMatrix.translate({ 0.0F, 0.0F, orbitOffset });
			}
			vec4f a{ 0.0F, 0.0F, 0.0F, 1.0F };
			vec4f b = cameraMatrix * a;
			forwardVector = normalize(vec3f{ -cameraMatrix[2][0], -cameraMatrix[2][1], -cameraMatrix[2][2] });
			rightVector = normalize(vec3f{ cameraMatrix[0][0], cameraMatrix[0][1], cameraMatrix[0][2] });
			upVector = normalize(vec3f{ cameraMatrix[1][0], cameraMatrix[1][1], cameraMatrix[1][2] });
			return *this;
		}

		Camera& update_cam_matrices() {
			update_cam_transform();
			viewMatrix.set(cameraMatrix).inverse();
			viewProjectionMatrix = projectionMatrix * viewMatrix;
			invViewProjectionMatrix.set(viewProjectionMatrix).inverse();
			//viewMatrix.set_identity().rotate(rotation.y, { 1, 0, 0 }).rotate(rotation.x, { 0, 1, 0 }).translate({ -position.x, -position.y, -position.z });
			return *this;
		}


		vec3f project(vec3f worldPos) {
			vec4f clip = viewProjectionMatrix * vec4f{ worldPos.x, worldPos.y, worldPos.z, 1.0F };
			float invW = 1.0F / clip.w;
			vec3f ndc{ clip.x * invW * 0.5F + 0.5F, clip.y * invW * 0.5F + 0.5F, clip.z * invW * 0.5F + 0.5F };
			return vec3f{ ndc.x * viewport.width + viewport.x, ndc.y * viewport.height + viewport.y, ndc.z };
		}

		vec3f unproject(vec3f screenPos) {
			vec2f ndc = vec2f{ (screenPos.x - viewport.x) / viewport.width, (screenPos.y - viewport.y) / viewport.height };
			vec4f unproj = invViewProjectionMatrix * vec4f{ ndc.x * 2.0F - 1.0F, ndc.y * 2.0F - 1.0F, screenPos.z * 2.0F - 1.0F, 1.0F };
			float invW = 1.0F / unproj.w;
			return vec3f{ unproj.x * invW, unproj.y * invW, unproj.z * invW };
		}

		vec3f unproject(vec2f screenPos, float planeDistance) {
			vec2f ndc = vec2f{ (screenPos.x - viewport.x) / viewport.width, (screenPos.y - viewport.y) / viewport.height };
			//0.502887 * 2.0F - 1.0F
			vec4f unproj = invProjectionMatrix * vec4f{ ndc.x * 2.0F - 1.0F, ndc.y * 2.0F - 1.0F, 0.5F, 1.0F };
			float invW = 1.0F / unproj.w;
			vec3f camSpace = normalize(vec3f{ unproj.x * invW, unproj.y * invW, unproj.z * invW });
			float intersect = 1.0F / camSpace.z;
			camSpace *= intersect * planeDistance;
			vec4f world = cameraMatrix * vec4f{ camSpace.x, camSpace.y, -camSpace.z, 1.0F};
			return vec3f{ world.x, world.y, world.z };
		}

		vec3f& get_forward_vector() {
			return forwardVector;
		}

		vec3f& get_right_vector() {
			return rightVector;
		}

		vec3f& get_up_vector() {
			return upVector;
		}

		vec3f get_offset_position() {
			return position - get_forward_vector() * orbitOffset;
		}
		
		void add_render_model(geom::Model& model) {
			if (geometrySets.empty() || (geometrySets.back().setModelCount >= 256)) {
				geometrySets.push_back(vku::GeometrySet{});
				scene->geometryManager.alloc_geo_set(&geometrySets.back());
			}
			geometrySets.back().add_model(model);
			if (model.get_selection() > 0) {
				if (selectedSets.empty() || (selectedSets.back().setModelCount >= 256)) {
					selectedSets.push_back(vku::GeometrySet{});
					//std::cout << selectedSets.back().setModelCount << std::endl;
					scene->geometryManager.alloc_geo_set(&selectedSets.back());
				}
				selectedSets.back().add_model(model);
			}
		}
	};
}