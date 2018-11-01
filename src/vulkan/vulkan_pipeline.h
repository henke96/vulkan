#pragma once

#include "vulkan_swapchain.h"

struct vulkan_pipeline_shader {
    long length;
    char *bytes;
};

struct vulkan_pipeline {
    struct vulkan_swapchain *swapchain;
    struct vulkan_pipeline_shader vert_shader;
    struct vulkan_pipeline_shader frag_shader;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkFramebuffer *framebuffers;
};

int vulkan_pipeline__try_init(struct vulkan_pipeline *this, struct vulkan_swapchain *base);
void vulkan_pipeline__free(struct vulkan_pipeline *this);

int vulkan_pipeline__try_init_pipeline(struct vulkan_pipeline *this);
void vulkan_pipeline__free_pipeline(struct vulkan_pipeline *this);