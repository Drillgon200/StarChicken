#include "Windowing.h"
#include <iostream>

namespace windowing {
	bool windowShouldClose = false;
	uint32_t width;
	uint32_t height;
	int32_t mouseDX{ 0 };
	int32_t mouseDY{ 0 };
	int32_t oldMx{-1};
	int32_t oldMy{-1};
	uint32_t framebufferWidth{ 0 };
	uint32_t framebufferHeight{ 0 };
	uint32_t currentCursor{CURSOR_TYPE_POINTER};
	int32_t captureX;
	int32_t captureY;
	bool mouseCaptured = false;
	void* userPointer = nullptr;


#ifdef _WIN32
#define USAGE_PAGE_GENERIC 0x01
#define USAGE_PAGE_GAME 0x05
#define MOUSE_USAGE 0x02
#define KEYBOARD_USAGE 0x06

	const wchar_t* CLASS_NAME = L"Star Chicken Windowing";
	inline constexpr uint32_t REQUIRED_EXTENSION_COUNT = 2;
	const char* REQUIRED_EXTENSIONS[REQUIRED_EXTENSION_COUNT] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	HINSTANCE hInstance;
	HWND hwnd;
	HCURSOR cursorPointer;
	HCURSOR cursorSizeVertical;
	HCURSOR cursorSizeHorizontal;
	HCURSOR cursorSizeDiagForward;
	HCURSOR cursorSizeDiagBackward;
	HCURSOR cursorSizeAll;
	HCURSOR cursorBeam;
	HCURSOR cursorLoading;

	void (*windowResizeCallback)(Window*, uint32_t, uint32_t) { nullptr };
	void (*keyCallback)(Window*, uint32_t, uint32_t) { nullptr };
	void (*mouseCallback)(Window*, uint32_t, uint32_t) { nullptr };

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		switch (uMsg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_KEYDOWN:
			if (wParam < 0 || wParam >= MAX_KEY_CODE) {
				break;
			}
			//std::cout << "Down: " << lParam << std::endl;
			if (keyCallback) {
				keyCallback(&hwnd, wParam, KEY_STATE_DOWN);
			}
			return 0;
		case WM_KEYUP:
			if (wParam < 0 || wParam >= MAX_KEY_CODE) {
				break;
			}
			//std::cout << "Up: " << lParam << std::endl;
			if (keyCallback) {
				keyCallback(&hwnd, wParam, KEY_STATE_UP);
			}
			return 0;
		case WM_INPUT:
		{
			UINT size = sizeof(RAWINPUT);
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
			RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(alloca(size));
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));
			if (raw->header.dwType == RIM_TYPEMOUSE) {
				RAWMOUSE rawMouse = raw->data.mouse;
				//std::cout << rawMouse.usButtonData << " " << rawMouse.ulButtons << " " << rawMouse.ulRawButtons << " " << rawMouse.usButtonFlags << std::endl;
				if ((rawMouse.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE) {
					bool isVirtualDesktop = (rawMouse.usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;

					int width = GetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
					int height = GetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

					int absoluteX = int((rawMouse.lLastX / 65535.0f) * width);
					int absoluteY = int((rawMouse.lLastY / 65535.0f) * height);
					if (oldMx != -1) {
						mouseDX += (absoluteX - oldMx);
						mouseDY += (absoluteY - oldMy);
					}
					oldMx = absoluteX;
					oldMy = absoluteY;
				} else {
					mouseDX += rawMouse.lLastX;
					mouseDY += rawMouse.lLastY;
				}
				if (mouseCallback) {
					for (ULONG i = 0; i < 5; i++) {
						ULONG currentButton = (rawMouse.ulButtons >> (i * 2)) & 1;
						if (currentButton) {
							mouseCallback(&hwnd, MOUSE_0 + i, KEY_STATE_DOWN);
						}
						currentButton = (rawMouse.ulButtons >> (i * 2 + 1)) & 1;
						if (currentButton) {
							mouseCallback(&hwnd, MOUSE_0 + i, KEY_STATE_UP);
						}
					}
					if (rawMouse.usButtonFlags & RI_MOUSE_WHEEL) {
						mouseCallback(&hwnd, MOUSE_WHEEL, rawMouse.usButtonData);
					}
				}
				if (mouseCaptured) {
					SetCursorPos(captureX, captureY);
				}
			} else if (raw->header.dwType == RIM_TYPEKEYBOARD) {
				RAWKEYBOARD rawKeyboard = raw->data.keyboard;
				//std::cout << "RAW: " << rawKeyboard.VKey << " " << rawKeyboard.Flags;
			}
		}
			return 0;
		case WM_SIZE:
			width = LOWORD(lParam);
			height = HIWORD(lParam);
			{
				RECT rect{};
				if (GetWindowRect(hwnd, &rect)) {
					framebufferWidth = rect.right - rect.left;
					framebufferHeight = rect.bottom - rect.top;
				}
				if (windowResizeCallback) {
					windowResizeCallback(&hwnd, width, height);
				}
			}
			return 0;
		case WM_PAINT:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		case WM_SETCURSOR:
			set_cursor(currentCursor);
			return 0;
		case WM_CLOSE:
			windowShouldClose = true;
			return 0;
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
#endif

	void init() {
#ifdef _WIN32
		hInstance = GetModuleHandle(0);
		
		cursorPointer = LoadCursor(NULL, IDC_ARROW);
		cursorSizeVertical = LoadCursor(NULL, IDC_SIZENS);
		cursorSizeHorizontal = LoadCursor(NULL, IDC_SIZEWE);
		cursorSizeDiagForward = LoadCursor(NULL, IDC_SIZENESW);
		cursorSizeDiagBackward = LoadCursor(NULL, IDC_SIZENWSE);
		cursorSizeAll = LoadCursor(NULL, IDC_SIZEALL);
		cursorBeam = LoadCursor(NULL, IDC_IBEAM);
		cursorLoading = LoadCursor(NULL, IDC_WAIT);
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
		RAWINPUTDEVICE mouse{};
		RAWINPUTDEVICE keyboard{};
		mouse.hwndTarget = hwnd;
		mouse.usUsagePage = USAGE_PAGE_GENERIC;
		mouse.usUsage = MOUSE_USAGE;
		mouse.dwFlags = RIDEV_INPUTSINK;
		keyboard.hwndTarget = hwnd;
		keyboard.usUsagePage = USAGE_PAGE_GENERIC;
		keyboard.usUsage = KEYBOARD_USAGE;
		keyboard.dwFlags = RIDEV_INPUTSINK;
		RAWINPUTDEVICE devices[] = { mouse, keyboard };
		RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));

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

