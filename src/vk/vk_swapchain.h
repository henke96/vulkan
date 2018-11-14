#pragma once

#include <vulkan/vulkan.h>
#include "vk_base.h"

struct vk_swapchain {
    struct vk_base *base;

    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkSurfaceFormatKHR surface_format;
    uint32_t image_count;
    VkImage *images;
    VkImageView *imageviews;
    VkCommandBuffer *command_buffers;
};

int vk_swapchain__try_init(struct vk_swapchain *this, struct vk_base *base);
void vk_swapchain__free(struct vk_swapchain *this);

int vk_swapchain__try_init_swapchain(struct vk_swapchain *this, int width, int height, VkImageUsageFlagBits image_usage);
void vk_swapchain__free_swapchain(struct vk_swapchain *this);