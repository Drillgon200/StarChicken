#include "Gui.h"
#include "..\Engine.h"
#include "..\RenderSubsystem.h"
#include "..\InputSubsystem.h"
#include "..\util\Util.h"
#include "..\scene\Scene.h"

#define MIN_PANEL_SIZE 40.0F
#define PANEL_PADDING 1.0F

namespace ui {

	enum VertexFlag {
		VERTEX_FLAG_NO_CORNER_TOP_LEFT = 1 << 0,
		VERTEX_FLAG_NO_CORNER_TOP_RIGHT = 1 << 1,
		VERTEX_FLAG_NO_CORNER_BOTTOM_LEFT = 1 << 2,
		VERTEX_FLAG_NO_CORNER_BOTTOM_RIGHT = 1 << 3,
		VERTEX_FLAG_INVERSE = 1 << 4,
		VERTEX_FLAG_NO_DISCARD = 1 << 5
	};

	vku::DynamicBuffer<GuiSolidVertex> solidGeoBuffer;
	vku::DynamicBuffer<GuiLineVertex> lineGeoBuffer;

	Gui::Gui(scene::Scene* scene) {
		this->scene = scene;
		mainPanel = new PanelSceneView(this, nullptr, { 0, 0, 1, 1 });
		mainPanel->set_root(&mainPanel);
		mainPanel->self = &mainPanel;
		mouseCaptureBox = nullptr;
		grabbedPanel = nullptr;
		activePanel = nullptr;
	}
	Gui::~Gui() {
		delete mainPanel;
	}

	bool Gui::on_click(float x, float y, uint32_t button, bool release) {
		bool didAction = mainPanel->mouse_click(x, y, button, release);
		if (release) {
			if (grabbedPanel) {
				grabbedPanel->sideGrabbed = dm::Direction2D::NONE;
				grabbedPanel = nullptr;
				grabbedPanelConnected.clear();
			}
		}
		return didAction;
	}
	void Gui::on_key_typed(uint32_t button, uint32_t state) {
		if (activePanel) {
			activePanel->key_typed(button, state);
		}
	}
	void Gui::mouse_drag(float x, float y, float dx, float dy) {
		if (mouseCaptureBox) {
			//Wrap cursor
			float newX = x + dx;
			float newY = y + dy;
			if (newX < mouseCaptureBox->minX) {
				newX = mouseCaptureBox->maxX;
			} else if (newX > mouseCaptureBox->maxX) {
				newX = mouseCaptureBox->minX;
			}
			if (newY < mouseCaptureBox->minY) {
				newY = mouseCaptureBox->maxY;
			} else if (newY > mouseCaptureBox->maxY) {
				newY = mouseCaptureBox->minY;
			}
			if ((newX != (x + dx)) || (newY != (y + dy))) {
				windowing::set_mouse(vec2f{ newX, newY });
				x = newX - dx;
				y = newY - dy;
			}
		}
		

		mainPanel->mouse_drag(x, y, dx, dy);
		if (grabbedPanel) {
			move_edge(grabbedPanel->sideGrabbed, (dm::direction2DAxis[static_cast<uint32_t>(grabbedPanel->sideGrabbed)] == dm::Axis::X) ? dx : dy);
		}
	}
	void Gui::mouse_hover(float x, float y) {
		mainPanel->mouse_over(x, y);
	}
	void Gui::resize(float newWidth, float newHeight) {
		vec2f scaling{ newWidth / mainPanel->get_bounds().maxX, newHeight / mainPanel->get_bounds().maxY };
		mainPanel->rescale(scaling);
	}
	void Gui::move_edge(dm::Direction2D edge, float amount) {
		uint32_t thisIdx = AxisAlignedBB2Df::directionIndices[static_cast<uint32_t>(edge)];
		uint32_t otherIdx = AxisAlignedBB2Df::indexOpposites[thisIdx];
		for (uint32_t i = 0; i < grabbedPanelConnected.size(); i++) {
			Panel& panel = *grabbedPanelConnected[i];
			//if (&panel == grabbedPanel) {
			//	continue;
			//}
			AxisAlignedBB2Df otherBox = panel.boundingBox;
			if (grabbedPanelConnected[i]->boundingBox[otherIdx] == grabbedPanel->boundingBox[thisIdx]) {
				otherBox[otherIdx] += amount;
			} else {
				otherBox[thisIdx] += amount;
			}
			float width = otherBox.width(dm::direction2DAxis[static_cast<uint32_t>(edge)]) - MIN_PANEL_SIZE;
			amount += signum(amount) * std::min(width, 0.0F);
		}
		for (uint32_t i = 0; i < grabbedPanelConnected.size(); i++) {
			if (grabbedPanelConnected[i] == grabbedPanel) {
				//Make sure to update the grabbed panel last so it doesn't change position and misalign with connected panels.
				continue;
			}
			if (grabbedPanelConnected[i]->boundingBox[otherIdx] == grabbedPanel->boundingBox[thisIdx]) {
				grabbedPanelConnected[i]->boundingBox[otherIdx] += amount;
			} else {
				grabbedPanelConnected[i]->boundingBox[thisIdx] += amount;
			}
			grabbedPanelConnected[i]->rescale(vec2f{ 1.0F });
		}
		grabbedPanel->boundingBox[thisIdx] += amount;
		grabbedPanel->rescale(vec2f{ 1.0F });
		mainPanel->recalc_bounds();
	}
	void Gui::render() {
		mainPanel->render();
	}
	void Gui::render_cameras(vku::WorldRenderPass pass) {
		std::vector<Panel*> panels{};
		panels.push_back(mainPanel);
		while (!panels.empty()) {
			Panel* current = panels.back();
			panels.pop_back();
			if (current->child0) {
				panels.push_back(current->child0);
				panels.push_back(current->child1);
			} else {
				current->render_cameras(pass);
			}
		}
	}
	void Gui::handle_input(float mouseX, float mouseY) {
		mainPanel->handle_input(mouseX, mouseY);
	}

