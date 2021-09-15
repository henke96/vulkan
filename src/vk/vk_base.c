#include "vk_base.h"
#include "../file/file_util.h"
#include <malloc.h>
#include <stdio.h>

static void free_instance(struct vk_base *this) {
	vkDestroyInstance(this->instance, 0);
}

#ifdef BASE_VALIDATION
static void free_debug_callback_to_instance(struct vk_base *this) {
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(this->instance, "vkDestroyDebugUtilsMessengerEXT");
	func(this->instance, this->callback, 0);
	free_instance(this);
}
#endif

static void free_from_window_surface(struct vk_base *this) {
	vkDestroySurfaceKHR(this->instance, this->surface, 0);
#ifdef BASE_VALIDATION
	free_debug_callback_to_instance(this);
#else
    free_instance(this);
#endif
}

static void free_from_device(struct vk_base *this) {
	vkDestroyDevice(this->device, 0);
    free_from_window_surface(this);
}

static void free_from_command_pool(struct vk_base *this) {
    vkDestroyCommandPool(this->device, this->command_pool, 0);
	free_from_device(this);
}

void vk_base__free(struct vk_base *this) {
	free_from_command_pool(this);
}

static int try_create_instance(struct vk_base *this, const char **extensions, int extension_count) {
	VkInstanceCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = 0;
#ifdef BASE_VALIDATION
	const char *new_extensions[extension_count + 1];
	int i = 0;
	for (; i < extension_count; ++i) {
		new_extensions[i] = extensions[i];
	}
	new_extensions[i] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	create_info.enabledExtensionCount = (uint32_t) extension_count + 1;
	create_info.ppEnabledExtensionNames = new_extensions;

	const char *validation_layers[1] = {"VK_LAYER_KHRONOS_validation"};
	create_info.enabledLayerCount = 1;
	create_info.ppEnabledLayerNames = validation_layers;
#else
	create_info.enabledExtensionCount = (uint32_t) extension_count;
	create_info.ppEnabledExtensionNames = extensions;
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = 0;
#endif
	create_info.pNext = 0;
	create_info.flags = 0;

	if (vkCreateInstance(&create_info, 0, &this->instance) != VK_SUCCESS) {
		return -1;
	}
	return 0;
}

#ifdef BASE_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		void *pUserData
) {
	printf("Validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}
#endif

static int try_create_device(struct vk_base *this) {
	this->queue_family_index = -1;
	uint32_t device_count;
	vkEnumeratePhysicalDevices(this->instance, &device_count, 0);
	VkPhysicalDevice devices[device_count];
	vkEnumeratePhysicalDevices(this->instance, &device_count, devices);

	for (int i = 0; i < device_count; ++i) {
		VkPhysicalDevice current_device = devices[i];

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(current_device, &queue_family_count, 0);
		VkQueueFamilyProperties queue_family_propertiess[queue_family_count];
		vkGetPhysicalDeviceQueueFamilyProperties(current_device, &queue_family_count, queue_family_propertiess);

		for (int j = 0; j < queue_family_count; ++j) {
			VkBool32 present_support;
			vkGetPhysicalDeviceSurfaceSupportKHR(current_device, (uint32_t) j, this->surface, &present_support);

			if (queue_family_propertiess[j].queueCount > 0 && queue_family_propertiess[j].queueFlags & VK_QUEUE_GRAPHICS_BIT && present_support) {
				this->queue_family_index = j;
				this->physical_device = current_device;
				goto break_outer;
			}
		}
	}
	break_outer:
	if (this->queue_family_index == -1) {
		return -1;
	}

	vkGetPhysicalDeviceMemoryProperties(this->physical_device, &this->memory_properties);

	VkDeviceQueueCreateInfo queue_create_info;
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.flags = 0;
	queue_create_info.pNext = 0;
	const float queue_priority = 1.0f;
	queue_create_info.pQueuePriorities = &queue_priority;
	queue_create_info.queueCount = 1;
	queue_create_info.queueFamilyIndex = (uint32_t) this->queue_family_index;

	VkPhysicalDeviceFeatures device_features = {0};

	VkDeviceCreateInfo device_create_info;
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pQueueCreateInfos = &queue_create_info;
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.pEnabledFeatures = &device_features;
	const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	device_create_info.enabledExtensionCount = 1;
	device_create_info.ppEnabledExtensionNames = device_extensions;
	device_create_info.flags = 0;
	device_create_info.pNext = 0;
#ifdef BASE_VALIDATION
	const char *validation_layers[1] = { "VK_LAYER_KHRONOS_validation" };
	device_create_info.enabledLayerCount = 1;
	device_create_info.ppEnabledLayerNames = validation_layers;
#else
	device_create_info.enabledLayerCount = 0;
	device_create_info.ppEnabledLayerNames = 0;
#endif
	if (vkCreateDevice(this->physical_device, &device_create_info, 0, &this->device) != VK_SUCCESS) {
		return -2;
	}
	vkGetDeviceQueue(this->device, (uint32_t) this->queue_family_index, 0, &this->queue);
	return 0;
}

static int try_create_command_pool(struct vk_base *this) {
	VkCommandPoolCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	create_info.pNext = 0;
	create_info.flags = 0;
	create_info.queueFamilyIndex = (uint32_t) this->queue_family_index;

	if (vkCreateCommandPool(this->device, &create_info, 0, &this->command_pool) != VK_SUCCESS) {
		return -1;
	}
	return 0;
}

int vk_base__try_init(struct vk_base *this, const char **extensions, int extension_count,
					  struct vk_base__create_surface callback) {
	int result;
	result = try_create_instance(this, extensions, extension_count);
	if (result < 0) {
		return -1;
	}
#ifdef BASE_VALIDATION
	VkDebugUtilsMessengerCreateInfoEXT create_info;
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debug_callback;
	create_info.pUserData = 0;
	create_info.flags = 0;
	create_info.pNext = 0;

	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(this->instance, "vkCreateDebugUtilsMessengerEXT");
	if (func(this->instance, &create_info, 0, &this->callback) != VK_SUCCESS) {
		free_instance(this);
		return -2;
	}
#endif
	if (callback.create_window_surface(callback.user_data, this->instance, &this->surface) != VK_SUCCESS) {
#ifdef BASE_VALIDATION
		free_debug_callback_to_instance(this);
#else
        free_instance(this);
#endif
		return -3;
	}

	result = try_create_device(this);
	if (result < 0) {
        free_from_window_surface(this);
		return -4;
	}

    result = try_create_command_pool(this);
    if (result < 0) {
		free_from_device(this);
        return -5;
    }
	return 0;
}

