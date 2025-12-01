#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "vulkan.h"

#include "sprite_vert.h"


// Make sure all the direcories are in place
void setup_directories() {
	_mkdir("logs");
}

void log(char* message) {
	FILE* f = fopen("logs/log.txt", "a");
	fprintf(f, message);
	fclose(f);
}


#ifdef WIN
	#include <windows.h>
	#include <winuser.h>
	#include <direct.h>

	#include "vulkan_win32.h"

	HWND hwnd = 0;
	uint8_t quit_to_desktop_requested = 0;

	LRESULT CALLBACK window_callback(HWND window, UINT message_type, WPARAM w_param, LPARAM l_param) {
		LRESULT result = 0;

		switch (message_type) {
			case WM_SIZE: {
				printf("WM_SIZE\n");
				break;
			}
			case WM_DESTROY: {
				printf("WM_DESTROY\n");
				break;
			}
			case WM_CLOSE: {
				printf("WM_CLOSE\n");
				quit_to_desktop_requested = 1;
				break;
			}
			case WM_ACTIVATEAPP: {
				printf("WM_ACTIVEAPP\n");
				break;
			}
			case WM_PAINT: {
				PAINTSTRUCT paint;
				HDC device_context = BeginPaint(window, &paint);
				//PatBlt(
				//	device_context, 
				//	paint.rcPaint.left, paint.rcPaint.top, 
				//	paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top,
				//	BLACKNESS
				//);
				EndPaint(window, &paint);
				break;
			}
			default: {
				result = DefWindowProc(window, message_type, w_param, l_param);
				break;
			}
		}

		return result;
	}

	void window_create() {
		HINSTANCE instance = GetModuleHandle(NULL);

		WNDCLASS wnd = { 0 };
		wnd.style = CS_HREDRAW | CS_VREDRAW | CS_CLASSDC | CS_OWNDC;
		wnd.lpfnWndProc = window_callback;
		wnd.hInstance = instance;
				//wnd.hIcon = ;
		wnd.lpszClassName = "puristapp";

		if (RegisterClass(&wnd)) {
			hwnd = CreateWindowEx(
				0,
				wnd.lpszClassName,
				"This must be a window!",
				WS_OVERLAPPEDWINDOW | WS_VISIBLE,
				CW_USEDEFAULT, CW_USEDEFAULT, // pos x, pos y
				CW_USEDEFAULT, CW_USEDEFAULT, // width, height
				0,
				0,
				instance,
				0
			);
		}
	}

	void window_get_size(uint32_t* p_width, uint32_t* p_height) {
		RECT rect;
		GetClientRect(hwnd, &rect);
		uint32_t width = rect.right - rect.left;
		uint32_t height = rect.bottom - rect.top;
		memcpy(p_width, &width, 4);
		memcpy(p_height, &height, 4);
	}
#endif


VkInstance instance;
VkPhysicalDevice physical_device;
VkDevice device;
VkSurfaceKHR surface;

typedef struct GPUInfo {
	uint8_t has_dedicated_transfer;
} GPUInfo;
GPUInfo gpuinfo; // Set during device creation

VkCommandPool command_pool;
VkCommandPool command_pool_transient;

VkQueue graphics_queue;

uint32_t buffer_strategy;
uint32_t swapchain_width;
uint32_t swapchain_height;
VkSwapchainKHR swapchain;
VkImage* swapchain_images;
VkImageView* swapchain_image_views;
VkCommandPool command_pool;
VkCommandBuffer* command_buffers;
VkSemaphore* available_semaphores;
VkSemaphore* finished_semaphores;
VkFence* fences;
uint32_t current_frame = 0;


VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT 	message_severity,
	VkDebugUtilsMessageTypeFlagsEXT			message_type,
	VkDebugUtilsMessengerCallbackDataEXT*	p_callback_data,
	void* 									p_userdata
) {
	char* severity = "UNKNOWN";
	if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		severity = "WARNING";
	} else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		severity = "ERROR";
	} else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		severity = "INFO";
	}
	printf("%s:\n", severity);
	printf("	%s\n\n", p_callback_data->pMessage);

	return VK_TRUE;
}


