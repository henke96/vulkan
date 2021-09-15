#include <stdio.h>
#include <stdlib.h>
#include "glfw_handler2.h"
#include <unistd.h>

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
		vkDestroySemaphore(this->base.device, this->render_finished_semaphores[i], 0);
		vkDestroySemaphore(this->base.device, this->image_available_semaphores[i], 0);
		vkDestroyFence(this->base.device, this->resource_fences[i], 0);
	}
}

static void free_semaphores_and_fences_below_index(struct glfw_handler *this, int i) {
	for (--i;i >= 0; --i) {
		vkDestroySemaphore(this->base.device, this->render_finished_semaphores[i], 0);
		vkDestroySemaphore(this->base.device, this->image_available_semaphores[i], 0);
		vkDestroyFence(this->base.device, this->resource_fences[i], 0);
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
		if (vkCreateSemaphore(this->base.device, &semaphore_create_info, 0, this->image_available_semaphores + i) != VK_SUCCESS) {
            free_semaphores_and_fences_below_index(this, i);
			return -1;
		}

		if (vkCreateSemaphore(this->base.device, &semaphore_create_info, 0, this->render_finished_semaphores + i) != VK_SUCCESS) {
			vkDestroySemaphore(this->base.device, this->image_available_semaphores[i], 0);
            free_semaphores_and_fences_below_index(this, i);
			return -2;
		}

		if (vkCreateFence(this->base.device, &fence_create_info, 0, this->resource_fences + i) != VK_SUCCESS) {
			vkDestroySemaphore(this->base.device, this->image_available_semaphores[i], 0);
			vkDestroySemaphore(this->base.device, this->render_finished_semaphores[i], 0);
            free_semaphores_and_fences_below_index(this, i);
			return -3;
		}
	}
	return 0;
}

