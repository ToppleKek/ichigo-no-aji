#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <cstdio>
#include <unistd.h>
#include <ctime>

#include "common.hpp"
#include "ichigo.hpp"

#ifdef ICHIGO_DEBUG
#include "thirdparty/imgui/imgui_impl_sdl2.h"
#endif

u32 Ichigo::Internal::window_width = 1600;
u32 Ichigo::Internal::window_height = 900;
OpenGL Ichigo::Internal::gl{};
Ichigo::KeyState Ichigo::Internal::keyboard_state[Ichigo::Keycode::IK_ENUM_COUNT] = {};
f32 Ichigo::Internal::dt = 0.0f;
f32 Ichigo::Internal::dpi_scale = 0.0f;
Ichigo::Gamepad Ichigo::Internal::gamepad{};
Ichigo::Mouse Ichigo::Internal::mouse{};

static SDL_Window *window;
static SDL_GLContext gl_context;
static bool init_completed = false;

struct Ichigo::Internal::PlatformFile {
    std::FILE *file_fd;
};

static Ichigo::Internal::PlatformFile open_files[32] = {};

Ichigo::Internal::PlatformFile *Ichigo::Internal::platform_open_file_write(const Bana::String path) {
    MAKE_STACK_STRING(path_cstr, path.length + 1);
    string_concat(path_cstr, path);
    string_concat(path_cstr, '\0');

    std::FILE *fd = std::fopen(path_cstr.data, "wb");

    if (!fd) {
        ICHIGO_ERROR("Failed to open file for writing!");
        return nullptr;
    }

    for (u32 i = 0; i < ARRAY_LEN(open_files); ++i) {
        if (open_files[i].file_fd == nullptr) {
            open_files[i].file_fd = fd;
            return &open_files[i];
        }
    }

    ICHIGO_ERROR("Too many files open!");
    return nullptr;
}

void Ichigo::Internal::platform_write_entire_file_sync(const char *path, const u8 *data, usize data_size) {
    std::FILE *fd = std::fopen(path, "wb");
    if (!fd) {
        ICHIGO_ERROR("Failed to open file for writing!");
        return;
    }

    std::fwrite(data, 1, data_size, fd);
    std::fclose(fd);
}

void Ichigo::Internal::platform_append_file_sync(Ichigo::Internal::PlatformFile *file, const u8 *data, usize data_size) {
    std::fwrite(data, 1, data_size, file->file_fd);
}

void Ichigo::Internal::platform_close_file(Ichigo::Internal::PlatformFile *file) {
    std::fclose(file->file_fd);
    file->file_fd = nullptr;
}

Bana::Optional<Bana::FixedArray<u8>> Ichigo::Internal::platform_read_entire_file_sync(const Bana::String path, Bana::Allocator allocator) {
    MAKE_STACK_STRING(path_cstr, path.length + 1);
    string_concat(path_cstr, path);
    string_concat(path_cstr, '\0');

    std::FILE *fd = std::fopen(path_cstr.data, "rb");

    if (!fd) {
        ICHIGO_ERROR("Failed to open file for reading!");
        return {};
    }

    std::fseek(fd, 0, SEEK_END);
    usize file_size = std::ftell(fd);
    std::fseek(fd, 0, SEEK_SET);

    Bana::FixedArray<u8> ret = make_fixed_array<u8>(file_size, allocator);
    ret.size                 = file_size;

    std::fread(ret.data, 1, file_size, fd);
    std::fclose(fd);

    return ret;
}

bool Ichigo::Internal::platform_file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

void Ichigo::Internal::platform_sleep(f64 t) {
    usleep(static_cast<u64>(t * 1000 * 1000));
}

f64 Ichigo::Internal::platform_get_current_time() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec / (f64) 1e9);
}

void Ichigo::Internal::platform_pause_audio() {
    ICHIGO_INFO("platform_pause_audio: audio not implemented on linux!");
}

void Ichigo::Internal::platform_resume_audio() {
    ICHIGO_INFO("platform_resume_audio: audio not implemented on linux!");
}

