#include "vk_shader_cache.h"

#include "../gca/gcn_analyzer.h"
#include "xxhash.h"
#include <vulkan/vulkan.h>
#include <stdio.h>

#include <unordered_map>

// todo; pull this out
struct vk_shader {
    VkShaderModule module;

    // Analyzer contains metadata required to update resources
    gcn_analyzer_t analyzer;
};

struct vk_shader_cache {
    gfx_vk_state* _state = nullptr;
    gcn_parser_t _parser;
    std::unordered_map<uint64_t, vk_shader*> _cache;

    vk_shader_cache(gfx_vk_state* state) 
        : _state(state) {
    }

    vk_shader* create_shader(gcn_stage_t stage, void* data, size_t sz) {
        printf("%s: Translating shader...\n", __FUNCTION__);

        // todo: find end of actual shadercode
        uint64_t hash = XXH64(data, sz, 0);

        vk_shader* shader = new vk_shader();

        gcn_analyzer_t *analyzer;
        gcn_translator_t *translator;
        uint32_t spirv_size;
        uint8_t *spirv_data;
        VkResult res;

        // Pass #1: Analyze the bytecode
        gcn_parser_init(&_parser);
        analyzer = &shader->analyzer;
        gcn_analyzer_init(analyzer);
        gcn_parser_parse(&_parser, (uint8_t*)data, &gcn_analyzer_callbacks, analyzer);

        // Pass #2: Translate the bytecode
        gcn_parser_init(&_parser);
        translator = gcn_translator_create(analyzer, stage);
        gcn_parser_parse(&_parser, (uint8_t*)data, &gcn_translator_callbacks, translator);
        spirv_data = gcn_translator_dump(translator, &spirv_size);

        // Create module
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = spirv_size;
        createInfo.pCode = (uint32_t*)spirv_data;
        res = vkCreateShaderModule(_state->device, &createInfo, NULL, &shader->module);
        if (res != VK_SUCCESS) {
            fprintf(stderr, "%s: vkCreateShaderModule failed!\n", __FUNCTION__);
            return nullptr;
        }
        
        _cache.emplace(hash, shader);
        return shader;
    }

    vk_shader* get_shader(gcn_stage_t stage, void* data, size_t sz) {
        uint64_t hash = XXH64(data, sz, 0);
        auto it = _cache.find(hash);
        if (it == _cache.end())
            return create_shader(stage, data, sz);
        return it->second;
    }
};

extern "C" {
    struct vk_shader_cache* create_vk_shader_cache(struct gfx_vk_state* state) {
        return new vk_shader_cache(state);
    }
    
    struct vk_shader* get_vk_shader(struct vk_shader_cache* c, gcn_stage_t stage, void* data, size_t sz) {
        return c->get_shader(stage, data, sz);
    }

    gcn_analyzer_t* get_vk_shader_analysis(vk_shader* s) {
        return &s->analyzer;
    }

    VkShaderModule get_vk_shader_module(vk_shader* s) {
        return s->module;
    }
}
