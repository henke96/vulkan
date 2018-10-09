#include "vulkan_handler.h"
#include "../file/file.h"
#include <malloc.h>
#include <stdio.h>

#define SWAPCHAIN_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#define PIPELINE_SAMPLES VK_SAMPLE_COUNT_1_BIT

static int try_create_instance(struct vulkan_handler *this, const char **extensions, int extension_count) {
	VkApplicationInfo app_info;
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = 0;
	app_info.applicationVersion = 0;
	app_info.pEngineName = 0;
	app_info.engineVersion = 0;
	app_info.pNext = 0;
	app_info.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
#ifdef VULKAN_HANDLER_VALIDATION
	if (extension_count > 63) {
		return -1;
	}
	const char *new_extensions[64];
	int i = 0;
	for (; i < extension_count; ++i) {
		new_extensions[i] = extensions[i];
	}
	new_extensions[i] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	create_info.enabledExtensionCount = extension_count + 1;
	create_info.ppEnabledExtensionNames = new_extensions;

	const char *validation_layers[1] = {"VK_LAYER_LUNARG_standard_validation"};
	create_info.enabledLayerCount = 1;
	create_info.ppEnabledLayerNames = validation_layers;
#else
	create_info.enabledExtensionCount = extension_count;
	create_info.ppEnabledExtensionNames = extensions;
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = 0;
#endif
	create_info.pNext = 0;
	create_info.flags = 0;

	if (vkCreateInstance(&create_info, 0, &this->instance) != VK_SUCCESS) {
		return -2;
	}
	return 0;
}

#ifdef VULKAN_HANDLER_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
		void *pUserData
) {
	printf("Validation layer: %s\n", pCallbackData->pMessage);
	return VK_FALSE;
}
#endif

static int try_init_device(struct vulkan_handler *this) {
	int queue_family_index = -1;
	uint32_t device_count;
	vkEnumeratePhysicalDevices(this->instance, &device_count, 0);
	VkPhysicalDevice *devices = malloc(device_count * sizeof(*devices));
	if (devices == 0) {
		return -3;
	}
	vkEnumeratePhysicalDevices(this->instance, &device_count, devices);
	for (int i = 0; i < device_count; ++i) {
		VkPhysicalDevice current_device = devices[i];
		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(current_device, &queue_family_count, 0);
		VkQueueFamilyProperties *queue_family_propertiess = malloc(queue_family_count * sizeof(*queue_family_propertiess));
		if (queue_family_propertiess == 0) {
			return -4;
		}
		vkGetPhysicalDeviceQueueFamilyProperties(current_device, &queue_family_count, queue_family_propertiess);
		for (int j = 0; j < queue_family_count; ++j) {
			VkQueueFamilyProperties *queue_family_properties = queue_family_propertiess + j;

			VkBool32 present_support;
			vkGetPhysicalDeviceSurfaceSupportKHR(current_device, j, this->surface, &present_support);

			if (queue_family_properties->queueCount > 0 && queue_family_properties->queueFlags & VK_QUEUE_GRAPHICS_BIT && present_support) {
				queue_family_index = j;
				this->physical_device = current_device;
				free(queue_family_propertiess);
				goto device_search_done;
			}
		}
		free(queue_family_propertiess);
	}
	device_search_done:
	free(devices);
	if (queue_family_index == -1) {
		return -5;
	}

	VkDeviceQueueCreateInfo queue_create_info;
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.flags = 0;
	queue_create_info.pNext = 0;
	const float queue_priority = 1.0f;
	queue_create_info.pQueuePriorities = &queue_priority;
	queue_create_info.queueCount = 1;
	queue_create_info.queueFamilyIndex = queue_family_index;

	VkPhysicalDeviceFeatures device_features = {0};

	VkDeviceCreateInfo device_create_info;
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.pQueueCreateInfos = &queue_create_info;
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.pEnabledFeatures = &device_features;
	const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	device_create_info.enabledExtensionCount = 1;
	device_create_info.ppEnabledExtensionNames = device_extensions;
	device_create_info.flags = 0;
	device_create_info.pNext = 0;
#ifdef VULKAN_HANDLER_VALIDATION
	const char *validation_layers[1] = { "VK_LAYER_LUNARG_standard_validation" };
	device_create_info.enabledLayerCount = 1;
	device_create_info.ppEnabledLayerNames = validation_layers;
#else
	device_create_info.enabledLayerCount = 0;
	device_create_info.ppEnabledLayerNames = 0;
#endif

	if (vkCreateDevice(this->physical_device, &device_create_info, 0, &this->device) != VK_SUCCESS) {
		return -1;
	}
	vkGetDeviceQueue(this->device, queue_family_index, 0, &this->queue);
	return 0;
}

