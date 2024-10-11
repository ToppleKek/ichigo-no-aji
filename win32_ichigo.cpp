#define _CRT_SECURE_NO_WARNINGS
#include "common.hpp"
#include <cstdio>
#include "ichigo.hpp"

#define UNICODE
#include <windows.h>
#include <dsound.h>

#include <gl/GL.h>
#include "thirdparty/imgui/imgui_impl_win32.h"

#define AUDIO_SAMPLES_BUFFER_SIZE AUDIO_CHANNEL_COUNT * sizeof(i16) * AUDIO_SAMPLE_RATE * 4
#define DSOUND_BUFFER_SIZE AUDIO_CHANNEL_COUNT * sizeof(i16) * AUDIO_SAMPLE_RATE * 8

u32 Ichigo::Internal::window_width = 1920;
u32 Ichigo::Internal::window_height = 1080;
OpenGL Ichigo::Internal::gl{};
Ichigo::KeyState Ichigo::Internal::keyboard_state[Ichigo::Keycode::IK_ENUM_COUNT] = {};
f32 Ichigo::Internal::dt = 0.0f;
f32 Ichigo::Internal::dpi_scale = 0.0f;

static LPDIRECTSOUND8 direct_sound;
static IDirectSoundBuffer8 *secondary_dsound_buffer = nullptr;
static u8 audio_samples[AUDIO_SAMPLES_BUFFER_SIZE]{};
static i64 last_write_cursor_position = -1;
static i64 performance_frequency;
static f64 last_frame_time;
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

static i64 win32_get_timestamp() {
    LARGE_INTEGER i;
    QueryPerformanceCounter(&i);
    return i.QuadPart;
}

static i64 win32_get_time_ms() {
    return win32_get_timestamp() * 1000 / performance_frequency;
}

std::FILE *Ichigo::Internal::platform_open_file(const std::string &path, const std::string &mode) {
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

bool Ichigo::Internal::platform_file_exists(const char *path) {
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

Util::IchigoVector<std::string> Ichigo::Internal::platform_recurse_directory(const std::string &path, const char **extension_filter, const u16 extension_filter_count) {
    Util::IchigoVector<std::string> ret;

    i32 buf_size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    assert(buf_size > 0);
    wchar_t *wide_buf = new wchar_t[buf_size]();
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide_buf, buf_size);

    visit_directory(wide_buf, &ret, extension_filter, extension_filter_count);

    delete[] wide_buf;
    return ret;
}

void Ichigo::Internal::platform_sleep(f64 t) {
    Sleep((u32) (t * 1000));
}

f64 Ichigo::Internal::platform_get_current_time() {
    return (f64) win32_get_timestamp() / (f64) performance_frequency;
}

static void init_dsound(HWND window) {
    assert(SUCCEEDED(DirectSoundCreate8(nullptr, &direct_sound, nullptr)) && SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_NORMAL)));
}

static void realloc_dsound_buffer(u32 samples_per_second, u32 buffer_size) {
    if (secondary_dsound_buffer)
        secondary_dsound_buffer->Release();

    WAVEFORMATEX wave_format = {};
    wave_format.wFormatTag = WAVE_FORMAT_PCM;
    wave_format.nChannels = 2;
    wave_format.nSamplesPerSec = samples_per_second;
    wave_format.wBitsPerSample = 16;
    wave_format.nBlockAlign = wave_format.nChannels * wave_format.wBitsPerSample / 8;
    wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
    wave_format.cbSize = 0;

    DSBUFFERDESC secondary_buffer_description = {};
    secondary_buffer_description.dwSize = sizeof(secondary_buffer_description);
    secondary_buffer_description.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_TRUEPLAYPOSITION;
    secondary_buffer_description.dwBufferBytes = buffer_size;
    secondary_buffer_description.lpwfxFormat = &wave_format;

    IDirectSoundBuffer *query_secondary_dsound_buffer;

    assert(SUCCEEDED(direct_sound->CreateSoundBuffer(&secondary_buffer_description, &query_secondary_dsound_buffer, nullptr)));
    assert(SUCCEEDED(query_secondary_dsound_buffer->QueryInterface(IID_IDirectSoundBuffer8, reinterpret_cast<void **>(&secondary_dsound_buffer))));

    query_secondary_dsound_buffer->Release();
}

