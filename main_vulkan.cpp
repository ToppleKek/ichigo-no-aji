#include <cassert>
#include "common.hpp"
#include "ichigo.hpp"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_impl_vulkan.h"

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
EMBED("build/frag.spv", fragment_shader)
EMBED("build/vert.spv", vertex_shader)
}

static f32 scale = 1;
static ImGuiStyle initial_style;
static ImFontConfig font_config;

static u8 current_frame = 0;
IchigoVulkan::Context Ichigo::vk_context{};
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

static const VkVertexInputBindingDescription VERTEX_BINDING_DESCRIPTION = {
    0,
    sizeof(Vertex),
    VK_VERTEX_INPUT_RATE_VERTEX
};

static const VkVertexInputAttributeDescription VERTEX_ATTRIBUTE_DESCRIPTIONS[] = {
    {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, pos)},
    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)}
};

static Vertex vertices[] = {
    {{0.0f, -0.5f}, {0.2f, 0.0f, 1.0f}},
    {{0.5f, 0.5f}, {0.2f, 0.0f, 1.0f}},
    {{0.5f, -0.5f}, {0.2f, 0.0f, 1.0f}},

    {{0.0f, -0.5f}, {0.2f, 0.0f, 1.0f}},
    {{0.5f, 0.5f}, {0.2f, 0.0f, 1.0f}},
    {{0.0f, 0.5f}, {0.2f, 0.0f, 1.0f}},
    // {{0.0f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    // {{0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    // {{0.0f, 0.5f}, {1.0f, 1.0f, 1.0f}},
};