void vulkan_create_swapchain() {
	VkSurfaceCapabilitiesKHR surface_caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps);

	uint32_t surface_format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, NULL);
	VkSurfaceFormatKHR* surface_formats = (VkSurfaceFormatKHR*) malloc(sizeof(VkSurfaceFormatKHR) * surface_format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &surface_format_count, surface_formats);

	uint32_t surface_present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &surface_present_mode_count, NULL);
	VkPresentModeKHR* surface_present_modes = (VkPresentModeKHR*) malloc(sizeof(VkPresentModeKHR) * surface_present_mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &surface_present_mode_count, surface_present_modes);

	VkSwapchainCreateInfoKHR swapchain_ci = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0
	};

	window_get_size(&swapchain_ci.imageExtent.width, &swapchain_ci.imageExtent.height);
	swapchain_width = swapchain_ci.imageExtent.width;
	swapchain_height = swapchain_ci.imageExtent.height;
	if (swapchain_ci.imageExtent.width <= 0 || swapchain_ci.imageExtent.height <= 0) {
		// Do not create swapchain with width * height <= 0
		free(surface_formats);
		free(surface_present_modes);
		return;
	}

	swapchain_ci.surface = surface;
	swapchain_ci.minImageCount = min(max(surface_caps.minImageCount, 2), surface_caps.maxImageCount); // want double buffering
	swapchain_ci.imageFormat = VK_FORMAT_R8G8B8A8_SRGB; // TODO: check for compatibility
	swapchain_ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // check for compatibility
	swapchain_ci.imageArrayLayers = 1;
	swapchain_ci.imageUsage = 	VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
								VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_ci.queueFamilyIndexCount = 0;
	swapchain_ci.pQueueFamilyIndices = NULL;
	swapchain_ci.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	
	for (uint32_t i = 0; i < surface_present_mode_count; i++) {
		VkPresentModeKHR present_mode_candidate = surface_present_modes[i];

		// TODO: select present mode based on vsync toggle
		// Mailbox is simply the superior present mode: no vsync and no tearing
		if (present_mode_candidate == VK_PRESENT_MODE_MAILBOX_KHR) {
			swapchain_ci.presentMode = present_mode_candidate;
			break;
		}

		// FIFO is the next best
		if (present_mode_candidate == VK_PRESENT_MODE_FIFO_KHR) {
			swapchain_ci.presentMode = present_mode_candidate;
			continue; // Keep looking for the better option
		}

		// This is defeat; we settle for less, but all hope may not yet be lost
		swapchain_ci.presentMode = present_mode_candidate; 
	}

	swapchain_ci.clipped = 0;
	swapchain_ci.oldSwapchain = 0;

	VkResult result = vkCreateSwapchainKHR(device, &swapchain_ci, NULL, &swapchain);
	free(surface_present_modes);
	free(surface_formats);
	if (result != VK_SUCCESS) {
		printf("Swapchain creation failed with result: %d\n", result);
	}

	uint32_t image_count = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, NULL);
	swapchain_images = (VkImage*) malloc(sizeof(VkImage) * image_count);
	swapchain_image_views = (VkImageView*) malloc(sizeof(VkImageView) * image_count);
	vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images);

	for (uint32_t i = 0; i < image_count; i++) {
		VkImageViewCreateInfo image_view_ci = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
			swapchain_images[i],
			VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_R8G8B8A8_SRGB,
			{ 0 }, // VkComponentMapping
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } //VkImageSubresourceRange
		};
		vkCreateImageView(device, &image_view_ci, NULL, swapchain_image_views + i);
	}

	buffer_strategy = image_count;
}

void vulkan_create_command_buffers() {
	command_buffers = (VkCommandBuffer*) malloc(sizeof(VkCommandBuffer) * buffer_strategy);

	VkCommandBufferAllocateInfo command_buffer_ci = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		buffer_strategy
	};
	vkAllocateCommandBuffers(device, &command_buffer_ci, command_buffers);
}

void vulkan_create_sync_objects() {
	available_semaphores = (VkSemaphore*) malloc(sizeof(VkSemaphore) * buffer_strategy);
	finished_semaphores = (VkSemaphore*) malloc(sizeof(VkSemaphore) * buffer_strategy);
	fences = (VkFence*) malloc(sizeof(VkFence) * buffer_strategy);
	for (uint32_t i = 0; i < buffer_strategy; i++) {
		VkSemaphoreCreateInfo semaphore_ci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0 };
		VkFenceCreateInfo fence_ci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, VK_FENCE_CREATE_SIGNALED_BIT };

		vkCreateSemaphore(device, &semaphore_ci, NULL, available_semaphores + i);
		vkCreateSemaphore(device, &semaphore_ci, NULL, finished_semaphores + i);
		vkCreateFence(device, &fence_ci, NULL, fences + i);
	}
}

