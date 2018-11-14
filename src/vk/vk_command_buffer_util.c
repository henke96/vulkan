#include "vk_command_buffer_util.h"

VkCommandBuffer vk_command_buffer_util__begin_single_time(struct vk_base *base) {
    VkCommandBufferAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.pNext = 0;
    allocate_info.commandBufferCount = 1;
    allocate_info.commandPool = base->command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer result;
    vkAllocateCommandBuffers(base->device, &allocate_info, &result);

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pNext = 0;
    begin_info.pInheritanceInfo = 0;

    vkBeginCommandBuffer(result, &begin_info);

    return result;
}
void vk_command_buffer_util__end_single_time(VkCommandBuffer command_buffer, struct vk_base *base) {
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info;
    submit_info.pNext = 0;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pWaitSemaphores = 0;
    submit_info.waitSemaphoreCount = 0;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = 0;
    submit_info.pWaitDstStageMask = 0;

    vkQueueSubmit(base->queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(base->queue);

    vkFreeCommandBuffers(base->device, base->command_pool, 1, &command_buffer);
}
