#pragma once

#include <vulkan/vulkan.h>

struct vulkan_handler {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	int queue_family_index;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkExtent2D swapchain_extent;
	VkSurfaceFormatKHR swapchain_surface_format;
	uint32_t swapchain_image_count;
	VkImage *swapchain_images;
	VkImageView *swapchain_imageviews;
	VkRenderPass render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	VkFramebuffer *swapchain_framebuffers;
	VkCommandPool command_pool;
	VkCommandBuffer *swapchain_command_buffers;
#ifdef VULKAN_HANDLER_VALIDATION
	VkDebugUtilsMessengerEXT callback;
#endif
};

void vulkan_handler__free(struct vulkan_handler *this);

struct vulkan_handler__create_surface {
    VkResult (*create_window_surface)(void *user_data, VkInstance instance, VkSurfaceKHR *surface_out);
    void *user_data;
};

int vulkan_handler__try_init(struct vulkan_handler *this, const char **extensions, int extension_count, int window_width, int window_height, struct vulkan_handler__create_surface callback);
int vulkan_handler__try_recreate_swapchain(struct vulkan_handler *this, int window_width, int window_height, struct vulkan_handler__create_surface callback);