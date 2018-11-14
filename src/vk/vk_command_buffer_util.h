#pragma once
#include <vulkan/vulkan.h>
#include "vk_base.h"

VkCommandBuffer vk_command_buffer_util__begin_single_time(struct vk_base *base);
void vk_command_buffer_util__end_single_time(VkCommandBuffer command_buffer, struct vk_base *base);