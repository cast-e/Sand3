#include "vulkan.hpp"

#include <fmt/base.h>

#include <algorithm>
#include <cstring>

#include "material_manager.hpp"
#include "resources/simulation_spv.h"

bool Vulkan::initialized = false;
bool Vulkan::prevent_downclocking = false;
uint32_t Vulkan::width = 0;
uint32_t Vulkan::height = 0;

VkInstance Vulkan::instance = VK_NULL_HANDLE;
VkPhysicalDevice Vulkan::physical_device = VK_NULL_HANDLE;
VkDevice Vulkan::device = VK_NULL_HANDLE;
VkQueue Vulkan::compute_queue = VK_NULL_HANDLE;
uint32_t Vulkan::compute_queue_family = 0;

VkCommandPool Vulkan::command_pool = VK_NULL_HANDLE;
VkCommandBuffer Vulkan::command_buffer = VK_NULL_HANDLE;
VkFence Vulkan::fence = VK_NULL_HANDLE;

VkShaderModule Vulkan::compute_shader_module = VK_NULL_HANDLE;
VkDescriptorSetLayout Vulkan::descriptor_set_layout = VK_NULL_HANDLE;
VkPipelineLayout Vulkan::pipeline_layout = VK_NULL_HANDLE;
VkPipeline Vulkan::compute_pipeline = VK_NULL_HANDLE;
VkDescriptorPool Vulkan::descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet Vulkan::descriptor_set = VK_NULL_HANDLE;

VkBuffer Vulkan::current_grid_buffer = VK_NULL_HANDLE;
VkDeviceMemory Vulkan::current_grid_memory = VK_NULL_HANDLE;

VkBuffer Vulkan::next_grid_buffer = VK_NULL_HANDLE;
VkDeviceMemory Vulkan::next_grid_memory = VK_NULL_HANDLE;

VkBuffer Vulkan::rules_buffer = VK_NULL_HANDLE;
VkDeviceMemory Vulkan::rules_memory = VK_NULL_HANDLE;
GpuRulesHeader* Vulkan::rules_mapped = nullptr;

VkBuffer Vulkan::display_buffer = VK_NULL_HANDLE;
VkDeviceMemory Vulkan::display_memory = VK_NULL_HANDLE;
uint32_t* Vulkan::display_mapped = nullptr;

VkBuffer Vulkan::stats_buffer = VK_NULL_HANDLE;
VkDeviceMemory Vulkan::stats_memory = VK_NULL_HANDLE;
uint32_t* Vulkan::stats_mapped = nullptr;

VkBuffer Vulkan::staging_buffer = VK_NULL_HANDLE;
VkDeviceMemory Vulkan::staging_memory = VK_NULL_HANDLE;
uint32_t* Vulkan::staging_mapped = nullptr;

uint32_t Vulkan::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags preferred,
								  VkMemoryPropertyFlags required) {
	VkPhysicalDeviceMemoryProperties mem_properties;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

	// First try to match preferred | required
	VkMemoryPropertyFlags combined = preferred | required;
	for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
		if ((type_filter & (1u << i)) && (mem_properties.memoryTypes[i].propertyFlags & combined) == combined) {
			return i;
		}
	}

	// Fallback to required only
	for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
		if ((type_filter & (1u << i)) && (mem_properties.memoryTypes[i].propertyFlags & required) == required) {
			return i;
		}
	}
	return 0xFFFFFFFF;
}

bool Vulkan::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags preferred_properties,
						   VkMemoryPropertyFlags required_properties, VkBuffer& buffer, VkDeviceMemory& buffer_memory) {
	VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buffer_info.size = size;
	buffer_info.usage = usage;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
		return false;
	}

	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);

	uint32_t mem_type_idx = find_memory_type(mem_reqs.memoryTypeBits, preferred_properties, required_properties);
	if (mem_type_idx == 0xFFFFFFFF) {
		// Fallback for device local if strictly needed
		mem_type_idx =
			find_memory_type(mem_reqs.memoryTypeBits, 0, required_properties & ~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (mem_type_idx == 0xFFFFFFFF) {
			return false;
		}
	}

	VkMemoryAllocateInfo alloc_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	alloc_info.allocationSize = mem_reqs.size;
	alloc_info.memoryTypeIndex = mem_type_idx;

	if (vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
		return false;
	}

	if (vkBindBufferMemory(device, buffer, buffer_memory, 0) != VK_SUCCESS) {
		return false;
	}

	return true;
}

