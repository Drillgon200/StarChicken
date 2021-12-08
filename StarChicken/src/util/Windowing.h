#pragma once

#include <stdint.h>
#include <malloc.h>
#include <vulkan/vulkan.h>
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

#ifdef _WIN32
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
	void set_window_resize_callback(Window* window, void (*windowResizeCallback)(Window*, uint32_t, uint32_t));

	void cleanup();
}