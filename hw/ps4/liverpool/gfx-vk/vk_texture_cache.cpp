#include "vk_texture_cache.h"
#include "vk_texture.h"
#include "vk_buffer.h"
#include "xxhash.h"

#include <queue>
#include <memory>
#include <unordered_map>
#include <utility>

struct vk_texture_cache {
    std::queue<std::pair<uint64_t, vk_texture*>> _residentTextures;
    std::queue<vk_buffer*> _uploadBuffers;

    std::unordered_map<uint64_t, vk_texture*> _hashmap;

    VmaPool _uploadPool;
    VmaPool _texturePool;
    gfx_vk_state* _state = nullptr;
    vk_texture_cache(gfx_vk_state* s) 
        : _state(s) {

        VmaPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.blockSize = 256 * 0x100000;
        poolCreateInfo.memoryTypeIndex = _state->memidx_host_vis_coherent;
        poolCreateInfo.maxBlockCount = 1;
        poolCreateInfo.flags = VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;

        CHECK_VK_RES(vmaCreatePool(_state->allocator, &poolCreateInfo, &_uploadPool));

        poolCreateInfo = {};
        poolCreateInfo.blockSize = 256 * 0x100000;
        poolCreateInfo.memoryTypeIndex = _state->memidx_device_local;
        poolCreateInfo.maxBlockCount = 1;
        poolCreateInfo.flags = VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT;

        CHECK_VK_RES(vmaCreatePool(_state->allocator, &poolCreateInfo, &_texturePool));
    }

    vk_texture* create_texture(VkCommandBuffer cmd, void* data, VkDeviceSize sz, uint32_t width, uint32_t height,
            VkFormat format, VkComponentMapping mapping) {
        
        uint64_t hash = XXH64(data, sz, 0);

        auto check = _hashmap.find(hash);
        if (check != _hashmap.end()) {
            return check->second;
        }
        
        // create staging buffer 
        VmaAllocationCreateInfo aci = {};
        aci.pool = _uploadPool;

        auto buf = create_vk_buffer(_state, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, sz, &aci);

        void* mapptr = map_vk_buffer(buf);
        memcpy(mapptr, data, sz);
        unmap_vk_buffer(buf);

        aci = {};
        aci.pool = _texturePool;
        auto tex = create_vk_texture(_state, width, height, format, VK_IMAGE_LAYOUT_UNDEFINED, mapping, &aci);

        change_vk_texture_layout(tex, cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = width;
        bufferCopyRegion.imageExtent.height = height;
        bufferCopyRegion.imageExtent.depth = 1;
        bufferCopyRegion.bufferOffset = 0;

        vkCmdCopyBufferToImage(cmd,
            get_vk_buffer(buf), get_vk_texture_image(tex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &bufferCopyRegion);

        change_vk_texture_layout(tex, cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        printf("new texture uploading: width: 0x%x height: 0x%x sz: 0x%x\n", width, height, sz);
        _hashmap[hash] = tex;
        _uploadBuffers.push(buf);
        _residentTextures.push(std::pair<uint64_t, vk_texture*>(hash, tex));

        return tex;
    }

    void clear_upload() {
        while(!_uploadBuffers.empty()) {
            destroy_vk_buffer(_uploadBuffers.front());
            _uploadBuffers.pop();
        }
    }

    ~vk_texture_cache() {
        vmaDestroyPool(_state->allocator, _uploadPool);
        vmaDestroyPool(_state->allocator, _texturePool);
    }
};

extern "C" {
    struct vk_texture_cache* create_vk_texture_cache(gfx_vk_state* s) {
        return new vk_texture_cache(s);
    }

    void destroy_texture_cache(struct vk_texture_cache* c) {
        delete c;
    }

    struct vk_texture* texture_cache_create_texture(struct vk_texture_cache* c, VkCommandBuffer cmd, void* data, VkDeviceSize sz, uint32_t width, uint32_t height,
            VkFormat format, VkComponentMapping mapping) {
        return c->create_texture(cmd, data, sz, width, height, format, mapping);
    }

    void texture_cache_clear_upload_buffer(struct vk_texture_cache* c) {
        c->clear_upload();
    }
}