static int try_create_swapchain(struct vulkan_handler *this) {
	this->swapchain_extent.width = this->window_width;
	this->swapchain_extent.height = this->window_height;
	VkSwapchainCreateInfoKHR create_info;
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = this->surface;
	create_info.minImageCount = 1;
	create_info.pNext = 0;
	create_info.flags = 0;
	create_info.imageFormat = SWAPCHAIN_FORMAT;
	create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	create_info.imageExtent = this->swapchain_extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = 0;
	create_info.pQueueFamilyIndices = 0;
	create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(this->device, &create_info, 0, &this->swapchain) != VK_SUCCESS) {
		return -1;
	}

	vkGetSwapchainImagesKHR(this->device, this->swapchain, &this->swapchain_image_count, 0);
	this->swapchain_images = (VkImage *) malloc(this->swapchain_image_count*sizeof(VkImage));
	if (!this->swapchain_images) {
		return -2;
	}
	vkGetSwapchainImagesKHR(this->device, this->swapchain, &this->swapchain_image_count, this->swapchain_images);
	return 0;
}

static int try_create_image_views(struct vulkan_handler *this) {
	this->swapchain_imageviews = (VkImageView *) malloc(this->swapchain_image_count*sizeof(VkImageView));
	if (!this->swapchain_imageviews) {
		return -1;
	}

	for (int i = 0; i < this->swapchain_image_count; ++i) {
		VkImageViewCreateInfo create_info;
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = this->swapchain_images[i];
		create_info.flags = 0;
		create_info.pNext = 0;
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = SWAPCHAIN_FORMAT;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;

		if (vkCreateImageView(this->device, &create_info, 0, this->swapchain_imageviews + i) != VK_SUCCESS) {
			return -2;
		}
	}
	return 0;
}

static int try_create_shader_module(struct vulkan_handler *this, char *code, long length, VkShaderModule *out_shader_module) {
	VkShaderModuleCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.pNext = 0;
	create_info.flags = 0;
	create_info.pCode = (uint32_t *) code;
	create_info.codeSize = (size_t) length;

	if (vkCreateShaderModule(this->device, &create_info, 0, out_shader_module) != VK_SUCCESS) {
		return -1;
	}
	return 0;
}

static int try_create_render_pass(struct vulkan_handler *this) {
	VkAttachmentDescription attachment_description;
	attachment_description.format = SWAPCHAIN_FORMAT;
	attachment_description.samples = PIPELINE_SAMPLES;
	attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; //TODO modify these
	attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachment_description.flags = 0;

	VkAttachmentReference attachment_reference;
	attachment_reference.attachment = 0;
	attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass_description;
	subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_description.colorAttachmentCount = 1;
	subpass_description.pColorAttachments = &attachment_reference;
	subpass_description.flags = 0;
	subpass_description.inputAttachmentCount = 0;
	subpass_description.pInputAttachments = 0;
	subpass_description.pResolveAttachments = 0;
	subpass_description.preserveAttachmentCount = 0;
	subpass_description.pPreserveAttachments = 0;
	subpass_description.pDepthStencilAttachment = 0;

	VkRenderPassCreateInfo create_info;
	create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	create_info.flags = 0;
	create_info.pNext = 0;
	create_info.attachmentCount = 1;
	create_info.pAttachments = &attachment_description;
	create_info.subpassCount = 1;
	create_info.pSubpasses = &subpass_description;
	create_info.dependencyCount = 0;
	create_info.pDependencies = 0;

	if (vkCreateRenderPass(this->device, &create_info, 0, &this->render_pass) != VK_SUCCESS) {
		return -1;
	}
	return 0;
}