	void Gui::mouse_scroll(float amount) {
		if (activePanel) {
			activePanel->mouse_scroll(amount);
		}
	}


	Panel::Panel(Gui* gui, Panel** top, AxisAlignedBB2Df bounds) : gui{ gui }, root{ top }, widgets{}, boundingBox{ bounds }, child0{ nullptr }, child1{ nullptr }, sideGrabbed{ dm::Direction2D::NONE }, self{ nullptr } {
	}
	Panel::~Panel() {
		if (child0) {
			delete child0;
			delete child1;
		} else {
		}
	}

	void Panel::render() {
		if (child0 != nullptr) {
			child0->render();
			child1->render();
		} else {
			//Draw rounded rectangle to show panel
			NineSlice nineSlice{};
			nineSlice.width = 50;
			nineSlice.height = 50;
			nineSlice.xSlice[0] = 0.2F;
			nineSlice.xSlice[1] = 0.8F;
			nineSlice.ySlice[0] = 0.2F;
			nineSlice.ySlice[1] = 0.8F;
			AxisAlignedBB2Df box = boundingBox.expand(-PANEL_PADDING);
			draw_nine_sliced(box.minX, box.minY, box.maxX - box.minX, box.maxY - box.minY, 0, false, 0.26F, 0.26F, 0.26F, 1.0F, nineSlice, resources::whiteTexIdx, 0);
			for (uint32_t i = 0; i < widgets.size(); i++) {
				widgets[i].render();
			}
		}
	}

	void Panel::split(dm::Direction2D fromSide, float split) {
		if (child0) {
			return;
		}
		gui->activePanel = nullptr;
		dm::Axis axis = dm::direction2DAxis[static_cast<uint32_t>(fromSide)];
		if (axis == dm::Axis::X && ((split - boundingBox.minX) < MIN_PANEL_SIZE ||
			(boundingBox.maxX - split) < MIN_PANEL_SIZE)) {
			return;
		} else if (axis == dm::Axis::Y && ((split - boundingBox.minY) < MIN_PANEL_SIZE ||
			(boundingBox.maxY - split) < MIN_PANEL_SIZE)) {
			return;
		}
		Panel* newParent = new Panel(gui, root, boundingBox);
		*self = newParent;
		newParent->self = self;
		
		newParent->child0 = this;
		self = &newParent->child0;
		newParent->child1 = this->clone();
		newParent->child1->self = &newParent->child1;
		
		switch (axis) {
		case dm::Axis::X:
			newParent->child0->boundingBox.maxX = split;
			newParent->child1->boundingBox.minX = split;
			break;
		case dm::Axis::Y:
			newParent->child0->boundingBox.maxY = split;
			newParent->child1->boundingBox.minY = split;
			break;
		}
		newParent->child0->rescale(vec2f{ 1.0F });
		newParent->child1->rescale(vec2f{ 1.0F });
	}

