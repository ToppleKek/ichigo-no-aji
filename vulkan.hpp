/*
    Vulkan render backend module.

    Author: Braeden Hong
      Date: October 30, 2023
*/

#pragma once

#include <vulkan/vulkan.h>
#include "common.hpp"

#define ICHIGO_MAX_FRAMES_IN_FLIGHT 2

namespace IchigoVulkan {
struct Context {
    VkQueue queue;
    VkDevice logical_device;
    VkInstance vk_instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice selected_gpu;
    VkDescriptorPool descriptor_pool;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkSwapchainKHR swapchain;
    VkRenderPass render_pass;
    VkPipeline graphics_pipeline;

    VkFramebuffer *frame_buffers;
    u64 frame_buffer_count;

    VkImage *swapchain_images;
    u64 swapchain_image_count;

    VkImageView *swapchain_image_views;
    u64 swapchain_image_view_count;

    VkCommandPool command_pool;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;

    VkCommandBuffer command_buffers[ICHIGO_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore image_acquired_semaphores[ICHIGO_MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_complete_semaphores[ICHIGO_MAX_FRAMES_IN_FLIGHT];
    VkFence fences[ICHIGO_MAX_FRAMES_IN_FLIGHT];
    VkExtent2D extent;
    u32 queue_family_index;

    void init(const char **extensions, u32 num_extensions);
    void create_buffer(u64 size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkBuffer *buffer, VkDeviceMemory *buffer_memory);
    void create_swapchain_and_images(u32 window_width, u32 window_height);
    void create_framebuffers();
    void rebuild_swapchain(u32 window_width, u32 window_height);
};
}  // namespace IchigoVulkan
