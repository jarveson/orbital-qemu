#pragma once

#include "gfx_vk_helpers.h"
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif
    struct vk_texture* create_vk_texture(gfx_vk_state* s, uint32_t width, uint32_t height,
            VkFormat format, VkImageLayout initialLayout, VkComponentMapping mapping, VmaAllocationCreateInfo* aci);
    void remove_vk_texture(struct vk_texture* tex);
    VkImage get_vk_texture_image(struct vk_texture* tex);
    VkImageView get_vk_texture_view(struct vk_texture* tex);
    void change_vk_texture_layout(struct vk_texture* tex, VkCommandBuffer cmd, VkImageLayout newLayout, 
            VkAccessFlags newAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

#ifdef __cplusplus
}
#endif
