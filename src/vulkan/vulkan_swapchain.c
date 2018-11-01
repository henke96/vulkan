#include <malloc.h>
#include "vulkan_swapchain.h"

static int try_create_framebuffers(struct vulkan_swapchain *this) {
    this->framebuffers = malloc(this->image_count*sizeof(*this->framebuffers));
    if (!this->framebuffers) {
        return -1;
    }

    for (int i = 0; i < this->image_count; ++i) {
        VkFramebufferCreateInfo create_info;
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = this->render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = (this->swapchain_imageviews + i);
        create_info.width = this->swapchain_extent.width;
        create_info.height = this->swapchain_extent.height;
        create_info.layers = 1;
        create_info.flags = 0;
        create_info.pNext = 0;

        if (vkCreateFramebuffer(this->device, &create_info, 0, this->swapchain_framebuffers + i) != VK_SUCCESS) {
            free(this->swapchain_framebuffers);
            return -2;
        }
    }
    return 0;
}

static int try_create_command_buffers(struct vulkan_swapchain *this) {
    this->command_buffers = malloc(this->image_count*sizeof(*this->command_buffers));
    if (!this->command_buffers) {
        return -1;
    }

    VkCommandBufferAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = this->base->command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = this->image_count;
    allocate_info.pNext = 0;

    if (vkAllocateCommandBuffers(this->base->device, &allocate_info, this->command_buffers) != VK_SUCCESS) {
        free(this->command_buffers);
        return -2;
    }
    return 0;
}
