#include <stdio.h>
#include "glfw_handler.h"

static VkResult create_window_surface(void *surface_creator, VkInstance instance, VkSurfaceKHR *surface) {
	struct glfw_handler *this = (struct glfw_handler *) surface_creator;
	return glfwCreateWindowSurface(instance, this->window, 0, surface);
}

static void free_glfw(struct glfw_handler *this) {
	glfwDestroyWindow(this->window);
	glfwTerminate();
}

static void free_semaphores(struct glfw_handler *this) {
	vkDestroySemaphore(this->vulkan_handler.device, this->render_finished, 0);
	vkDestroySemaphore(this->vulkan_handler.device, this->image_available, 0);
}

static int create_semaphores(struct glfw_handler *this) {
	VkSemaphoreCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	create_info.pNext = 0;
	create_info.flags = 0;

	if (vkCreateSemaphore(this->vulkan_handler.device, &create_info, 0, &this->image_available) != VK_SUCCESS) {
		return -1;
	}
	if (vkCreateSemaphore(this->vulkan_handler.device, &create_info, 0, &this->render_finished) != VK_SUCCESS) {
		vkDestroySemaphore(this->vulkan_handler.device, this->image_available, 0);
		return -2;
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

static int draw_frame(struct glfw_handler *this) {
	uint32_t image_index;

	vkAcquireNextImageKHR(this->vulkan_handler.device, this->vulkan_handler.swapchain, 0xFFFFFFFFFFFFFFFF, this->image_available, VK_NULL_HANDLE, &image_index);

	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;//VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = 0;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &this->image_available;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = (this->vulkan_handler.swapchain_command_buffers + image_index);
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &this->render_finished;

	if (vkQueueSubmit(this->vulkan_handler.queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
		return -1;
	}

	VkPresentInfoKHR present_info;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &this->render_finished;
	present_info.pImageIndices = &image_index;
	present_info.pResults = 0;
	present_info.pNext = 0;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &this->vulkan_handler.swapchain;

	vkQueuePresentKHR(this->vulkan_handler.queue, &present_info);
	return 0;
}

int glfw_handler__try_init(struct glfw_handler *this, int width, int height, char *title, int fullscreen) {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWmonitor *monitor = NULL;
	if (fullscreen) {
		monitor = glfwGetPrimaryMonitor();
	}
	this->window = glfwCreateWindow(width, height, title, monitor, 0);
	uint32_t extension_count;
	const char **extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	int result = vulkan_handler__try_init(&this->vulkan_handler, extensions, extension_count, width, height, create_window_surface, this);
	if (result < 0) {
		free_glfw(this);
		return -1;
	}

	result = create_semaphores(this);
	if (result < 0) {
		vulkan_handler__free(&this->vulkan_handler);
		free_glfw(this);
	}
	return 0;
}

void glfw_handler__free(struct glfw_handler *this) {
	free_semaphores(this);
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
		glfwPollEvents();
		if (draw_frame(this) < 0) {
			return -2;
		}
		++frames;
		double delta_time = glfwGetTime() - prev_time;
		if (delta_time >= 1.0) {
			printf("%f FPS\n", frames / delta_time);
			frames = 0;
            prev_time = glfwGetTime();
		}
	}
	vkDeviceWaitIdle(this->vulkan_handler.device);
	return 0;
}