bool Vulkan::init(uint32_t sim_width, uint32_t sim_height) {
	if (initialized) {
		return true;
	}

	width = sim_width;
	height = sim_height;

	// 1. Initialize volk
	if (volkInitialize() != VK_SUCCESS) {
		fmt::print("Vulkan: volkInitialize failed. Vulkan is not available.\n");
		return false;
	}

	// 2. Create Vulkan Instance
	VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app_info.pApplicationName = "Sand3";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "Sand3Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	instance_info.pApplicationInfo = &app_info;

	if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create Vulkan instance.\n");
		return false;
	}

	volkLoadInstance(instance);

	// 3. Select Physical Device with Compute support
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	if (device_count == 0) {
		fmt::print("Vulkan: No Vulkan physical devices found.\n");
		shutdown();
		return false;
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

	int best_score = -1;
	for (const auto& dev : devices) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(dev, &props);

		uint32_t queue_family_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_count, nullptr);
		std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_count, queue_families.data());

		int queue_idx = -1;
		for (uint32_t i = 0; i < queue_family_count; ++i) {
			if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
				queue_idx = static_cast<int>(i);
				break;
			}
		}

		if (queue_idx == -1)
			continue;

		int score = 0;
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score += 1000;
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			score += 100;

		if (score > best_score) {
			best_score = score;
			physical_device = dev;
			compute_queue_family = static_cast<uint32_t>(queue_idx);
		}
	}

	if (physical_device == VK_NULL_HANDLE) {
		fmt::print("Vulkan: No physical device with compute support found.\n");
		shutdown();
		return false;
	}

	VkPhysicalDeviceProperties selected_props;
	vkGetPhysicalDeviceProperties(physical_device, &selected_props);
	fmt::print("Vulkan: Selected GPU: {}\n", selected_props.deviceName);

	// 4. Create Logical Device and Queue
	float queue_priority = 1.0f;
	VkDeviceQueueCreateInfo queue_create_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	queue_create_info.queueFamilyIndex = compute_queue_family;
	queue_create_info.queueCount = 1;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkDeviceCreateInfo device_create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	device_create_info.queueCreateInfoCount = 1;
	device_create_info.pQueueCreateInfos = &queue_create_info;

	if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create logical device.\n");
		shutdown();
		return false;
	}

	volkLoadDevice(device);
	vkGetDeviceQueue(device, compute_queue_family, 0, &compute_queue);

	// 5. Command Pool and Buffer
	VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = compute_queue_family;

	if (vkCreateCommandPool(device, &pool_info, nullptr, &command_pool) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create command pool.\n");
		shutdown();
		return false;
	}

	VkCommandBufferAllocateInfo alloc_cmd_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	alloc_cmd_info.commandPool = command_pool;
	alloc_cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_cmd_info.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device, &alloc_cmd_info, &command_buffer) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to allocate command buffer.\n");
		shutdown();
		return false;
	}

	VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create fence.\n");
		shutdown();
		return false;
	}

	// 6. Shader Module
	VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	shader_info.codeSize = simulation_spv_len;
	shader_info.pCode = reinterpret_cast<const uint32_t*>(simulation_spv);

	if (vkCreateShaderModule(device, &shader_info, nullptr, &compute_shader_module) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create compute shader module.\n");
		shutdown();
		return false;
	}

	// 7. Descriptor Set Layout
	std::array<VkDescriptorSetLayoutBinding, 5> bindings{};
	for (uint32_t i = 0; i < 5; ++i) {
		bindings[i].binding = i;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}

	VkDescriptorSetLayoutCreateInfo desc_layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	desc_layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
	desc_layout_info.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &desc_layout_info, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create descriptor set layout.\n");
		shutdown();
		return false;
	}

	// 8. Pipeline Layout
	VkPushConstantRange push_constant_range{};
	push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_constant_range.offset = 0;
	push_constant_range.size = sizeof(GpuPushConstants);

	VkPipelineLayoutCreateInfo pipeline_layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pipeline_layout_info.setLayoutCount = 1;
	pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
	pipeline_layout_info.pushConstantRangeCount = 1;
	pipeline_layout_info.pPushConstantRanges = &push_constant_range;

	if (vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create pipeline layout.\n");
		shutdown();
		return false;
	}

	// 9. Compute Pipeline
	VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	pipeline_info.layout = pipeline_layout;
	pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pipeline_info.stage.module = compute_shader_module;
	pipeline_info.stage.pName = "main";

	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &compute_pipeline) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create compute pipeline.\n");
		shutdown();
		return false;
	}

	// 10. Descriptor Pool & Set
	VkDescriptorPoolSize pool_size{};
	pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	pool_size.descriptorCount = 5;

	VkDescriptorPoolCreateInfo desc_pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	desc_pool_info.maxSets = 1;
	desc_pool_info.poolSizeCount = 1;
	desc_pool_info.pPoolSizes = &pool_size;

	if (vkCreateDescriptorPool(device, &desc_pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to create descriptor pool.\n");
		shutdown();
		return false;
	}

	VkDescriptorSetAllocateInfo desc_alloc_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	desc_alloc_info.descriptorPool = descriptor_pool;
	desc_alloc_info.descriptorSetCount = 1;
	desc_alloc_info.pSetLayouts = &descriptor_set_layout;

	if (vkAllocateDescriptorSets(device, &desc_alloc_info, &descriptor_set) != VK_SUCCESS) {
		fmt::print("Vulkan: Failed to allocate descriptor set.\n");
		shutdown();
		return false;
	}

	// 11. Create Buffers
	VkDeviceSize grid_size_bytes = static_cast<VkDeviceSize>(width) * height * sizeof(uint32_t);

	create_buffer(grid_size_bytes,
				  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
					  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, current_grid_buffer,
				  current_grid_memory);

	create_buffer(
		grid_size_bytes,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, next_grid_buffer, next_grid_memory);

	create_buffer(sizeof(GpuRulesHeader), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, rules_buffer,
				  rules_memory);
	vkMapMemory(device, rules_memory, 0, sizeof(GpuRulesHeader), 0, reinterpret_cast<void**>(&rules_mapped));

	create_buffer(
		grid_size_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, display_buffer, display_memory);
	vkMapMemory(device, display_memory, 0, grid_size_bytes, 0, reinterpret_cast<void**>(&display_mapped));
	std::memset(display_mapped, 0, grid_size_bytes);

	create_buffer(sizeof(uint32_t) * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stats_buffer,
				  stats_memory);
	vkMapMemory(device, stats_memory, 0, sizeof(uint32_t) * 4, 0, reinterpret_cast<void**>(&stats_mapped));
	stats_mapped[0] = 0;

	create_buffer(
		grid_size_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_memory);
	vkMapMemory(device, staging_memory, 0, grid_size_bytes, 0, reinterpret_cast<void**>(&staging_mapped));
	std::memset(staging_mapped, 0, grid_size_bytes);

	// 12. Update Descriptor Set
	std::array<VkDescriptorBufferInfo, 5> buffer_infos{};
	buffer_infos[0] = {current_grid_buffer, 0, grid_size_bytes};
	buffer_infos[1] = {next_grid_buffer, 0, grid_size_bytes};
	buffer_infos[2] = {rules_buffer, 0, sizeof(GpuRulesHeader)};
	buffer_infos[3] = {display_buffer, 0, grid_size_bytes};
	buffer_infos[4] = {stats_buffer, 0, sizeof(uint32_t) * 4};

	std::array<VkWriteDescriptorSet, 5> writes{};
	for (uint32_t i = 0; i < 5; ++i) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = descriptor_set;
		writes[i].dstBinding = i;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].descriptorCount = 1;
		writes[i].pBufferInfo = &buffer_infos[i];
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	initialized = true;
	update_rules();

	return true;
}

