#include "vk_buffer.h"

struct vk_buffer {
    gfx_vk_state* _state = nullptr;
    VkBuffer _buf = VK_NULL_HANDLE;
    VkBufferCreateInfo _info = {};
    VmaAllocation _allocation;

    vk_buffer(gfx_vk_state* s, VkBufferUsageFlags usage, VkDeviceSize sz, VmaAllocationCreateInfo* aci) 
        : _state(s) {

        _info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        _info.usage = usage;
        _info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        _info.size = sz;

        CHECK_VK_RES(vmaCreateBuffer(_state->allocator, &_info, aci, &_buf, &_allocation, nullptr));
    }

    void* map() {
        void* ptr;
        CHECK_VK_RES(vmaMapMemory(_state->allocator, _allocation, &ptr));
        return ptr;
    }

    void unmap() {
        vmaUnmapMemory(_state->allocator, _allocation);
    }

    ~vk_buffer() {
        if (_buf != VK_NULL_HANDLE) {
            vmaDestroyBuffer(_state->allocator, _buf, _allocation);
        }
    }
};

extern "C" {
    struct vk_buffer* create_vk_buffer(gfx_vk_state* s, VkBufferUsageFlags usage, VkDeviceSize sz, VmaAllocationCreateInfo* aci) {
        return new vk_buffer(s, usage, sz, aci);
    }

    void destroy_vk_buffer(struct vk_buffer* buf) {
        delete buf;
    }

    void* map_vk_buffer(vk_buffer* buf) {
        return buf->map();
    }

    VkBuffer get_vk_buffer(vk_buffer* buf) {
        return buf->_buf;
    }

    void unmap_vk_buffer(vk_buffer* buf) {
        buf->unmap();
    }
}