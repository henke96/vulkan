#include "glfw/glfw_handler.h"
#include <stdio.h>

int main(int argc, char **argv) {
	struct glfw_handler handler;
	int result = glfw_handler__try_init(&handler, 1920, 1080, "Vulkan", 1);
	if (result < 0) {
		return -1;
	}
	result = glfw_handler__try_run(&handler);
	if (result < 0) {
        glfw_handler__free(&handler);
		return -2;
	}
    glfw_handler__free(&handler);
	return 0;
}