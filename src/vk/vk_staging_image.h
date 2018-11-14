#pragma once

#include <vulkan/vulkan.h>

struct vk_base;

struct vk_staging_image {
    struct vk_base *base;
    VkDeviceSize size;
    VkImage staging_image;
    VkDeviceMemory staging_image_memory;
};

int vk_staging_image__try_init(struct vk_staging_image *this, struct vk_base *base, int width, int height);
void vk_staging_image__free(struct vk_staging_image *this);

void *vk_staging_image__map(struct vk_staging_image *this);
void vk_staging_image__unmap(struct vk_staging_image *this);