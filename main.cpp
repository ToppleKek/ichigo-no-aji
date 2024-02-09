#include <cassert>

#include "common.hpp"
#include "math.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ichigo.hpp"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_impl_opengl3.h"

#define EMBED(FNAME, VNAME)                                                               \
    __asm__(                                                                              \
        ".section .rodata    \n"                                                          \
        ".global " #VNAME "    \n.align 16\n" #VNAME ":    \n.incbin \"" FNAME            \
        "\"       \n"                                                                     \
        ".global " #VNAME "_end\n.align 1 \n" #VNAME                                      \
        "_end:\n.byte 1                   \n"                                             \
        ".global " #VNAME "_len\n.align 16\n" #VNAME "_len:\n.int " #VNAME "_end-" #VNAME \
        "\n"                                                                              \
        ".align 16           \n.text    \n");                                             \
    alignas(16) extern const unsigned char VNAME[];                                       \
    alignas(16) extern const unsigned char *const VNAME##_end;                            \
    extern const unsigned int VNAME##_len;

extern "C" {
EMBED("noto.ttf", noto_font)
EMBED("shaders/opengl/frag.glsl", fragment_shader_source)
EMBED("shaders/opengl/vert.glsl", vertex_shader_source)
}

static f32 scale = 1;
static ImGuiStyle initial_style;
static ImFontConfig font_config;

static u32 vertex_array_id;
static u32 vertex_buffer_id;
static u32 vertex_shader_id;
static u32 fragment_shader_id;
static u32 shader_program_id;
static u32 element_buffer_id;

static char string_buffer[1024];

bool Ichigo::must_rebuild_swapchain = false;

struct Vertex {
    Vec2<f32> pos;
    Vec3<f32> color;
};

// static Vertex vertices[] = {
//     {{0.0f, -0.5f}, {0.2f, 0.0f, 1.0f}},
//     {{0.5f, 0.5f}, {0.2f, 0.0f, 1.0f}},
//     {{0.5f, -0.5f}, {0.2f, 0.0f, 1.0f}},

//     {{0.0f, -0.5f}, {0.2f, 0.0f, 1.0f}},
//     {{0.5f, 0.5f}, {0.2f, 0.0f, 1.0f}},
//     {{0.0f, 0.5f}, {0.2f, 0.0f, 1.0f}},
//     // {{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
//     // {{0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
//     // {{0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}},
// };

Vec3<f32> vertices[] = {
    {300.f, 100.f, 0.0f}, // top left
    {600.f, 100.f, 0.0f},
    {300.f, 600.f, 0.0f}, // bottom left
    {600.f, 600.f, 0.0f},
};
// Vec3<f32> vertices[] = {
//     {-0.5f, 0.5f, 0.0f}, // top
//     {0.5f, 0.5f, 0.0f},
//     {-0.5f, -0.5f, 0.0f},
//     {0.5f, -0.5f, 0.0f},
// };

u32 indicies[] = {
    0, 2, 3,
    0, 1, 3,
};