	void set_keyboard_callback(Window* window, void(*keyCallback)(Window* window, uint32_t key, uint32_t state)) {
		windowing::keyCallback = keyCallback;
	}

	void set_mouse_callback(Window* window, void(*mouseCallback)(Window* window, uint32_t button, uint32_t state)) {
		windowing::mouseCallback = mouseCallback;
	}

	void cleanup() {
#ifdef _WIN32
		DestroyWindow(hwnd);
#endif
	}

	void get_delta_mouse(int32_t* x, int32_t* y) {
		*x = mouseDX;
		*y = mouseDY;
		mouseDX = 0;
		mouseDY = 0;
	}

	void get_mouse(int32_t* x, int32_t* y) {
#ifdef _WIN32
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(hwnd, &point);
		*x = point.x;
		*y = point.y;
#endif
	}

	void set_mouse(int32_t x, int32_t y) {
#ifdef _WIN32
		POINT point;
		point.x = x;
		point.y = y;
		ClientToScreen(hwnd, &point);
		SetCursorPos(point.x, point.y);
#endif
	}

	void set_cursor(uint32_t cursor) {
		if (mouseCaptured) {
			return;
		}
		currentCursor = cursor;
#ifdef _WIN32
		switch (cursor) {
		case CURSOR_TYPE_NONE:
			SetCursor(NULL);
			return;
		case CURSOR_TYPE_POINTER:
			SetCursor(cursorPointer);
			return;
		case CURSOR_TYPE_SIZE_VERTICAL:
			SetCursor(cursorSizeVertical);
			return;
		case CURSOR_TYPE_SIZE_HORIZONTAL:
			SetCursor(cursorSizeHorizontal);
			return;
		case CURSOR_TYPE_SIZE_DIAG_FORWARD:
			SetCursor(cursorSizeDiagForward);
			return;
		case CURSOR_TYPE_SIZE_DIAG_BACKWARD:
			SetCursor(cursorSizeDiagBackward);
			return;
		case CURSOR_TYPE_SIZE_ALL:
			SetCursor(cursorSizeAll);
			return;
		case CURSOR_TYPE_BEAM:
			SetCursor(cursorBeam);
			return;
		case CURSOR_TYPE_LOADING:
			SetCursor(cursorLoading);
			return;
		}
#endif
	}

	void set_mouse_captured(bool capture) {
#ifdef _WIN32
		if (capture) {
			SetCapture(hwnd);
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			captureX = cursorPos.x;
			captureY = cursorPos.y;
			SetCursorPos(captureX, captureY);
			set_cursor(CURSOR_TYPE_NONE);
			mouseCaptured = true;
		} else {
			mouseCaptured = false;
			ReleaseCapture();
			set_cursor(CURSOR_TYPE_POINTER);
		}
#endif
	}
	bool mouse_captured() {
		return mouseCaptured;
	}
}