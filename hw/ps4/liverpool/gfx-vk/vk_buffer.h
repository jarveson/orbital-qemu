#pragma once

#include "gfx_vk_helpers.h"
#include <vulkan/vulkan.h>

extern "C" {
    struct vk_buffer* create_vk_buffer(gfx_vk_state* s, VkBufferUsageFlags usage, VkDeviceSize sz, VmaAllocationCreateInfo* aci);
    void destroy_vk_buffer(struct vk_buffer* buf);
    VkBuffer get_vk_buffer(vk_buffer* buf);
    void* map_vk_buffer(vk_buffer* buf);
    void unmap_vk_buffer(vk_buffer* buf);
}
