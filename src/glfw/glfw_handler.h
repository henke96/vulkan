#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../vulkan/vulkan_handler.h"

struct glfw_handler {
    struct vulkan_handler vulkan_handler;
    GLFWwindow *window;
};

int glfw_handler__try_init(struct glfw_handler *this, int width, int height, char *title);
void glfw_handler__free(struct glfw_handler *this);
void glfw_handler__run(struct glfw_handler *this);