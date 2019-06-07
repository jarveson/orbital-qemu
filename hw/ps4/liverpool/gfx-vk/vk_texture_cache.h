#ifndef PS4_LIVERPOOL_GFX_VK_TEX_CACHE
#define PS4_LIVERPOOL_GFX_VK_TEX_CACHE

#include "gfx_vk_helpers.h"
#ifdef __cplusplus
extern "C" {
#endif
    struct vk_texture_cache* create_vk_texture_cache(gfx_vk_state* s);
    void destroy_texture_cache(struct vk_texture_cache* c);
    struct vk_texture* texture_cache_create_texture(struct vk_texture_cache* c, VkCommandBuffer cmd, void* data, VkDeviceSize sz, uint32_t width, uint32_t height,
            VkFormat format, VkComponentMapping mapping);
    void texture_cache_clear_upload_buffer(struct vk_texture_cache* c); 
#ifdef __cplusplus
}
#endif

#endif