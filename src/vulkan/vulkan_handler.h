#pragma once

#include <vulkan/vulkan.h>

struct vulkan_handler {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	int queue_family_index;
	VkSurfaceKHR surface;
	VkRenderPass render_pass;
	VkPipelineLayout pipeline_layout;
	VkPipeline graphics_pipeline;
	VkCommandPool command_pool;
#ifdef VULKAN_HANDLER_VALIDATION
	VkDebugUtilsMessengerEXT callback;
#endif
};

void vulkan_handler__free(struct vulkan_handler *this);

struct vulkan_handler__create_surface {
    VkResult (*create_window_surface)(void *user_data, VkInstance instance, VkSurfaceKHR *surface_out);
    void *user_data;
};

int vulkan_handler__try_init(struct vulkan_handler *this, const char **extensions, int extension_count, struct vulkan_handler__create_surface callback);
void vulkan_handler__free_command_buffers_to_swapchain(struct vulkan_handler *this);
int vulkan_handler__try_create_swapchain_to_command_buffers(struct vulkan_handler *this, int window_width, int window_height);
