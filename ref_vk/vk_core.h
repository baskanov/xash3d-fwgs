#pragma once
#include "vk_common.h"

#include "xash3d_types.h"
#include "com_strings.h" // S_ERROR

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

qboolean R_VkInit( void );
void R_VkShutdown( void );

// FIXME load from embedded static structs
VkShaderModule loadShader(const char *filename);
VkSemaphore createSemaphore( void );
void destroySemaphore(VkSemaphore sema);
VkFence createFence( void );
void destroyFence(VkFence fence);

// FIXME arena allocation, ...
typedef struct device_memory_s
{
	VkDeviceMemory device_memory;
	uint32_t offset;
} device_memory_t;

device_memory_t allocateDeviceMemory(VkMemoryRequirements req, VkMemoryPropertyFlags props);
void freeDeviceMemory(device_memory_t *mem);

typedef struct vk_buffer_s
{
	device_memory_t device_memory;
	VkBuffer buffer;

	void *mapped;
	uint32_t size;
} vk_buffer_t;

typedef struct physical_device_s {
	VkPhysicalDevice device;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkPhysicalDeviceProperties properties;
} physical_device_t;

// FIXME dynamic; better estimate
#define MAX_DESC_SETS 4096

typedef struct descriptor_pool_s
{
	VkDescriptorPool pool;
	int next_free;
	//uint32_t *free_set;

	VkDescriptorSet sets[MAX_DESC_SETS];
	VkDescriptorSetLayout one_texture_layout;

	// FIXME HOW THE F
	VkDescriptorSet ubo_sets[1];
	VkDescriptorSetLayout one_uniform_buffer_layout;
} descriptor_pool_t;

typedef struct vulkan_core_s {
	uint32_t vulkan_version;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;

	byte *pool;

	qboolean debug;
	struct {
		VkSurfaceKHR surface;
		uint32_t num_surface_formats;
		VkSurfaceFormatKHR *surface_formats;

		uint32_t num_present_modes;
		VkPresentModeKHR *present_modes;
	} surface;

	physical_device_t physical_device;
	VkDevice device;
	VkQueue queue;

	VkCommandPool command_pool;
	VkCommandBuffer cb;

	vk_buffer_t staging;

	VkSampler default_sampler;
	descriptor_pool_t descriptor_pool;
} vulkan_core_t;

extern vulkan_core_t vk_core;

const char *resultName(VkResult result);

// TODO make this not fatal: devise proper error handling strategies
// FIXME Host_Error does not cause process to exit, we need to handle this manually
#define XVK_CHECK(f) do { \
		const VkResult result = f; \
		if (result != VK_SUCCESS) { \
			gEngine.Host_Error( S_ERROR "%s:%d " #f " failed (%d): %s\n", \
				__FILE__, __LINE__, result, resultName(result)); \
		} \
	} while(0)

#define INSTANCE_FUNCS(X) \
	X(vkDestroyInstance) \
	X(vkEnumeratePhysicalDevices) \
	X(vkGetPhysicalDeviceProperties) \
	X(vkGetPhysicalDeviceProperties2) \
	X(vkGetPhysicalDeviceFeatures2) \
	X(vkGetPhysicalDeviceQueueFamilyProperties) \
	X(vkGetPhysicalDeviceSurfaceSupportKHR) \
	X(vkGetPhysicalDeviceMemoryProperties) \
	X(vkGetPhysicalDeviceSurfacePresentModesKHR) \
	X(vkGetPhysicalDeviceSurfaceFormatsKHR) \
	X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
	X(vkGetPhysicalDeviceFormatProperties) \
	X(vkCreateDevice) \
	X(vkGetDeviceProcAddr) \
	X(vkDestroyDevice) \
	X(vkDestroySurfaceKHR) \

#define DEVICE_FUNCS(X) \
	X(vkGetDeviceQueue) \
	X(vkCreateSwapchainKHR) \
	X(vkGetSwapchainImagesKHR) \
	X(vkDestroySwapchainKHR) \
	X(vkCreateImageView) \
	X(vkCreateFramebuffer) \
	X(vkCreateRenderPass) \
	X(vkCreatePipelineLayout) \
	X(vkCreateGraphicsPipelines) \
	X(vkCreateShaderModule) \
	X(vkCreateCommandPool) \
	X(vkAllocateCommandBuffers) \
	X(vkCreateBuffer) \
	X(vkGetBufferMemoryRequirements) \
	X(vkAllocateMemory) \
	X(vkBindBufferMemory) \
	X(vkMapMemory) \
	X(vkUnmapMemory) \
	X(vkDestroyBuffer) \
	X(vkFreeMemory) \
	X(vkAcquireNextImageKHR) \
	X(vkCmdBeginRenderPass) \
	X(vkCmdExecuteCommands) \
	X(vkCmdEndRenderPass) \
	X(vkEndCommandBuffer) \
	X(vkQueueSubmit) \
	X(vkQueuePresentKHR) \
	X(vkWaitForFences) \
	X(vkResetFences) \
	X(vkCreateSemaphore) \
	X(vkDestroySemaphore) \
	X(vkCreateFence) \
	X(vkDestroyFence) \
	X(vkBeginCommandBuffer) \
	X(vkCmdBindPipeline) \
	X(vkCmdBindVertexBuffers) \
	X(vkCmdDraw) \
	X(vkDestroyCommandPool) \
	X(vkDestroyImageView) \
	X(vkDestroyFramebuffer) \
	X(vkDestroyRenderPass) \
	X(vkDestroyShaderModule) \
	X(vkDestroyPipeline) \
	X(vkDestroyPipelineLayout) \
	X(vkCreateImage) \
	X(vkGetImageMemoryRequirements) \
	X(vkBindImageMemory) \
	X(vkCmdPipelineBarrier) \
	X(vkCmdCopyBufferToImage) \
	X(vkQueueWaitIdle) \
	X(vkDestroyImage) \
	X(vkCmdBindDescriptorSets) \
	X(vkCreateSampler) \
	X(vkDestroySampler) \
	X(vkCreateDescriptorPool) \
	X(vkDestroyDescriptorPool) \
	X(vkCreateDescriptorSetLayout) \
	X(vkAllocateDescriptorSets) \
	X(vkUpdateDescriptorSets) \
	X(vkDestroyDescriptorSetLayout) \
	X(vkCmdSetViewport) \
	X(vkCmdSetScissor) \
	X(vkCmdUpdateBuffer) \
	X(vkCmdBindIndexBuffer) \
	X(vkCmdDrawIndexed) \

#define X(f) extern PFN_##f f;
	DEVICE_FUNCS(X)
	INSTANCE_FUNCS(X)
#undef X