	void Panel::join(uint32_t toKill) {
		Panel* keep;
		if (toKill == 0) {
			keep = child1;
		} else {
			keep = child0;
		}
		widgets = keep->widgets;
		delete child0;
		delete child1;
	}

	void Panel::rescale(vec2f scaling) {
		if (child0) {
			child0->rescale(scaling);
			child1->rescale(scaling);
			boundingBox = child0->boundingBox.box_union(child1->boundingBox);
		} else {
			boundingBox.minX *= scaling.x;
			boundingBox.maxX *= scaling.x;
			boundingBox.minY *= scaling.y;
			boundingBox.maxY *= scaling.y;
		}
	}

	//Weird flood fill connectivity search. I've since thought of a couple better algorithms and data structures
	//to do this better, but it works now and I don't want to spend the time to rewrite it.
	void Panel::find_connected_panels(dm::Direction2D edge, std::vector<Panel*>& list) {
		//Side indices for float comparisons
		uint32_t thisIdx = AxisAlignedBB2Df::directionIndices[static_cast<uint32_t>(edge)];
		uint32_t otherIdx = AxisAlignedBB2Df::indexOpposites[thisIdx];
		//Recursive search the panel tree to find any that match this edge
		std::vector<Panel*> panelStack{};
		panelStack.push_back(*root);
		while (!panelStack.empty()) {
			Panel* current = panelStack.back();
			panelStack.pop_back();
			if (current == this) {
				continue;
			}
			if (current->child0) {
				panelStack.push_back(current->child0);
				panelStack.push_back(current->child1);
				continue;
			}
			if (current->boundingBox[otherIdx] == boundingBox[thisIdx] && !util::vector_contains(list, current)) {
				list.push_back(current);
				//Find any panels connected to the this one that aren't connected to the one that's moving. Recursively find ones connected along an unbroken line.
				current->find_connected_panels(dm::direction2DOpposites[static_cast<uint32_t>(edge)], list);
			}
		}
	}
	void Panel::recalc_bounds() {
		if (child0) {
			child0->recalc_bounds();
			child1->recalc_bounds();
			boundingBox = child0->boundingBox.box_union(child1->boundingBox);
		}
	}

	Panel* Panel::clone() {
		Panel* p = new Panel(gui, root, boundingBox);
		p->child0 = child0;
		p->child1 = child1;
		p->widgets = widgets;
		return p;
	}

	void Panel::render_cameras(vku::WorldRenderPass pass) {

	}

	void Panel::mouse_over(float x, float y) {
		if (child0) {
			if (child0->boundingBox.contains(vec2f{ x, y })) {
				child0->mouse_over(x, y);
			} else if (child1->boundingBox.contains(vec2f{ x, y })) {
				child1->mouse_over(x, y);
			}
		} else {
			gui->activePanel = this;
			if (((boundingBox.minX + PANEL_PADDING) - x) >= 0 && boundingBox[AxisAlignedBB2Df::IDX_LEFT] != (*root)->boundingBox[AxisAlignedBB2Df::IDX_LEFT]) {
				windowing::set_cursor(windowing::CURSOR_TYPE_SIZE_HORIZONTAL);
			} else if (((boundingBox.maxX - PANEL_PADDING) - x) < PANEL_PADDING && boundingBox[AxisAlignedBB2Df::IDX_RIGHT] != (*root)->boundingBox[AxisAlignedBB2Df::IDX_RIGHT]) {
				windowing::set_cursor(windowing::CURSOR_TYPE_SIZE_HORIZONTAL);
			} else if (((boundingBox.minY + PANEL_PADDING) - y) >= 0 && boundingBox[AxisAlignedBB2Df::IDX_DOWN] != (*root)->boundingBox[AxisAlignedBB2Df::IDX_DOWN]) {
				windowing::set_cursor(windowing::CURSOR_TYPE_SIZE_VERTICAL);
			} else if (((boundingBox.maxY - PANEL_PADDING) - y) < PANEL_PADDING && boundingBox[AxisAlignedBB2Df::IDX_UP] != (*root)->boundingBox[AxisAlignedBB2Df::IDX_UP]) {
				windowing::set_cursor(windowing::CURSOR_TYPE_SIZE_VERTICAL);
			} else {
				windowing::set_cursor(windowing::CURSOR_TYPE_POINTER);
			}
		}
	}

