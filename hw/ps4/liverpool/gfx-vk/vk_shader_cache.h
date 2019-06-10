#pragma once

#include "gfx_vk_helpers.h"

#include "../gca/gcn.h"
#include "../gca/gcn_parser.h"
#include "../gca/gcn_resource.h"
#include "../gca/gcn_translator.h"
#include "../gca/gfx_7_2_d.h"

#ifdef __cplusplus
extern "C" {
#endif
    struct vk_shader;

    struct vk_shader_cache* create_vk_shader_cache(struct gfx_vk_state* state);
    struct vk_shader* get_vk_shader(struct vk_shader_cache* c, gcn_stage_t stage, void* data, size_t sz);
    struct gcn_analyzer_t* get_vk_shader_analysis(struct vk_shader* s);
    VkShaderModule get_vk_shader_module(struct vk_shader* s);
#ifdef __cplusplus
}
#endif
