#pragma once

#include <vulkan/vulkan.h>

struct vk_base;

struct vk_staging_buffer {
    struct vk_base *base;
    VkDeviceSize size;
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
};

int vk_staging_buffer__try_init(struct vk_staging_buffer *this, struct vk_base *base, int width, int height);
void vk_staging_buffer__free(struct vk_staging_buffer *this);

void *vk_staging_buffer__map(struct vk_staging_buffer *this);
void vk_staging_buffer__unmap(struct vk_staging_buffer *this);