	void Panel::mouse_drag(float x, float y, float dx, float dy) {
		if (child0) {
			if (child0->boundingBox.contains(vec2f{ x, y })) {
				child0->mouse_drag(x, y, dx, dy);
			}
			if (child1->boundingBox.contains(vec2f{ x, y })) {
				child1->mouse_drag(x, y, dx, dy);
			}
		}
	}

	void Panel::mouse_scroll(float amount) {
	}

	bool Panel::mouse_click(float x, float y, uint32_t button, bool release) {
		if (child0) {
			if (child0->boundingBox.contains(vec2f{ x, y })) {
				return child0->mouse_click(x, y, button, release);
			} else if (child1->boundingBox.contains(vec2f{ x, y })) {
				return child1->mouse_click(x, y, button, release);
			}
		} else {
			if (!release) {
				if (((boundingBox.minX + PANEL_PADDING) - x) >= 0) {
					sideGrabbed = dm::Direction2D::LEFT;
				} else if (((boundingBox.maxX - PANEL_PADDING) - x) < PANEL_PADDING) {
					sideGrabbed = dm::Direction2D::RIGHT;
				} else if (((boundingBox.minY + PANEL_PADDING) - y) >= 0) {
					sideGrabbed = dm::Direction2D::DOWN;
				} else if (((boundingBox.maxY - PANEL_PADDING) - y) < PANEL_PADDING) {
					sideGrabbed = dm::Direction2D::UP;
				}
				if (sideGrabbed != dm::Direction2D::NONE) {
					uint32_t idx = AxisAlignedBB2Df::directionIndices[static_cast<uint32_t>(sideGrabbed)];
					if (boundingBox[idx] == (*root)->boundingBox[idx]) {
						sideGrabbed = dm::Direction2D::NONE;
					} else {
						find_connected_panels(sideGrabbed, gui->grabbedPanelConnected);
						gui->grabbedPanel = this;
					}
					return true;
				}
			}
		}
		return false;
	}

	void Panel::handle_input(float mouseX, float mouseY) {
		if (child0) {
			if (child0->boundingBox.contains({ mouseX, mouseY })) {
				child0->handle_input(mouseX, mouseY);
			} else if (child1->boundingBox.contains({ mouseX, mouseY })) {
				child1->handle_input(mouseX, mouseY);
			}
		}
	}

	void Panel::key_typed(uint32_t key, uint32_t action) {
		if (!child0) {
			if ((key == windowing::KEY_R) && engine::userInput.is_key_down(windowing::KEY_CTRL) && (action == windowing::KEY_STATE_DOWN)) {
				vec2f mousePos = windowing::get_mouse();
				float distX = std::min(boundingBox.maxX - mousePos.x, mousePos.x - boundingBox.minX);
				float distY = std::min(boundingBox.maxY - mousePos.y, mousePos.y - boundingBox.minY);
				if (distY > distX) {
					split(dm::Direction2D::DOWN, mousePos.y);
				} else {
					split(dm::Direction2D::LEFT, mousePos.x);
				}
			}
		}
	}

	void Panel::set_root(Panel** root) {
		this->root = root;
	}

	AxisAlignedBB2Df& Panel::get_bounds() {
		return boundingBox;
	}

	PanelSceneView::PanelSceneView(Gui* gui, Panel** top, AxisAlignedBB2Df bounds) : Panel(gui, top, bounds), prevCamOrbitOffset{ 1.0F }{
		camera = new scene::Camera(gui->scene, gui->scene->get_renderer().get_framebuffer());
		camera->set_fov(90.0F).set_position({ 0, 0, 0 }).set_orbit_offset(length(vec3f{5, 5, 5})).set_rotation({ -45, 45, 0 }).set_projection_type(scene::CAMERA_PROJECTION_PERSPECTIVE).set_viewport(boundingBox.minX, boundingBox.minY, boundingBox.width(dm::Axis::X), boundingBox.width(dm::Axis::Y)).update_cam_matrices();
		gui->scene->add_camera(camera);
	}
	
