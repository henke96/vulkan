#pragma once

#include <vulkan/vulkan.h>

struct vulkan_base {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	VkQueue queue;
	int queue_family_index;
	VkSurfaceKHR surface;
	VkCommandPool command_pool;
#ifdef VULKAN_BASE_VALIDATION
	VkDebugUtilsMessengerEXT callback;
#endif
};

void vulkan_base__free(struct vulkan_base *this);

struct vulkan_base__create_surface {
    VkResult (*create_window_surface)(void *user_data, VkInstance instance, VkSurfaceKHR *surface_out);
    void *user_data;
};

int vulkan_base__try_init(struct vulkan_base *this, const char **extensions, int extension_count, struct vulkan_base__create_surface callback);
