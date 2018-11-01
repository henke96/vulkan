#pragma once

#include <vulkan/vulkan.h>
#include "vulkan_base.h"

struct vulkan_swapchain_shader {
    long length;
    char *bytes;
};

struct vulkan_swapchain {
    struct vulkan_base *base;
    struct vulkan_swapchain_shader vert_shader;
    struct vulkan_swapchain_shader frag_shader;

    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;
    uint32_t image_count;
    VkImage *images;
    VkImageView *imageviews;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;
    VkFramebuffer *framebuffers;
    VkCommandBuffer *command_buffers;
};

int vulkan_swapchain__try_init(struct vulkan_swapchain *this, struct vulkan_base *base);
void vulkan_swapchain__free(struct vulkan_swapchain *this);

int vulkan_swapchain__try_init_swapchain(struct vulkan_swapchain *this, int window_width, int window_height);
void vulkan_swapchain__free_swapchain(struct vulkan_swapchain *this);