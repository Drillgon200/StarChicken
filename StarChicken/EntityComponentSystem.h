#pragma once

#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/common.hpp>
#include <glm/gtx/quaternion.hpp>
#include "JobSystem.h"

namespace ecs {
	using Entity = uint32_t;

	const Entity NULL_ENTITY = 0;

	struct HierarchyComponent {
		Entity parent = NULL_ENTITY;
		glm::mat4 world_transform;
	};

	struct TransformComponent {
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;

		glm::mat4 toMatrix() {
			glm::mat4 matrix = glm::toMat4(rotation);
			glm::scale(matrix, scale);
			matrix[3][0] = position.x;
			matrix[3][1] = position.y;
			matrix[3][2] = position.z;
			return matrix;
		}
	};

	class ComponentSystem;

	class IComponentManager {
	public:
		virtual void deleteEntity(Entity ent) = 0;
		virtual void makeParent(Entity parent, Entity child) = 0;
		//Need a method to delete from inside the class itself, since the type is unknown when called. May or may not be a hack, I don't know that much C++ yet.
		virtual void delete_manager() = 0;
	};

	template<typename T>
	class ComponentManager : public IComponentManager {
	private:
		friend class ComponentSystem;

		ComponentSystem* sys;
		std::unordered_map<Entity, uint16_t> componentsByEntity{};
		std::unordered_map<uint16_t, Entity> entitiesByComponent{};
		std::vector<T> components{};
		bool ordered;
	public:

		ComponentManager(ComponentSystem* system, bool keepOrder) {
			sys = system;
			ordered = keepOrder;
		}

		void createEntity(Entity ent) {
			T component{};
			componentsByEntity[ent] = static_cast<uint16_t>(components.size());
			entitiesByComponent[static_cast<uint16_t>(components.size())] = ent;
			components.emplace_back(component);
		}

		void setComponent(Entity ent, T& component) {
			if (componentsByEntity.find(ent) != componentsByEntity.end()) {
				throw std::runtime_error("Component for provided entity already exists!");
			}
			componentsByEntity[ent] = static_cast<uint16_t>(components.size());
			entitiesByComponent[static_cast<uint16_t>(components.size())] = ent;
			components.emplace_back(component);
		}

