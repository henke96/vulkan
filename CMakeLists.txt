cmake_minimum_required(VERSION 3.12)
project(vulkan_base C)

set(CMAKE_VERBOSE_MAKEFILE off)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "-Wall -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DVULKAN_BASE_VALIDATION")
set(CMAKE_C_FLAGS_RELEASE "-O3")

add_executable(vulkan_base src/main.c src/vulkan/vulkan_base.c src/vulkan/vulkan_base.h src/glfw/glfw_handler.c src/glfw/glfw_handler.h src/file/file.c src/file/file.h src/vulkan/vulkan_swapchain.c src/vulkan/vulkan_swapchain.h)

find_package(Vulkan)
message(STATUS "${Vulkan_LIBRARIES}")
find_package(glfw3)
message(STATUS "${glfw3_LIBRARIES}")
target_include_directories(vulkan_base PRIVATE "${Vulkan_INCLUDE_DIRS}" "${glfw3_INCLUDE_DIRS}")
target_link_libraries(vulkan_base "${Vulkan_LIBRARIES}" glfw)

FILE(COPY shaders/frag.spv DESTINATION "${CMAKE_BINARY_DIR}/shaders")
FILE(COPY shaders/vert.spv DESTINATION "${CMAKE_BINARY_DIR}/shaders")