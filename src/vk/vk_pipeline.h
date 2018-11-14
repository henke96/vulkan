#pragma once

#include "vk_swapchain.h"

struct vk_pipeline_shader {
    long length;
    char *bytes;
};

struct vk_pipeline {
    struct vk_swapchain *swapchain;
    struct vk_pipeline_shader vert_shader;
    struct vk_pipeline_shader frag_shader;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkFramebuffer *framebuffers;
};

int vk_pipeline__try_init(struct vk_pipeline *this, struct vk_swapchain *base);
void vk_pipeline__free(struct vk_pipeline *this);

int vk_pipeline__try_init_pipeline(struct vk_pipeline *this);
void vk_pipeline__free_pipeline(struct vk_pipeline *this);