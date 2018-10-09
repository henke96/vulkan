#pragma once

#include <vulkan/vulkan.h>

struct vulkan_handler {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkExtent2D swapchain_extent;
	uint32_t swapchain_image_count;
	VkImage *swapchain_images;
	VkImageView *swapchain_imageviews;
	VkRenderPass render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	int window_width;
	int window_height;
#ifdef VULKAN_HANDLER_VALIDATION
	VkDebugUtilsMessengerEXT callback;
#endif
};

void vulkan_handler__free(struct vulkan_handler *this);
int vulkan_handler__try_init(struct vulkan_handler *this, const char **extensions, int extension_count, int window_width, int window_height, VkResult (*create_window_surface)(void *, VkInstance, VkSurfaceKHR *), void *surface_creator);