#include "glfw/glfw_handler.h"
#include <stdio.h>

int main(int argc, char **argv) {
	struct glfw_handler glfw_handler;
	int result = glfw_handler__try_init(&glfw_handler, 800, 600, "Vulkan", 0);
	if (result < 0) {
		return -1;
	}
	result = glfw_handler__try_run(&glfw_handler);
	if (result < 0) {
		glfw_handler__free(&glfw_handler);
		return -2;
	}
	glfw_handler__free(&glfw_handler);
	return 0;
}