static void frame_render() {
    ImGui::Render();
    auto imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data->DisplaySize.x <= 0.0f || imgui_draw_data->DisplaySize.y <= 0.0f)
        return;

    auto proj = glm::ortho(0.0f, 1280.0f, 0.0f, 720.0f, -1.0f, 1.0f);
    i32 proj_uniform = Ichigo::gl.glGetUniformLocation(shader_program_id, "proj");
    Ichigo::gl.glUniformMatrix4fv(proj_uniform, 1, GL_FALSE, glm::value_ptr(proj));

    Ichigo::gl.glViewport(0, 0, Ichigo::window_width, Ichigo::window_height);
    Ichigo::gl.glClearColor(0, 0, 0, 1.0f);
    Ichigo::gl.glClear(GL_COLOR_BUFFER_BIT);

    Ichigo::gl.glUseProgram(shader_program_id);
    Ichigo::gl.glBindVertexArray(vertex_array_id);
    Ichigo::gl.glDrawElements(GL_TRIANGLES, ARRAY_LEN(indicies), GL_UNSIGNED_INT, 0);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Ichigo::do_frame(float dpi_scale) {
    static bool do_wireframe = 0;

    if (dpi_scale != scale) {
        ICHIGO_INFO("scaling to scale=%f", dpi_scale);
        auto io = ImGui::GetIO();
        {
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            io.Fonts->Clear();
            io.Fonts->AddFontFromMemoryTTF((void *) noto_font, noto_font_len, static_cast<i32>(18 * dpi_scale), &font_config, io.Fonts->GetGlyphRangesJapanese());
            io.Fonts->Build();
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }
        // Scale all Dear ImGui sizes based on the inital style
        std::memcpy(&ImGui::GetStyle(), &initial_style, sizeof(initial_style));
        ImGui::GetStyle().ScaleAllSizes(dpi_scale);
        scale = dpi_scale;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("main_window", nullptr);

    ImGui::Text("がんばりまー");
    ImGui::Text("FPS=%f", ImGui::GetIO().Framerate);

    ImGui::Checkbox("Wireframe", &do_wireframe);

    ImGui::End();

    ImGui::EndFrame();

    Ichigo::gl.glPolygonMode(GL_FRONT_AND_BACK, do_wireframe ? GL_LINE : GL_FILL);

    if (Ichigo::window_height != 0 && Ichigo::window_width != 0)
        frame_render();
}

// Initialization for the UI module
void Ichigo::init() {
    font_config.FontDataOwnedByAtlas = false;
    font_config.OversampleH = 2;
    font_config.OversampleV = 2;
    font_config.RasterizerMultiply = 1.5f;

    // Init imgui
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplOpenGL3_Init();
        initial_style = ImGui::GetStyle();
    }

    // Fonts
    {
        auto io = ImGui::GetIO();
        io.Fonts->AddFontFromMemoryTTF((void *) noto_font, noto_font_len, 18, &font_config, io.Fonts->GetGlyphRangesJapanese());
    }

    ICHIGO_INFO("GL_VERSION=%s", Ichigo::gl.glGetString(GL_VERSION));

    vertex_shader_id = Ichigo::gl.glCreateShader(GL_VERTEX_SHADER);

    const GLchar *pv = (const GLchar *) vertex_shader_source;
    Ichigo::gl.glShaderSource(vertex_shader_id, 1, &pv, (const i32 *) &vertex_shader_source_len);
    Ichigo::gl.glCompileShader(vertex_shader_id);

    i32 success;
    Ichigo::gl.glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &success);

    if (!success) {
        Ichigo::gl.glGetShaderInfoLog(vertex_shader_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Vertex shader compilation failed:\n%s", string_buffer);
        std::exit(1);
    }

    fragment_shader_id = Ichigo::gl.glCreateShader(GL_FRAGMENT_SHADER);

    const GLchar *pf = (const GLchar *) fragment_shader_source;
    Ichigo::gl.glShaderSource(fragment_shader_id, 1, &pf, (const i32 *) &fragment_shader_source_len);
    Ichigo::gl.glCompileShader(fragment_shader_id);

    Ichigo::gl.glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &success);

    if (!success) {
        Ichigo::gl.glGetShaderInfoLog(fragment_shader_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Fragment shader compilation failed:\n%s", string_buffer);
        std::exit(1);
    }

    shader_program_id = Ichigo::gl.glCreateProgram();
    Ichigo::gl.glAttachShader(shader_program_id, vertex_shader_id);
    Ichigo::gl.glAttachShader(shader_program_id, fragment_shader_id);
    Ichigo::gl.glLinkProgram(shader_program_id);

    Ichigo::gl.glGetProgramiv(shader_program_id, GL_LINK_STATUS, &success);

    if (!success) {
        Ichigo::gl.glGetProgramInfoLog(shader_program_id, sizeof(string_buffer), nullptr, string_buffer);
        ICHIGO_ERROR("Shader program link failed:\n%s", string_buffer);
        std::exit(1);
    }

    Ichigo::gl.glDeleteShader(vertex_shader_id);
    Ichigo::gl.glDeleteShader(fragment_shader_id);

    Ichigo::gl.glGenVertexArrays(1, &vertex_array_id);
    Ichigo::gl.glGenBuffers(1, &vertex_buffer_id);

    Ichigo::gl.glBindVertexArray(vertex_array_id);

    Ichigo::gl.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    Ichigo::gl.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    Ichigo::gl.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), 0);
    Ichigo::gl.glEnableVertexAttribArray(0);

    Ichigo::gl.glGenBuffers(1, &element_buffer_id);

    Ichigo::gl.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer_id);
    Ichigo::gl.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicies), indicies, GL_STATIC_DRAW);
}

void Ichigo::deinit() {}
