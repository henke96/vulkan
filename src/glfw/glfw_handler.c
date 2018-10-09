#include "glfw_handler.h"

VkResult create_window_surface(void *surface_creator, VkInstance instance, VkSurfaceKHR *surface) {
	struct glfw_handler *this = (struct glfw_handler *) surface_creator;
	return glfwCreateWindowSurface(instance, this->window, 0, surface);
}

static void free_glfw(struct glfw_handler *this) {
	glfwDestroyWindow(this->window);
	glfwTerminate();
}

int glfw_handler__try_init(struct glfw_handler *this, int width, int height, char *title) {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	this->window = glfwCreateWindow(width, height, title, 0, 0);

	uint32_t extension_count;
	const char **extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	int result = vulkan_handler__try_init(&this->vulkan_handler, extensions, extension_count, width, height, create_window_surface, this);
	if (result < 0) {
		free_glfw(this);
		return -1;
	}
	return 0;
}

void glfw_handler__free(struct glfw_handler *this) {
	vulkan_handler__free(&this->vulkan_handler);
	free_glfw(this);
}

void glfw_handler__run(struct glfw_handler *this) {
	while (!glfwWindowShouldClose(this->window)) {
		glfwPollEvents();
	}
}
