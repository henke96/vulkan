cmake_minimum_required(VERSION 3.12)
project(vulkan_base C)

set(CMAKE_VERBOSE_MAKEFILE off)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS "-Wall -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DVULKAN_BASE_VALIDATION")
set(CMAKE_C_FLAGS_RELEASE "-O3")

add_executable(vulkan_base src/main.c src/vulkan/vulkan_base.c src/vulkan/vulkan_base.h src/glfw/glfw_handler.c src/glfw/glfw_handler.h src/file/file.c src/file/file.h src/vulkan/vulkan_swapchain.c src/vulkan/vulkan_swapchain.h)

find_package(vulkan)
message(STATUS "${Vulkan_LIBRARIES}")
target_include_directories(vulkan_base PRIVATE "${Vulkan_INCLUDE_DIRS}" "C:/glfw-3.2.1.bin.WIN64/include")
target_link_libraries(vulkan_base "${Vulkan_LIBRARIES}" "C:/glfw-3.2.1.bin.WIN64/lib-mingw-w64/libglfw3.a")

FILE(COPY shaders/frag.spv DESTINATION "${CMAKE_BINARY_DIR}/shaders")
FILE(COPY shaders/vert.spv DESTINATION "${CMAKE_BINARY_DIR}/shaders")