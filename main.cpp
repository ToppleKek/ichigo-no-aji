#include <cassert>

#include "common.hpp"
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
    extern const __declspec(align(16)) unsigned char VNAME[];                             \
    extern const __declspec(align(16)) unsigned char *const VNAME##_end;                  \
    extern const unsigned int VNAME##_len;

extern "C" {
EMBED("noto.ttf", noto_font)
EMBED("shaders/opengl/frag.glsl", fragment_shader_source)
EMBED("shaders/opengl/vert.glsl", vertex_shader_source)
}

static f32 scale = 1;
static ImGuiStyle initial_style;
static ImFontConfig font_config;

static u32 vertex_buffer_id;
static u32 vertex_shader_id;
bool Ichigo::must_rebuild_swapchain = false;


struct Vec2 {
    f32 x;
    f32 y;
};

struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
};
struct Vertex {
    Vec2 pos;
    Vec3 color;
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

Vec3 vertices[] = {
    {-0.5f, -0.5f, 0.0f},
    {0.5f, -0.5f, 0.0f},
    {0.0f, 0.5f, 0.0f},
};

static void frame_render() {
    ImGui::Render();
    auto imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data->DisplaySize.x <= 0.0f || imgui_draw_data->DisplaySize.y <= 0.0f)
        return;

    Ichigo::ogl_ctx.glViewport(0, 0, Ichigo::window_width, Ichigo::window_height);
    Ichigo::ogl_ctx.glClearColor(0, 0, 0, 1.0f);
    Ichigo::ogl_ctx.glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Ichigo::do_frame(float dpi_scale) {
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

    ImGui::End();

    ImGui::EndFrame();

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

    ICHIGO_INFO("GL_VERSION=%s", Ichigo::ogl_ctx.glGetString(GL_VERSION));

    Ichigo::ogl_ctx.glGenBuffers(1, &vertex_buffer_id);
    Ichigo::ogl_ctx.glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_id);
    Ichigo::ogl_ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    vertex_shader_id = Ichigo::ogl_ctx.glCreateShader(GL_VERTEX_SHADER);

    const GLchar *p = (const GLchar *) vertex_shader_source;
    Ichigo::ogl_ctx.glShaderSource(vertex_shader_id, 1, &p, nullptr);
    Ichigo::ogl_ctx.glCompileShader(vertex_shader_id);
}

void Ichigo::deinit() {}
