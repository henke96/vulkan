#include <malloc.h>
#include "vulkan_swapchain.h"
#include "../file/file.h"

#define PIPELINE_SAMPLES VK_SAMPLE_COUNT_1_BIT
#define PREFERRED_IMAGE_COUNT 4

static void free_swapchain(struct vulkan_swapchain *this) {
    vkDestroySwapchainKHR(this->base->device, this->swapchain, 0);
    free(this->images);
}

static void free_from_image_views(struct vulkan_swapchain *this) {
    for (int i = 0; i < this->image_count; ++i) {
        vkDestroyImageView(this->base->device, this->imageviews[i], 0);
    }
    free(this->imageviews);
    free_swapchain(this);
}

static void free_from_render_pass(struct vulkan_swapchain *this) {
    vkDestroyRenderPass(this->base->device, this->render_pass, 0);
    free_from_image_views(this);
}

static void free_from_graphics_pipeline(struct vulkan_swapchain *this) {
    vkDestroyPipeline(this->base->device, this->graphics_pipeline, 0);
    vkDestroyPipelineLayout(this->base->device, this->pipeline_layout, 0);
    free_from_render_pass(this);
}

static void free_from_framebuffers(struct vulkan_swapchain *this) {
    for (int i = 0; i < this->image_count; ++i) {
        vkDestroyFramebuffer(this->base->device, this->framebuffers[i], 0);
    }
    free(this->framebuffers);
    free_from_graphics_pipeline(this);
}

static void free_from_command_buffers(struct vulkan_swapchain *this) {
    vkFreeCommandBuffers(this->base->device, this->base->command_pool, this->image_count, this->command_buffers);
    free(this->command_buffers);
    free_from_framebuffers(this);
}

struct try_query_swapchain {
    int result;
    VkPresentModeKHR best_present_mode;
    uint32_t best_image_count;
    VkSurfaceFormatKHR best_surface_format;
}
static try_query_swapchain(struct vulkan_swapchain *this) {
    struct try_query_swapchain result;

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->base->physical_device, this->base->surface, &capabilities);

    if (capabilities.maxImageCount < PREFERRED_IMAGE_COUNT) {
        result.best_image_count = capabilities.maxImageCount;
    } else {
        result.best_image_count = PREFERRED_IMAGE_COUNT;
    }

    result.best_present_mode = VK_PRESENT_MODE_FIFO_KHR; // Always supported

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(this->base->physical_device, this->base->surface, &present_mode_count, 0);
    VkPresentModeKHR present_modes[present_mode_count];
    vkGetPhysicalDeviceSurfacePresentModesKHR(this->base->physical_device, this->base->surface, &present_mode_count, present_modes);

    for (int i = 0; i < present_mode_count; ++i) {
        VkPresentModeKHR present_mode = present_modes[i];
        if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            result.best_present_mode = present_mode;
            break;
        }
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            result.best_present_mode = present_mode;
        }
    }

    uint32_t surface_format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(this->base->physical_device, this->base->surface, &surface_format_count, 0);
    if (surface_format_count < 1) {
        result.result = -1;
        return result;
    }
    VkSurfaceFormatKHR surface_formats[surface_format_count];
    vkGetPhysicalDeviceSurfaceFormatsKHR(this->base->physical_device, this->base->surface, &surface_format_count, surface_formats);
    if (surface_formats[0].format == VK_FORMAT_UNDEFINED) {
        result.best_surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        result.best_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    } else {
        for (int i = 0; i < surface_format_count; ++i) {
            VkSurfaceFormatKHR surface_format = surface_formats[i];
            if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                result.best_surface_format = surface_format;
                goto surface_format_found;
            }
        }
        result.best_surface_format = surface_formats[0];
    }
    surface_format_found:
    result.result = 0;
    return result;
}

static int try_create_swapchain(struct vulkan_swapchain *this, int window_width, int window_height) {
    struct try_query_swapchain query = try_query_swapchain(this);
    if (query.result < 0) {
        return -1;
    }
    this->surface_format = query.best_surface_format;

    this->extent.width = (uint32_t) window_width;
    this->extent.height = (uint32_t) window_height;
    VkSwapchainCreateInfoKHR create_info;
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = this->base->surface;
    create_info.minImageCount = query.best_image_count;
    create_info.pNext = 0;
    create_info.flags = 0;
    create_info.imageFormat = this->surface_format.format;
    create_info.imageColorSpace = this->surface_format.colorSpace;
    create_info.imageExtent = this->extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // TODO depends on application
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = 0;
    create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = query.best_present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(this->base->device, &create_info, 0, &this->swapchain) != VK_SUCCESS) {
        return -2;
    }

    vkGetSwapchainImagesKHR(this->base->device, this->swapchain, &this->image_count, 0);
    this->images = malloc(this->image_count*sizeof(*this->images));
    if (!this->images) {
        return -3;
    }
    vkGetSwapchainImagesKHR(this->base->device, this->swapchain, &this->image_count, this->images);
    return 0;
}

