#pragma once

#include <vulkan/vulkan.h>
#include "vulkan_base.h"

struct vulkan_swapchain {
    struct vulkan_base *base;

    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;
    uint32_t image_count;
    VkImage *images;
    VkImageView *imageviews;
    VkCommandBuffer *command_buffers;
};

int vulkan_swapchain__try_init(struct vulkan_swapchain *this, struct vulkan_base *base);
void vulkan_swapchain__free(struct vulkan_swapchain *this);

int vulkan_swapchain__try_init_swapchain(struct vulkan_swapchain *this, int width, int height, VkImageUsageFlagBits image_usage);
void vulkan_swapchain__free_swapchain(struct vulkan_swapchain *this);