/*
    Present one frame. Begin the vulkan render pass, fill command buffers with Dear ImGui draw data,
    and submit the queue for presentation.
*/
static void frame_render() {
    ImGui::Render();
    auto imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data->DisplaySize.x <= 0.0f || imgui_draw_data->DisplaySize.y <= 0.0f)
        return;

    vkWaitForFences(Ichigo::vk_context.logical_device, 1, &Ichigo::vk_context.fences[current_frame], VK_TRUE, UINT64_MAX);

    u32 image_index;
    auto err = vkAcquireNextImageKHR(Ichigo::vk_context.logical_device, Ichigo::vk_context.swapchain, UINT64_MAX, Ichigo::vk_context.image_acquired_semaphores[current_frame], VK_NULL_HANDLE, &image_index);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        Ichigo::must_rebuild_swapchain = true;
        return;
    }

    VK_ASSERT_OK(err);

    vkResetFences(Ichigo::vk_context.logical_device, 1, &Ichigo::vk_context.fences[current_frame]);
    vkResetCommandBuffer(Ichigo::vk_context.command_buffers[current_frame], 0);

    VkCommandBufferBeginInfo render_begin_info{};
    render_begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    render_begin_info.flags            = 0;
    render_begin_info.pInheritanceInfo = nullptr;

    VK_ASSERT_OK(vkBeginCommandBuffer(Ichigo::vk_context.command_buffers[current_frame], &render_begin_info));

    // ** DRAW BEGIN **
    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass        = Ichigo::vk_context.render_pass;
    render_pass_info.framebuffer       = Ichigo::vk_context.frame_buffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = Ichigo::vk_context.extent;

    VkClearValue clear               = {{{0.0f, 0.0f, 0.0f}}};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues    = &clear;

    vkCmdBeginRenderPass(Ichigo::vk_context.command_buffers[current_frame], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(Ichigo::vk_context.command_buffers[current_frame], VK_PIPELINE_BIND_POINT_GRAPHICS, Ichigo::vk_context.graphics_pipeline);

    VkDeviceSize vertex_buffer_offsets = 0;
    vkCmdBindVertexBuffers(Ichigo::vk_context.command_buffers[current_frame], 0, 1, &Ichigo::vk_context.vertex_buffer, &vertex_buffer_offsets);

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = Ichigo::vk_context.extent.width;
    viewport.height   = Ichigo::vk_context.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(Ichigo::vk_context.command_buffers[current_frame], 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = Ichigo::vk_context.extent;
    vkCmdSetScissor(Ichigo::vk_context.command_buffers[current_frame], 0, 1, &scissor);

    vkCmdDraw(Ichigo::vk_context.command_buffers[current_frame], ARRAY_LEN(vertices), 1, 0, 0);
    ImGui_ImplVulkan_RenderDrawData(imgui_draw_data, Ichigo::vk_context.command_buffers[current_frame]);

    vkCmdEndRenderPass(Ichigo::vk_context.command_buffers[current_frame]);
    // ** DRAW END **

    VK_ASSERT_OK(vkEndCommandBuffer(Ichigo::vk_context.command_buffers[current_frame]));

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{};
    submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount   = 1;
    submit_info.pWaitSemaphores      = &Ichigo::vk_context.image_acquired_semaphores[current_frame];
    submit_info.pWaitDstStageMask    = &wait_stages;
    submit_info.commandBufferCount   = 1;
    submit_info.pCommandBuffers      = &Ichigo::vk_context.command_buffers[current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores    = &Ichigo::vk_context.render_complete_semaphores[current_frame];

    VK_ASSERT_OK(vkQueueSubmit(Ichigo::vk_context.queue, 1, &submit_info, Ichigo::vk_context.fences[current_frame]));

    VkPresentInfoKHR present_info{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &Ichigo::vk_context.render_complete_semaphores[current_frame];
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &Ichigo::vk_context.swapchain;
    present_info.pImageIndices      = &image_index;

    VK_ASSERT_OK(vkQueuePresentKHR(Ichigo::vk_context.queue, &present_info));
    current_frame = (current_frame + 1) % ICHIGO_MAX_FRAMES_IN_FLIGHT;
}

/*
    Process one frame. Get input, draw UI, etc.
    Parameter 'dpi_scale': The DPI scale of the application on this frame
*/
void Ichigo::do_frame(float dpi_scale) {
    // If the swapchain becomes out of date or suboptimal (window resizes for instance) we must rebuild the swapchain
    if (Ichigo::must_rebuild_swapchain) {
        ICHIGO_INFO("Rebuilding swapchain");
        u64 start = __rdtsc();
        ImGui_ImplVulkan_SetMinImageCount(2);
        Ichigo::vk_context.rebuild_swapchain(Ichigo::window_width, Ichigo::window_height);
        ICHIGO_INFO("Took %llu", __rdtsc() - start);
        Ichigo::must_rebuild_swapchain = false;
    }

    // If the current scale is different from the scale this frame, we must scale the UI
    if (dpi_scale != scale) {
        ICHIGO_INFO("scaling to scale=%f", dpi_scale);
        auto io = ImGui::GetIO();
        // Scale font by reuploading a scaled version to the GPU
        {
            io.Fonts->Clear();
            io.Fonts->AddFontFromMemoryTTF((void *) noto_font, noto_font_len, static_cast<i32>(18 * dpi_scale), &font_config, io.Fonts->GetGlyphRangesJapanese());
            io.Fonts->Build();

            vkQueueWaitIdle(Ichigo::vk_context.queue);
            ImGui_ImplVulkan_DestroyFontsTexture();

            // Upload fonts to GPU
            VkCommandPool command_pool = Ichigo::vk_context.command_pool;
            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool        = command_pool;
            allocate_info.commandBufferCount = 1;
            allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            VkCommandBuffer command_buffer = VK_NULL_HANDLE;
            assert(vkAllocateCommandBuffers(Ichigo::vk_context.logical_device, &allocate_info, &command_buffer) == VK_SUCCESS);

            auto err = vkResetCommandPool(Ichigo::vk_context.logical_device, command_pool, 0);
            VK_ASSERT_OK(err);
            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(command_buffer, &begin_info);
            VK_ASSERT_OK(err);

            ImGui_ImplVulkan_CreateFontsTexture();

            VkSubmitInfo end_info{};
            end_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            end_info.commandBufferCount = 1;
            end_info.pCommandBuffers    = &command_buffer;
            err = vkEndCommandBuffer(command_buffer);
            VK_ASSERT_OK(err);
            err = vkQueueSubmit(Ichigo::vk_context.queue, 1, &end_info, VK_NULL_HANDLE);
            VK_ASSERT_OK(err);

            err = vkDeviceWaitIdle(Ichigo::vk_context.logical_device);
            VK_ASSERT_OK(err);
            ImGui_ImplVulkan_DestroyFontsTexture();

            vkFreeCommandBuffers(Ichigo::vk_context.logical_device, command_pool, 1, &command_buffer);
        }
        // Scale all Dear ImGui sizes based on the inital style
        std::memcpy(&ImGui::GetStyle(), &initial_style, sizeof(initial_style));
        ImGui::GetStyle().ScaleAllSizes(dpi_scale);
        scale = dpi_scale;
    }

    ImGui_ImplVulkan_NewFrame();
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

    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(Ichigo::vk_context.selected_gpu, Ichigo::vk_context.queue_family_index, Ichigo::vk_context.surface, &res);
    assert(res == VK_TRUE);

    const VkFormat request_surface_image_format = VK_FORMAT_B8G8R8A8_UNORM;
    const VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

    // ** Select a surface format **
    uint32_t surface_format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Ichigo::vk_context.selected_gpu, Ichigo::vk_context.surface, &surface_format_count, nullptr);
    VkSurfaceFormatKHR *surface_formats = reinterpret_cast<VkSurfaceFormatKHR *>(platform_alloca(surface_format_count * sizeof(VkSurfaceFormatKHR)));
    vkGetPhysicalDeviceSurfaceFormatsKHR(Ichigo::vk_context.selected_gpu, Ichigo::vk_context.surface, &surface_format_count, surface_formats);

    for (u32 i = 0; i < surface_format_count; ++i) {
        if (surface_formats[i].format == request_surface_image_format && surface_formats[i].colorSpace == request_surface_color_space) {
            Ichigo::vk_context.surface_format = surface_formats[i];
            goto found_format;
        }
    }

    Ichigo::vk_context.surface_format = surface_formats[0];

found_format:
    // ** Select a present format **

#define ICHIGO_PREFERRED_PRESENT_MODE VK_PRESENT_MODE_MAILBOX_KHR
// #define ICHIGO_PREFERRED_PRESENT_MODE VK_PRESENT_MODE_FIFO_KHR
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(Ichigo::vk_context.selected_gpu, Ichigo::vk_context.surface, &present_mode_count, nullptr);
    assert(present_mode_count != 0);
    VkPresentModeKHR *present_modes = reinterpret_cast<VkPresentModeKHR *>(platform_alloca(present_mode_count * sizeof(VkPresentModeKHR)));
    vkGetPhysicalDeviceSurfacePresentModesKHR(Ichigo::vk_context.selected_gpu, Ichigo::vk_context.surface, &present_mode_count, present_modes);

    for (u32 i = 0; i < present_mode_count; ++i) {
        if (present_modes[i] == ICHIGO_PREFERRED_PRESENT_MODE) {
            Ichigo::vk_context.present_mode = ICHIGO_PREFERRED_PRESENT_MODE;
            goto found_present_mode;
        }
    }

    // FIFO guaranteed to be available
    Ichigo::vk_context.present_mode = VK_PRESENT_MODE_FIFO_KHR;

found_present_mode:
    VkSurfaceCapabilitiesKHR surface_capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Ichigo::vk_context.selected_gpu, Ichigo::vk_context.surface, &surface_capabilities);

    // ** Swapchain, images, and image views **
    Ichigo::vk_context.create_swapchain_and_images(Ichigo::window_width, Ichigo::window_height);

    // ** Pipeline **
    // Create shaders
    VkShaderModule vertex_shader_module;
    VkShaderModule fragment_shader_module;
    {
        VkShaderModuleCreateInfo shader_create_info{};
        shader_create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_create_info.codeSize = vertex_shader_len;
        shader_create_info.pCode    = reinterpret_cast<const u32 *>(vertex_shader);
        VK_ASSERT_OK(vkCreateShaderModule(Ichigo::vk_context.logical_device, &shader_create_info, VK_NULL_HANDLE, &vertex_shader_module));
    }
    {
        VkShaderModuleCreateInfo shader_create_info{};
        shader_create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_create_info.codeSize = fragment_shader_len;
        shader_create_info.pCode    = reinterpret_cast<const u32 *>(fragment_shader);
        VK_ASSERT_OK(vkCreateShaderModule(Ichigo::vk_context.logical_device, &shader_create_info, VK_NULL_HANDLE, &fragment_shader_module));
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {
    {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_shader_module,
            .pName  = "main"
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_shader_module,
            .pName  = "main"
        }
    };

    const static VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info{};
    dynamic_state_create_info.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.dynamicStateCount = 2;
    dynamic_state_create_info.pDynamicStates    = dynamic_states;

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount   = 1;
    vertex_input_info.pVertexBindingDescriptions      = &VERTEX_BINDING_DESCRIPTION;
    vertex_input_info.vertexAttributeDescriptionCount = 2;
    vertex_input_info.pVertexAttributeDescriptions    = VERTEX_ATTRIBUTE_DESCRIPTIONS;

    VkPipelineInputAssemblyStateCreateInfo ia_info{};
    ia_info.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<f32>(Ichigo::vk_context.extent.width);
    viewport.height   = static_cast<f32>(Ichigo::vk_context.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset        = {0, 0};
    scissor.extent.width  = Ichigo::vk_context.extent.width;
    scissor.extent.height = Ichigo::vk_context.extent.height;

    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports    = &viewport;
    viewport_info.scissorCount  = 1;
    viewport_info.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer_create_info{};
    rasterizer_create_info.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_create_info.depthClampEnable        = VK_FALSE;
    rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_create_info.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer_create_info.lineWidth               = 1.0f;
    rasterizer_create_info.cullMode                = VK_CULL_MODE_NONE;
    rasterizer_create_info.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading      = 1.0f;
    multisampling.pSampleMask           = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable      = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_attachment{};
    color_attachment.blendEnable         = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info{};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo blend_info{};
    blend_info.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable   = VK_FALSE;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments    = &color_attachment;

    VkPipelineLayout pipeline_layout;
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount         = 0;
    layout_info.pSetLayouts            = nullptr;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges    = nullptr;

    VK_ASSERT_OK(vkCreatePipelineLayout(Ichigo::vk_context.logical_device, &layout_info, VK_NULL_HANDLE, &pipeline_layout));

    // ** Render pass **
    VkAttachmentDescription color_attachment_description{};
    color_attachment_description.format         = Ichigo::vk_context.surface_format.format;
    color_attachment_description.samples        = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_description.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_description.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_description.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_description.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference{};
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments    = &color_attachment_reference;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments    = &color_attachment_description;
    render_pass_info.subpassCount    = 1;
    render_pass_info.pSubpasses      = &subpass_description;

    VkSubpassDependency dependency{};
    dependency.srcSubpass            = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass            = 0;
    dependency.srcStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask         = 0;
    dependency.dstStageMask          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies   = &dependency;

    VK_ASSERT_OK(vkCreateRenderPass(Ichigo::vk_context.logical_device, &render_pass_info, VK_NULL_HANDLE, &Ichigo::vk_context.render_pass));

    VkGraphicsPipelineCreateInfo pipeline_create_info{};
    pipeline_create_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.stageCount          = 2;
    pipeline_create_info.pStages             = shader_stages;
    pipeline_create_info.pVertexInputState   = &vertex_input_info;
    pipeline_create_info.pInputAssemblyState = &ia_info;
    pipeline_create_info.pViewportState      = &viewport_info;
    pipeline_create_info.pRasterizationState = &rasterizer_create_info;
    pipeline_create_info.pMultisampleState   = &multisampling;
    pipeline_create_info.pDepthStencilState  = nullptr;
    pipeline_create_info.pColorBlendState    = &blend_info;
    pipeline_create_info.pDynamicState       = &dynamic_state_create_info;
    pipeline_create_info.layout              = pipeline_layout;
    pipeline_create_info.renderPass          = Ichigo::vk_context.render_pass;
    pipeline_create_info.subpass             = 0;

    VK_ASSERT_OK(vkCreateGraphicsPipelines(Ichigo::vk_context.logical_device, VK_NULL_HANDLE, 1, &pipeline_create_info, VK_NULL_HANDLE, &Ichigo::vk_context.graphics_pipeline));

    // ** Frame buffers **
    Ichigo::vk_context.create_framebuffers();

    // ** Command pool **
    VkCommandPoolCreateInfo command_pool_create_info{};
    command_pool_create_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    command_pool_create_info.queueFamilyIndex = Ichigo::vk_context.queue_family_index;

    VK_ASSERT_OK(vkCreateCommandPool(Ichigo::vk_context.logical_device, &command_pool_create_info, nullptr, &Ichigo::vk_context.command_pool));

    // ** Vertex buffers **
    Ichigo::vk_context.create_buffer(
        sizeof(Vertex) * ARRAY_LEN(vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &Ichigo::vk_context.vertex_buffer,
        &Ichigo::vk_context.vertex_buffer_memory
    );

    // ** Command buffers **
    VkCommandBufferAllocateInfo command_buffer_alloc_info{};
    command_buffer_alloc_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_alloc_info.commandPool        = Ichigo::vk_context.command_pool;
    command_buffer_alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_alloc_info.commandBufferCount = ICHIGO_MAX_FRAMES_IN_FLIGHT;

    VK_ASSERT_OK(vkAllocateCommandBuffers(Ichigo::vk_context.logical_device, &command_buffer_alloc_info, Ichigo::vk_context.command_buffers));

    // ** Syncronization **
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < ICHIGO_MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_ASSERT_OK(vkCreateSemaphore(Ichigo::vk_context.logical_device, &semaphore_create_info, nullptr, &Ichigo::vk_context.image_acquired_semaphores[i]));
        VK_ASSERT_OK(vkCreateSemaphore(Ichigo::vk_context.logical_device, &semaphore_create_info, nullptr, &Ichigo::vk_context.render_complete_semaphores[i]));
        VK_ASSERT_OK(vkCreateFence(Ichigo::vk_context.logical_device, &fence_create_info, nullptr, &Ichigo::vk_context.fences[i]));
    }

    void *vertex_buffer_data;
    vkMapMemory(Ichigo::vk_context.logical_device, Ichigo::vk_context.vertex_buffer_memory, 0, VK_WHOLE_SIZE, 0, &vertex_buffer_data);
    std::memcpy(vertex_buffer_data, vertices, sizeof(Vertex) * ARRAY_LEN(vertices));
    vkUnmapMemory(Ichigo::vk_context.logical_device, Ichigo::vk_context.vertex_buffer_memory);

    // Init imgui
    {
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance       = Ichigo::vk_context.vk_instance;
        init_info.PhysicalDevice = Ichigo::vk_context.selected_gpu;
        init_info.Device         = Ichigo::vk_context.logical_device;
        init_info.QueueFamily    = Ichigo::vk_context.queue_family_index;
        init_info.Queue          = Ichigo::vk_context.queue;
        init_info.PipelineCache  = VK_NULL_HANDLE;
        init_info.DescriptorPool = Ichigo::vk_context.descriptor_pool;
        init_info.Subpass        = 0;
        init_info.MinImageCount  = 2;
        init_info.ImageCount     = Ichigo::vk_context.swapchain_image_count;
        init_info.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplVulkan_Init(&init_info, Ichigo::vk_context.render_pass);
        initial_style = ImGui::GetStyle();
    }

    // Fonts
    {
        auto io = ImGui::GetIO();
        io.Fonts->AddFontFromMemoryTTF((void *) noto_font, noto_font_len, 18, &font_config, io.Fonts->GetGlyphRangesJapanese());

        // Upload fonts to GPU
        VkCommandPool command_pool = Ichigo::vk_context.command_pool;
        VkCommandBuffer command_buffer = Ichigo::vk_context.command_buffers[current_frame];

        auto err = vkResetCommandPool(Ichigo::vk_context.logical_device, command_pool, 0);
        VK_ASSERT_OK(err);
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        VK_ASSERT_OK(err);

        ImGui_ImplVulkan_CreateFontsTexture();

        VkSubmitInfo end_info{};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        VK_ASSERT_OK(err);
        err = vkQueueSubmit(Ichigo::vk_context.queue, 1, &end_info, VK_NULL_HANDLE);
        VK_ASSERT_OK(err);

        err = vkDeviceWaitIdle(Ichigo::vk_context.logical_device);
        VK_ASSERT_OK(err);
        ImGui_ImplVulkan_DestroyFontsTexture();
        // io.Fonts->Build();
    }
}

/*
    Cleanup done before closing the application
*/
void Ichigo::deinit() {}