static int try_create_image_views(struct vulkan_swapchain *this) {
    this->imageviews = malloc(this->image_count*sizeof(*this->imageviews));
    if (!this->imageviews) {
        return -1;
    }

    for (int i = 0; i < this->image_count; ++i) {
        VkImageViewCreateInfo create_info;
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = this->images[i];
        create_info.flags = 0;
        create_info.pNext = 0;
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = this->surface_format.format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(this->base->device, &create_info, 0, this->imageviews + i) != VK_SUCCESS) {
            free(this->imageviews);
            return -2;
        }
    }
    return 0;
}

static int try_create_render_pass(struct vulkan_swapchain *this) {
    VkAttachmentDescription attachment_description;
    attachment_description.format = this->surface_format.format;
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

    if (vkCreateRenderPass(this->base->device, &create_info, 0, &this->render_pass) != VK_SUCCESS) {
        return -1;
    }
    return 0;
}

static int try_create_shader_module(struct vulkan_swapchain *this, const char *code, long length, VkShaderModule *out_shader_module) {
    VkShaderModuleCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pNext = 0;
    create_info.flags = 0;
    create_info.pCode = (uint32_t *) code;
    create_info.codeSize = (size_t) length;

    if (vkCreateShaderModule(this->base->device, &create_info, 0, out_shader_module) != VK_SUCCESS) {
        return -1;
    }
    return 0;
}

static int try_create_graphics_pipeline(struct vulkan_swapchain *this) {
    VkShaderModule vert_shader_module;
    if (try_create_shader_module(this, this->vert_shader.bytes, this->vert_shader.length, &vert_shader_module) < 0) {
        return -1;
    }
    VkShaderModule frag_shader_module;
    if (try_create_shader_module(this, this->frag_shader.bytes, this->frag_shader.length, &frag_shader_module) < 0) {
        vkDestroyShaderModule(this->base->device, vert_shader_module, 0);
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
    viewport.width = (float) this->extent.width;
    viewport.height = (float) this->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkOffset2D scissor_offset;
    scissor_offset.x = 0;
    scissor_offset.y = 0;

    VkRect2D scissor;
    scissor.extent = this->extent;
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

    if (vkCreatePipelineLayout(this->base->device, &pipeline_layout_create_info, 0, &this->pipeline_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(this->base->device, vert_shader_module, 0);
        vkDestroyShaderModule(this->base->device, frag_shader_module, 0);
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

    if (vkCreateGraphicsPipelines(this->base->device, VK_NULL_HANDLE, 1, &pipeline_create_info, 0, &this->graphics_pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(this->base->device, this->pipeline_layout, 0);
        vkDestroyShaderModule(this->base->device, vert_shader_module, 0);
        vkDestroyShaderModule(this->base->device, frag_shader_module, 0);
        return -4;
    }

    vkDestroyShaderModule(this->base->device, vert_shader_module, 0);
    vkDestroyShaderModule(this->base->device, frag_shader_module, 0);
    return 0;
}

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
        create_info.pAttachments = (this->imageviews + i);
        create_info.width = this->extent.width;
        create_info.height = this->extent.height;
        create_info.layers = 1;
        create_info.flags = 0;
        create_info.pNext = 0;

        if (vkCreateFramebuffer(this->base->device, &create_info, 0, this->framebuffers + i) != VK_SUCCESS) {
            free(this->framebuffers);
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

void vulkan_swapchain__free(struct vulkan_swapchain *this) {
    free(this->vert_shader.bytes);
    free(this->frag_shader.bytes);
}

int vulkan_swapchain__try_init(struct vulkan_swapchain *this, struct vulkan_base *base) {
    this->base = base;
    struct file__try_read vert_read = file__try_read("shaders/vert.spv");
    if (vert_read.result < 0) {
        return -1;
    }
    this->vert_shader.length = vert_read.length;
    this->vert_shader.bytes = vert_read.malloc_bytes;
    struct file__try_read frag_read = file__try_read("shaders/frag.spv");
    if (frag_read.result < 0) {
        free(vert_read.malloc_bytes);
        return -2;
    }
    this->frag_shader.length = frag_read.length;
    this->frag_shader.bytes = frag_read.malloc_bytes;
    return 0;
}

void vulkan_swapchain__free_swapchain(struct vulkan_swapchain *this) {
    free_from_command_buffers(this);
}

int vulkan_swapchain__try_init_swapchain(struct vulkan_swapchain *this, int window_width, int window_height) {
    int result;
    result = try_create_swapchain(this, window_width, window_height);
    if (result < 0) {
        return -1;
    }

    result = try_create_image_views(this);
    if (result < 0) {
        free_swapchain(this);
        return -2;
    }

    result = try_create_render_pass(this);
    if (result < 0) {
        free_from_image_views(this);
        return -3;
    }

    result = try_create_graphics_pipeline(this);
    if (result < 0) {
        free_from_render_pass(this);
        return -4;
    }

    result = try_create_framebuffers(this);
    if (result < 0) {
        free_from_graphics_pipeline(this);
        return -5;
    }

    result = try_create_command_buffers(this);
    if (result < 0) {
        free_from_framebuffers(this);
        return -6;
    }
    return 0;
}
