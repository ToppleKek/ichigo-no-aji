#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <cstdio>

#include "common.hpp"
#include "ichigo.hpp"


#include "thirdparty/imgui/imgui_impl_sdl2.h"

u32 Ichigo::window_width = 1920;
u32 Ichigo::window_height = 1080;
OpenGL Ichigo::gl{};

static SDL_Window *window;
static SDL_GLContext gl_context;
static u64 last_tick_time = 0;
static Ichigo::KeyState keyboard_state[Ichigo::Keycode::IK_ENUM_COUNT] = {};
static u32 previous_height = 1920;
static u32 previous_width = 1080;
static bool in_sizing_loop = false;
static bool init_completed = false;

std::FILE *Ichigo::platform_open_file(const std::string &path, const std::string &mode) {
    return fopen(path.c_str(), mode.c_str());
}

// TODO: Implement
bool Ichigo::platform_file_exists(const char *path) {
    return true;
}

// TODO: Implement
Util::IchigoVector<std::string> Ichigo::platform_recurse_directory(const std::string &path, const char **extension_filter, const u16 extension_filter_count) {
    return {};
}

// static void platform_do_frame() {
//     ImGui_ImplSDL2_NewFrame();
//     Ichigo::do_frame(1.0, 0.0, keyboard_state);
//     // SwapBuffers(hdc);
// }

i32 main() {
    assert(SDL_Init(SDL_INIT_VIDEO) >= 0);

    window = SDL_CreateWindow("Ichigo no Aji!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1600, 900, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);

    #define GET_ADDR_OF_OPENGL_FUNCTION(FUNC_NAME) Ichigo::gl.FUNC_NAME = (Type_##FUNC_NAME *) SDL_GL_GetProcAddress(#FUNC_NAME); assert(Ichigo::gl.FUNC_NAME != nullptr)
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

    Ichigo::init();
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    init_completed = true;

    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                std::printf("deinit() now\n");
                Ichigo::deinit();
                return 0;
            }

            // if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                
            // }
        }

        ImGui_ImplSDL2_NewFrame();

        // SDL_PumpEvents();
        i32 sdl_keycount;
        const u8 *keystate = SDL_GetKeyboardState(&sdl_keycount);


#define SET_KEY_STATE(IK_KEY) keyboard_state[IK_KEY].down_this_frame = keyboard_state[IK_KEY].up && keystate[i] ? 1 : 0; keyboard_state[IK_KEY].down = keystate[i] == 1; keyboard_state[IK_KEY].up = keystate[i] == 0
        for (i32 i = 0; i < sdl_keycount; ++i) {
                switch (i) {
                    // case VK_LBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_1);       break;
                    // case VK_RBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_2);       break;
                    // case VK_MBUTTON:  SET_KEY_STATE(Ichigo::IK_MOUSE_3);       break;
                    // case VK_XBUTTON1: SET_KEY_STATE(Ichigo::IK_MOUSE_4);       break;
                    // case VK_XBUTTON2: SET_KEY_STATE(Ichigo::IK_MOUSE_5);       break;
                    case SDL_SCANCODE_BACKSPACE:     SET_KEY_STATE(Ichigo::IK_BACKSPACE);     break;
                    case SDL_SCANCODE_TAB:      SET_KEY_STATE(Ichigo::IK_TAB);           break;
                    case SDL_SCANCODE_RETURN:   SET_KEY_STATE(Ichigo::IK_ENTER);         break;
                    case SDL_SCANCODE_LSHIFT:   SET_KEY_STATE(Ichigo::IK_LEFT_SHIFT);    break;
                    case SDL_SCANCODE_LCTRL: SET_KEY_STATE(Ichigo::IK_LEFT_CONTROL);  break;
                    case SDL_SCANCODE_RSHIFT:   SET_KEY_STATE(Ichigo::IK_RIGHT_SHIFT);   break;
                    case SDL_SCANCODE_RCTRL: SET_KEY_STATE(Ichigo::IK_RIGHT_CONTROL); break;
                    case SDL_SCANCODE_LALT:     SET_KEY_STATE(Ichigo::IK_ALT);           break;
                    case SDL_SCANCODE_ESCAPE:   SET_KEY_STATE(Ichigo::IK_ESCAPE);        break;
                    case SDL_SCANCODE_SPACE:    SET_KEY_STATE(Ichigo::IK_SPACE);         break;
                    case SDL_SCANCODE_PAGEUP:    SET_KEY_STATE(Ichigo::IK_PAGE_UP);       break;
                    case SDL_SCANCODE_PAGEDOWN:     SET_KEY_STATE(Ichigo::IK_PAGE_DOWN);     break;
                    case SDL_SCANCODE_END:      SET_KEY_STATE(Ichigo::IK_END);           break;
                    case SDL_SCANCODE_HOME:     SET_KEY_STATE(Ichigo::IK_HOME);          break;
                    case SDL_SCANCODE_LEFT:     SET_KEY_STATE(Ichigo::IK_LEFT);          break;
                    case SDL_SCANCODE_UP:       SET_KEY_STATE(Ichigo::IK_UP);            break;
                    case SDL_SCANCODE_RIGHT:    SET_KEY_STATE(Ichigo::IK_RIGHT);         break;
                    case SDL_SCANCODE_DOWN:     SET_KEY_STATE(Ichigo::IK_DOWN);          break;
                    case SDL_SCANCODE_PRINTSCREEN: SET_KEY_STATE(Ichigo::IK_PRINT_SCREEN);  break;
                    case SDL_SCANCODE_INSERT:   SET_KEY_STATE(Ichigo::IK_INSERT);        break;
                    case SDL_SCANCODE_DELETE:   SET_KEY_STATE(Ichigo::IK_DELETE);        break;
                }
            // if ((vk_code >= '0' && vk_code <= '9') || (vk_code >= 'A' && vk_code <= 'Z'))
                // SET_KEY_STATE(vk_code);
        }


        u64 new_tick_time = SDL_GetTicks64();
        u64 delta = new_tick_time - last_tick_time;
        last_tick_time = new_tick_time;

        Ichigo::do_frame(1.0, delta / 1000.0f, keyboard_state);
        SDL_GL_SwapWindow(window);
    }

    return 0;
}
