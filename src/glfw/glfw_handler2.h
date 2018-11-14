#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../vk/vk_base.h"
#include "../vk/vk_swapchain.h"
#include "../vk/vk_staging_buffer.h"
#include "../vk/vk_pipeline.h"

#define FRAME_RESOURCES 2

struct glfw_handler {
	struct vk_base base;
	struct vk_swapchain swapchain;
	struct vk_staging_buffer test_texture;
	GLFWwindow *window;
	VkSemaphore image_available_semaphores[FRAME_RESOURCES];
	VkSemaphore render_finished_semaphores[FRAME_RESOURCES];
	VkFence resource_fences[FRAME_RESOURCES];
	int resources_index;
	int should_recreate_swapchain;
};

int glfw_handler__try_init(struct glfw_handler *this, int width, int height, char *title, int is_fullscreen);
void glfw_handler__free(struct glfw_handler *this);
int glfw_handler__try_run(struct glfw_handler *this);