void vulkan_init() {
	{ // VULKAN INSTANCE AND DEBUG UTIL CREATION
		VkApplicationInfo app_info = {
			VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
			"puristapp", APP_VERSION,
			"puristengine", ENGINE_VERSION,
			VK_API_VERSION_1_2
		};

		uint32_t layer_count = 1;
		char** pp_layers = (char**) malloc(sizeof(char*) * layer_count);
		pp_layers[0] = "VK_LAYER_KHRONOS_validation";

		uint32_t extension_count = 4;
		char** pp_extensions = (char**) malloc(sizeof(char*) * extension_count);
		pp_extensions[0] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		pp_extensions[1] = VK_KHR_SURFACE_EXTENSION_NAME;
		pp_extensions[2] = "VK_KHR_get_surface_capabilities2";
		pp_extensions[3] = "VK_KHR_win32_surface";

		VkDebugUtilsMessengerCreateInfoEXT debug_callback_ci = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, NULL, 0,

			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			//VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,

			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
	        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,

	        vulkan_debug_callback, NULL
		};

		VkInstanceCreateInfo instance_ci = {
			VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, &debug_callback_ci, 0,
			&app_info,
			layer_count, pp_layers,
			extension_count, pp_extensions
		};

		VkResult result = vkCreateInstance(&instance_ci, NULL, &instance);
		free(pp_layers);
		free(pp_extensions);

		if (result != VK_SUCCESS) {
			printf("Instance creation failed with reslt: %d\n", result);
		}
	}

	uint32_t relevant_queue_flags =  (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

	{ // VULKAN PHYSICAL DEVICE SELECTION
		uint32_t physical_device_count = 0;
		vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);
		VkPhysicalDevice* physical_devices = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * physical_device_count);;
		vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);
	 
		uint32_t best_score = 0;
		uint32_t best_index = 0;
		GPUInfo best_gpuinfo = { 0 };

		// Assign a score to all physical devices and choose the best one
		for (uint32_t i = 0; i < physical_device_count; i++) {
			VkPhysicalDevice physical_device_candidate = physical_devices[i];
			VkPhysicalDeviceVulkan12Properties props12 = {
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES, NULL
			};
			VkPhysicalDeviceProperties2 props = {
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &props12
			};

			vkGetPhysicalDeviceProperties2(physical_device_candidate, &props);

			uint32_t candidate_score = 0;
			GPUInfo candidate_gpuinfo = { 0 };

			// API version must be at 1.2 or higher!
			if (props.properties.apiVersion < VK_API_VERSION_1_2) continue;

			// Discrete devices are immediately given a huge score boost because they are almost always better than integrated ones
			if (props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) candidate_score += 1000;

			// The number of graphics, compute and transfer queues greatly affects score
			uint32_t queue_family_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device_candidate, &queue_family_count, NULL);
			VkQueueFamilyProperties* queue_family_props = (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
			vkGetPhysicalDeviceQueueFamilyProperties(physical_device_candidate, &queue_family_count, queue_family_props);
			uint32_t queue_count;
			for (uint32_t qi = 0; qi < queue_family_count; qi++) {
				VkQueueFamilyProperties queue_props = queue_family_props[qi];
				if (queue_props.queueFlags & relevant_queue_flags) {
					queue_count += queue_props.queueCount;
				}

				// GPUs that expose a queue specifically with only the TRANSFER bit usually have a dedicated hardware copy engine.
				// In those cases, it can be beneficial for larger apps to stream resources via this async queue.
				if ((queue_props.queueFlags & relevant_queue_flags) == VK_QUEUE_TRANSFER_BIT) {
					candidate_gpuinfo.has_dedicated_transfer = 1;
				}
			}
			free(queue_family_props);
			candidate_score += queue_count * 50;

			if (candidate_score > best_score) {
				best_score = candidate_score;
				best_index = i;
				best_gpuinfo = candidate_gpuinfo;
			}
		}
		physical_device = physical_devices[best_index];
		gpuinfo = best_gpuinfo;
		free(physical_devices);
	}

	{ // VULKAN DEVICE AND QUEUE CREATION
		VkPhysicalDeviceFeatures enabled_features = { 0 };
		VkPhysicalDeviceVulkan12Features enabled_features12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, NULL }; 
		enabled_features.samplerAnisotropy = 1;
		enabled_features12.descriptorIndexing = 1;
		enabled_features12.runtimeDescriptorArray = 1;
		enabled_features12.shaderUniformBufferArrayNonUniformIndexing = 1;
		enabled_features12.shaderSampledImageArrayNonUniformIndexing = 1;
		enabled_features12.shaderStorageBufferArrayNonUniformIndexing = 1;

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
		VkQueueFamilyProperties* queue_family_props = (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_family_props);

		uint32_t max_queue_count = 8; // Using more than 8 queues seems insanely unlikely. Could probably even set to 4...
		uint32_t queue_count = 0;
		VkDeviceQueueCreateInfo* queues_ci = (VkDeviceQueueCreateInfo*) malloc(sizeof(VkDeviceQueueCreateInfo) * max_queue_count);
		float priority = 1.0f;
		for (uint32_t i = 0; i < queue_family_count; i++) {
			VkQueueFamilyProperties props = queue_family_props[i];

			// Right now only a graphics- and compute-capable queue is needed. Pretty much every GPU offers such a queue.
			// May have to implement some kind of fallback logic nevertheless...
			if ((props.queueFlags & relevant_queue_flags) == relevant_queue_flags) {
				VkDeviceQueueCreateInfo ci = {
					VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0,
					i, // family index
					1, // queue count
					&priority
				};
				queues_ci[queue_count] = ci;
				queue_count += 1;
			}
		}

		uint32_t extension_count = 2;
		char** pp_extensions = (char**) malloc(sizeof(char*) * extension_count);
		pp_extensions[0] = "VK_KHR_dynamic_rendering";
		pp_extensions[1] = "VK_KHR_swapchain";

		VkDeviceCreateInfo device_ci = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &enabled_features12, 0,
			queue_count, queues_ci, // queues
			0, NULL, // layers (outdated)
			extension_count, pp_extensions, // extensions
			&enabled_features
		};

		VkResult result = vkCreateDevice(physical_device, &device_ci, NULL, &device);
		free(pp_extensions);
		free(queue_family_props);
		free(queues_ci);

		if (result != VK_SUCCESS) {
			printf("Device creation failed with result: %d\n", result);
		}

		vkGetDeviceQueue(device, 0, 0, &graphics_queue); // TODO: more sophisticated please
	}

	{ // SURFACE CREATION
		#ifdef WIN
		VkWin32SurfaceCreateInfoKHR surface_ci = { 
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, NULL, 0,
			GetModuleHandle(NULL), hwnd
		};
		VkResult result = vkCreateWin32SurfaceKHR(instance, &surface_ci, NULL, &surface);
		if (result != VK_SUCCESS) {
			printf("Win32 Surface creation failed with result: %d\n", result);
		}
		#endif

		#ifdef LINUX
		VkXcbSurfaceCreateInfoKHR surface_ci = {

		};
		VkResult result = vkCreateXcbSurfaceKHR(instance, &surface_ci, NULL, &surface);
		if (result != VK_SUCCESS) {
			printf("Xcb Surface creation failed with result: %d\n", result);
		}
		#endif 
	}

	{ // COMMAND POOL CREATION
		// For the command buffers that are part of the render loop
		VkCommandPoolCreateInfo command_pool_ci = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			0 // TODO: select target queue family during device creation
		};
		vkCreateCommandPool(device, &command_pool_ci, NULL, &command_pool);

		// For single time use command buffers
		command_pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		vkCreateCommandPool(device, &command_pool_ci, NULL, &command_pool_transient);
	}

	{ // SWAPCHAIN, COMMAND BUFFER AND SYNC OBJECTS CREATION
		vulkan_create_swapchain();
		vulkan_create_command_buffers();
		vulkan_create_sync_objects();
	}
}