static int try_create_graphics_pipeline(struct vulkan_handler *this) {
	struct file__read vert_shader_code = file__read("shaders/vert.spv");
	if (vert_shader_code.result < 0) {
		return -1;
	}
	struct file__read frag_shader_code = file__read("shaders/frag.spv");
	if (frag_shader_code.result < 0) {
		return -2;
	}
	VkShaderModule vert_shader_module;
	if (try_create_shader_module(this, vert_shader_code.malloc_bytes, vert_shader_code.length, &vert_shader_module) < 0) {
		return -3;
	}
	VkShaderModule frag_shader_module;
	if (try_create_shader_module(this, frag_shader_code.malloc_bytes, frag_shader_code.length, &frag_shader_module) < 0) {
		return -4;
	}

	VkPipelineShaderStageCreateInfo vert_shader_create_info;
	vert_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_create_info.pNext = 0;
	vert_shader_create_info.flags = 0;
	vert_shader_create_info.module = vert_shader_module;
	vert_shader_create_info.pName = "main";
	vert_shader_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_create_info.pSpecializationInfo = 0;

	VkPipelineShaderStageCreateInfo frag_shader_create_info;
	frag_shader_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_create_info.pNext = 0;
	frag_shader_create_info.flags = 0;
	frag_shader_create_info.module = frag_shader_module;
	frag_shader_create_info.pName = "main";
	frag_shader_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_create_info.pSpecializationInfo = 0;

	VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_create_info, frag_shader_create_info};

	VkPipelineVertexInputStateCreateInfo vertex_input_create_info;
	vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_create_info.flags = 0;
	vertex_input_create_info.pNext = 0;
	vertex_input_create_info.vertexBindingDescriptionCount = 0;
	vertex_input_create_info.pVertexBindingDescriptions = 0;
	vertex_input_create_info.vertexAttributeDescriptionCount = 0;
	vertex_input_create_info.pVertexAttributeDescriptions = 0;

	VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_create_info;
	pipeline_input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipeline_input_assembly_create_info.pNext = 0;
	pipeline_input_assembly_create_info.flags = 0;
	pipeline_input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipeline_input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) this->swapchain_extent.width;
	viewport.height = (float) this->swapchain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkOffset2D scissor_offset;
	scissor_offset.x = 0;
	scissor_offset.y = 0;

	VkRect2D scissor;
	scissor.extent = this->swapchain_extent;
	scissor.offset = scissor_offset;

	VkPipelineViewportStateCreateInfo viewport_state_create_info;
	viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state_create_info.pNext = 0;
	viewport_state_create_info.flags = 0;
	viewport_state_create_info.viewportCount = 1;
	viewport_state_create_info.pViewports = &viewport;
	viewport_state_create_info.scissorCount = 1;
	viewport_state_create_info.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer_state_create_info;
	rasterizer_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_state_create_info.flags = 0;
	rasterizer_state_create_info.pNext = 0;
	rasterizer_state_create_info.depthClampEnable = VK_FALSE;
	rasterizer_state_create_info.rasterizerDiscardEnable = VK_FALSE;
	rasterizer_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_state_create_info.lineWidth = 1.0f;
	rasterizer_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer_state_create_info.depthBiasEnable = VK_FALSE;
	rasterizer_state_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_state_create_info.depthBiasClamp = 0.0f;
	rasterizer_state_create_info.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisample_state_create_info;
	multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state_create_info.pNext = 0;
	multisample_state_create_info.flags = 0;
	multisample_state_create_info.sampleShadingEnable = VK_FALSE;
	multisample_state_create_info.rasterizationSamples = PIPELINE_SAMPLES;
	multisample_state_create_info.minSampleShading = 1.0f;
	multisample_state_create_info.pSampleMask = 0;
	multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
	multisample_state_create_info.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_attachment_state;
	color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment_state.blendEnable = VK_FALSE;
	color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo color_blend_state_create_info;
	color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_state_create_info.logicOpEnable = VK_FALSE;
	color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
	color_blend_state_create_info.attachmentCount = 1;
	color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
	color_blend_state_create_info.blendConstants[0] = 0.0f;
	color_blend_state_create_info.blendConstants[1] = 0.0f;
	color_blend_state_create_info.blendConstants[2] = 0.0f;
	color_blend_state_create_info.blendConstants[3] = 0.0f;
	color_blend_state_create_info.flags = 0;
	color_blend_state_create_info.pNext = 0;

	VkPipelineLayoutCreateInfo pipeline_layout_create_info;
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = 0;
	pipeline_layout_create_info.pSetLayouts = 0;
	pipeline_layout_create_info.pushConstantRangeCount = 0;
	pipeline_layout_create_info.pPushConstantRanges = 0;
	pipeline_layout_create_info.pNext = 0;
	pipeline_layout_create_info.flags = 0;

	if (vkCreatePipelineLayout(this->device, &pipeline_layout_create_info, 0, &this->pipeline_layout) != VK_SUCCESS) {
		return -5;
	}

	VkGraphicsPipelineCreateInfo pipeline_create_info;
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.pNext = 0;
	pipeline_create_info.flags = 0;
	pipeline_create_info.stageCount = 2;
	pipeline_create_info.pStages = shader_stages;
	pipeline_create_info.pVertexInputState = &vertex_input_create_info;
	pipeline_create_info.pInputAssemblyState = &pipeline_input_assembly_create_info;
	pipeline_create_info.pViewportState = &viewport_state_create_info;
	pipeline_create_info.pRasterizationState = &rasterizer_state_create_info;
	pipeline_create_info.pMultisampleState = &multisample_state_create_info;
	pipeline_create_info.pDepthStencilState = 0;
	pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
	pipeline_create_info.pDynamicState = 0;
	pipeline_create_info.layout = this->pipeline_layout;
	pipeline_create_info.renderPass = this->render_pass;
	pipeline_create_info.subpass = 0;
	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.basePipelineIndex = -1;
	pipeline_create_info.pTessellationState = 0;

	if (vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipeline_create_info, 0, &this->graphics_pipeline) != VK_SUCCESS) {
		return -6;
	}

	vkDestroyShaderModule(this->device, vert_shader_module, 0);
	vkDestroyShaderModule(this->device, frag_shader_module, 0);

	free(vert_shader_code.malloc_bytes);
	free(frag_shader_code.malloc_bytes);
	return 0;
}

