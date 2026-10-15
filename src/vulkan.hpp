#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include <volk.h>

struct Cell;

struct GpuPushConstants {
	uint32_t pass_type;
	uint32_t phase_x;
	uint32_t phase_y;
	uint32_t frame_count;
	uint32_t sim_width;
	uint32_t sim_height;
	uint32_t reverse_x;
	uint32_t reverse_y;
	uint32_t step_x;
	uint32_t step_y;
	int32_t heatmap_attr;
	uint32_t heatmap_flags;
	float heatmap_view_min;
	float heatmap_view_max;
	uint32_t attr_layout;
	uint32_t attr_types;
};

struct GpuAttrCondition {
	float const_val;
	uint32_t attr_index;
	uint32_t op;
	uint32_t target;
};

struct GpuCondition {
	int32_t dx;
	int32_t dy;
	uint32_t allowed_mask[8];
	GpuAttrCondition attr_conds[2];
};

struct GpuAttrAction {
	uint32_t expr_start;
	uint32_t expr_count;
	uint32_t pad[2];
};

struct GpuAction {
	int32_t dx;
	int32_t dy;
	uint32_t target_material;
	uint32_t updated;
	GpuAttrAction attr_actions[3];
};

struct GpuRuleVariant {
	uint32_t cond_start;
	uint32_t cond_count;
	uint32_t act_start;
	uint32_t act_count;
};

struct GpuRule {
	uint32_t variant_start;
	uint32_t variant_count;
	uint32_t chance_scaled;
	uint32_t flags;
};

struct GpuGradientStop {
	float r;
	float g;
	float b;
	float a;
	float pos;
	float pad[3];
};

struct GpuColorModifier {
	int32_t attr_index;
	float attr_min;
	float attr_max;
	uint32_t stop_count;
	GpuGradientStop stops[8];
};

struct GpuMaterial {
	uint32_t rule_start;
	uint32_t rule_count;
	uint32_t color;
	float default_attr[3];
	uint32_t pad[2];
	GpuColorModifier modifiers[2];
};

struct GpuAttrMeta {
	float min_val;
	float max_val;
	float default_val;
	uint32_t type;
};

struct GpuExprOp {
	uint32_t type;
	int32_t dx;
	int32_t dy;
	float val;
};

struct GpuRulesHeader {
	GpuMaterial materials[256];
	GpuAttrMeta attr_meta[3];
	uint32_t total_rules;
	uint32_t total_variants;
	uint32_t total_conditions;
	uint32_t total_actions;
	uint32_t total_expr_ops;
	uint32_t heatmap_stop_count;
	uint32_t pad_h[2];
	GpuGradientStop heatmap_stops[8];
	GpuRule rules[2048];
	GpuRuleVariant variants[4096];
	GpuCondition conditions[32768];
	GpuAction actions[16384];
	GpuExprOp expr_ops[4096];
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

	static void upload_grid(const Cell* cells, uint32_t count);
	static void download_grid(Cell* cells, uint32_t count);
	static void copy_staging_to_grid();
	static void clear();

	static uint32_t* get_display_buffer();
	static Cell* get_staging_cells() { return staging_mapped; }
	static uint32_t get_changed_cells();

	static void set_heatmap_mode(int attr_idx) { heatmap_attr_index = attr_idx; }
	static void refresh_display(bool upload_pending = false);
	static void keep_awake();
	static bool is_prevent_downclock_enabled() { return prevent_downclocking; }
	static void set_prevent_downclock(bool enable) { prevent_downclocking = enable; }
	static bool is_integrated_gpu() { return is_integrated; }

private:
	static bool initialized;
	static bool is_integrated;
	static bool prevent_downclocking;
	static int heatmap_attr_index;
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

	static VkBuffer current_grid_buffer;
	static VkDeviceMemory current_grid_memory;

	static VkBuffer next_grid_buffer;
	static VkDeviceMemory next_grid_memory;

	static VkBuffer rules_buffer;
	static VkDeviceMemory rules_memory;
	static GpuRulesHeader* rules_mapped;

	static VkBuffer display_buffer;
	static VkDeviceMemory display_memory;
	static uint32_t* display_mapped;

	static VkBuffer stats_buffer;
	static VkDeviceMemory stats_memory;
	static uint32_t* stats_mapped;

	static VkBuffer staging_buffer;
	static VkDeviceMemory staging_memory;
	static Cell* staging_mapped;

	static uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags preferred,
									 VkMemoryPropertyFlags required);
	static bool create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags preferred_properties,
							  VkMemoryPropertyFlags required_properties, VkBuffer& buffer,
							  VkDeviceMemory& buffer_memory);
};