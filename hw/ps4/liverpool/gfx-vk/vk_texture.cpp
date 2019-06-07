#include "vk_texture.h"
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>

struct vk_texture {
    gfx_vk_state* _state = nullptr;
    VkImage _image = VK_NULL_HANDLE;
    VkImageCreateInfo _info = {};
    VkImageLayout _currentLayout;
    VkImageView _view;
    VkAccessFlags _accessFlags = 0;

    VmaAllocation _allocation;

    vk_texture(gfx_vk_state* s, uint32_t width, uint32_t height, VkFormat format, 
        VkImageLayout initialLayout, VkComponentMapping mapping, VmaAllocationCreateInfo* aci)
        : _state(s), _currentLayout(initialLayout) {
        
        _info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        _info.imageType = VK_IMAGE_TYPE_2D;
        _info.format = format;
        _info.extent.width = width;
        _info.extent.height = height;
        _info.extent.depth = 1;
        _info.mipLevels = 1;
        _info.arrayLayers = 1;
        _info.samples = VK_SAMPLE_COUNT_1_BIT;
        _info.tiling = VK_IMAGE_TILING_OPTIMAL;
        _info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        _info.flags = 0;
        _info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        _info.initialLayout = initialLayout;

        CHECK_VK_RES(vmaCreateImage(s->allocator, &_info, aci, &_image, &_allocation, nullptr));

        // todo: move this out of here
        VkImageViewCreateInfo imgView = {};
        imgView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imgView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgView.format = format;
        imgView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgView.subresourceRange.baseMipLevel = 0;
        imgView.subresourceRange.levelCount = 1;
        imgView.subresourceRange.baseArrayLayer = 0;
        imgView.subresourceRange.layerCount = 1;
        imgView.image = _image;
        imgView.components = mapping;
        CHECK_VK_RES(vkCreateImageView(s->device, &imgView, nullptr, &_view));
    }

    void change_layout(VkCommandBuffer cmd, VkImageLayout newLayout, VkAccessFlags newAccess, 
        VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)  {

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = _image;
        barrier.srcAccessMask = _accessFlags;
        barrier.dstAccessMask = newAccess;
        barrier.oldLayout = _currentLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        barrier.subresourceRange.baseMipLevel = 0,
        barrier.subresourceRange.levelCount = 1,
        barrier.subresourceRange.baseArrayLayer = 0,
        barrier.subresourceRange.layerCount = 1,

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        _currentLayout = newLayout;
        _accessFlags = newAccess;
    }

    ~vk_texture() {
        if (_image != VK_NULL_HANDLE) {
            vmaDestroyImage(_state->allocator, _image, _allocation);
        }
    }
};

extern "C" {
    struct vk_texture* create_vk_texture(gfx_vk_state* s, uint32_t width, uint32_t height,
            VkFormat format, VkImageLayout initialLayout, VkComponentMapping mapping, VmaAllocationCreateInfo* aci) {
        return new vk_texture(s, width, height, format, initialLayout, mapping, aci);
    }

    VkImageView get_vk_texture_view(struct vk_texture* tex) {
        return tex->_view;
    }

    void remove_vk_texture(struct vk_texture* tex) {
        delete tex;
    }

    VkImage get_vk_texture_image(struct vk_texture* tex) {
        return tex->_image;
    }

    void change_vk_texture_layout(struct vk_texture* tex, VkCommandBuffer cmd, VkImageLayout newLayout, 
            VkAccessFlags newAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        tex->change_layout(cmd, newLayout, newAccess, srcStage, dstStage);
    }
}