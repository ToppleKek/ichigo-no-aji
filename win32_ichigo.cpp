#define _CRT_SECURE_NO_WARNINGS
#include "common.hpp"
#include <cstdio>
#include "ichigo.hpp"

#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <gl/GL.h>
#include "thirdparty/imgui/imgui_impl_win32.h"

u32 Ichigo::window_width = 1920;
u32 Ichigo::window_height = 1080;

static HWND window_handle;
static HDC hdc;
static HGLRC wgl_context;
static bool in_sizing_loop = false;
static bool init_completed = false;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static wchar_t *to_wide_char(const char *str) {
    i32 buf_size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    assert(buf_size > 0);
    wchar_t *wide_buf = new wchar_t[buf_size];
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wide_buf, buf_size);
    return wide_buf;
}

static inline void free_wide_char_conversion(wchar_t *buf) {
    delete[] buf;
}

static inline void free_wide_char_conversion(char *buf) {
    delete[] buf;
}

static char *from_wide_char(const wchar_t *str) {
    i32 u8_buf_size = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
    assert(u8_buf_size > 0);
    char *u8_bytes = new char[u8_buf_size]();
    WideCharToMultiByte(CP_UTF8, 0, str, -1, u8_bytes, u8_buf_size, nullptr, nullptr);
    return u8_bytes;
}

std::FILE *Ichigo::platform_open_file(const std::string &path, const std::string &mode) {
    i32 buf_size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    i32 mode_buf_size = MultiByteToWideChar(CP_UTF8, 0, mode.c_str(), -1, nullptr, 0);
    assert(buf_size > 0 && mode_buf_size > 0);
    wchar_t *wide_buf = new wchar_t[buf_size];
    wchar_t *mode_wide_buf = new wchar_t[mode_buf_size];
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide_buf, buf_size);
    MultiByteToWideChar(CP_UTF8, 0, mode.c_str(), -1, mode_wide_buf, mode_buf_size);
    // std::wprintf(L"platform_open_file: wide_buf=%s mode_wide_buf=%s\n", wide_buf, mode_wide_buf);
    std::FILE *ret = _wfopen(wide_buf, mode_wide_buf);

    // TODO: Should this actually exist? If we want to check if a file exists then should we have a different platform function for it?
    // assert(ret != nullptr);
    delete[] wide_buf;
    delete[] mode_wide_buf;
    return ret;
}

bool Ichigo::platform_file_exists(const char *path) {
    wchar_t *wide_path = to_wide_char(path);
    DWORD attributes = GetFileAttributesW(wide_path);
    bool ret = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    free_wide_char_conversion(wide_path);
    return ret;
}

static bool is_filtered_file(const wchar_t *filename, const char **extension_filter, const u16 extension_filter_count) {
    // Find the last period in the file name
    u64 period_index = 0;
    for (u64 current_index = 0; filename[current_index] != '\0'; ++current_index) {
        if (filename[current_index] == '.')
            period_index = current_index;
    }

    wchar_t ext_wide[16] = {};
    for (u32 i = 0; i < extension_filter_count; ++i) {
        const char *ext = extension_filter[i];
        i32 buf_size = MultiByteToWideChar(CP_UTF8, 0, ext, -1, nullptr, 0);
        assert(buf_size <= 16);
        MultiByteToWideChar(CP_UTF8, 0, ext, -1, ext_wide, buf_size);

        if (std::wcscmp(&filename[period_index + 1], ext_wide) == 0)
            return true;
    }

    return false;
}

void visit_directory(const wchar_t *path, Util::IchigoVector<std::string> *files, const char **extension_filter, const u16 extension_filter_count) {
    HANDLE find_handle;
    WIN32_FIND_DATAW find_data;
    wchar_t path_with_filter[2048] = {};
    std::wcscat(path_with_filter, path);
    std::wcscat(path_with_filter, L"\\*");

    if ((find_handle = FindFirstFileW(path_with_filter, &find_data)) != INVALID_HANDLE_VALUE) {
        do {
            if (std::wcscmp(find_data.cFileName, L".") == 0 || std::wcscmp(find_data.cFileName, L"..") == 0)
                continue;

            if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                wchar_t sub_dir[2048] = {};
                _snwprintf(sub_dir, 2048, L"%s/%s", path, find_data.cFileName);
                visit_directory(sub_dir, files, extension_filter, extension_filter_count);
            } else {
                if (!is_filtered_file(find_data.cFileName, extension_filter, extension_filter_count))
                    continue;

                wchar_t full_path[2048] = {};
                _snwprintf(full_path, 2048, L"%s/%s", path, find_data.cFileName);
                i32 u8_buf_size = WideCharToMultiByte(CP_UTF8, 0, full_path, -1, nullptr, 0, nullptr, nullptr);
                char *u8_bytes = new char[u8_buf_size]();
                WideCharToMultiByte(CP_UTF8, 0, full_path, -1, u8_bytes, u8_buf_size, nullptr, nullptr);

                files->append(u8_bytes);
                delete[] u8_bytes;
            }
        } while (FindNextFileW(find_handle, &find_data) != 0);

        FindClose(find_handle);
    } else {
        auto error = GetLastError();
        std::printf("win32 plat: Failed to create find handle! error=%lu\n", error);
    }
}