void vulkan_handler__free(struct vulkan_handler *this) {
	vkDestroyPipeline(this->device, this->graphics_pipeline, 0);
	vkDestroyPipelineLayout(this->device, this->pipeline_layout, 0);
	vkDestroyRenderPass(this->device, this->render_pass, 0);
	for (int i = 0; i < this->swapchain_image_count; ++i) {
		vkDestroyImageView(this->device, this->swapchain_imageviews[i], 0);
	}
	vkDestroySwapchainKHR(this->device, this->swapchain, 0);
	vkDestroyDevice(this->device, 0);
	vkDestroySurfaceKHR(this->instance, this->surface, 0);
#ifdef VULKAN_HANDLER_VALIDATION
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(this->instance, "vkDestroyDebugUtilsMessengerEXT");
	func(this->instance, this->callback, 0);
#endif
	vkDestroyInstance(this->instance, 0);
}

int vulkan_handler__try_init(struct vulkan_handler *this, const char **extensions, int extension_count, int window_width, int window_height, VkResult (*create_window_surface)(void *, VkInstance, VkSurfaceKHR *), void *surface_creator) {
	this->window_width = window_width;
	this->window_height = window_height;
	int result;

	result = try_create_instance(this, extensions, extension_count);
	if (result < 0) {
		return -1;
	}
#ifdef VULKAN_HANDLER_VALIDATION
	VkDebugUtilsMessengerCreateInfoEXT create_info;
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debug_callback;
	create_info.pUserData = 0;
	create_info.flags = 0;
	create_info.pNext = 0;

	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(this->instance, "vkCreateDebugUtilsMessengerEXT");
	if (func(this->instance, &create_info, 0, &this->callback) != VK_SUCCESS) {
		return -2;
	}
#endif
	result = create_window_surface(surface_creator, this->instance, &this->surface);
	if (result != VK_SUCCESS) {
		return -3;
	}

	result = try_init_device(this);
	if (result < 0) {
		return -4;
	}

	result = try_create_swapchain(this);
	if (result < 0) {
		return -5;
	}

	result = try_create_image_views(this);
	if (result < 0) {
		return -6;
	}

	result = try_create_render_pass(this);
	if (result < 0) {
		return -7;
	}

	result = try_create_graphics_pipeline(this);
	if (result < 0) {
		return -8;
	}
	return 0;
}