bool Vulkan::resize(uint32_t new_width, uint32_t new_height) {
	if (!initialized || !device) {
		return false;
	}

	vkDeviceWaitIdle(device);

	width = new_width;
	height = new_height;

	// Destroy old size-dependent buffers
	if (staging_memory) {
		vkUnmapMemory(device, staging_memory);
		vkFreeMemory(device, staging_memory, nullptr);
		vkDestroyBuffer(device, staging_buffer, nullptr);
		staging_memory = VK_NULL_HANDLE;
		staging_buffer = VK_NULL_HANDLE;
		staging_mapped = nullptr;
	}
	if (display_memory) {
		vkUnmapMemory(device, display_memory);
		vkFreeMemory(device, display_memory, nullptr);
		vkDestroyBuffer(device, display_buffer, nullptr);
		display_memory = VK_NULL_HANDLE;
		display_buffer = VK_NULL_HANDLE;
		display_mapped = nullptr;
	}
	if (next_grid_memory) {
		vkFreeMemory(device, next_grid_memory, nullptr);
		vkDestroyBuffer(device, next_grid_buffer, nullptr);
		next_grid_memory = VK_NULL_HANDLE;
		next_grid_buffer = VK_NULL_HANDLE;
	}
	if (current_grid_memory) {
		vkFreeMemory(device, current_grid_memory, nullptr);
		vkDestroyBuffer(device, current_grid_buffer, nullptr);
		current_grid_memory = VK_NULL_HANDLE;
		current_grid_buffer = VK_NULL_HANDLE;
	}

	// Recreate size-dependent buffers
	VkDeviceSize grid_size_bytes = static_cast<VkDeviceSize>(width) * height * sizeof(uint32_t);

	create_buffer(grid_size_bytes,
				  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
					  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, current_grid_buffer,
				  current_grid_memory);

	create_buffer(
		grid_size_bytes,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, next_grid_buffer, next_grid_memory);

	create_buffer(
		grid_size_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, display_buffer, display_memory);
	vkMapMemory(device, display_memory, 0, grid_size_bytes, 0, reinterpret_cast<void**>(&display_mapped));
	std::memset(display_mapped, 0, grid_size_bytes);

	create_buffer(
		grid_size_bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_memory);
	vkMapMemory(device, staging_memory, 0, grid_size_bytes, 0, reinterpret_cast<void**>(&staging_mapped));
	std::memset(staging_mapped, 0, grid_size_bytes);

	// Update descriptor set
	std::array<VkDescriptorBufferInfo, 5> buffer_infos{};
	buffer_infos[0] = {current_grid_buffer, 0, grid_size_bytes};
	buffer_infos[1] = {next_grid_buffer, 0, grid_size_bytes};
	buffer_infos[2] = {rules_buffer, 0, sizeof(GpuRulesHeader)};
	buffer_infos[3] = {display_buffer, 0, grid_size_bytes};
	buffer_infos[4] = {stats_buffer, 0, sizeof(uint32_t) * 4};

	std::array<VkWriteDescriptorSet, 5> writes{};
	for (uint32_t i = 0; i < 5; ++i) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = descriptor_set;
		writes[i].dstBinding = i;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].descriptorCount = 1;
		writes[i].pBufferInfo = &buffer_infos[i];
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	return true;
}

