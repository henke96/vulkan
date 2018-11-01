#include <malloc.h>
#include "vulkan_swapchain.h"

#define IMAGE_USAGE VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
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

static void free_from_command_buffers(struct vulkan_swapchain *this) {
    vkFreeCommandBuffers(this->base->device, this->base->command_pool, this->image_count, this->command_buffers);
    free(this->command_buffers);
    free_from_image_views(this);
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
    create_info.imageUsage = IMAGE_USAGE;
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
}

int vulkan_swapchain__try_init(struct vulkan_swapchain *this, struct vulkan_base *base) {
    this->base = base;
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

    result = try_create_command_buffers(this);
    if (result < 0) {
        free_from_image_views(this);
        return -3;
    }
    return 0;
}
