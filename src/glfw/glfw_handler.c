#include <stdio.h>
#include "glfw_handler.h"

#define MAX_UINT64 0xFFFFFFFFFFFFFFFF

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
		vkDestroySemaphore(this->vulkan_base.device, this->render_finished_semaphores[i], 0);
		vkDestroySemaphore(this->vulkan_base.device, this->image_available_semaphores[i], 0);
		vkDestroyFence(this->vulkan_base.device, this->resource_fences[i], 0);
	}
}

static void free_semaphores_and_fences_below(struct glfw_handler *this, int i) {
	for (--i;i >= 0; --i) {
		vkDestroySemaphore(this->vulkan_base.device, this->render_finished_semaphores[i], 0);
		vkDestroySemaphore(this->vulkan_base.device, this->image_available_semaphores[i], 0);
		vkDestroyFence(this->vulkan_base.device, this->resource_fences[i], 0);
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
		if (vkCreateSemaphore(this->vulkan_base.device, &semaphore_create_info, 0, this->image_available_semaphores + i) != VK_SUCCESS) {
			free_semaphores_and_fences_below(this, i);
			return -1;
		}

		if (vkCreateSemaphore(this->vulkan_base.device, &semaphore_create_info, 0, this->render_finished_semaphores + i) != VK_SUCCESS) {
			vkDestroySemaphore(this->vulkan_base.device, this->image_available_semaphores[i], 0);
			free_semaphores_and_fences_below(this, i);
			return -2;
		}

		if (vkCreateFence(this->vulkan_base.device, &fence_create_info, 0, this->resource_fences + i) != VK_SUCCESS) {
			vkDestroySemaphore(this->vulkan_base.device, this->image_available_semaphores[i], 0);
			vkDestroySemaphore(this->vulkan_base.device, this->render_finished_semaphores[i], 0);
			free_semaphores_and_fences_below(this, i);
			return -3;
		}
	}
	return 0;
}