static int try_record_command_buffers(struct glfw_handler *this) {
	for (int i = 0; i < this->swapchain.image_count; ++i) {
		VkCommandBufferBeginInfo command_begin_info;
		command_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_begin_info.pNext = 0;
		command_begin_info.pInheritanceInfo = 0;
		command_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		if (vkBeginCommandBuffer(this->swapchain.command_buffers[i], &command_begin_info) != VK_SUCCESS) {
			return -1;
		}

		VkImageMemoryBarrier barrier1;
		barrier1.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier1.pNext = 0;
		barrier1.srcAccessMask = 0;
		barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier1.image = this->swapchain.images[i];
		barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier1.subresourceRange.baseMipLevel = 0;
		barrier1.subresourceRange.levelCount = 1;
		barrier1.subresourceRange.baseArrayLayer = 0;
		barrier1.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(this->swapchain.command_buffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &barrier1);

		VkBufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = (VkOffset3D) {0, 0, 0};
        region.imageExtent = (VkExtent3D) {this->swapchain.extent.width, this->swapchain.extent.height, 1};

		vkCmdCopyBufferToImage(this->swapchain.command_buffers[i], this->test_texture.staging_buffer, this->swapchain.images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageMemoryBarrier barrier2;
		barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier2.pNext = 0;
		barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier2.dstAccessMask = 0; // TODO: is this right?
		barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier2.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier2.image = this->swapchain.images[i];
		barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier2.subresourceRange.baseMipLevel = 0;
		barrier2.subresourceRange.levelCount = 1;
		barrier2.subresourceRange.baseArrayLayer = 0;
		barrier2.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(this->swapchain.command_buffers[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, 0, 0, 0, 1, &barrier2);

		if (vkEndCommandBuffer(this->swapchain.command_buffers[i]) != VK_SUCCESS) {
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

	vkDeviceWaitIdle(this->base.device);
    vk_swapchain__free_swapchain(&this->swapchain);

	if (vk_swapchain__try_init_swapchain(&this->swapchain, width, height, VK_IMAGE_USAGE_TRANSFER_DST_BIT) < 0) {
		return -1;
	}

	if (try_record_command_buffers(this) < 0) {
		return -2;
	}
	return 0;
}

static int draw_frame(struct glfw_handler *this, uint32_t *data) {
	this->resources_index = (this->resources_index + 1) % FRAME_RESOURCES;

	int omg1 = rand() % 255;
	int omg2 = rand() % 255;
	int omg3 = rand() % 255;


	for (int i = 0; i < this->test_texture.size/4; ++i) {
		data[i] = (omg1 << 16) | (omg2 << 8) | omg3;
	}

	vkWaitForFences(this->base.device, 1, this->resource_fences + this->resources_index, VK_TRUE, MAX_UINT64);
	vkResetFences(this->base.device, 1, this->resource_fences + this->resources_index);

	uint32_t image_index;
	while (1) {
		VkResult vk_result = vkAcquireNextImageKHR(this->base.device, this->swapchain.swapchain, MAX_UINT64,
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
	VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	VkSubmitInfo submit_info;
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = 0;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = this->image_available_semaphores + this->resources_index;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = this->swapchain.command_buffers + image_index;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = this->render_finished_semaphores + this->resources_index;

	if (vkQueueSubmit(this->base.queue, 1, &submit_info, this->resource_fences[this->resources_index]) != VK_SUCCESS) {
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
	present_info.pSwapchains = &this->swapchain.swapchain;

	VkResult vk_result = vkQueuePresentKHR(this->base.queue, &present_info);
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
	printf("Size callback %d %d\n", width, height);
	this->should_recreate_swapchain = 1;
}

int glfw_handler__try_init(struct glfw_handler *this, int width, int height, char *title, int is_fullscreen) {
	this->resources_index = 0;
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	GLFWmonitor *monitor = NULL;
	if (is_fullscreen) {
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
	struct vk_base__create_surface callback;
	callback.create_window_surface = create_window_surface;
	callback.user_data = this;
	int result = vk_base__try_init(&this->base, extensions, extension_count, callback);
	if (result < 0) {
		free_glfw(this);
		return -1;
	}

	result = vk_swapchain__try_init(&this->swapchain, &this->base);
	if (result < 0) {
        vk_base__free(&this->base);
		free_glfw(this);
		return -2;
	}

	result = vk_swapchain__try_init_swapchain(&this->swapchain, width, height, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	if (result < 0) {
        vk_swapchain__free(&this->swapchain);
        vk_base__free(&this->base);
	    free_glfw(this);
	    return -3;
	}

	result = create_semaphores_and_fences(this);
	if (result < 0) {
        vk_swapchain__free_swapchain(&this->swapchain);
        vk_swapchain__free(&this->swapchain);
        vk_base__free(&this->base);
		free_glfw(this);
		return -4;
	}
	vkDeviceWaitIdle(this->base.device);
	result = vk_staging_buffer__try_init(&this->test_texture, &this->base, width, height);
	if (result < 0) {
        free_semaphores_and_fences(this);
        vk_swapchain__free_swapchain(&this->swapchain);
        vk_swapchain__free(&this->swapchain);
        vk_base__free(&this->base);
        free_glfw(this);
	}

	//TODO lol
	uint32_t *data = vk_staging_buffer__map(&this->test_texture);
	for (int i = 0; i < this->test_texture.size/4; ++i) {
		data[i] = (100 << 24) | (100 << 16) | (100 << 8) | (100 << 0);
	}
	vk_staging_buffer__unmap(&this->test_texture);
	vkDeviceWaitIdle(this->base.device);
	return 0;
}

void glfw_handler__free(struct glfw_handler *this) {
    vk_staging_buffer__free(&this->test_texture);
    free_semaphores_and_fences(this);
    vk_swapchain__free_swapchain(&this->swapchain);
    vk_swapchain__free(&this->swapchain);
    vk_base__free(&this->base);
    free_glfw(this);
}

int glfw_handler__try_run(struct glfw_handler *this) {
	uint32_t *data = vk_staging_buffer__map(&this->test_texture);
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

		int result = draw_frame(this, data);
		if (result < 0) {
			return -3;
		}
	}
	vk_staging_buffer__unmap(&this->test_texture);
	vkDeviceWaitIdle(this->base.device);
	return 0;
}
