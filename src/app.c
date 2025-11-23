#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

#include "vulkan.h"


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

	HWND hwnd = 0;
	uint8_t close_requested = 0;

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
				close_requested = 1;
				break;
			}
			case WM_ACTIVATEAPP: {
				printf("WM_ACTIVEAPP\n");
				break;
			}
			case WM_PAINT: {
				PAINTSTRUCT paint;
				HDC device_context = BeginPaint(window, &paint);
				PatBlt(
					device_context, 
					paint.rcPaint.left, paint.rcPaint.top, 
					paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top,
					BLACKNESS
				);
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

		if (hwnd != NULL) {
			while (!close_requested) {
				MSG msg;
				BOOL status = GetMessage(&msg, 0, 0, 0);
				if (status > 0) { 
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				} else break;
			}	
		}

		DestroyWindow(hwnd);
	}
#endif


VkInstance instance;
VkPhysicalDevice physical_device;
VkDevice device;
VkSurfaceKHR surface;

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


void vulkan_init() {
	VkApplicationInfo app_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
		"puristapp", APP_VERSION,
		"puristengine", ENGINE_VERSION,
		VK_API_VERSION_1_2
	};

	uint32_t layer_count = 1;
	char** pp_layers = (char**) malloc(sizeof(char*) * layer_count);
	char* layer_validation = "VK_LAYER_KHRONOS_validation";
	pp_layers[0] = layer_validation;

	uint32_t extension_count = 3;
	char** pp_extensions = (char**) malloc(sizeof(char*) * extension_count);
	char* extension_surface_caps2 = "VK_KHR_get_surface_capabilities2";
	pp_extensions[0] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	pp_extensions[1] = VK_KHR_SURFACE_EXTENSION_NAME;
	pp_extensions[2] = extension_surface_caps2;

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

	vkCreateInstance(&instance_ci, NULL, &instance);

	uint32_t physical_device_count = 0;
	vkEnumeratePhysicalDevices(instance, &physical_device_count, NULL);
	VkPhysicalDevice* physical_devices = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * physical_device_count);;
	vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);

	for (uint32_t i = 0; i < physical_device_count; i++) {
		VkPhysicalDevice physical_device_candidate = physical_devices[i];
		VkPhysicalDeviceVulkan12Properties props12 = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES, NULL
		};
		VkPhysicalDeviceProperties2 props = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &props12
		};

		vkGetPhysicalDeviceProperties2(physical_device_candidate, &props);

		// Skip integrated GPUs. If all GPUs are integrated, the first one will be selected
		if (props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) continue;
		
		// API version must be at 1.2 or higher!
		if (props.properties.apiVersion < VK_API_VERSION_1_2) continue;

		physical_device = physical_device_candidate;
		break;
	}

	VkPhysicalDeviceFeatures features = { 0 };
	features.samplerAnisotropy = TRUE;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
	VkQueueFamilyProperties* queue_family_props = (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_family_props);

	for (uint32_t i = 0; i < queue_family_count; i++) {
		VkQueueFamilyProperties props = queue_family_props[i];

		props
	}

	VkDeviceQueueCreateInfo* queue_ci = (VkDeviceQueueCreateInfo*) malloc(sizeof(VkDeviceQueueCreateInfo));

	VkDeviceCreateInfo device_ci = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL,
		0,
		0, NULL, // queues
		0, NULL, // layers
		0, NULL, // extensions
		&features
	};

	//vkCreateDevice(physical_device, &device_ci, NULL, &device);
}

void vulkan_destroy() {
	vkDestroyInstance(instance, NULL);
}


void app_run() {
	printf("very cool\n");

	setup_directories();

	//window_create();

	vulkan_init();
	vulkan_destroy();
}