void Vulkan::shutdown() {
	if (!instance) {
		return;
	}

	if (device) {
		vkDeviceWaitIdle(device);

		if (staging_memory) {
			vkUnmapMemory(device, staging_memory);
			vkFreeMemory(device, staging_memory, nullptr);
			vkDestroyBuffer(device, staging_buffer, nullptr);
			staging_memory = VK_NULL_HANDLE;
			staging_buffer = VK_NULL_HANDLE;
			staging_mapped = nullptr;
		}

		if (stats_memory) {
			vkUnmapMemory(device, stats_memory);
			vkFreeMemory(device, stats_memory, nullptr);
			vkDestroyBuffer(device, stats_buffer, nullptr);
			stats_memory = VK_NULL_HANDLE;
			stats_buffer = VK_NULL_HANDLE;
			stats_mapped = nullptr;
		}

		if (display_memory) {
			vkUnmapMemory(device, display_memory);
			vkFreeMemory(device, display_memory, nullptr);
			vkDestroyBuffer(device, display_buffer, nullptr);
			display_memory = VK_NULL_HANDLE;
			display_buffer = VK_NULL_HANDLE;
			display_mapped = nullptr;
		}

		if (rules_memory) {
			vkUnmapMemory(device, rules_memory);
			vkFreeMemory(device, rules_memory, nullptr);
			vkDestroyBuffer(device, rules_buffer, nullptr);
			rules_memory = VK_NULL_HANDLE;
			rules_buffer = VK_NULL_HANDLE;
			rules_mapped = nullptr;
		}

		if (next_grid_memory) {
			vkFreeMemory(device, next_grid_memory, nullptr);
			vkDestroyBuffer(device, next_grid_buffer, nullptr);
			next_grid_memory = VK_NULL_HANDLE;
			next_grid_buffer = VK_NULL_HANDLE;
		}

		if (current_grid_memory) {
			vkFreeMemory(device, current_grid_memory, nullptr);
			vkDestroyBuffer(device, current_grid_buffer, nullptr);
			current_grid_memory = VK_NULL_HANDLE;
			current_grid_buffer = VK_NULL_HANDLE;
		}

		if (descriptor_pool) {
			vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
			descriptor_pool = VK_NULL_HANDLE;
		}

		if (compute_pipeline) {
			vkDestroyPipeline(device, compute_pipeline, nullptr);
			compute_pipeline = VK_NULL_HANDLE;
		}

		if (pipeline_layout) {
			vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
			pipeline_layout = VK_NULL_HANDLE;
		}

		if (descriptor_set_layout) {
			vkDestroyDescriptorSetLayout(device, descriptor_set_layout, nullptr);
			descriptor_set_layout = VK_NULL_HANDLE;
		}

		if (compute_shader_module) {
			vkDestroyShaderModule(device, compute_shader_module, nullptr);
			compute_shader_module = VK_NULL_HANDLE;
		}

		if (fence) {
			vkDestroyFence(device, fence, nullptr);
			fence = VK_NULL_HANDLE;
		}

		if (command_pool) {
			vkDestroyCommandPool(device, command_pool, nullptr);
			command_pool = VK_NULL_HANDLE;
		}

		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	vkDestroyInstance(instance, nullptr);
	instance = VK_NULL_HANDLE;

	initialized = false;
}

void Vulkan::update_rules() {
	if (!initialized || !rules_mapped) {
		return;
	}

	std::memset(rules_mapped, 0, sizeof(GpuRulesHeader));

	uint32_t current_rule_idx = 0;
	uint32_t current_var_idx = 0;

	for (uint32_t mat_id = 0; mat_id < 256; ++mat_id) {
		const auto& rm = MaterialManager::get_runtime_material(static_cast<uint8_t>(mat_id));
		rules_mapped->materials[mat_id].color = rm.packed_color;
		rules_mapped->materials[mat_id].rule_start = current_rule_idx;
		rules_mapped->materials[mat_id].rule_count = static_cast<uint32_t>(rm.rules.size());

		for (const auto& r : rm.rules) {
			if (current_rule_idx >= 2048) {
				break;
			}

			GpuRule& gpu_r = rules_mapped->rules[current_rule_idx++];
			gpu_r.variant_start = current_var_idx;
			gpu_r.variant_count = static_cast<uint32_t>(r.variants.size());
			gpu_r.chance_scaled = static_cast<uint32_t>(std::clamp(r.chance, 0.0f, 100.0f) * 1000.0f);

			for (const auto& v : r.variants) {
				if (current_var_idx >= 4096) {
					break;
				}

				GpuRuleVariant& gpu_v = rules_mapped->variants[current_var_idx++];
				uint32_t wildcard_mask = 0;
				for (uint32_t n = 0; n < 25; ++n) {
					if (v.when[n].all()) {
						wildcard_mask |= (1u << n);
					}
					for (uint32_t w = 0; w < 8; ++w) {
						uint32_t word = 0;
						for (uint32_t b = 0; b < 32; ++b) {
							if (v.when[n].test(w * 32 + b)) {
								word |= (1u << b);
							}
						}
						gpu_v.when_bits[n * 8 + w] = word;
					}
					gpu_v.then_val[n] = v.then[n];
				}
				gpu_v.when_wildcard = wildcard_mask;
			}
		}
	}

	rules_mapped->total_rules = current_rule_idx;
	rules_mapped->total_variants = current_var_idx;
}

void Vulkan::step(uint32_t frame_count, bool upload_pending) {
	if (!initialized) {
		return;
	}

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fence);

	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkDeviceSize grid_size_bytes = static_cast<VkDeviceSize>(width) * height * sizeof(uint32_t);

	// 0. If upload is pending (user painted or modified cells), copy staging_buffer to current_grid_buffer
	if (upload_pending) {
		VkBufferCopy upload_copy{};
		upload_copy.srcOffset = 0;
		upload_copy.dstOffset = 0;
		upload_copy.size = grid_size_bytes;
		vkCmdCopyBuffer(command_buffer, staging_buffer, current_grid_buffer, 1, &upload_copy);

		VkMemoryBarrier upload_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		upload_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		upload_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
							 &upload_barrier, 0, nullptr, 0, nullptr);
	}

	// 1. Reset stats (changed_cells = 0)
	vkCmdFillBuffer(command_buffer, stats_buffer, 0, sizeof(uint32_t), 0);

	// 2. Copy current_grid to next_grid so next_grid starts with current materials and updated = 0
	VkBufferCopy copy_region{};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = grid_size_bytes;
	vkCmdCopyBuffer(command_buffer, current_grid_buffer, next_grid_buffer, 1, &copy_region);

	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
						 &barrier, 0, nullptr, 0, nullptr);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0,
							nullptr);

	// 3. 25 Simulation phases
	const bool reverse_x = (frame_count % 2 == 0);
	const bool reverse_y = (frame_count % 2 == 1);

	GpuPushConstants pc{};
	pc.pass_type = 0;
	pc.frame_count = frame_count;
	pc.sim_width = width;
	pc.sim_height = height;
	pc.reverse_x = reverse_x ? 1 : 0;
	pc.reverse_y = reverse_y ? 1 : 0;

	VkMemoryBarrier phase_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	phase_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	phase_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

	for (uint32_t py_step = 0; py_step < 5; ++py_step) {
		uint32_t py = reverse_y ? (4 - py_step) : py_step;
		for (uint32_t px_step = 0; px_step < 5; ++px_step) {
			uint32_t px = reverse_x ? (4 - px_step) : px_step;

			pc.phase_x = px;
			pc.phase_y = py;

			vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

			uint32_t count_x = (width + 4 - px) / 5;
			uint32_t count_y = (height + 4 - py) / 5;
			uint32_t group_x = (count_x + 15) / 16;
			uint32_t group_y = (count_y + 15) / 16;

			vkCmdDispatch(command_buffer, group_x, group_y, 1);

			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &phase_barrier, 0, nullptr, 0, nullptr);
		}
	}

	// 4. Pass 1: Render and Finalize
	pc.pass_type = 1;
	vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

	uint32_t render_group_x = (width + 15) / 16;
	uint32_t render_group_y = (height + 15) / 16;
	vkCmdDispatch(command_buffer, render_group_x, render_group_y, 1);

	// 5. Barrier between compute shader write and copy to staging_buffer
	VkMemoryBarrier copy_staging_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	copy_staging_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	copy_staging_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1,
						 &copy_staging_barrier, 0, nullptr, 0, nullptr);

	// 6. Copy current_grid_buffer to staging_buffer so CPU cells can be kept in sync
	VkBufferCopy download_copy{};
	download_copy.srcOffset = 0;
	download_copy.dstOffset = 0;
	download_copy.size = grid_size_bytes;
	vkCmdCopyBuffer(command_buffer, current_grid_buffer, staging_buffer, 1, &download_copy);

	// 7. Barrier for host reads on display_buffer, stats_buffer, and staging_buffer
	VkMemoryBarrier host_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	host_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	host_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host_barrier, 0, nullptr, 0, nullptr);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(compute_queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}