static int try_record_command_buffers(struct glfw_handler *this) {
	for (int i = 0; i < this->vulkan_swapchain.image_count; ++i) {
		VkCommandBufferBeginInfo command_begin_info;
		command_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_begin_info.pNext = 0;
		command_begin_info.pInheritanceInfo = 0;
		command_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		if (vkBeginCommandBuffer(this->vulkan_swapchain.command_buffers[i], &command_begin_info) != VK_SUCCESS) {
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
		render_pass_begin_info.renderPass = this->vulkan_swapchain.render_pass;
		render_pass_begin_info.framebuffer = this->vulkan_swapchain.framebuffers[i];
		render_pass_begin_info.renderArea.offset = render_area_offset;
		render_pass_begin_info.renderArea.extent = this->vulkan_swapchain.extent;
		render_pass_begin_info.clearValueCount = 1;
		render_pass_begin_info.pClearValues = &clear_value;

		vkCmdBeginRenderPass(this->vulkan_swapchain.command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(this->vulkan_swapchain.command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->vulkan_swapchain.graphics_pipeline);
		vkCmdDraw(this->vulkan_swapchain.command_buffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(this->vulkan_swapchain.command_buffers[i]);

		if (vkEndCommandBuffer(this->vulkan_swapchain.command_buffers[i]) != VK_SUCCESS) {
			return -2;
		}
	}
	return 0;
}

enum try_recreate_swapchain {
    TRY_RECREATE_SWAPCHAIN__NO_AREA = 1
}
static try_recreate_swapchain(struct glfw_handler *this) {
	int width, height;
	glfwGetFramebufferSize(this->window, &width, &height);

	if (width == 0 || height == 0) {
		return TRY_RECREATE_SWAPCHAIN__NO_AREA;
	}

	vkDeviceWaitIdle(this->vulkan_base.device);
	vulkan_swapchain__free_swapchain(&this->vulkan_swapchain);

	if (vulkan_swapchain__try_init_swapchain(&this->vulkan_swapchain, width, height) < 0) {
		return -1;
	}

	if (try_record_command_buffers(this) < 0) {
		return -2;
	}
	return 0;
}

static int draw_frame(struct glfw_handler *this) {
	this->resources_index = (this->resources_index + 1) % FRAME_RESOURCES;

	vkWaitForFences(this->vulkan_base.device, 1, this->resource_fences + this->resources_index, VK_TRUE, MAX_UINT64);
	vkResetFences(this->vulkan_base.device, 1, this->resource_fences + this->resources_index);

	uint32_t image_index;
	while (1) {
		VkResult vk_result = vkAcquireNextImageKHR(this->vulkan_base.device, this->vulkan_swapchain.swapchain, MAX_UINT64,
												this->image_available_semaphores[this->resources_index], VK_NULL_HANDLE,
												&image_index);

		if (vk_result == VK_SUCCESS || vk_result == VK_SUBOPTIMAL_KHR) {
			break;
		} else if (vk_result == VK_ERROR_OUT_OF_DATE_KHR) {
			int result = try_recreate_swapchain(this);
			if (result < 0) {
				return -1;
			} else if (result == TRY_RECREATE_SWAPCHAIN__NO_AREA) {
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
	submit_info.pCommandBuffers = this->vulkan_swapchain.command_buffers + image_index;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = this->render_finished_semaphores + this->resources_index;

	if (vkQueueSubmit(this->vulkan_base.queue, 1, &submit_info, this->resource_fences[this->resources_index]) != VK_SUCCESS) {
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
	present_info.pSwapchains = &this->vulkan_swapchain.swapchain;

	VkResult vk_result = vkQueuePresentKHR(this->vulkan_base.queue, &present_info);
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

	GLFWmonitor *monitor = NULL;
	if (fullscreen) {
		monitor = glfwGetPrimaryMonitor();
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	} else {
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	}
	this->window = glfwCreateWindow(width, height, title, monitor, 0);
	glfwSetWindowUserPointer(this->window, this);
	glfwSetFramebufferSizeCallback(this->window, framebuffer_size_callback);

	uint32_t extension_count;
	const char **extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	struct vulkan_base__create_surface callback;
	callback.create_window_surface = create_window_surface;
	callback.user_data = this;
	int result = vulkan_base__try_init(&this->vulkan_base, extensions, extension_count, callback);
	if (result < 0) {
		free_glfw(this);
		return -1;
	}

	result = vulkan_swapchain__try_init(&this->vulkan_swapchain, &this->vulkan_base);
	if (result < 0) {
		vulkan_base__free(&this->vulkan_base);
		free_glfw(this);
		return -2;
	}

	result = vulkan_swapchain__try_init_swapchain(&this->vulkan_swapchain, width, height);
	if (result < 0) {
		vulkan_swapchain__free(&this->vulkan_swapchain);
	    vulkan_base__free(&this->vulkan_base);
	    free_glfw(this);
	    return -3;
	}

	result = create_semaphores_and_fences(this);
	if (result < 0) {
		vulkan_swapchain__free_swapchain(&this->vulkan_swapchain);
		vulkan_swapchain__free(&this->vulkan_swapchain);
		vulkan_base__free(&this->vulkan_base);
		free_glfw(this);
		return -4;
	}

	result = vulkan_texture__try_init(&this->test_texture, &this->vulkan_base, width, height);
	if (result < 0) {
        free_semaphores_and_fences(this);
        vulkan_swapchain__free_swapchain(&this->vulkan_swapchain);
        vulkan_swapchain__free(&this->vulkan_swapchain);
        vulkan_base__free(&this->vulkan_base);
        free_glfw(this);
	}
	return 0;
}

void glfw_handler__free(struct glfw_handler *this) {
    vulkan_texture__free(&this->test_texture);
	free_semaphores_and_fences(this);
	vulkan_swapchain__free_swapchain(&this->vulkan_swapchain);
	vulkan_swapchain__free(&this->vulkan_swapchain);
	vulkan_base__free(&this->vulkan_base);
	free_glfw(this);
}

int glfw_handler__try_run(struct glfw_handler *this) {
	if (try_record_command_buffers(this) < 0) {
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
			} else if (result == TRY_RECREATE_SWAPCHAIN__NO_AREA) {
				continue;
			}
			this->should_recreate_swapchain = 0;
		}

		int result = draw_frame(this);
		if (result < 0) {
			return -3;
		}
	}
	vkDeviceWaitIdle(this->vulkan_base.device);
	return 0;
}