	PanelSceneView::~PanelSceneView() {
		delete camera;
	}

	void PanelSceneView::rescale(vec2f scaling) {
		Panel::rescale(scaling);
		camera->set_viewport(boundingBox.minX, boundingBox.minY, boundingBox.width(dm::Axis::X), boundingBox.width(dm::Axis::Y));
		/*vec3f projection = camera->project(vec3f{0, 0, 0});
		std::cout << camera->projectionMatrix << std::endl << std::endl;
		std::cout << "project, 0 0 0 " << projection << std::endl;
		std::cout << "unproject, 0 0 0 " << camera->unproject(projection) << std::endl;
		std::cout << "unproject 2, 0 0 0 " << camera->unproject(vec2f{projection.x, projection.y}, camera->orbitOffset) << std::endl;
		projection = camera->project(vec3f{ 0, -1, 0 });
		std::cout << "project, 0 -1 0 " << projection << std::endl;
		std::cout << "unproject, 0 -1 0 " << camera->unproject(projection) << std::endl;
		std::cout << "unproject 2, 0 -1 0 " << camera->unproject(vec2f{ projection.x, projection.y }, length(vec3f{5, 6, 5})) << std::endl;
		projection = camera->project(vec3f{ 0, 1, 0 });
		std::cout << "project, 0 1 0 " << projection << std::endl;
		std::cout << "unproject, 0 1 0 " << camera->unproject(projection) << std::endl;
		std::cout << "unproject 2, 0 1 0 " << camera->unproject(vec2f{ projection.x, projection.y }, length(vec3f{ 5, 4, 5 })) << std::endl;*/
	}

	void PanelSceneView::render() {
		//Draw rounded rectangle to show panel
		NineSlice nineSlice{};
		nineSlice.width = 50;
		nineSlice.height = 50;
		nineSlice.xSlice[0] = 0.2F;
		nineSlice.xSlice[1] = 0.8F;
		nineSlice.ySlice[0] = 0.2F;
		nineSlice.ySlice[1] = 0.8F;
		AxisAlignedBB2Df box = boundingBox.expand(-PANEL_PADDING);
		draw_nine_sliced(box.minX, box.minY, box.maxX - box.minX, box.maxY - box.minY, 0, false, 0.0F, 0.0F, 0.0F, 0.0F, nineSlice, resources::whiteTexIdx, 0);
		for (uint32_t i = 0; i < widgets.size(); i++) {
			widgets[i].render();
		}
	}

	void PanelSceneView::mouse_scroll(float amount) {
		if (windowing::mouse_captured()) {
			return;
		}
		amount = -amount;
		float norm = (amount / fabs(amount)) * 0.5F + 0.5F;
		float scale = 0.8F + norm * 0.4F;
		camera->set_orbit_offset(camera->orbitOffset * scale).update_cam_matrices();
	}