void Vulkan::upload_grid(const uint8_t* materials, uint32_t count) {
	if (!initialized || !staging_mapped) {
		return;
	}

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fence);

	for (uint32_t i = 0; i < count; ++i) {
		staging_mapped[i] = static_cast<uint32_t>(materials[i]);
	}

	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkBufferCopy copy_region{};
	copy_region.size = static_cast<VkDeviceSize>(count) * sizeof(uint32_t);
	vkCmdCopyBuffer(command_buffer, staging_buffer, current_grid_buffer, 1, &copy_region);
	vkCmdCopyBuffer(command_buffer, staging_buffer, next_grid_buffer, 1, &copy_region);

	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
						 &barrier, 0, nullptr, 0, nullptr);

	// Pass 1: Render display
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0,
							nullptr);

	GpuPushConstants pc{};
	pc.pass_type = 1;
	pc.sim_width = width;
	pc.sim_height = height;
	vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

	uint32_t render_group_x = (width + 15) / 16;
	uint32_t render_group_y = (height + 15) / 16;
	vkCmdDispatch(command_buffer, render_group_x, render_group_y, 1);

	VkMemoryBarrier host_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	host_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	host_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
						 &host_barrier, 0, nullptr, 0, nullptr);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(compute_queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}

void Vulkan::download_grid(uint8_t* materials, uint32_t count) {
	if (!initialized || !staging_mapped) {
		return;
	}

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fence);

	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkBufferCopy copy_region{};
	copy_region.size = static_cast<VkDeviceSize>(count) * sizeof(uint32_t);
	vkCmdCopyBuffer(command_buffer, current_grid_buffer, staging_buffer, 1, &copy_region);

	VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0,
						 nullptr, 0, nullptr);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(compute_queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);

	for (uint32_t i = 0; i < count; ++i) {
		materials[i] = static_cast<uint8_t>(staging_mapped[i] & 0xFFu);
	}
}