static void win32_write_samples(u8 *buffer, u32 offset, u32 bytes_to_write) {
    u8 *region1;
    u8 *region2;
    u32 region1_size;
    u32 region2_size;
    assert(SUCCEEDED(secondary_dsound_buffer->Lock(offset, bytes_to_write, (void **) &region1, (LPDWORD) &region1_size, (void **) &region2, (LPDWORD) &region2_size, 0)));
    std::memcpy(region1, buffer, region1_size);
    std::memcpy(region2, buffer + region1_size, region2_size);
    secondary_dsound_buffer->Unlock(region1, region1_size, region2, region2_size);
}

static void platform_do_frame() {
    f64 frame_start_time = Ichigo::Internal::platform_get_current_time();

    ImGui_ImplWin32_NewFrame();

    Ichigo::Internal::dpi_scale = ImGui_ImplWin32_GetDpiScaleForHwnd(window_handle);
    Ichigo::Internal::dt        =  frame_start_time - last_frame_time;
    Ichigo::Internal::do_frame();

    u32 play_cursor = 0;
    u32 write_cursor = 0;
    assert(SUCCEEDED(secondary_dsound_buffer->GetCurrentPosition((LPDWORD) &play_cursor, (LPDWORD) &write_cursor)));

    if (last_write_cursor_position == -1) {
        Ichigo::Internal::fill_sample_buffer(audio_samples, AUDIO_SAMPLES_BUFFER_SIZE, 0);
        last_write_cursor_position = write_cursor;
    } else {
        u32 write_cursor_delta;
        if (last_write_cursor_position > write_cursor)
            write_cursor_delta = DSOUND_BUFFER_SIZE - last_write_cursor_position + write_cursor;
        else
            write_cursor_delta = write_cursor - last_write_cursor_position;

        if (write_cursor_delta == 0)
            goto skip;

        Ichigo::Internal::fill_sample_buffer(audio_samples, AUDIO_SAMPLES_BUFFER_SIZE, write_cursor_delta);
        last_write_cursor_position = write_cursor;
    }

    win32_write_samples(audio_samples, write_cursor, AUDIO_SAMPLES_BUFFER_SIZE);

skip:
    // Ichigo::Internal::fill_sample_buffer(audio_samples, sizeof(audio_samples));
    last_frame_time = frame_start_time;
    f64 frame_time = Ichigo::Internal::platform_get_current_time() - frame_start_time;
    f64 sleep_time = Ichigo::Internal::target_frame_time - frame_time;

    if (sleep_time > 0.0)
        Ichigo::Internal::platform_sleep(sleep_time);

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

    case WM_KEYDOWN:
    case WM_KEYUP: {
        u32 vk_code = (u32) wparam;
        bool was_down = ((lparam & (1 << 30)) != 0);
        bool is_down  = ((lparam & (1 << 31)) == 0);
        // ICHIGO_INFO("VK input: %u was_down: %u is_down %u", vk_code, was_down, is_down);

#define SET_KEY_STATE(IK_KEY) Ichigo::Internal::keyboard_state[IK_KEY].down = is_down; Ichigo::Internal::keyboard_state[IK_KEY].up = !is_down; Ichigo::Internal::keyboard_state[IK_KEY].down_this_frame = is_down && !was_down; Ichigo::Internal::keyboard_state[IK_KEY].up_this_frame = !is_down && was_down
        switch (vk_code) {
            case VK_LBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_1);       break;
            case VK_RBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_2);       break;
            case VK_MBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_3);       break;
            case VK_XBUTTON1: SET_KEY_STATE(Ichigo::IK_MOUSE_4);       break;
            case VK_XBUTTON2: SET_KEY_STATE(Ichigo::IK_MOUSE_5);       break;
            case VK_BACK:     SET_KEY_STATE(Ichigo::IK_BACKSPACE);     break;
            case VK_TAB:      SET_KEY_STATE(Ichigo::IK_TAB);           break;
            case VK_RETURN:   SET_KEY_STATE(Ichigo::IK_ENTER);         break;
            case VK_LSHIFT:   SET_KEY_STATE(Ichigo::IK_LEFT_SHIFT);    break;
            case VK_LCONTROL: SET_KEY_STATE(Ichigo::IK_LEFT_CONTROL);  break;
            case VK_RSHIFT:   SET_KEY_STATE(Ichigo::IK_RIGHT_SHIFT);   break;
            case VK_RCONTROL: SET_KEY_STATE(Ichigo::IK_RIGHT_CONTROL); break;
            case VK_MENU:     SET_KEY_STATE(Ichigo::IK_ALT);           break;
            case VK_ESCAPE:   SET_KEY_STATE(Ichigo::IK_ESCAPE);        break;
            case VK_SPACE:    SET_KEY_STATE(Ichigo::IK_SPACE);         break;
            case VK_PRIOR:    SET_KEY_STATE(Ichigo::IK_PAGE_UP);       break;
            case VK_NEXT:     SET_KEY_STATE(Ichigo::IK_PAGE_DOWN);     break;
            case VK_END:      SET_KEY_STATE(Ichigo::IK_END);           break;
            case VK_HOME:     SET_KEY_STATE(Ichigo::IK_HOME);          break;
            case VK_LEFT:     SET_KEY_STATE(Ichigo::IK_LEFT);          break;
            case VK_UP:       SET_KEY_STATE(Ichigo::IK_UP);            break;
            case VK_RIGHT:    SET_KEY_STATE(Ichigo::IK_RIGHT);         break;
            case VK_DOWN:     SET_KEY_STATE(Ichigo::IK_DOWN);          break;
            case VK_SNAPSHOT: SET_KEY_STATE(Ichigo::IK_PRINT_SCREEN);  break;
            case VK_INSERT:   SET_KEY_STATE(Ichigo::IK_INSERT);        break;
            case VK_DELETE:   SET_KEY_STATE(Ichigo::IK_DELETE);        break;
        }

        if ((vk_code >= '0' && vk_code <= '9') || (vk_code >= 'A' && vk_code <= 'Z'))
            SET_KEY_STATE(vk_code);