	void PanelSceneView::mouse_drag(float x, float y, float dx, float dy) {
		if (windowing::mouse_captured() || (gui->mouseCaptureBox != &boundingBox)) {
			return;
		}
		if (engine::userInput.is_mouse_down(windowing::MOUSE_MIDDLE)) {
			if (camera->orbitOffset == 0.0F) {
				camera->set_orbit_offset(prevCamOrbitOffset);
				camera->set_position(camera->position + camera->forwardVector * camera->orbitOffset);
			}
			if (engine::userInput.is_key_down(windowing::KEY_SHIFT)) {
				vec3f pos1 = camera->unproject(vec2f{ x, y }, camera->orbitOffset);
				vec3f pos2 = camera->unproject(vec2f{ x + dx, y + dy }, camera->orbitOffset);
				vec3f position = camera->position;
				//position -= camera->get_right_vector() * dx * 0.01F * camera->orbitOffset;
				//position += camera->get_up_vector() * dy * 0.01F * camera->orbitOffset;
				position += (pos2 - pos1);
				camera->set_position(position).update_cam_matrices().set_viewport(boundingBox.minX, boundingBox.minY, boundingBox.width(dm::Axis::X), boundingBox.width(dm::Axis::Y));
			} else {
				vec3f rotation = camera->rotation;
				rotation.x -= dy * 0.35F;
				rotation.y -= dx * 0.35F;
				camera->set_rotation(rotation).update_cam_matrices().set_viewport(boundingBox.minX, boundingBox.minY, boundingBox.width(dm::Axis::X), boundingBox.width(dm::Axis::Y));
			}
		}
	}
	bool PanelSceneView::mouse_click(float x, float y, uint32_t button, bool release) {
		bool didAction = Panel::mouse_click(x, y, button, release);
		if (button == windowing::MOUSE_MIDDLE) {
			if (release) {
				gui->mouseCaptureBox = nullptr;
			} else {
				gui->mouseCaptureBox = &boundingBox;
			}
		}
		return didAction;
	}
	void PanelSceneView::handle_input(float mouseX, float mouseY) {
		if (!windowing::mouse_captured()) {
			return;
		}
		if (camera->orbitOffset != 0.0F) {
			prevCamOrbitOffset = camera->orbitOffset;
			camera->set_position(camera->get_offset_position());
			camera->set_orbit_offset(0.0F);
		}
		float forwardMovement = 0.0F;
		float sideMovement = 0.0F;
		float verticalMovement = 0.0F;
		if (engine::userInput.is_key_down(windowing::KEY_W)) {
			forwardMovement += 5.0F;
		}
		if (engine::userInput.is_key_down(windowing::KEY_A)) {
			sideMovement -= 5.0F;
		}
		if (engine::userInput.is_key_down(windowing::KEY_S)) {
			forwardMovement -= 5.0F;
		}
		if (engine::userInput.is_key_down(windowing::KEY_D)) {
			sideMovement += 5.0F;
		}
		if (engine::userInput.is_key_down(windowing::KEY_SPACE)) {
			verticalMovement += 5.0F;
		}
		if (engine::userInput.is_key_down(windowing::KEY_SHIFT)) {
			verticalMovement -= 5.0F;
		}

		vec3f camPos = camera->position;
		vec3f rotation = camera->rotation;

		rotation.x -= engine::userInput.get_delta_mouse().y * 0.15F;
		rotation.x = clamp<float>(rotation.x, -90.0F, 90.0F);
		rotation.y -= engine::userInput.get_delta_mouse().x * 0.15F;
		camera->set_rotation(rotation).update_cam_transform();

		camPos += camera->get_forward_vector() * forwardMovement * engine::deltaTime;
		camPos += camera->get_right_vector() * sideMovement * engine::deltaTime;
		camPos.y += verticalMovement * engine::deltaTime;

		camera->set_position(camPos).update_cam_matrices().set_viewport(boundingBox.minX, boundingBox.minY, boundingBox.width(dm::Axis::X), boundingBox.width(dm::Axis::Y));
	}

	void PanelSceneView::render_cameras(vku::WorldRenderPass pass) {
		gui->scene->get_renderer().render_world(*camera, pass);
	}

	Panel* PanelSceneView::clone() {
		if (child0) {
			throw std::runtime_error("Scene view panel should not have children!");
		}
		PanelSceneView* p = new PanelSceneView(gui, root, boundingBox);
		p->widgets = widgets;
		p->camera->set(camera);
		return p;
	}

	void draw_rect(float x, float y, float width, float height, float zLevel, float uMin, float vMin, float uMax, float vMax, vec4ui8 color, uint16_t texIdx, uint16_t flags) {
		uint16_t quadIndices[6]{ 0, 1, 2, 2, 3, 0 };

		GuiSolidVertex vertices[4];
		vertices[0].pos = { x, y, zLevel };
		vertices[0].tex = { uMin, vMin };
		vertices[1].pos = { x, y + height, zLevel };
		vertices[1].tex = { uMin, vMax };
		vertices[2].pos = { x + width, y + height, zLevel };
		vertices[2].tex = { uMax, vMax };
		vertices[3].pos = { x + width, y, zLevel };
		vertices[3].tex = { uMax, vMin };
		for (uint32_t i = 0; i < 4; i++) {
			vertices[i].color = color;
			vertices[i].texIdx = texIdx;
			vertices[i].flags = flags;
		}

		solidGeoBuffer.add_vertices_and_indices(4, vertices, 6, quadIndices);
	}

	void draw_rect(float x, float y, float width, float height, float zLevel, float uMin, float vMin, float uMax, float vMax, float r, float g, float b, float a, uint16_t texIdx, uint16_t flags) {
		vec4ui8 color{ r, g, b, a };
		draw_rect(x, y, width, height, zLevel, uMin, vMin, uMax, vMax, color, texIdx, flags);
	}