void Vulkan::copy_staging_to_grid() {
	if (!initialized || !staging_mapped) {
		return;
	}

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fence);

	VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * sizeof(uint32_t);

	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	VkBufferCopy copy_region{};
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, staging_buffer, current_grid_buffer, 1, &copy_region);
	vkCmdCopyBuffer(command_buffer, staging_buffer, next_grid_buffer, 1, &copy_region);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(compute_queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}

void Vulkan::clear() {
	if (!initialized || !staging_mapped || !display_mapped) {
		return;
	}

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fence);

	VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * sizeof(uint32_t);
	std::memset(staging_mapped, 0, size);

	const uint32_t bg_color = MaterialManager::get_runtime_material(0).packed_color;
	for (size_t i = 0; i < width * height; ++i) {
		display_mapped[i] = bg_color;
	}

	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	vkCmdFillBuffer(command_buffer, current_grid_buffer, 0, size, 0);
	vkCmdFillBuffer(command_buffer, next_grid_buffer, 0, size, 0);
	vkCmdFillBuffer(command_buffer, stats_buffer, 0, sizeof(uint32_t) * 4, 0);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(compute_queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}

void Vulkan::refresh_display() {
	if (!initialized) {
		return;
	}

	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fence);

	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0,
							nullptr);

	GpuPushConstants pc{};
	pc.pass_type = 1;
	pc.sim_width = width;
	pc.sim_height = height;
	vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

	uint32_t render_group_x = (width + 15) / 16;
	uint32_t render_group_y = (height + 15) / 16;
	vkCmdDispatch(command_buffer, render_group_x, render_group_y, 1);

	VkMemoryBarrier host_barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
	host_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	host_barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1,
						 &host_barrier, 0, nullptr, 0, nullptr);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(compute_queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}

uint32_t* Vulkan::get_display_buffer() { return display_mapped; }

uint32_t Vulkan::get_changed_cells() {
	if (stats_mapped) {
		return stats_mapped[0];
	}
	return 0;
}

void Vulkan::keep_awake() {
	if (!initialized) {
		return;
	}

	vkResetFences(device, 1, &fence);
	vkResetCommandBuffer(command_buffer, 0);

	VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(command_buffer, &begin_info);

	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0,
							nullptr);

	GpuPushConstants pc{};
	pc.pass_type = 2;  // Pass 2: Empty GPU loop to keep GPU clocks boosted
	pc.sim_width = width;
	pc.sim_height = height;
	vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

	uint32_t group_x = (width + 15) / 16;
	uint32_t group_y = (height + 15) / 16;
	vkCmdDispatch(command_buffer, group_x, group_y, 1);

	vkEndCommandBuffer(command_buffer);

	VkSubmitInfo submit_info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	vkQueueSubmit(compute_queue, 1, &submit_info, fence);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
}