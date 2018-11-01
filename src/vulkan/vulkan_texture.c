#include "vulkan_texture.h"
#include "vulkan_base.h"

void *vulkan_texture__map(struct vulkan_texture *this) {
    void *data;
    vkMapMemory(this->base->device, this->staging_buffer_memory, 0, this->size, 0, &data);
    return data;
}
void vulkan_texture__unmap(struct vulkan_texture *this) {
    vkUnmapMemory(this->base->device, this->staging_buffer_memory);
}

int vulkan_texture__try_init(struct vulkan_texture *this, struct vulkan_base *base, int width, int height) {
    this->base = base;
    this->size = (VkDeviceSize) width*height*4;

    VkBufferCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = this->size;
    create_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = 0;
    create_info.pNext = 0;
    create_info.flags = 0;

    if (vkCreateBuffer(base->device, &create_info, 0, &this->staging_buffer) != VK_SUCCESS) {
        return -1;
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(base->device, this->staging_buffer, &memory_requirements);

    int memory_type_index = -1;
    for (int i = 0; i < base->memory_properties.memoryTypeCount; ++i) {
        if ((memory_requirements.memoryTypeBits & (1 << i)) && (base->memory_properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            memory_type_index = i;
            break;
        }
    }

    if (memory_type_index < 0) {
        vkDestroyBuffer(base->device, this->staging_buffer, 0);
        return -2;
    }

    VkMemoryAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = (uint32_t) memory_type_index;
    allocate_info.pNext = 0;

    if (vkAllocateMemory(base->device, &allocate_info, 0, &this->staging_buffer_memory) != VK_SUCCESS) {
        vkDestroyBuffer(base->device, this->staging_buffer, 0);
        return -3;
    }
    vkBindBufferMemory(base->device, this->staging_buffer, this->staging_buffer_memory, 0);
    return 0;
}

void vulkan_texture__free(struct vulkan_texture *this) {
    vkDestroyBuffer(this->base->device, this->staging_buffer, 0);
    vkFreeMemory(this->base->device, this->staging_buffer_memory, 0);
}
