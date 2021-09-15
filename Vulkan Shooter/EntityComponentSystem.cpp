#include <unordered_map>
#include <stdexcept>

namespace ecs {

	typedef struct Entity uint32_t;

	template<typename T>
	class ComponentManager {
		std::unordered_map<Entity, uint16_t> componentsByEntity{};
		std::unordered_map<uint16_t, Entity> entitiesByComponent{};
		std::vector<T> components{};

	public:

		void createEntity(Entity ent) {
			T component{};
			componentsByEntity[ent] = static_cast<uint16_t>(components.size());
			entitiesByComponent[static_cast<uint16_t>(components.size())] = ent;
			components.emplace_back(component);
		}

		void deleteEntity(Entity ent) {
			uint16_t idx = componentsByEntity.find(ent);
			if (idx != componentsByEntity.end()) {
				uint16_t end = components.size() - 1;
				if (end == idx) {
					components.pop_back();
					componentsByEntity.erase(ent);
					entitiesByComponent.erase(end);
				} else {
					componentsByEntity[entitiesByComponent[end]] = idx;
					componentsByEntity.erase(ent);
					entitiesByComponent[idx] = entitiesByComponent[end];
					entitiesByComponent.erase(end);
					components[idx] = components[end];
					components.pop_back();
				}
			}
		}

		T* getComponentForEntity(Entity e) {
			uint16_t idx = componentsByEntity.find(e);
			if (comp != componentsByEntity.end()) {
				return &components[idx];
			}
			throw std::runtime_error("Entity not found");
		}
	};
}