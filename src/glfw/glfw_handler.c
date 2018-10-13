#include <stdio.h>
#include "glfw_handler.h"

static VkResult create_window_surface(void *user_data, VkInstance instance, VkSurfaceKHR *surface_out) {
	struct glfw_handler *this = (struct glfw_handler *) user_data;
	return glfwCreateWindowSurface(instance, this->window, 0, surface_out);
}

static void free_glfw(struct glfw_handler *this) {
	glfwDestroyWindow(this->window);
	glfwTerminate();
}

static void free_semaphores_and_fences(struct glfw_handler *this) {
	for (int i = 0; i < FRAME_RESOURCES; ++i) {
		vkDestroySemaphore(this->vulkan_handler.device, this->render_finished_semaphores[i], 0);
		vkDestroySemaphore(this->vulkan_handler.device, this->image_available_semaphores[i], 0);
		vkDestroyFence(this->vulkan_handler.device, this->resource_fences[i], 0);
	}
}

static void free_semaphores_and_fences_below(struct glfw_handler *this, int i) {
	for (--i;i >= 0; --i) {
		vkDestroySemaphore(this->vulkan_handler.device, this->render_finished_semaphores[i], 0);
		vkDestroySemaphore(this->vulkan_handler.device, this->image_available_semaphores[i], 0);
		vkDestroyFence(this->vulkan_handler.device, this->resource_fences[i], 0);
	}
}

static int create_semaphores_and_fences(struct glfw_handler *this) {
	VkSemaphoreCreateInfo semaphore_create_info;
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphore_create_info.pNext = 0;
	semaphore_create_info.flags = 0;

	VkFenceCreateInfo fence_create_info;
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.pNext = 0;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	int i = 0;
	for (; i < FRAME_RESOURCES; ++i) {
		if (vkCreateSemaphore(this->vulkan_handler.device, &semaphore_create_info, 0, this->image_available_semaphores + i) != VK_SUCCESS) {
			free_semaphores_and_fences_below(this, i);
			return -1;
		}

		if (vkCreateSemaphore(this->vulkan_handler.device, &semaphore_create_info, 0, this->render_finished_semaphores + i) != VK_SUCCESS) {
			vkDestroySemaphore(this->vulkan_handler.device, this->image_available_semaphores[i], 0);
			free_semaphores_and_fences_below(this, i);
			return -2;
		}

		if (vkCreateFence(this->vulkan_handler.device, &fence_create_info, 0, this->resource_fences + i) != VK_SUCCESS) {
			vkDestroySemaphore(this->vulkan_handler.device, this->image_available_semaphores[i], 0);
			vkDestroySemaphore(this->vulkan_handler.device, this->render_finished_semaphores[i], 0);
			free_semaphores_and_fences_below(this, i);
			return -3;
		}
	}
	return 0;
}

