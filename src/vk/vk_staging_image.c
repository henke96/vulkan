#include "vk_staging_image.h"
#include "vk_base.h"

void *vk_staging_image__map(struct vk_staging_image *this) {
    void *data;
    vkMapMemory(this->base->device, this->staging_image_memory, 0, this->size, 0, &data);
    return data;
}
void vk_staging_image__unmap(struct vk_staging_image *this) {
    vkUnmapMemory(this->base->device, this->staging_image_memory);
}

int vk_staging_image__try_init(struct vk_staging_image *this, struct vk_base *base, int width, int height) {
    this->base = base;
    this->size = (VkDeviceSize) width*height*4;

    VkImageCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.pNext = 0;
    create_info.flags = 0;
    create_info.imageType = VK_IMAGE_TYPE_2D;
    create_info.extent.width = (uint32_t) width;
    create_info.extent.height = (uint32_t) height;
    create_info.extent.depth = 1;
    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;
    create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    create_info.tiling = VK_IMAGE_TILING_LINEAR;
    create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = 0;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(base->device, &create_info, 0, &this->staging_image) != VK_SUCCESS) {
        return -1;
    }

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(base->device, this->staging_image, &memory_requirements);

    int memory_type_index = -1;
    for (int i = 0; i < base->memory_properties.memoryTypeCount; ++i) {
        if ((memory_requirements.memoryTypeBits & (1 << i)) && (base->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            memory_type_index = i;
            break;
        }
    }

    if (memory_type_index < 0) {
        vkDestroyImage(base->device, this->staging_image, 0);
        return -2;
    }

    VkMemoryAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = (uint32_t) memory_type_index;
    allocate_info.pNext = 0;

    if (vkAllocateMemory(base->device, &allocate_info, 0, &this->staging_image_memory) != VK_SUCCESS) {
        vkDestroyImage(base->device, this->staging_image, 0);
        return -3;
    }
    vkBindImageMemory(base->device, this->staging_image, this->staging_image_memory, 0);
    return 0;
}

void vk_staging_image__free(struct vk_staging_image *this) {
    vkDestroyImage(this->base->device, this->staging_image, 0);
    vkFreeMemory(this->base->device, this->staging_image_memory, 0);
}
