#include "Windowing.h"
#include <iostream>

namespace windowing {
	bool windowShouldClose = false;
	uint32_t width;
	uint32_t height;
	uint32_t framebufferWidth{ 0 };
	uint32_t framebufferHeight{ 0 };
	void* userPointer = nullptr;

#ifdef _WIN32
	const wchar_t* CLASS_NAME = L"Star Chicken Windowing";
	const uint32_t REQUIRED_EXTENSION_COUNT = 2;
	const char* REQUIRED_EXTENSIONS[REQUIRED_EXTENSION_COUNT] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	HINSTANCE hInstance;
	HWND hwnd;

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_KEYDOWN:
			return 0;
		case WM_KEYUP:
			return 0;
		case WM_SIZE:
			width = LOWORD(lParam);
			height = HIWORD(lParam);
			{
				RECT rect{};
				if (GetWindowRect(hwnd, &rect)) {
					framebufferWidth = rect.right - rect.left;
					framebufferHeight = rect.top - rect.bottom;
				}
			}
			return 0;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);



			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

			EndPaint(hwnd, &ps);
		}
		return 0;
		case WM_CLOSE:
			windowShouldClose = true;
			return 0;
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
#endif

	void (*windowResizeCallback)(Window*, uint32_t, uint32_t);

	void init() {
#ifdef _WIN32
		hInstance = GetModuleHandle(0);
#endif
	}

	Window* create_window(uint32_t width, uint32_t height, const char* name, bool resizable) {
#ifdef _WIN32
		WNDCLASS wc{};
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = CLASS_NAME;

		RegisterClass(&wc);

		size_t nameSize = strlen(name) + 1;
		wchar_t* wName = reinterpret_cast<wchar_t*>(alloca(nameSize * sizeof(wchar_t)));
		size_t numConverted;
		mbstowcs_s(&numConverted, wName, nameSize, name, nameSize - 1);

		windowing::width = width;
		windowing::height = height;
		hwnd = CreateWindowEx(0, CLASS_NAME, wName, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);
		if (!hwnd) {
			return nullptr;
		}
		ShowWindow(hwnd, SW_SHOWDEFAULT);
		RECT rect{};
		if (GetWindowRect(hwnd, &rect)) {
			framebufferWidth = rect.right - rect.left;
			framebufferHeight = rect.bottom - rect.top;
		}
		return &hwnd;
#endif
	}

	VkResult create_window_surface(VkInstance instance, Window* window, VkSurfaceKHR* surface) {
#ifdef _WIN32
		VkWin32SurfaceCreateInfoKHR surfaceInfo{};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.hinstance = hInstance;
		surfaceInfo.hwnd = *window;
		surfaceInfo.flags = 0;
		return vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, surface);
#endif
	}

	void poll_events(Window* window) {
#ifdef _WIN32
		MSG msg{};
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif
	}

	bool window_should_close(Window* window) {
		return windowShouldClose;
	}

	void get_framebuffer_size(Window* window, uint32_t* width, uint32_t* height) {
		*width = framebufferWidth;
		*height = framebufferHeight;
	}

	bool query_vulkan_support() {
#ifdef _WIN32
		void* vk = LoadLibraryA("vulkan-1.dll");
		if (!vk) {
			return false;
		}
		return true;
#endif
	}

	const char** get_required_extensions(uint32_t* count) {
#ifdef _WIN32
		*count = REQUIRED_EXTENSION_COUNT;
		return REQUIRED_EXTENSIONS;
#endif
	}

	void set_user_ptr(Window* window, void* userPointer) {
		windowing::userPointer = userPointer;
	}
	void* get_user_ptr(Window* window) {
		return windowing::userPointer;
	}
	void set_window_resize_callback(Window* window, void (*windowResizeCallback)(Window*, uint32_t, uint32_t)) {
		windowing::windowResizeCallback = windowResizeCallback;
	}

	void cleanup() {
		DestroyWindow(hwnd);
	}
}