#undef SET_KEY_STATE
    } break;

    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(window, &paint);

        if (init_completed) {
            u32 height = paint.rcPaint.bottom - paint.rcPaint.top;
            u32 width = paint.rcPaint.right - paint.rcPaint.left;

            if (height <= 0 || width <= 0)
                break;

            if (height != Ichigo::Internal::window_height || width != Ichigo::Internal::window_width) {
                Ichigo::Internal::window_width = width;
                Ichigo::Internal::window_height = height;
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

    assert(timeBeginPeriod(1) == TIMERR_NOERROR);

    // Create win32 window
    WNDCLASS window_class      = {};
    window_class.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc   = window_proc;
    window_class.hInstance     = instance;
    window_class.lpszClassName = L"ichigo";
    RegisterClass(&window_class);
    window_handle = CreateWindowEx(0, window_class.lpszClassName, L"Ichigo no Aji!", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, Ichigo::Internal::window_width, Ichigo::Internal::window_height, nullptr,
                                   nullptr, instance, nullptr);

    assert(window_handle);

    init_dsound(window_handle);
    realloc_dsound_buffer(AUDIO_SAMPLE_RATE, DSOUND_BUFFER_SIZE);
    win32_write_samples(audio_samples, 0, AUDIO_SAMPLES_BUFFER_SIZE);
    secondary_dsound_buffer->Play(0 ,0, DSBPLAY_LOOPING);

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


    // i32 context_attribs[] = {
    //     WGL_CONTEXT_MAJOR_VERSION
    // };

    wgl_context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, wgl_context);

    using Type_wglCreateContextAttribsARB = HGLRC (HDC, HGLRC, const i32 *);

    Type_wglCreateContextAttribsARB *wglCreateContextAttribsARB = (Type_wglCreateContextAttribsARB *) wglGetProcAddress("wglCreateContextAttribsARB");
    assert(wglCreateContextAttribsARB);

    using Type_wglSwapIntervalEXT = bool (i32);
    Type_wglSwapIntervalEXT *wglSwapIntervalEXT = (Type_wglSwapIntervalEXT *) wglGetProcAddress("wglSwapIntervalEXT");
    assert(wglSwapIntervalEXT);

    wglDeleteContext(wgl_context);
    wgl_context = wglCreateContextAttribsARB(hdc, 0, nullptr);
    wglMakeCurrent(hdc, wgl_context);
    wglSwapIntervalEXT(0);

#define GET_ADDR_OF_OPENGL_FUNCTION(FUNC_NAME) Ichigo::Internal::gl.FUNC_NAME = (Type_##FUNC_NAME *) wglGetProcAddress(#FUNC_NAME); assert(Ichigo::Internal::gl.FUNC_NAME != nullptr)
    Ichigo::Internal::gl.glViewport       = glViewport;
    Ichigo::Internal::gl.glClearColor     = glClearColor;
    Ichigo::Internal::gl.glClear          = glClear;
    Ichigo::Internal::gl.glGetString      = glGetString;
    Ichigo::Internal::gl.glPolygonMode    = glPolygonMode;
    Ichigo::Internal::gl.glGetError       = glGetError;
    Ichigo::Internal::gl.glTexParameterf  = glTexParameterf;
    Ichigo::Internal::gl.glTexParameterfv = glTexParameterfv;
    Ichigo::Internal::gl.glTexParameteri  = glTexParameteri;
    Ichigo::Internal::gl.glTexParameteriv = glTexParameteriv;
    Ichigo::Internal::gl.glTexImage2D     = glTexImage2D;
    Ichigo::Internal::gl.glEnable         = glEnable;
    Ichigo::Internal::gl.glDisable        = glDisable;
    Ichigo::Internal::gl.glBlendFunc      = glBlendFunc;

    GET_ADDR_OF_OPENGL_FUNCTION(glGenBuffers);
    GET_ADDR_OF_OPENGL_FUNCTION(glGenVertexArrays);
    GET_ADDR_OF_OPENGL_FUNCTION(glGenTextures);
    GET_ADDR_OF_OPENGL_FUNCTION(glBindBuffer);
    GET_ADDR_OF_OPENGL_FUNCTION(glBindVertexArray);
    GET_ADDR_OF_OPENGL_FUNCTION(glBindTexture);
    GET_ADDR_OF_OPENGL_FUNCTION(glBufferData);
    GET_ADDR_OF_OPENGL_FUNCTION(glCreateShader);
    GET_ADDR_OF_OPENGL_FUNCTION(glShaderSource);
    GET_ADDR_OF_OPENGL_FUNCTION(glCompileShader);
    GET_ADDR_OF_OPENGL_FUNCTION(glGetShaderiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glGetProgramiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glGetShaderInfoLog);
    GET_ADDR_OF_OPENGL_FUNCTION(glGetProgramInfoLog);
    GET_ADDR_OF_OPENGL_FUNCTION(glCreateProgram);
    GET_ADDR_OF_OPENGL_FUNCTION(glAttachShader);
    GET_ADDR_OF_OPENGL_FUNCTION(glLinkProgram);
    GET_ADDR_OF_OPENGL_FUNCTION(glUseProgram);
    GET_ADDR_OF_OPENGL_FUNCTION(glDeleteShader);
    GET_ADDR_OF_OPENGL_FUNCTION(glVertexAttribPointer);
    GET_ADDR_OF_OPENGL_FUNCTION(glEnableVertexAttribArray);
    GET_ADDR_OF_OPENGL_FUNCTION(glDisableVertexAttribArray);
    GET_ADDR_OF_OPENGL_FUNCTION(glDrawArrays);
    GET_ADDR_OF_OPENGL_FUNCTION(glDrawElements);
    GET_ADDR_OF_OPENGL_FUNCTION(glGetUniformLocation);
    GET_ADDR_OF_OPENGL_FUNCTION(glTexParameterIiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glTexParameterIuiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glGenerateMipmap);
    GET_ADDR_OF_OPENGL_FUNCTION(glEnablei);
    GET_ADDR_OF_OPENGL_FUNCTION(glDisablei);
    GET_ADDR_OF_OPENGL_FUNCTION(glBlendFunci);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform1f);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform2f);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform3f);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform4f);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform1i);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform2i);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform3i);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform4i);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform1ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform2ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform3ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform4ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform1ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform2ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform3ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform4ui);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform1fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform2fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform3fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform4fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform1iv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform2iv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform3iv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform4iv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform1uiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform2uiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform3uiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniform4uiv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix2fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix3fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix4fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix2x3fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix3x2fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix2x4fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix4x2fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix3x4fv);
    GET_ADDR_OF_OPENGL_FUNCTION(glUniformMatrix4x3fv);

    Ichigo::Internal::init();

    // Platform init
    ImGui_ImplWin32_InitForOpenGL(window_handle);

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    performance_frequency = frequency.QuadPart;

    last_frame_time = Ichigo::Internal::platform_get_current_time();
    init_completed = true;

    for (;;) {
        for (u32 i = 0; i < Ichigo::IK_ENUM_COUNT; ++i) {
            Ichigo::Internal::keyboard_state[i].down_this_frame = false;
            Ichigo::Internal::keyboard_state[i].up_this_frame   = false;
        }

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
    Ichigo::Internal::deinit();
    return 0;
}
