#pragma once

#include <stdint.h>
#include <malloc.h>
#include <vulkan/vulkan.h>
#include "DrillMath.h"
#ifdef _WIN32
//Windows code from microsoft's documentation
#include <Windows.h>
#include <vulkan/vulkan_win32.h>
#else
#error Windowing - Operating system unsupported!
#endif

namespace windowing {

	extern bool windowShouldClose;
	extern uint32_t width;
	extern uint32_t height;
	extern uint32_t framebufferWidth;
	extern uint32_t framebufferHeight;
	extern void* userPointer;


	inline constexpr uint32_t CURSOR_TYPE_NONE = 0x0;
	inline constexpr uint32_t CURSOR_TYPE_POINTER = 0x1;
	inline constexpr uint32_t CURSOR_TYPE_SIZE_VERTICAL = 0x2;
	inline constexpr uint32_t CURSOR_TYPE_SIZE_HORIZONTAL = 0x3;
	inline constexpr uint32_t CURSOR_TYPE_SIZE_DIAG_FORWARD = 0x4;
	inline constexpr uint32_t CURSOR_TYPE_SIZE_DIAG_BACKWARD = 0x5;
	inline constexpr uint32_t CURSOR_TYPE_SIZE_ALL = 0x6;
	inline constexpr uint32_t CURSOR_TYPE_BEAM = 0x7;
	inline constexpr uint32_t CURSOR_TYPE_LOADING = 0x8;

	inline constexpr uint32_t KEY_STATE_DOWN = 0x0;
	inline constexpr uint32_t MOUSE_BUTTON_STATE_DOWN = KEY_STATE_DOWN;
	inline constexpr uint32_t KEY_STATE_UP = 0x1;
	inline constexpr uint32_t MOUSE_BUTTON_STATE_UP = KEY_STATE_UP;
	inline constexpr uint32_t KEY_STATE_HOLD = 0x2;

	inline constexpr uint32_t MOUSE_WHEEL = 0x00;
	inline constexpr uint32_t MOUSE_0 = 0x01;
	inline constexpr uint32_t MOUSE_1 = 0x02;
	inline constexpr uint32_t MOUSE_2 = 0x03;
	inline constexpr uint32_t MOUSE_3 = 0x04;
	inline constexpr uint32_t MOUSE_4 = 0x05;
	inline constexpr uint32_t MOUSE_LEFT = MOUSE_0;
	inline constexpr uint32_t MOUSE_RIGHT = MOUSE_1;
	inline constexpr uint32_t MOUSE_MIDDLE = MOUSE_2;

	inline constexpr uint32_t MAX_MOUSE_CODE = 0x10;

#ifdef _WIN32

	inline constexpr uint32_t KEY_0 = 0x30;
	inline constexpr uint32_t KEY_1 = 0x31;
	inline constexpr uint32_t KEY_2 = 0x32;
	inline constexpr uint32_t KEY_3 = 0x33;
	inline constexpr uint32_t KEY_4 = 0x34;
	inline constexpr uint32_t KEY_5 = 0x35;
	inline constexpr uint32_t KEY_6 = 0x36;
	inline constexpr uint32_t KEY_7 = 0x37;
	inline constexpr uint32_t KEY_8 = 0x38;
	inline constexpr uint32_t KEY_9 = 0x39;
	inline constexpr uint32_t KEY_A = 0x41;
	inline constexpr uint32_t KEY_B = 0x42;
	inline constexpr uint32_t KEY_C = 0x43;
	inline constexpr uint32_t KEY_D = 0x44;
	inline constexpr uint32_t KEY_E = 0x45;
	inline constexpr uint32_t KEY_F = 0x46;
	inline constexpr uint32_t KEY_G = 0x47;
	inline constexpr uint32_t KEY_H = 0x48;
	inline constexpr uint32_t KEY_I = 0x49;
	inline constexpr uint32_t KEY_J = 0x4A;
	inline constexpr uint32_t KEY_K = 0x4B;
	inline constexpr uint32_t KEY_L = 0x4C;
	inline constexpr uint32_t KEY_M = 0x4D;
	inline constexpr uint32_t KEY_N = 0x4E;
	inline constexpr uint32_t KEY_O = 0x4F;
	inline constexpr uint32_t KEY_P = 0x50;
	inline constexpr uint32_t KEY_Q = 0x51;
	inline constexpr uint32_t KEY_R = 0x52;
	inline constexpr uint32_t KEY_S = 0x53;
	inline constexpr uint32_t KEY_T = 0x54;
	inline constexpr uint32_t KEY_U = 0x55;
	inline constexpr uint32_t KEY_V = 0x56;
	inline constexpr uint32_t KEY_W = 0x57;
	inline constexpr uint32_t KEY_X = 0x58;
	inline constexpr uint32_t KEY_Y = 0x59;
	inline constexpr uint32_t KEY_Z = 0x5A;
	inline constexpr uint32_t KEY_NUMPAD0 = 0x60;
	inline constexpr uint32_t KEY_NUMPAD1 = 0x61;
	inline constexpr uint32_t KEY_NUMPAD2 = 0x62;
	inline constexpr uint32_t KEY_NUMPAD3 = 0x63;
	inline constexpr uint32_t KEY_NUMPAD4 = 0x64;
	inline constexpr uint32_t KEY_NUMPAD5 = 0x65;
	inline constexpr uint32_t KEY_NUMPAD6 = 0x66;
	inline constexpr uint32_t KEY_NUMPAD7 = 0x67;
	inline constexpr uint32_t KEY_NUMPAD8 = 0x68;
	inline constexpr uint32_t KEY_NUMPAD9 = 0x69;

