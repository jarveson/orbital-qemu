#pragma once

#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <assert.h>

#define CHECK_VK_RES(e) { VkResult _res = (e); if (_res != VK_SUCCESS) {printf("%s: fail, error: %d", __FUNCTION__, _res); assert(0);};}

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gfx_vk_state {
    VkDevice device;
    VmaAllocator allocator;
    uint32_t memidx_host_vis_coherent;
    uint32_t memidx_device_local;
    struct vk_texture_cache* tex_cache;
    struct vk_shader_cache* shader_cache;
} gfx_vk_state;

struct vk_texture_cache* create_texture_cache(gfx_vk_state* state);

#ifdef __cplusplus
}
#endif