i32 main() {
    assert(SDL_Init(SDL_INIT_VIDEO) >= 0);

    window = SDL_CreateWindow("MUSEKININ", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Ichigo::Internal::window_width, Ichigo::Internal::window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(0);

    #define GET_ADDR_OF_OPENGL_FUNCTION(FUNC_NAME) Ichigo::Internal::gl.FUNC_NAME = (Type_##FUNC_NAME *) SDL_GL_GetProcAddress(#FUNC_NAME); assert(Ichigo::Internal::gl.FUNC_NAME != nullptr)
    GET_ADDR_OF_OPENGL_FUNCTION(glViewport);
    GET_ADDR_OF_OPENGL_FUNCTION(glClearColor);
    GET_ADDR_OF_OPENGL_FUNCTION(glClear);
    GET_ADDR_OF_OPENGL_FUNCTION(glGetString);
    GET_ADDR_OF_OPENGL_FUNCTION(glPolygonMode);
    GET_ADDR_OF_OPENGL_FUNCTION(glGetError);
    GET_ADDR_OF_OPENGL_FUNCTION(glTexParameterf);
    GET_ADDR_OF_OPENGL_FUNCTION(glTexParameterfv);
    GET_ADDR_OF_OPENGL_FUNCTION(glTexParameteri);
    GET_ADDR_OF_OPENGL_FUNCTION(glTexParameteriv);
    GET_ADDR_OF_OPENGL_FUNCTION(glTexImage2D);
    GET_ADDR_OF_OPENGL_FUNCTION(glEnable);
    GET_ADDR_OF_OPENGL_FUNCTION(glEnablei);
    GET_ADDR_OF_OPENGL_FUNCTION(glDisable);
    GET_ADDR_OF_OPENGL_FUNCTION(glDisablei);
    GET_ADDR_OF_OPENGL_FUNCTION(glBlendFunc);
    GET_ADDR_OF_OPENGL_FUNCTION(glBlendFunci);

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
    GET_ADDR_OF_OPENGL_FUNCTION(glFinish);

    Ichigo::Internal::init();

#ifdef ICHIGO_DEBUG
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
#endif

    init_completed = true;

    static f64 last_tick_time = 0.0;
    for (;;) {
        f64 frame_begin = Ichigo::Internal::platform_get_current_time();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#ifdef ICHIGO_DEBUG
            ImGui_ImplSDL2_ProcessEvent(&event);
#endif
            if (event.type == SDL_QUIT) {
                std::printf("deinit() now\n");
                Ichigo::Internal::deinit();
                return 0;
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                Ichigo::Internal::window_width  = event.window.data1;
                Ichigo::Internal::window_height = event.window.data2;
            }
        }

#ifdef ICHIGO_DEBUG
        ImGui_ImplSDL2_NewFrame();
#endif
        // SDL_PumpEvents();
        u32 mouse_button_state = SDL_GetMouseState(&Ichigo::Internal::mouse.pos.x, &Ichigo::Internal::mouse.pos.y);

#define SET_MOUSE_BTN_STATE(MOUSE_BUTTON, SDL_BUTTON_CODE)                                                                                                             \
        do {                                                                                                                                                           \
            Ichigo::Internal::mouse.MOUSE_BUTTON.down_this_frame = (mouse_button_state  & SDL_BUTTON(SDL_BUTTON_CODE)) && !Ichigo::Internal::mouse.MOUSE_BUTTON.down;  \
            Ichigo::Internal::mouse.MOUSE_BUTTON.down            = mouse_button_state   & SDL_BUTTON(SDL_BUTTON_CODE);                                                 \
            Ichigo::Internal::mouse.MOUSE_BUTTON.up              = !(mouse_button_state & SDL_BUTTON(SDL_BUTTON_CODE));                                                \
            Ichigo::Internal::mouse.MOUSE_BUTTON.up_this_frame   = !(mouse_button_state & SDL_BUTTON(SDL_BUTTON_CODE)) && Ichigo::Internal::mouse.MOUSE_BUTTON.down;   \
        } while (0)

#ifdef ICHIGO_DEBUG
        if (!ImGui::GetIO().WantCaptureMouse) {
#else
        {
#endif
            SET_MOUSE_BTN_STATE(left_button, 1);
            SET_MOUSE_BTN_STATE(middle_button, 2);
            SET_MOUSE_BTN_STATE(right_button, 3);
        }
#undef SET_MOUSE_BTN_STATE

        i32 sdl_keycount;
        const u8 *keystate = SDL_GetKeyboardState(&sdl_keycount);


#define SET_KEY_STATE(IK_KEY) Ichigo::Internal::keyboard_state[IK_KEY].down_this_frame = Ichigo::Internal::keyboard_state[IK_KEY].up && keystate[i] ? 1 : 0; Ichigo::Internal::keyboard_state[IK_KEY].down = keystate[i] == 1; Ichigo::Internal::keyboard_state[IK_KEY].up = keystate[i] == 0

#ifdef ICHIGO_DEBUG
        if (!ImGui::GetIO().WantCaptureKeyboard) {
#else
        {
#endif
            for (i32 i = 0; i < sdl_keycount; ++i) {
                if (i >= SDL_SCANCODE_A && i <= SDL_SCANCODE_Z) {
                    SET_KEY_STATE(i + Ichigo::IK_A - SDL_SCANCODE_A);
                }

                switch (i) {
                    case SDL_SCANCODE_BACKSPACE:   SET_KEY_STATE(Ichigo::IK_BACKSPACE);     break;
                    case SDL_SCANCODE_TAB:         SET_KEY_STATE(Ichigo::IK_TAB);           break;
                    case SDL_SCANCODE_RETURN:      SET_KEY_STATE(Ichigo::IK_ENTER);         break;
                    case SDL_SCANCODE_LSHIFT:      SET_KEY_STATE(Ichigo::IK_LEFT_SHIFT);    break;
                    case SDL_SCANCODE_LCTRL:       SET_KEY_STATE(Ichigo::IK_LEFT_CONTROL);  break;
                    case SDL_SCANCODE_RSHIFT:      SET_KEY_STATE(Ichigo::IK_RIGHT_SHIFT);   break;
                    case SDL_SCANCODE_RCTRL:       SET_KEY_STATE(Ichigo::IK_RIGHT_CONTROL); break;
                    case SDL_SCANCODE_LALT:        SET_KEY_STATE(Ichigo::IK_LEFT_ALT);      break;
                    case SDL_SCANCODE_ESCAPE:      SET_KEY_STATE(Ichigo::IK_ESCAPE);        break;
                    case SDL_SCANCODE_SPACE:       SET_KEY_STATE(Ichigo::IK_SPACE);         break;
                    case SDL_SCANCODE_PAGEUP:      SET_KEY_STATE(Ichigo::IK_PAGE_UP);       break;
                    case SDL_SCANCODE_PAGEDOWN:    SET_KEY_STATE(Ichigo::IK_PAGE_DOWN);     break;
                    case SDL_SCANCODE_END:         SET_KEY_STATE(Ichigo::IK_END);           break;
                    case SDL_SCANCODE_HOME:        SET_KEY_STATE(Ichigo::IK_HOME);          break;
                    case SDL_SCANCODE_LEFT:        SET_KEY_STATE(Ichigo::IK_LEFT);          break;
                    case SDL_SCANCODE_UP:          SET_KEY_STATE(Ichigo::IK_UP);            break;
                    case SDL_SCANCODE_RIGHT:       SET_KEY_STATE(Ichigo::IK_RIGHT);         break;
                    case SDL_SCANCODE_DOWN:        SET_KEY_STATE(Ichigo::IK_DOWN);          break;
                    case SDL_SCANCODE_PRINTSCREEN: SET_KEY_STATE(Ichigo::IK_PRINT_SCREEN);  break;
                    case SDL_SCANCODE_INSERT:      SET_KEY_STATE(Ichigo::IK_INSERT);        break;
                    case SDL_SCANCODE_DELETE:      SET_KEY_STATE(Ichigo::IK_DELETE);        break;
                    case SDL_SCANCODE_0:           SET_KEY_STATE(Ichigo::IK_0);             break;
                    case SDL_SCANCODE_1:           SET_KEY_STATE(Ichigo::IK_1);             break;
                    case SDL_SCANCODE_2:           SET_KEY_STATE(Ichigo::IK_2);             break;
                    case SDL_SCANCODE_3:           SET_KEY_STATE(Ichigo::IK_3);             break;
                    case SDL_SCANCODE_4:           SET_KEY_STATE(Ichigo::IK_4);             break;
                    case SDL_SCANCODE_5:           SET_KEY_STATE(Ichigo::IK_5);             break;
                    case SDL_SCANCODE_6:           SET_KEY_STATE(Ichigo::IK_6);             break;
                    case SDL_SCANCODE_7:           SET_KEY_STATE(Ichigo::IK_7);             break;
                    case SDL_SCANCODE_8:           SET_KEY_STATE(Ichigo::IK_8);             break;
                    case SDL_SCANCODE_9:           SET_KEY_STATE(Ichigo::IK_9);             break;
                    case SDL_SCANCODE_F1:          SET_KEY_STATE(Ichigo::IK_F1);            break;
                    case SDL_SCANCODE_F2:          SET_KEY_STATE(Ichigo::IK_F2);            break;
                    case SDL_SCANCODE_F3:          SET_KEY_STATE(Ichigo::IK_F3);            break;
                    case SDL_SCANCODE_F4:          SET_KEY_STATE(Ichigo::IK_F4);            break;
                    case SDL_SCANCODE_F5:          SET_KEY_STATE(Ichigo::IK_F5);            break;
                    case SDL_SCANCODE_F6:          SET_KEY_STATE(Ichigo::IK_F6);            break;
                    case SDL_SCANCODE_F7:          SET_KEY_STATE(Ichigo::IK_F7);            break;
                    case SDL_SCANCODE_F8:          SET_KEY_STATE(Ichigo::IK_F8);            break;
                    case SDL_SCANCODE_F9:          SET_KEY_STATE(Ichigo::IK_F9);            break;
                    case SDL_SCANCODE_F10:         SET_KEY_STATE(Ichigo::IK_F10);           break;
                    case SDL_SCANCODE_F11:         SET_KEY_STATE(Ichigo::IK_F11);           break;
                    case SDL_SCANCODE_F12:         SET_KEY_STATE(Ichigo::IK_F12);           break;
                }
            }
        }

        f64 new_tick_time = Ichigo::Internal::platform_get_current_time();
        f64 delta = new_tick_time - last_tick_time;
        last_tick_time = new_tick_time;

        Ichigo::Internal::dt = delta;
        Ichigo::Internal::dpi_scale = 1.0f; // TODO: ?

        Ichigo::Internal::do_frame();

        f64 frame_time = Ichigo::Internal::platform_get_current_time() - frame_begin;
        f64 sleep_time = Ichigo::Internal::target_frame_time - frame_time;

        if (sleep_time > 0.0)
            Ichigo::Internal::platform_sleep(sleep_time);

        SDL_GL_SwapWindow(window);
    }

    return 0;
}