	void draw_rect(float x, float y, float width, float height, float zLevel, float uMin, float vMin, float uMax, float vMax, uint16_t texIdx, uint16_t flags) {
		draw_rect(x, y, width, height, zLevel, uMin, vMin, uMax, vMax, 1.0F, 1.0F, 1.0F, 1.0F, texIdx, flags);
	}
	void draw_rect(float x, float y, float width, float height, float zLevel, float uMin, float vMin, float uMax, float vMax, uint16_t flags) {
		draw_rect(x, y, width, height, zLevel, uMin, vMin, uMax, vMax, resources::whiteTexIdx, flags);
	}

	void draw_xtiled_rect(float x, float y, float width, float height, float tileWidth, float zLevel, float uMin, float vMin, float uMax, float vMax, vec4ui8 color, uint16_t texIdx, uint16_t flags) {
		for (float i = 0; i < width; i += tileWidth) {
			float overshotX = (std::max(width, i + tileWidth) - width);
			float currentWidth = tileWidth - overshotX;
			float currentUMax = remap(currentWidth, 0.0F, tileWidth, uMin, uMax);
			draw_rect(x + i, y, currentWidth, height, zLevel, uMin, vMin, currentUMax, vMax, color, texIdx, flags);
		}
	}

	void draw_ytiled_rect(float x, float y, float width, float height, float tileHeight, float zLevel, float uMin, float vMin, float uMax, float vMax, vec4ui8 color, uint16_t texIdx, uint16_t flags) {
		for (float j = 0; j < height; j += tileHeight) {
			float overshotY = (std::max(height, j + tileHeight) - height);
			float currentHeight = tileHeight - overshotY;
			float currentVMax = remap(currentHeight, 0.0F, tileHeight, vMin, vMax);
			draw_rect(x, y + j, height, currentHeight, zLevel, uMin, vMin, uMax, currentVMax, color, texIdx, flags);
		}
	}

	void draw_tiled_rect(float x, float y, float width, float height, float tileWidth, float tileHeight, float zLevel, float uMin, float vMin, float uMax, float vMax, vec4ui8 color, uint16_t texIdx, uint16_t flags) {
		for (float i = 0; i < width; i += tileWidth) {
			float overshotX = (std::max(width, i + tileWidth) - width);
			float currentWidth = tileWidth - overshotX;
			float currentUMax = remap(currentWidth, 0.0F, tileWidth, uMin, uMax);
			for (float j = 0; j < height; j += tileHeight) {
				float overshotY = (std::max(height, j + tileHeight) - height);
				float currentHeight = tileHeight - overshotY;
				float currentVMax = remap(currentHeight, 0.0F, tileHeight, vMin, vMax);
				draw_rect(x + i, y + j, currentWidth, currentHeight, zLevel, uMin, vMin, currentUMax, currentVMax, color, texIdx, flags);
			}
		}
	}

