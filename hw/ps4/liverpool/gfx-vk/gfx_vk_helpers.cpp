#include "gfx_vk_helpers.h"
#include "vk_texture_cache.h"

extern "C" {
    struct vk_texture_cache* create_texture_cache(gfx_vk_state* state) {
        return create_vk_texture_cache(state);
    }
}