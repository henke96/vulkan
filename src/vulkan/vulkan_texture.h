#pragma once

#include <vulkan/vulkan.h>

struct vulkan_base;

struct vulkan_texture {
    struct vulkan_base *base;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
};

int vulkan_texture__try_init(struct vulkan_texture *this, struct vulkan_base *base, int width, int height);
void vulkan_texture__free(struct vulkan_texture *this);