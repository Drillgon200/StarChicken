#pragma once

#include "Engine.h"
#include "util/Windowing.h"

namespace engine {
	class InputSubsystem {
	private:
		bool keyState[windowing::MAX_KEY_CODE]{};
		bool mouseState[windowing::MAX_MOUSE_CODE]{};
		vec2f mousePos{ 0.0F };
		vec2f lastMousePos{ 0.0F };
		vec2f deltaMouse{ 0.0F };
		vec2f deltaMouseLast{ 0.0F };
		float scrollAmount{ 0.0F };
	public:
		void on_key_pressed(windowing::Window* window, uint32_t keyCode, uint32_t state) {
			if (state == windowing::KEY_STATE_DOWN) {
				keyState[keyCode] = true;
				if (keyCode == windowing::KEY_ESC) {
					windowing::set_mouse_captured(!windowing::mouse_captured());
				}
			} else if (state == windowing::KEY_STATE_UP) {
				keyState[keyCode] = false;
			}
		}
		void on_mouse_used(windowing::Window* window, uint32_t button, uint32_t state) {
			if (button == windowing::MOUSE_WHEEL) {
				scrollAmount += static_cast<float>(static_cast<int16_t>(state));
				return;
			}
			if (state == windowing::MOUSE_BUTTON_STATE_DOWN) {
				mouseState[button] = true;
			} else if (state == windowing::MOUSE_BUTTON_STATE_UP) {
				mouseState[button] = false;
			}
		}
		void handle_input() {
			scrollAmount = 0.0F;
			windowing::poll_events(engine::window);

			lastMousePos = mousePos;
			mousePos = windowing::get_mouse();

			int32_t mx;
			int32_t my;
			windowing::get_delta_mouse(&mx, &my);
			deltaMouse = vec2f{ static_cast<float>(mx), static_cast<float>(my) };
			deltaMouseLast = mousePos - lastMousePos;
		}
		bool is_key_down(uint32_t key) {
			return keyState[key];
		}
		bool is_mouse_down(uint32_t mouse) {
			return mouseState[mouse];
		}
		vec2f get_mouse_pos() {
			return mousePos;
		}
		vec2f get_last_mouse_pos() {
			return lastMousePos;
		}
		vec2f get_delta_mouse() {
			return deltaMouse;
		}
		vec2f get_last_delta_mouse() {
			return deltaMouseLast;
		}
		float get_scroll() {
			return scrollAmount;
		}
		void set_mouse_pos(vec2f pos) {
			windowing::set_mouse(pos);
			mousePos = windowing::get_mouse();
		}
	};
}