		void deleteEntity(Entity ent) override {
			if (componentsByEntity.find(ent) != componentsByEntity.end()) {
				uint16_t idx = componentsByEntity[ent];
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

		T* getComponent(Entity e) {
			std::unordered_map<Entity, uint16_t>::iterator idx = componentsByEntity.find(e);
			if (idx != componentsByEntity.end()) {
				return &components[idx->second];
			}
			return nullptr;
		}

		bool hasComponent(Entity e) {
			return getComponent(e) != nullptr;
		}

		void makeParent(Entity parent, Entity child) override {
			if (ordered) {

			}
		}

		void moveItem(uint16_t from, uint16_t to) {
			assert(from < components.size());
			assert(to < components.size());
			if (from == to){
				return;
			}
			if (ordered) {
				Entity tEnt = entitiesByComponent[to];
				T tTo = std::move(components[to]);

				uint8_t direction = from > to ? -1 : 1;
				for (uint16_t i = from; i != to; i += direction) {
					uint16_t next = i + direction;
					components[i] = std::move(components[next]);
					entitiesByComponent[i] = entitiesByComponent[next];
					componentsByEntity[entitiesByComponent[i]] = i;
				}

				componentsByEntity[tEnt] = from;
				entitiesByComponent[from] = tEnt;
				components[from] = std::move(tTo);
			} else {
				Entity tEnt = entitiesByComponent[to];
				T tTo = std::move(components[to]);

				componentsByEntity[to] = componentsByEntity[from];
				entitiesByComponent[to] = entitiesByComponent[from];
				components[to] = std::move(components[from]);

				componentsByEntity[tEnt] = from;
				entitiesByComponent[from] = tEnt;
				components[from] = std::move(tTo);
			}
		}

		uint32_t size() {
			return components.size();
		}

		void delete_manager() override {
			delete this;
		}
	};

	class ComponentSystem {
	private:
		//Default components present in every entity
		ComponentManager<HierarchyComponent>* hierarchyManager = nullptr;
		ComponentManager<TransformComponent>* transformManager = nullptr;

		std::unordered_map<Entity, std::vector<IComponentManager*>> componentDeletes{};
		std::unordered_map<uintptr_t, uintptr_t> componentMap{};
		std::vector<IComponentManager*> allComponentManagers{};
		std::vector<Entity> removedEntityIds{};
		uint32_t currentEntityId = 1;

	public:
		job::RWSpinLock lock{};

		ComponentSystem() {
			hierarchyManager = newComponentManager<HierarchyComponent>(true);
			transformManager = newComponentManager<TransformComponent>(true);
		}
		~ComponentSystem() {
			for (IComponentManager* man : allComponentManagers) {
				man->delete_manager();
			}
		}

		template<typename T>
		ComponentManager<T>* newComponentManager(bool keepOrder) {
			uintptr_t typeId = reinterpret_cast<uintptr_t>(&typeid(T));
			assert(componentMap.find(typeId) == componentMap.end());
			ComponentManager<T>* man = new ComponentManager<T>(this, keepOrder);
			allComponentManagers.push_back(man);
			componentMap[typeId] = reinterpret_cast<uintptr_t>(man);
			return man;
		}

		template<typename T>
		ComponentManager<T>* getComponentManager() {
			uintptr_t typeId = reinterpret_cast<uintptr_t>(&typeid(T));
			std::unordered_map<uintptr_t, uintptr_t>::iterator itr = componentMap.find(typeId);
			if (itr == componentMap.end())
				return nullptr;
			return reinterpret_cast<ComponentManager<T>*>(itr->second);
		}

		//TODO keep order
		template<typename T>
		void addComponent(Entity e, T& component, bool keepOrdered) {
			uintptr_t typeId = reinterpret_cast<uintptr_t>(&typeid(T));
			uintptr_t manager = componentMap[typeId];
			ComponentManager<T>* man = nullptr;
			if (manager == 0) {
				man = newComponentManager<T>(keepOrdered);
				man->setComponent(e, component);
			} else {
				man = reinterpret_cast<ComponentManager<T>*>(manager);
				man->setComponent(e, component);
			}

			if (componentDeletes.find(e) == componentDeletes.end()) {
				componentDeletes[e] = std::vector<IComponentManager*>();
			}
			componentDeletes[e].push_back(man);
		}

		template<typename T>
		T* getComponent(Entity e) {
			uintptr_t typeId = reinterpret_cast<uintptr_t>(&typeid(T));
			uintptr_t manager = componentMap[typeId];
			if (manager != 0) {
				ComponentManager<T>* man = reinterpret_cast<ComponentManager<T>*>(manager);
				return man->getComponent(e);
			} else {
				return nullptr;
			}
		}

		template<typename T>
		bool hasComponent(Entity e) {
			return getComponent<T>(e) != nullptr;
		}

		void makeParent(Entity parent, Entity child) {
			HierarchyComponent* cHier = getComponent<HierarchyComponent>(child);
			TransformComponent* cTransform = getComponent<TransformComponent>(child);
			cHier->parent = parent;
			if (parent == NULL_ENTITY){
				cHier->world_transform = cTransform->toMatrix();
				return;
			}
			HierarchyComponent* pHier = getComponent<HierarchyComponent>(parent);
			cHier->world_transform = pHier->world_transform * cTransform->toMatrix();

			std::vector<IComponentManager*> pTypes = componentDeletes[parent];
			std::vector<IComponentManager*> cTypes = componentDeletes[child];
			for (IComponentManager* man : pTypes) {
				//Why doesn't vector have a contains method?
				if (std::count(cTypes.begin(), cTypes.end(), man)) {
					man->makeParent(parent, child);
				}
			}
		}

		Entity createEntity() {
			Entity ent = NULL_ENTITY;
			if (removedEntityIds.size() > 0) {
				ent = removedEntityIds.back();
				removedEntityIds.pop_back();
			} else {
				ent = currentEntityId++;
			}
			HierarchyComponent hier{};
			this->addComponent<HierarchyComponent>(ent, hier, true);
			TransformComponent tran{};
			this->addComponent<TransformComponent>(ent, tran, true);
		}

		template<typename T>
		void runSystem(void (*func)(T&)) {
			ComponentManager<T>* man = getComponentManager<T>();
			for (T& t : man->components) {
				func(t);
			}
		}

		template<typename T>
		struct RangeFuncArg {
			std::vector<T>& components;
			void (*func)(T&);
			uint16_t min;
			uint16_t max;
		};

		template<typename T>
		static void runFuncForRange(void* vparg) {
			RangeFuncArg<T>* arg = reinterpret_cast<RangeFuncArg<T>>(vparg);
			for (uint16_t i = arg->min; i < arg->max; i++) {
				arg->func(arg->components[i]);
			}
		}

		template<typename T>
		void runSystemParallel(void (*func)(T&), job::JobSystem& jobSystem, uint16_t minPerJob = 16) {
			ComponentManager<T>* man = getComponentManager<T>();
			uint16_t count = std::min(man->size()/minPerJob + 1, jobSystem.thread_count());
			uint16_t per = (man->size() + (count-1))/count;
			job::JobDecl decls[count];
			RangeFuncArg<T> args[count];
			for (uint16_t i = 0; i < count; i++) {
				uint16_t min = i * per;
				uint16_t max = std::min(min + per, man->size());
				args[i] = {man->components, func, min, max};
				decls[i] = job::JobDecl(runFuncForRange<T>, reinterpret_cast<void*>(args[i]));
			}
			jobSystem.start_jobs_and_wait_for_counter(&decls, count);
		}

		void removeEntity(Entity ent);
	};
}