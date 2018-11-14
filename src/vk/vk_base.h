#pragma once

#include <vulkan/vulkan.h>

struct vk_base {
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkDevice device;
	VkQueue queue;
	int queue_family_index;
	VkSurfaceKHR surface;
	VkCommandPool command_pool;
#ifdef BASE_VALIDATION
	VkDebugUtilsMessengerEXT callback;
#endif
};

void vk_base__free(struct vk_base *this);

struct vk_base__create_surface {
    VkResult (*create_window_surface)(void *user_data, VkInstance instance, VkSurfaceKHR *surface_out);
    void *user_data;
};

int vk_base__try_init(struct vk_base *this, const char **extensions, int extension_count, struct vk_base__create_surface callback);
