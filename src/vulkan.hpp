#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include <volk.h>

struct GpuPushConstants {
	uint32_t pass_type;			  // 0 = simulation phase, 1 = render & finalize
	uint32_t phase_x;			  // 0..4
	uint32_t phase_y;			  // 0..4
	uint32_t frame_count;		  // current frame index
	uint32_t sim_width;			  // grid width
	uint32_t sim_height;		  // grid height
	uint32_t reverse_x;			  // 0 or 1
	uint32_t reverse_y;			  // 0 or 1
};

struct GpuRuleVariant {
	uint32_t when_bits[200];  // 25 neighbors * 8 uints (256-bit bitset)
	uint32_t then_val[25];	  // 25 neighbor targets (255 = unchanged)
	uint32_t when_wildcard;	  // bitmask of wildcard neighbors (all 256 bits set)
	uint32_t pad[6];		  // align to 928 bytes
};

struct GpuRule {
	uint32_t variant_start;
	uint32_t variant_count;
	uint32_t chance_scaled;	 // 0..100000 (100000 = 100%)
	uint32_t pad;
};

struct GpuMaterial {
	uint32_t rule_start;
	uint32_t rule_count;
	uint32_t color;	 // packed 0xAABBGGRR
	uint32_t pad;
};

struct GpuRulesHeader {
	GpuMaterial materials[256];
	uint32_t total_rules;
	uint32_t total_variants;
	uint32_t pad0;
	uint32_t pad1;
	GpuRule rules[2048];
	GpuRuleVariant variants[4096];
};

class Vulkan {
public:
	Vulkan() = delete;

	static bool init(uint32_t sim_width, uint32_t sim_height);
	static bool resize(uint32_t new_width, uint32_t new_height);
	static void shutdown();
	static bool is_available() { return initialized; }

	static void update_rules();
	static void step(uint32_t frame_count, bool upload_pending = false);

	static void upload_grid(const uint8_t* materials, uint32_t count);
	static void download_grid(uint8_t* materials, uint32_t count);
	static void copy_staging_to_grid();
	static void clear();

	static uint32_t* get_display_buffer();
	static uint32_t* get_staging_buffer() { return staging_mapped; }
	static uint32_t get_changed_cells();

	static void refresh_display();
	static void keep_awake();
	static bool is_prevent_downclock_enabled() { return prevent_downclocking; }
	static void set_prevent_downclock(bool enable) { prevent_downclocking = enable; }

private:
	static bool initialized;
	static bool prevent_downclocking;
	static uint32_t width;
	static uint32_t height;

	static VkInstance instance;
	static VkPhysicalDevice physical_device;
	static VkDevice device;
	static VkQueue compute_queue;
	static uint32_t compute_queue_family;

	static VkCommandPool command_pool;
	static VkCommandBuffer command_buffer;
	static VkFence fence;

	static VkShaderModule compute_shader_module;
	static VkDescriptorSetLayout descriptor_set_layout;
	static VkPipelineLayout pipeline_layout;
	static VkPipeline compute_pipeline;
	static VkDescriptorPool descriptor_pool;
	static VkDescriptorSet descriptor_set;

	// Grid state buffers
	static VkBuffer current_grid_buffer;
	static VkDeviceMemory current_grid_memory;

	static VkBuffer next_grid_buffer;
	static VkDeviceMemory next_grid_memory;

	// Rules buffer
	static VkBuffer rules_buffer;
	static VkDeviceMemory rules_memory;
	static GpuRulesHeader* rules_mapped;

	// Display output buffer
	static VkBuffer display_buffer;
	static VkDeviceMemory display_memory;
	static uint32_t* display_mapped;

	// Stats buffer
	static VkBuffer stats_buffer;
	static VkDeviceMemory stats_memory;
	static uint32_t* stats_mapped;

	// Staging buffer for transfers
	static VkBuffer staging_buffer;
	static VkDeviceMemory staging_memory;
	static uint32_t* staging_mapped;

	static uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags preferred,
									 VkMemoryPropertyFlags required);
	static bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags preferred_properties,
							  VkMemoryPropertyFlags required_properties, VkBuffer& buffer,
							  VkDeviceMemory& buffer_memory);
};