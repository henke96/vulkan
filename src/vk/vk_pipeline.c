#include <malloc.h>
#include "vk_pipeline.h"
#include "../file/file_util.h"

#define PIPELINE_SAMPLES VK_SAMPLE_COUNT_1_BIT

static void free_render_pass(struct vk_pipeline *this) {
    vkDestroyRenderPass(this->swapchain->base->device, this->render_pass, 0);
}

static void free_from_graphics_pipeline(struct vk_pipeline *this) {
    vkDestroyPipeline(this->swapchain->base->device, this->pipeline, 0);
    vkDestroyPipelineLayout(this->swapchain->base->device, this->pipeline_layout, 0);
    free_render_pass(this);
}

static void free_from_framebuffers(struct vk_pipeline *this) {
    for (int i = 0; i < this->swapchain->image_count; ++i) {
        vkDestroyFramebuffer(this->swapchain->base->device, this->framebuffers[i], 0);
    }
    free(this->framebuffers);
    free_from_graphics_pipeline(this);
}

static int try_create_render_pass(struct vk_pipeline *this) {
    VkAttachmentDescription attachment_description;
    attachment_description.format = this->swapchain->surface_format.format;
    attachment_description.samples = PIPELINE_SAMPLES;
    attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //TODO modify these
    attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachment_description.flags = 0;

    VkAttachmentReference attachment_reference;
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description;
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &attachment_reference;
    subpass_description.flags = 0;
    subpass_description.inputAttachmentCount = 0;
    subpass_description.pInputAttachments = 0;
    subpass_description.pResolveAttachments = 0;
    subpass_description.preserveAttachmentCount = 0;
    subpass_description.pPreserveAttachments = 0;
    subpass_description.pDepthStencilAttachment = 0;

    //TODO dependency experimental
    VkSubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    VkRenderPassCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.flags = 0;
    create_info.pNext = 0;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &attachment_description;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass_description;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    if (vkCreateRenderPass(this->swapchain->base->device, &create_info, 0, &this->render_pass) != VK_SUCCESS) {
        return -1;
    }
    return 0;
}

static int try_create_shader_module(struct vk_pipeline *this, const char *code, long length, VkShaderModule *out_shader_module) {
    VkShaderModuleCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pNext = 0;
    create_info.flags = 0;
    create_info.pCode = (uint32_t *) code;
    create_info.codeSize = (size_t) length;

    if (vkCreateShaderModule(this->swapchain->base->device, &create_info, 0, out_shader_module) != VK_SUCCESS) {
        return -1;
    }
    return 0;
}