	void draw_nine_sliced(float x, float y, float width, float height, float zLevel, bool tileRepeat, float r, float g, float b, float a, NineSlice& slice, uint16_t texIdx, uint16_t flags) {
		vec4ui8 color{ r, g, b, a };
		vec2f innerSize{};
		innerSize.x = width - (slice.xSlice[0] * slice.width + (slice.width - slice.xSlice[1] * slice.width));
		innerSize.y = height - (slice.ySlice[0] * slice.height + (slice.height - slice.ySlice[1] * slice.height));

		//Technically I should be able to stitch this together more efficiently with indices, but whatever, I'm sure it won't be a real problem.

		//top left
		draw_rect(x, y, slice.xSlice[0] * slice.width, slice.ySlice[0] * slice.height, zLevel, 0, 0, slice.xSlice[0], slice.ySlice[0], color, texIdx, flags);
		//top right
		draw_rect(x + width - slice.width * (1-slice.xSlice[1]), y, slice.width - slice.xSlice[1] * slice.width, slice.ySlice[0] * slice.height, zLevel, slice.xSlice[1], 0, 1, slice.ySlice[0], color, texIdx, flags);
		//bottom left
		draw_rect(x, y + height - slice.height * (1.0F - slice.ySlice[1]), slice.xSlice[0] * slice.width, slice.height - slice.ySlice[1] * slice.height, zLevel, 0, slice.ySlice[1], slice.xSlice[0], 1, color, texIdx, flags);
		//bottom right
		draw_rect(x + width - slice.width * (1 - slice.xSlice[1]), y + height - slice.height * (1.0F - slice.ySlice[1]), slice.width - slice.xSlice[1] * slice.width, slice.height - slice.ySlice[1] * slice.height, zLevel, slice.xSlice[1], slice.ySlice[1], 1, 1, color, texIdx, flags);
		
		if (tileRepeat) {
			vec2f innerTileSize{ (slice.xSlice[1] - slice.xSlice[0]) * slice.width, (slice.ySlice[1] - slice.ySlice[0]) * slice.height };
			//middle top
			draw_xtiled_rect(x + slice.xSlice[0] * slice.width, y, innerSize.x, slice.ySlice[0] * slice.height, innerTileSize.x, zLevel, slice.xSlice[0], 0, slice.xSlice[1], slice.ySlice[0], color, texIdx, flags);
			//middle right
			draw_ytiled_rect(x + slice.xSlice[0] * slice.width + innerSize.x, y + slice.ySlice[0] * slice.height, slice.width - slice.xSlice[1] * slice.width, innerSize.y, innerTileSize.y, zLevel, slice.xSlice[1], slice.ySlice[0], 1, slice.ySlice[1], color, texIdx, flags);
			//middle bottom
			draw_xtiled_rect(x + slice.xSlice[0] * slice.width, y + slice.ySlice[0] * slice.height + innerSize.y, innerSize.x, slice.height - slice.ySlice[1] * slice.height, innerTileSize.x, zLevel, slice.xSlice[0], slice.ySlice[1], slice.xSlice[1], 1, color, texIdx, flags);
			//middle left
			draw_ytiled_rect(x, y + slice.ySlice[0] * slice.height, slice.xSlice[0] * slice.width, innerSize.y, innerTileSize.y, zLevel, 0, slice.ySlice[0], slice.xSlice[0], slice.ySlice[1], color, texIdx, flags);
			//middle
			draw_tiled_rect(x + slice.xSlice[0] * slice.width, y + slice.ySlice[0] * slice.height, innerSize.x, innerSize.y, innerTileSize.x, innerTileSize.y, zLevel, slice.xSlice[0], slice.ySlice[0], slice.xSlice[1], slice.ySlice[1], color, texIdx, flags);
		} else {
			//middle top
			draw_rect(x + slice.xSlice[0] * slice.width, y, innerSize.x, slice.ySlice[0] * slice.height, zLevel, slice.xSlice[0], 0, slice.xSlice[1], slice.ySlice[0], color, texIdx, flags);
			//middle right
			draw_rect(x + slice.xSlice[0] * slice.width + innerSize.x, y + slice.ySlice[0] * slice.height, slice.width - slice.xSlice[1] * slice.width, innerSize.y, zLevel, slice.xSlice[1], slice.ySlice[0], 1, slice.ySlice[1], color, texIdx, flags);
			//middle bottom
			draw_rect(x + slice.xSlice[0] * slice.width, y + slice.ySlice[0] * slice.height + innerSize.y , innerSize.x, slice.height - slice.ySlice[1] * slice.height, zLevel, slice.xSlice[0], slice.ySlice[1], slice.xSlice[1], 1, color, texIdx, flags);
			//middle left
			draw_rect(x, y + slice.ySlice[0] * slice.height, slice.xSlice[0] * slice.width, innerSize.y, zLevel, 0, slice.ySlice[0], slice.xSlice[0], slice.ySlice[1], color, texIdx, flags);
			//middle
			draw_rect(x + slice.xSlice[0] * slice.width, y + slice.ySlice[0] * slice.height, innerSize.x, innerSize.y, zLevel, slice.xSlice[0], slice.ySlice[0], slice.xSlice[1], slice.ySlice[1], color, texIdx, flags);
		}
	}

	void draw_gui(VkCommandBuffer cmdBuf) {
		solidGeoBuffer.draw(cmdBuf);
	}

	void init() {
		solidGeoBuffer.init(1024);
		lineGeoBuffer.init(128);
	}

	void cleanup() {
		solidGeoBuffer.cleanup();
		lineGeoBuffer.cleanup();
	}
}