	inline constexpr uint32_t KEY_TAB = 0x09;
	inline constexpr uint32_t KEY_RETURN = 0x0D;
	inline constexpr uint32_t KEY_SHIFT = 0x10;
	inline constexpr uint32_t KEY_CTRL = 0x11;
	inline constexpr uint32_t KEY_ALT = 0x12;
	inline constexpr uint32_t KEY_CAPS_LOCK = 0x14;
	inline constexpr uint32_t KEY_ESC = 0x1B;
	inline constexpr uint32_t KEY_SPACE = 0x20;
	inline constexpr uint32_t KEY_LEFT = 0x25;
	inline constexpr uint32_t KEY_UP = 0x26;
	inline constexpr uint32_t KEY_RIGHT = 0x27;
	inline constexpr uint32_t KEY_DOWN = 0x28;
	inline constexpr uint32_t KEY_BACKSPACE = 0x08;
	inline constexpr uint32_t KEY_DELETE = 0x2E;

	inline constexpr uint32_t KEY_F1 = 0x70;
	inline constexpr uint32_t KEY_F2 = 0x71;
	inline constexpr uint32_t KEY_F3 = 0x72;
	inline constexpr uint32_t KEY_F4 = 0x73;
	inline constexpr uint32_t KEY_F5 = 0x74;
	inline constexpr uint32_t KEY_F6 = 0x75;
	inline constexpr uint32_t KEY_F7 = 0x76;
	inline constexpr uint32_t KEY_F8 = 0x77;
	inline constexpr uint32_t KEY_F9 = 0x78;
	inline constexpr uint32_t KEY_F10 = 0x79;
	inline constexpr uint32_t KEY_F11 = 0x7A;
	inline constexpr uint32_t KEY_F12 = 0x7B;

	inline constexpr uint32_t MAX_KEY_CODE = 0xFF;

	typedef HWND Window;

	extern const wchar_t* CLASS_NAME;
	extern HINSTANCE hInstance;
	extern HWND hwnd;

#endif

	extern void (*windowResizeCallback)(Window*, uint32_t, uint32_t);

	void init();

	Window* create_window(uint32_t width, uint32_t height, const char* name, bool resizable);

	VkResult create_window_surface(VkInstance instance, Window* window, VkSurfaceKHR* surface);

	void poll_events(Window* window);

	bool window_should_close(Window* window);

	void get_framebuffer_size(Window* window, uint32_t* width, uint32_t* height);

	bool query_vulkan_support();
	const char** get_required_extensions(uint32_t* count);

	void set_user_ptr(Window* window, void* userPointer);
	void* get_user_ptr(Window* window);
	void set_window_resize_callback(Window* window, void (*windowResizeCallback)(Window* window, uint32_t x, uint32_t y));
	void set_keyboard_callback(Window* window, void(*keyCallback)(Window* window, uint32_t key, uint32_t state));
	void set_mouse_callback(Window* window, void(*mouseCallback)(Window* window, uint32_t button, uint32_t state));
	void get_delta_mouse(int32_t* x, int32_t* y);
	void get_mouse(int32_t* x, int32_t* y);
	inline vec2f get_mouse() {
		int32_t x;
		int32_t y;
		get_mouse(&x, &y);
		return vec2f{ static_cast<float>(x), static_cast<float>(y) };
	}
	void set_mouse(int32_t x, int32_t y);
	inline void set_mouse(vec2f mousePos) {
		set_mouse(static_cast<int32_t>(mousePos.components[0]), static_cast<int32_t>(mousePos.components[1]));
	}
	void set_cursor(uint32_t cursor);
	void set_mouse_captured(bool capture);
	bool mouse_captured();

	void cleanup();
}