static int try_create_graphics_pipeline(struct vk_pipeline *this) {
    VkShaderModule vert_shader_module;
    if (try_create_shader_module(this, this->vert_shader.bytes, this->vert_shader.length, &vert_shader_module) < 0) {
        return -1;
    }
    VkShaderModule frag_shader_module;
    if (try_create_shader_module(this, this->frag_shader.bytes, this->frag_shader.length, &frag_shader_module) < 0) {
        vkDestroyShaderModule(this->swapchain->base->device, vert_shader_module, 0);
        return -2;
    }

    VkPipelineShaderStageCreateInfo vert_shader_create_info;
    vert_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_create_info.pNext = 0;
    vert_shader_create_info.flags = 0;
    vert_shader_create_info.module = vert_shader_module;
    vert_shader_create_info.pName = "main";
    vert_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_create_info.pSpecializationInfo = 0;

    VkPipelineShaderStageCreateInfo frag_shader_create_info;
    frag_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_create_info.pNext = 0;
    frag_shader_create_info.flags = 0;
    frag_shader_create_info.module = frag_shader_module;
    frag_shader_create_info.pName = "main";
    frag_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_create_info.pSpecializationInfo = 0;

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_create_info, frag_shader_create_info};

    VkPipelineVertexInputStateCreateInfo vertex_input_create_info;
    vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_create_info.flags = 0;
    vertex_input_create_info.pNext = 0;
    vertex_input_create_info.vertexBindingDescriptionCount = 0;
    vertex_input_create_info.pVertexBindingDescriptions = 0;
    vertex_input_create_info.vertexAttributeDescriptionCount = 0;
    vertex_input_create_info.pVertexAttributeDescriptions = 0;

    VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info;
    pipeline_input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipeline_input_assembly_create_info.pNext = 0;
    pipeline_input_assembly_create_info.flags = 0;
    pipeline_input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipeline_input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) this->swapchain->extent.width;
    viewport.height = (float) this->swapchain->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkOffset2D scissor_offset;
    scissor_offset.x = 0;
    scissor_offset.y = 0;

    VkRect2D scissor;
    scissor.extent = this->swapchain->extent;
    scissor.offset = scissor_offset;

    VkPipelineViewportStateCreateInfo viewport_state_create_info;
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.pNext = 0;
    viewport_state_create_info.flags = 0;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.pViewports = &viewport;
    viewport_state_create_info.scissorCount = 1;
    viewport_state_create_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info;
    rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.flags = 0;
    rasterization_state_create_info.pNext = 0;
    rasterization_state_create_info.depthClampEnable = VK_FALSE;
    rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.lineWidth = 1.0f;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state_create_info.depthBiasEnable = VK_FALSE;
    rasterization_state_create_info.depthBiasConstantFactor = 0.0f;
    rasterization_state_create_info.depthBiasClamp = 0.0f;
    rasterization_state_create_info.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info;
    multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create_info.pNext = 0;
    multisample_state_create_info.flags = 0;
    multisample_state_create_info.sampleShadingEnable = VK_FALSE;
    multisample_state_create_info.rasterizationSamples = PIPELINE_SAMPLES;
    multisample_state_create_info.minSampleShading = 1.0f;
    multisample_state_create_info.pSampleMask = 0;
    multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
    multisample_state_create_info.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state;
    color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.blendEnable = VK_FALSE;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info;
    color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.logicOpEnable = VK_FALSE;
    color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_create_info.attachmentCount = 1;
    color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
    color_blend_state_create_info.blendConstants[0] = 0.0f;
    color_blend_state_create_info.blendConstants[1] = 0.0f;
    color_blend_state_create_info.blendConstants[2] = 0.0f;
    color_blend_state_create_info.blendConstants[3] = 0.0f;
    color_blend_state_create_info.flags = 0;
    color_blend_state_create_info.pNext = 0;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info;
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = 0;
    pipeline_layout_create_info.pSetLayouts = 0;
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = 0;
    pipeline_layout_create_info.pNext = 0;
    pipeline_layout_create_info.flags = 0;

    if (vkCreatePipelineLayout(this->swapchain->base->device, &pipeline_layout_create_info, 0, &this->pipeline_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(this->swapchain->base->device, vert_shader_module, 0);
        vkDestroyShaderModule(this->swapchain->base->device, frag_shader_module, 0);
        return -3;
    }

    VkGraphicsPipelineCreateInfo pipeline_create_info;
    pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_create_info.pNext = 0;
    pipeline_create_info.flags = 0;
    pipeline_create_info.stageCount = 2;
    pipeline_create_info.pStages = shader_stages;
    pipeline_create_info.pVertexInputState = &vertex_input_create_info;
    pipeline_create_info.pInputAssemblyState = &pipeline_input_assembly_create_info;
    pipeline_create_info.pViewportState = &viewport_state_create_info;
    pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
    pipeline_create_info.pMultisampleState = &multisample_state_create_info;
    pipeline_create_info.pDepthStencilState = 0;
    pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
    pipeline_create_info.pDynamicState = 0;
    pipeline_create_info.layout = this->pipeline_layout;
    pipeline_create_info.renderPass = this->render_pass;
    pipeline_create_info.subpass = 0;
    pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_create_info.basePipelineIndex = -1;
    pipeline_create_info.pTessellationState = 0;

    if (vkCreateGraphicsPipelines(this->swapchain->base->device, VK_NULL_HANDLE, 1, &pipeline_create_info, 0, &this->pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(this->swapchain->base->device, this->pipeline_layout, 0);
        vkDestroyShaderModule(this->swapchain->base->device, vert_shader_module, 0);
        vkDestroyShaderModule(this->swapchain->base->device, frag_shader_module, 0);
        return -4;
    }

    vkDestroyShaderModule(this->swapchain->base->device, vert_shader_module, 0);
    vkDestroyShaderModule(this->swapchain->base->device, frag_shader_module, 0);
    return 0;
}

static int try_create_framebuffers(struct vk_pipeline *this) {
    this->framebuffers = malloc(this->swapchain->image_count*sizeof(*this->framebuffers));
    if (!this->framebuffers) {
        return -1;
    }

    for (int i = 0; i < this->swapchain->image_count; ++i) {
        VkFramebufferCreateInfo create_info;
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = this->render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = (this->swapchain->imageviews + i);
        create_info.width = this->swapchain->extent.width;
        create_info.height = this->swapchain->extent.height;
        create_info.layers = 1;
        create_info.flags = 0;
        create_info.pNext = 0;

        if (vkCreateFramebuffer(this->swapchain->base->device, &create_info, 0, this->framebuffers + i) != VK_SUCCESS) {
            free(this->framebuffers);
            return -2;
        }
    }
    return 0;
}

int vk_pipeline__try_init(struct vk_pipeline *this, struct vk_swapchain *swapchain) {
    this->swapchain = swapchain;

    struct file_util__try_read vert_read = file_util__try_read("shaders/vert.spv");
    if (vert_read.result < 0) {
        return -1;
    }
    this->vert_shader.length = vert_read.length;
    this->vert_shader.bytes = vert_read.malloc_bytes;
    struct file_util__try_read frag_read = file_util__try_read("shaders/frag.spv");
    if (frag_read.result < 0) {
        free(vert_read.malloc_bytes);
        return -2;
    }
    this->frag_shader.length = frag_read.length;
    this->frag_shader.bytes = frag_read.malloc_bytes;
    return 0;
}

void vk_pipeline__free(struct vk_pipeline *this) {
    free(this->vert_shader.bytes);
    free(this->frag_shader.bytes);
}

int vk_pipeline__try_init_pipeline(struct vk_pipeline *this) {
    int result;
    result = try_create_render_pass(this);
    if (result < 0) {
        return -1;
    }

    result = try_create_graphics_pipeline(this);
    if (result < 0) {
        free_render_pass(this);
        return -2;
    }

    result = try_create_framebuffers(this);
    if (result < 0) {
        free_from_graphics_pipeline(this);
        return -3;
    }
    return 0;
}

void vk_pipeline__free_pipeline(struct vk_pipeline *this) {
    free_from_framebuffers(this);
}