Util::IchigoVector<std::string> Ichigo::platform_recurse_directory(const std::string &path, const char **extension_filter, const u16 extension_filter_count) {
    Util::IchigoVector<std::string> ret;

    i32 buf_size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    assert(buf_size > 0);
    wchar_t *wide_buf = new wchar_t[buf_size]();
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide_buf, buf_size);

    visit_directory(wide_buf, &ret, extension_filter, extension_filter_count);

    delete[] wide_buf;
    return ret;
}

static void platform_do_frame() {
    ImGui_ImplWin32_NewFrame();
    Ichigo::do_frame(ImGui_ImplWin32_GetDpiScaleForHwnd(window_handle));
    SwapBuffers(hdc);
}

static LRESULT window_proc(HWND window, u32 msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_ENTERSIZEMOVE: {
        printf("WM_ENTERSIZEMOVE\n");
        in_sizing_loop = true;
    } break;

    case WM_EXITSIZEMOVE: {
        printf("WM_EXITSIZEMOVE\n");
        in_sizing_loop = false;
    } break;

    case WM_SIZE: {
        return 0;
    } break;

    case WM_DESTROY: {
        std::printf("WM_DESTROY\n");
        PostQuitMessage(0);
        return 0;
    } break;

    case WM_CLOSE: {
        std::printf("WM_CLOSE\n");
        PostQuitMessage(0);
        return 0;
    } break;

    case WM_ACTIVATEAPP: {
        return 0;
    } break;

    case WM_TIMER: {
        if (in_sizing_loop)
            platform_do_frame();

        return 0;
    } break;

    case WM_PAINT: {
        // std::printf("WM_PAINT lparam=%lld wparam=%lld\n", lparam, wparam);
        PAINTSTRUCT paint;
        BeginPaint(window, &paint);

        if (init_completed) {
            u32 height = paint.rcPaint.bottom - paint.rcPaint.top;
            u32 width = paint.rcPaint.right - paint.rcPaint.left;

            if (height <= 0 || width <= 0)
                break;

            if (height != Ichigo::window_height || width != Ichigo::window_width) {
                Ichigo::window_width = width;
                Ichigo::window_height = height;
            }

            platform_do_frame();
        }
        EndPaint(window, &paint);
        return 0;
    } break;
    case WM_MOVE: {
        // std::printf("WM_MOVE\n");
    } break;
    }

    if (ImGui_ImplWin32_WndProcHandler(window, msg, wparam, lparam))
        return 1;

    return DefWindowProc(window, msg, wparam, lparam);
}

i32 main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetConsoleOutputCP(CP_UTF8);
    auto instance = GetModuleHandle(nullptr);

    // Create win32 window
    WNDCLASS window_class      = {};
    window_class.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc   = window_proc;
    window_class.hInstance     = instance;
    window_class.lpszClassName = L"ichigo";
    RegisterClass(&window_class);
    window_handle = CreateWindowEx(0, window_class.lpszClassName, L"Ichigo no Aji!", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, Ichigo::window_width, Ichigo::window_height, nullptr,
                                   nullptr, instance, nullptr);

    assert(window_handle);

    hdc = GetDC(window_handle);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    i32 pixel_format = ChoosePixelFormat(hdc, &pfd);
    assert(pixel_format != 0);
    assert(SetPixelFormat(hdc, pixel_format, &pfd));

    wgl_context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, wgl_context);

    Ichigo::init();

    // Platform init
    ImGui_ImplWin32_InitForOpenGL(window_handle);
    init_completed = true;

    for (;;) {
        MSG message;
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                goto exit;

            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        platform_do_frame();
    }

exit:
    Ichigo::deinit();
    return 0;
}
