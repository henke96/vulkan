#include "glfw/glfw_handler.h"
#include <stdio.h>

int main(int argc, char **argv) {
	struct glfw_handler glfw_handler;
	int result = glfw_handler__try_init(&glfw_handler, 800, 600, "Vulkan");
	if (result < 0) {
		return result;
	}
	glfw_handler__run(&glfw_handler);
	glfw_handler__free(&glfw_handler);
	return 0;
}