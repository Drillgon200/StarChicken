#include "Scene.h"
#include "../resources/Models.h"
#include "../Engine.h"
#include "../RenderSubsystem.h"
#include "../resources/DefaultResources.h"
#include "../graphics/Framebuffer.h"
#include "../graphics/RenderPass.h"
#include "../util/Util.h"

namespace scene {

	void Scene::init() {
		activeObject = nullptr;
		renderer = new SceneRenderer(this);
	}

	void Scene::cleanup() {
		renderer->cleanup();
		for (geom::Model* model : cleanupModels) {
			delete model;
		}
		delete renderer;
	}

	void Scene::framebuffer_resized(VkCommandBuffer cmdBuf, uint32_t newX, uint32_t newY) {
		renderer->framebuffer_resized(cmdBuf, newX, newY);
	}

	void Scene::add_model(geom::Model* model) {
		sceneModels.push_back(model);
	}

	geom::Model* Scene::new_instance(geom::Mesh* mesh) {
		geom::Model* model = new geom::Model(mesh);
		renderer->geo_manager().add_model(*model);
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
			activeObject = renderer->geo_manager().get_object_list()[id];
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