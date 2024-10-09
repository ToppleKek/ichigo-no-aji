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


#include "thirdparty/imgui/imgui_impl_sdl2.h"

u32 Ichigo::Internal::window_width = 1600;
u32 Ichigo::Internal::window_height = 900;
OpenGL Ichigo::Internal::gl{};
Ichigo::KeyState Ichigo::Internal::keyboard_state[Ichigo::Keycode::IK_ENUM_COUNT] = {};
f32 Ichigo::Internal::dt = 0.0f;
f32 Ichigo::Internal::dpi_scale = 0.0f;

static SDL_Window *window;
static SDL_GLContext gl_context;
static bool init_completed = false;

std::FILE *Ichigo::Internal::platform_open_file(const std::string &path, const std::string &mode) {
    return fopen(path.c_str(), mode.c_str());
}

// TODO: Implement
bool Ichigo::Internal::platform_file_exists(const char *path) {
    return true;
}

// TODO: Implement
Util::IchigoVector<std::string> Ichigo::Internal::platform_recurse_directory(const std::string &path, const char **extension_filter, const u16 extension_filter_count) {
    return {};
}

void Ichigo::Internal::platform_sleep(f64 t) {
    usleep(static_cast<u64>(t * 1000 * 1000));
}

f64 Ichigo::Internal::platform_get_current_time() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec / (f64) 1e9);
}

// static void platform_do_frame() {
//     ImGui_ImplSDL2_NewFrame();
//     Ichigo::do_frame(1.0, 0.0, keyboard_state);
//     // SwapBuffers(hdc);
// }

i32 main() {
    assert(SDL_Init(SDL_INIT_VIDEO) >= 0);

    window = SDL_CreateWindow("Ichigo no Aji!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Ichigo::Internal::window_width, Ichigo::Internal::window_height, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

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


    Ichigo::Internal::init();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    init_completed = true;

    static f64 last_tick_time = 0.0;
    for (;;) {
        f64 frame_begin = Ichigo::Internal::platform_get_current_time();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
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

        ImGui_ImplSDL2_NewFrame();

        // SDL_PumpEvents();
        i32 sdl_keycount;
        const u8 *keystate = SDL_GetKeyboardState(&sdl_keycount);


#define SET_KEY_STATE(IK_KEY) Ichigo::Internal::keyboard_state[IK_KEY].down_this_frame = Ichigo::Internal::keyboard_state[IK_KEY].up && keystate[i] ? 1 : 0; Ichigo::Internal::keyboard_state[IK_KEY].down = keystate[i] == 1; Ichigo::Internal::keyboard_state[IK_KEY].up = keystate[i] == 0
        for (i32 i = 0; i < sdl_keycount; ++i) {
                switch (i) {
                    // case VK_LBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_1);       break;
                    // case VK_RBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_2);       break;
                    // case VK_MBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_3);       break;
                    // case VK_XBUTTON1: SET_KEY_STATE(Ichigo::IK_MOUSE_4);       break;
                    // case VK_XBUTTON2: SET_KEY_STATE(Ichigo::IK_MOUSE_5);       break;
                    case SDL_SCANCODE_BACKSPACE:   SET_KEY_STATE(Ichigo::IK_BACKSPACE);     break;
                    case SDL_SCANCODE_TAB:         SET_KEY_STATE(Ichigo::IK_TAB);           break;
                    case SDL_SCANCODE_RETURN:      SET_KEY_STATE(Ichigo::IK_ENTER);         break;
                    case SDL_SCANCODE_LSHIFT:      SET_KEY_STATE(Ichigo::IK_LEFT_SHIFT);    break;
                    case SDL_SCANCODE_LCTRL:       SET_KEY_STATE(Ichigo::IK_LEFT_CONTROL);  break;
                    case SDL_SCANCODE_RSHIFT:      SET_KEY_STATE(Ichigo::IK_RIGHT_SHIFT);   break;
                    case SDL_SCANCODE_RCTRL:       SET_KEY_STATE(Ichigo::IK_RIGHT_CONTROL); break;
                    case SDL_SCANCODE_LALT:        SET_KEY_STATE(Ichigo::IK_ALT);           break;
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
                }
            // if ((vk_code >= '0' && vk_code <= '9') || (vk_code >= 'A' && vk_code <= 'Z'))
                // SET_KEY_STATE(vk_code);
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