static int record_command_buffers(struct vulkan_handler *vulkan_handler) {
	for (int i = 0; i < vulkan_handler->swapchain_image_count; ++i) {
		VkCommandBufferBeginInfo command_begin_info;
		command_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_begin_info.pNext = 0;
		command_begin_info.pInheritanceInfo = 0;
		command_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		if (vkBeginCommandBuffer(vulkan_handler->swapchain_command_buffers[i], &command_begin_info) != VK_SUCCESS) {
			return -1;
		}

		VkOffset2D render_area_offset;
		render_area_offset.x = 0;
		render_area_offset.y = 0;

		VkClearValue clear_value;
		clear_value.color.float32[0] = 0.0f;
		clear_value.color.float32[1] = 0.0f;
		clear_value.color.float32[2] = 0.0f;
		clear_value.color.float32[3] = 1.0f;
		clear_value.depthStencil.depth = 0.0f;
		clear_value.depthStencil.stencil = 0;

		VkRenderPassBeginInfo render_pass_begin_info;
		render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_begin_info.pNext = 0;
		render_pass_begin_info.renderPass = vulkan_handler->render_pass;
		render_pass_begin_info.framebuffer = vulkan_handler->swapchain_framebuffers[i];
		render_pass_begin_info.renderArea.offset = render_area_offset;
		render_pass_begin_info.renderArea.extent = vulkan_handler->swapchain_extent;
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clear_value;

		vkCmdBeginRenderPass(vulkan_handler->swapchain_command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(vulkan_handler->swapchain_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan_handler->graphics_pipeline);
		vkCmdDraw(vulkan_handler->swapchain_command_buffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(vulkan_handler->swapchain_command_buffers[i]);

		if (vkEndCommandBuffer(vulkan_handler->swapchain_command_buffers[i]) != VK_SUCCESS) {
			return -2;
		}
	}
	return 0;
}

static int try_recreate_swapchain(struct glfw_handler *this) {
	int width, height;
	glfwGetFramebufferSize(this->window, &width, &height);

	if (width == 0 || height == 0) {
		return 1;
	}

	vkDeviceWaitIdle(this->vulkan_handler.device);
	vulkan_handler__free_command_buffers_to_swapchain(&this->vulkan_handler);

	if (vulkan_handler__try_create_swapchain_to_command_buffers(&this->vulkan_handler, width, height) < 0) {
		return -1;
	}

	if (record_command_buffers(&this->vulkan_handler) < 0) {
		return -2;
	}
	return 0;
}

#define MAX_UINT64 0xFFFFFFFFFFFFFFFF
static int draw_frame(struct glfw_handler *this) {
	this->resources_index = (this->resources_index + 1) % FRAME_RESOURCES;

	vkWaitForFences(this->vulkan_handler.device, 1, this->resource_fences + this->resources_index, VK_TRUE, MAX_UINT64);
	vkResetFences(this->vulkan_handler.device, 1, this->resource_fences + this->resources_index);

	uint32_t image_index;
	while (1) {
		VkResult vk_result = vkAcquireNextImageKHR(this->vulkan_handler.device, this->vulkan_handler.swapchain, MAX_UINT64,
												this->image_available_semaphores[this->resources_index], VK_NULL_HANDLE,
												&image_index);

		if (vk_result == VK_SUCCESS || vk_result == VK_SUBOPTIMAL_KHR) {
			break;
		} else if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
			int result = try_recreate_swapchain(this);
			if (result < 0) {
				return -1;
			} else if (result == 1) {
				return 0;
			}
		} else {
			return -2;
		}
	}
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = 0;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = this->image_available_semaphores + this->resources_index;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = this->vulkan_handler.swapchain_command_buffers + image_index;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = this->render_finished_semaphores + this->resources_index;

	if (vkQueueSubmit(this->vulkan_handler.queue, 1, &submit_info, this->resource_fences[this->resources_index]) != VK_SUCCESS) {
		return -3;
	}

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = this->render_finished_semaphores + this->resources_index;
	present_info.pImageIndices = &image_index;
	present_info.pResults = 0;
	present_info.pNext = 0;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &this->vulkan_handler.swapchain;

	VkResult vk_result = vkQueuePresentKHR(this->vulkan_handler.queue, &present_info);
	if (vk_result != VK_SUCCESS) {
		if (vk_result == VK_SUBOPTIMAL_KHR || vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
			int result = try_recreate_swapchain(this);
			if (result < 0) {
				return -4;
			}
		} else {
			return -5;
		}
	}
	return 0;
}

static void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
	struct glfw_handler *this = glfwGetWindowUserPointer(window);
	this->should_recreate_swapchain = 1;
}

int glfw_handler__try_init(struct glfw_handler *this, int width, int height, char *title, int fullscreen) {
	this->resources_index = 0;
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWmonitor *monitor = NULL;
	if (fullscreen) {
		monitor = glfwGetPrimaryMonitor();
	}
	this->window = glfwCreateWindow(width, height, title, monitor, 0);
	glfwSetWindowUserPointer(this->window, this);
	glfwSetFramebufferSizeCallback(this->window, framebuffer_size_callback);

	uint32_t extension_count;
	const char **extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	struct vulkan_handler__create_surface callback;
	callback.create_window_surface = create_window_surface;
	callback.user_data = this;
	int result = vulkan_handler__try_init(&this->vulkan_handler, extensions, extension_count, callback);
	if (result < 0) {
		free_glfw(this);
		return -1;
	}

	result = vulkan_handler__try_create_swapchain_to_command_buffers(&this->vulkan_handler, width, height);
	if (result < 0) {
	    vulkan_handler__free_command_buffers_to_swapchain(&this->vulkan_handler);
	    vulkan_handler__free(&this->vulkan_handler);
	    free_glfw(this);
	    return -2;
	}

	result = create_semaphores_and_fences(this);
	if (result < 0) {
		vulkan_handler__free(&this->vulkan_handler);
		free_glfw(this);
		return -3;
	}
	return 0;
}

void glfw_handler__free(struct glfw_handler *this) {
	free_semaphores_and_fences(this);
	vulkan_handler__free(&this->vulkan_handler);
	free_glfw(this);
}

int glfw_handler__try_run(struct glfw_handler *this) {
	if (record_command_buffers(&this->vulkan_handler) < 0) {
		return -1;
	}
	double prev_time = glfwGetTime();
	long frames = 0;
	while (!glfwWindowShouldClose(this->window)) {
		++frames;
		double delta_time = glfwGetTime() - prev_time;
		if (delta_time >= 1.0) {
			printf("%f FPS\n", frames / delta_time);
			frames = 0;
			prev_time = glfwGetTime();
		}

		glfwPollEvents();
		if (this->should_recreate_swapchain) {
			int result = try_recreate_swapchain(this);
			if (result < 0) {
				return -2;
			} else if (result == 1) {
				continue;
			}
			this->should_recreate_swapchain = 0;
		}

		int result = draw_frame(this);
		if (result < 0) {
			return -3;
		}
	}
	vkDeviceWaitIdle(this->vulkan_handler.device);
	return 0;
}
