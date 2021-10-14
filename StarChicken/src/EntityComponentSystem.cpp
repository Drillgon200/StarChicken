#include <unordered_map>
#include <stdexcept>
#include "EntityComponentSystem.h"

namespace ecs {



	void ComponentSystem::removeEntity(Entity ent) {
		if (componentDeletes.find(ent) == componentDeletes.end()) {
			throw std::runtime_error("Entity not found for remove!");
		}
		std::vector<IComponentManager*> types = componentDeletes[ent];
		for (IComponentManager* type : types) {
			type->deleteEntity(ent);
		}
		componentDeletes.erase(ent);
	}

}