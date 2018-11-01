#pragma once

#include "vulkan_swapchain.h"

struct vulkan_pipeline {
    struct vulkan_swapchain swapchain;
    struct vulkan_swapchain_shader vert_shader;
    struct vulkan_swapchain_shader frag_shader;
};