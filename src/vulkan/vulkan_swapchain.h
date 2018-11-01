#pragma once

#include <vulkan/vulkan.h>
#include "vulkan_handler.h"

struct vulkan_swapchain {
    struct vulkan_handler *base;
    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;
    uint32_t image_count;
    VkImage *images;
    VkImageView *imageviews;
    VkFramebuffer *framebuffers;
    VkCommandBuffer *command_buffers;
};

int vulkan_swapchain__try_init(struct vulkan_swapchain *this, struct vulkan_handler *base);
void vulkan_swapchain__free(struct vulkan_swapchain *this);

int vulkan_swapchain__try_create(struct vulkan_swapchain *this, int window_width, int window_height);
int vulkan_swapchain__try_destroy(struct vulkan_swapchain *this);