void vulkan_create_present_objects() {
	vulkan_create_swapchain();
	if (swapchain_width > 0 && swapchain_height > 0) {
		vulkan_create_command_buffers();
		vulkan_create_sync_objects();
	}
}

void vulkan_destroy_present_objects() {
	for (uint32_t i = 0; i < buffer_strategy; i++) {
		vkDestroyFence(device, fences[i], NULL);
		vkDestroySemaphore(device, available_semaphores[i], NULL);
		vkDestroySemaphore(device, finished_semaphores[i], NULL);
		vkDestroyImageView(device, swapchain_image_views[i], NULL);
	}
	vkDestroySwapchainKHR(device, swapchain, NULL);
	free(fences);
	free(available_semaphores);
	free(finished_semaphores);
	free(swapchain_image_views);
	free(swapchain_images);
}

void vulkan_recreate_present_objects() {
	vulkan_destroy_present_objects();
	vulkan_create_present_objects();
}

void draw_frame() {
	// Do not draw when swapchain is 0 in one dimension
	if (swapchain_width <= 0 || swapchain_height <= 0) {
		// Check if window size changed
		uint32_t width, height;
		window_get_size(&width, &height);
		if (width > 0 && height > 0) {
			// Don't recreate, just create since everything will have been destroyed already
			vulkan_create_present_objects();
		}
		return;
	}

	uint64_t uint64_max = 0xFFFFFFFFFFFFFFFF;

	vkWaitForFences(device, 1, fences + current_frame, VK_TRUE, uint64_max);
	vkResetFences(device, 1, fences + current_frame);

	uint32_t image_index;

	VkResult result_acquire = vkAcquireNextImageKHR(
		device,
		swapchain,
		uint64_max,
		available_semaphores[current_frame],
		NULL,
		&image_index
	);

	if (result_acquire == VK_ERROR_OUT_OF_DATE_KHR || result_acquire == VK_SUBOPTIMAL_KHR) {
		vkDeviceWaitIdle(device);
		vulkan_recreate_present_objects();
		return;
	}

	VkImage swapchain_image = swapchain_images[image_index];
	VkImageView swapchain_image_view = swapchain_image_views[image_index];
	VkCommandBuffer command_buffer = command_buffers[image_index];
	VkCommandBufferBeginInfo cmdbuf_begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0, NULL
	};
	vkBeginCommandBuffer(command_buffer, &cmdbuf_begin_info);
	{
		VkImageMemoryBarrier img_barrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			swapchain_image,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};
		vkCmdPipelineBarrier(
			command_buffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, NULL,
			0, NULL,
			1, &img_barrier
		);

		VkClearColorValue clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };
		VkImageSubresourceRange subresource_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		vkCmdClearColorImage(
			command_buffer,
			swapchain_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
  			&clear_color,
  			1, &subresource_range
		);

		img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		img_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		img_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		img_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier(
			command_buffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,
			0, NULL,
			0, NULL,
			1, &img_barrier
		);
	}
	vkEndCommandBuffer(command_buffer);

	VkPipelineStageFlags dst_pipeline_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		1, available_semaphores + current_frame, // wait semaphore
		&dst_pipeline_stage,
		1, command_buffers + image_index,
		1, finished_semaphores + current_frame // signal semaphore
	};
	vkQueueSubmit(graphics_queue, 1, &submit_info, fences[current_frame]);

	VkPresentInfoKHR present_info = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL,
		1, finished_semaphores + current_frame,
		1, &swapchain,
		&image_index,
		NULL
	};
	VkResult result_present = vkQueuePresentKHR(graphics_queue, &present_info);

	if (result_present == VK_ERROR_OUT_OF_DATE_KHR || result_present == VK_SUBOPTIMAL_KHR) {
		vkDeviceWaitIdle(device);
		vulkan_recreate_present_objects();
		return;
	}

	current_frame = (current_frame + 1) % buffer_strategy;
}

void begin_loop() {
	while (!quit_to_desktop_requested) {
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {			
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		draw_frame();
	}

	DestroyWindow(hwnd);
}


void app_run() {
	setbuf(stdout, NULL);

	setup_directories();

	window_create();

	vulkan_init();

	begin_loop();

	vkDeviceWaitIdle(device);

	vkDestroyCommandPool(device, command_pool, NULL);
	vkDestroyCommandPool(device, command_pool_transient, NULL);

	for (uint32_t i = 0; i < buffer_strategy; i++) {
		vkDestroyFence(device, fences[i], NULL);
		vkDestroySemaphore(device, available_semaphores[i], NULL);
		vkDestroySemaphore(device, finished_semaphores[i], NULL);
		vkDestroyImageView(device, swapchain_image_views[i], NULL);
	}
	vkDestroySwapchainKHR(device, swapchain, NULL);
	free(fences);
	free(available_semaphores);
	free(finished_semaphores);
	free(swapchain_image_views);
	free(swapchain_images);

	vkDestroySurfaceKHR(instance, surface, NULL);
	vkDestroyDevice(device, NULL);
	vkDestroyInstance(instance, NULL);
}