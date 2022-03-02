#pragma once
#include <vector>
#include "..\util\DrillMath.h"
#include "..\graphics\DynamicBuffer.h"

namespace scene {
	class Scene;
	class Camera;
}
namespace vku {
	enum WorldRenderPass;
}

namespace ui {

#pragma pack(push, 1)
	struct GuiSolidVertex {
		vec3f pos;
		vec2f tex;
		vec4ui8 color;
		uint16_t texIdx;
		uint16_t flags;
	};

	struct GuiLineVertex {
		vec3f pos;
		vec4ui8 color;
	};
#pragma pack(pop)

	struct Widget {
		vec2f anchor;
		vec2f offset;
		AxisAlignedBB2Df boundingBox;
		std::vector<Widget> children;

		Widget() : anchor{ 0, 0 }, offset{ 0, 0 }, boundingBox{}, children{} {

		}

		void move_to(vec2f location) {
			vec2f diff = location - offset;
			for (Widget& widget : children) {
				widget.move_to(widget.offset + diff);
			}
			offset = location;
		}

		void render() {
		}
	};

	class Gui;

	struct Panel {
		Gui* gui;
		Panel** root;
		std::vector<Widget> widgets;
		AxisAlignedBB2Df boundingBox;
		//Self is a pointer to the pointer that stores this (child0 or child1 in most cases, mainPanel in the gui in the root's case).
		Panel** self;
		Panel* child0;
		Panel* child1;
		dm::Direction2D sideGrabbed;

		Panel(Gui* gui, Panel** top, AxisAlignedBB2Df bounds);
		~Panel();

		virtual void render();
		virtual void split(dm::Direction2D fromSide, float split);
		virtual void join(uint32_t toKill);
		virtual void rescale(vec2f scaling);

		void find_connected_panels(dm::Direction2D edge, std::vector<Panel*>& list);
		void recalc_bounds();

		virtual void mouse_over(float x, float y);
		virtual void mouse_drag(float x, float y, float dx, float dy);
		virtual void mouse_scroll(float amount);
		virtual bool mouse_click(float x, float y, uint32_t button, bool release);
		virtual void key_typed(uint32_t key, uint32_t state);
		virtual void handle_input(float mouseX, float mouseY);
		virtual void render_cameras(vku::WorldRenderPass pass);


		void set_root(Panel** root);
		AxisAlignedBB2Df& get_bounds();

		virtual Panel* clone();
	};

	struct PanelSceneView : Panel {
		scene::Camera* camera;
		float prevCamOrbitOffset;

		PanelSceneView(Gui* gui, Panel** top, AxisAlignedBB2Df bounds);
		~PanelSceneView();
		void rescale(vec2f scaling) override;
		void render() override;
		void mouse_drag(float x, float y, float dx, float dy) override;
		void mouse_scroll(float amount) override;
		bool mouse_click(float x, float y, uint32_t button, bool release) override;
		void handle_input(float mouseX, float mouseY) override;

		void render_cameras(vku::WorldRenderPass pass) override;

		Panel* clone() override;
	};

	class Gui {
	private:
		friend class Panel;
		friend class PanelSceneView;
		scene::Scene* scene;
		Panel* mainPanel;
		Panel* grabbedPanel;
		//Active panel is set when mouse hovers over it
		Panel* activePanel;
		AxisAlignedBB2Df* mouseCaptureBox;
		std::vector<Panel*> grabbedPanelConnected{};

	public:
		Gui(scene::Scene* scene);
		~Gui();
		bool on_click(float x, float y, uint32_t button, bool release);
		void on_key_typed(uint32_t button, uint32_t state);
		void mouse_drag(float x, float y, float dx, float dy);
		void mouse_hover(float x, float y);
		void mouse_scroll(float amount);
		void handle_input(float mouseX, float mouseY);
		void resize(float newWidth, float newHeight);
		void move_edge(dm::Direction2D edge, float amount);
		//Doesn't really render anything, just adds draw data to a buffer. add_render_data_to_buffer sounds weird, ok?
		void render();

		void render_cameras(vku::WorldRenderPass pass);
	};

	void draw_rect(float x, float y, float width, float height, float zLevel, float uMin, float vMin, float uMax, float vMax, float r, float g, float b, float a, uint16_t texIdx, uint16_t flags);
	void draw_rect(float x, float y, float width, float height, float zLevel, float uMin, float vMin, float uMax, float vMax, uint16_t texIdx, uint16_t flags);
	void draw_rect(float x, float y, float width, float height, float zLevel, float uMin, float vMin, float uMax, float vMax, uint16_t flags);

	struct NineSlice {
		float width;
		float height;
		float xSlice[2];
		float ySlice[2];
	};

	void draw_nine_sliced(float x, float y, float width, float height, float zLevel, bool tileRepeat, float r, float g, float b, float a, NineSlice& slice, uint16_t texIdx, uint16_t flags);

	void draw_gui(VkCommandBuffer cmdBuf);
	void init();
	void cleanup();
}