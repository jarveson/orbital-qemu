/*
 * QEMU model of Liverpool's GFX device.
 *
 * Copyright (c) 2017-2018 Alexandro Sanchez Bach
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "lvp_gfx_shader.h"
#include "lvp_gfx_format.h"
#include "lvp_gfx.h"
#include "lvp_gart.h"
#include "gca/gcn.h"
#include "gca/gcn_parser.h"
#include "gca/gcn_resource.h"
#include "gca/gcn_translator.h"
#include "gca/gfx_7_2_d.h"
#include "ui/vk-helpers.h"

#include "qemu-common.h"
#include "exec/memory.h"

#include "gfx-vk/vk_texture_cache.h"
#include "gfx-vk/vk_shader_cache.h"

#include <vulkan/vulkan.h>

#include <string.h>

void gfx_shader_translate(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx, gcn_stage_t stage)
{
    gart_state_t *gart = gfx->gart;
    uint64_t pgm_addr, pgm_size;
    uint8_t *pgm_data;
    hwaddr mapped_size;

    memset(shader, 0, sizeof(gfx_shader_t));
    shader->stage = stage;

    switch (stage) {
    case GCN_STAGE_PS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_PS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_PS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_VS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_VS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_VS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_GS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_GS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_GS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_ES:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_ES];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_ES] | (pgm_addr << 32);
        break;
    case GCN_STAGE_HS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_HS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_HS] | (pgm_addr << 32);
        break;
    case GCN_STAGE_LS:
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_HI_LS];
        pgm_addr = gfx->mmio[mmSPI_SHADER_PGM_LO_LS] | (pgm_addr << 32);
        break;
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, stage);
        assert(0);
    }
    pgm_addr <<= 8;

    // Map shader bytecode into host userspace
    pgm_size = 0x1000; // TODO
    mapped_size = pgm_size;
    pgm_data = address_space_map(gart->as[vmid], pgm_addr, &mapped_size, false);
    switch (stage) {
    case GCN_STAGE_PS:
    case GCN_STAGE_VS:
        shader->vkShader = get_vk_shader(gfx->vk->cache.shader_cache, stage, pgm_data, mapped_size);
        break;
    case GCN_STAGE_GS:
    case GCN_STAGE_ES:
    case GCN_STAGE_HS:
    case GCN_STAGE_LS:
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, stage);
        assert(0);
        break;
    }
    address_space_unmap(gart->as[vmid], pgm_data, mapped_size, false, mapped_size);
}

void gfx_shader_translate_descriptors(
    gfx_shader_t *shader, gfx_state_t *gfx, VkDescriptorSetLayout *descSetLayout)
{
    gcn_analyzer_t *analyzer;
    VkDescriptorSetLayoutBinding *layoutBinding;
    VkDescriptorSetLayoutBinding layoutBindings[48];
    VkShaderStageFlags flags;
    VkDevice dev = gfx->vk->device;
    VkResult res;
    size_t binding = 0;
    size_t i;

    analyzer = get_vk_shader_analysis(shader->vkShader);
    flags = 0;
    switch (shader->stage) {
    case GCN_STAGE_PS:
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case GCN_STAGE_VS:
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
        break;
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, shader->stage);
        assert(0);
    }

    for (i = 0; i < analyzer->res_vh_count; i++) {
        layoutBinding = &layoutBindings[binding];
        layoutBinding->binding = binding;
        layoutBinding->descriptorCount = 1;
        layoutBinding->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        layoutBinding->pImmutableSamplers = NULL;
        layoutBinding->stageFlags = flags;
        binding += 1;
    }
    for (i = 0; i < analyzer->res_th_count; i++) {
        layoutBinding = &layoutBindings[binding];
        layoutBinding->binding = binding;
        layoutBinding->descriptorCount = 1;
        layoutBinding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        layoutBinding->pImmutableSamplers = NULL;
        layoutBinding->stageFlags = flags;
        binding += 1;
    }
    for (i = 0; i < analyzer->res_sh_count; i++) {
        layoutBinding = &layoutBindings[binding];
        layoutBinding->binding = binding;
        layoutBinding->descriptorCount = 1;
        layoutBinding->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        layoutBinding->pImmutableSamplers = NULL;
        layoutBinding->stageFlags = flags;
        binding += 1;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = binding;
    layoutInfo.pBindings = layoutBindings;

    res = vkCreateDescriptorSetLayout(dev, &layoutInfo, NULL, descSetLayout);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateDescriptorSetLayout failed!\n", __FUNCTION__);
    }
}

static void gfx_shader_update_vh(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    struct gcn_resource_vh_t *vh, vk_resource_vh_t *vkres)
{
    gart_state_t *gart = gfx->gart;
    VkDevice dev = gfx->vk->device;
    VkResult res;

    // Create buffer, destroying previous one (if any)
    if (vkres->buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(dev, vkres->buf, NULL);
        vkFreeMemory(dev, vkres->mem, NULL);
    }
    VkBufferCreateInfo bufInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .size = (vh->stride ? vh->stride : 1) * vh->num_records,
    };
    if (vkCreateBuffer(dev, &bufInfo, NULL, &vkres->buf) != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateBuffer failed!\n", __FUNCTION__);
        return;
    }

    // Allocate memory for buffer
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(dev, vkres->buf, &memReqs);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = vk_find_memory_type(gfx->vk, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    res = vkAllocateMemory(dev, &allocInfo, NULL, &vkres->mem);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkAllocateMemory failed!\n", __FUNCTION__);
        return;
    }
    res = vkBindBufferMemory(dev, vkres->buf, vkres->mem, 0);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkBindBufferMemory failed!\n", __FUNCTION__);
        return;
    }

    // Copy memory for buffer
    void *data_dst;
    void *data_src;
    uint64_t addr_src;
    hwaddr size_src = bufInfo.size;
    vkMapMemory(dev, vkres->mem, 0, bufInfo.size, 0, &data_dst);
    addr_src = vh->base;
    data_src = address_space_map(gart->as[vmid], addr_src, &size_src, false);
    memcpy(data_dst, data_src, (size_t)bufInfo.size);
    address_space_unmap(gart->as[vmid], data_src, size_src, false, size_src);
    vkUnmapMemory(dev, vkres->mem);
}

static void gfx_shader_update_th(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    struct gcn_resource_th_t *th, vk_resource_th_t *vkres)
{
    gart_state_t *gart = gfx->gart;

    VkFormat format = getVkFormat_byImgDataNumFormat(th->dfmt, th->nfmt);
    VkComponentMapping mapping = getVkCompMapping_byGcnMapping(th->dst_sel_x, th->dst_sel_y, th->dst_sel_z, th->dst_sel_w);
    size_t texelSize = getTexelSize_fromImgFormat(th->dfmt);

    // todo: dont use temp buffer for this, but my head is hurting from this c/c++ interop so it stays here for now
    void *data_src;
    uint64_t addr_src;
    uint32_t srcpitch = th->ext.pitch == 0 ? texelSize * (th->width+1) : texelSize * (th->ext.pitch+1);
    uint32_t dstpitch = texelSize * (th->width+1);

    hwaddr size_src = (srcpitch * (th->height+1));
    addr_src = th->base256 << 8;
    data_src = address_space_map(gart->as[vmid], addr_src, &size_src, false);

    uint32_t dstsize = dstpitch * (th->height+1);
    void *data_dst = malloc(dstsize);
    if (srcpitch != dstpitch) {
        void* tmpSrc = data_src;
        void* tmpDst = data_dst;
        for(int i = 0; i < th->height+1; ++i) {
            memcpy(tmpDst, tmpSrc, dstpitch);
            tmpSrc += srcpitch;
            tmpDst += dstpitch;
        }
    }
    else
        memcpy(data_dst, data_src, (size_t)dstsize);

    address_space_unmap(gart->as[vmid], data_src, size_src, false, size_src);
    vkres->texture = texture_cache_create_texture(gfx->vk->cache.tex_cache, gfx->vkcmdbuf, data_dst, dstsize, th->width+1, th->height+1, format, mapping);
    free(data_dst);
}

static void gfx_shader_update_sh(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    struct gcn_resource_sh_t *sh, vk_resource_sh_t *vkres)
{
    VkDevice dev = gfx->vk->device;
    VkResult res;

    if (vkres->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(dev, vkres->sampler, NULL);
    }

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    res = vkCreateSampler(dev, &samplerInfo, NULL, &vkres->sampler);
    if (res != VK_SUCCESS) {
        fprintf(stderr, "%s: vkCreateSampler failed!\n", __FUNCTION__);
        return;
    }
}

void gfx_shader_update(gfx_shader_t *shader, uint32_t vmid, gfx_state_t *gfx,
    VkDescriptorSet descSet)
{
    gcn_dependency_context_t dep_ctxt = {};
    gcn_analyzer_t *analyzer;
    gcn_resource_t *res;
    VkDevice dev = gfx->vk->device;
    int binding;
    size_t i;

    // Prepare dependency context
    switch (shader->stage) {
    case GCN_STAGE_PS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_PS_0];
        break;
    case GCN_STAGE_VS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_VS_0];
        break;
    case GCN_STAGE_GS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_GS_0];
        break;
    case GCN_STAGE_ES:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_ES_0];
        break;
    case GCN_STAGE_HS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_HS_0];
        break;
    case GCN_STAGE_LS:
        dep_ctxt.user_sgpr = &gfx->mmio[mmSPI_SHADER_USER_DATA_LS_0];
        break;
    default:
        fprintf(stderr, "%s: Unsupported shader stage (%d)!\n", __FUNCTION__, shader->stage);
        assert(0);
    }

    // Update resources
    analyzer = get_vk_shader_analysis(shader->vkShader);
    for (i = 0; i < analyzer->res_vh_count; i++) {
        res = analyzer->res_vh[i];
        if (!gcn_resource_update(res, &dep_ctxt))
            continue;
        gfx_shader_update_vh(shader, vmid, gfx, &res->vh, &shader->vk_res_vh[i]);
    }
    for (i = 0; i < analyzer->res_th_count; i++) {
        res = analyzer->res_th[i];
        if (!gcn_resource_update(res, &dep_ctxt))
            continue;
        gfx_shader_update_th(shader, vmid, gfx, &res->th, &shader->vk_res_th[i]);
    }
    for (i = 0; i < analyzer->res_sh_count; i++) {
        res = analyzer->res_sh[i];
        if (!gcn_resource_update(res, &dep_ctxt))
            continue;
        gfx_shader_update_sh(shader, vmid, gfx, &res->sh, &shader->vk_res_sh[i]);
    }

    // Update descriptors
    binding = 0;
    for (i = 0; i < analyzer->res_vh_count; i++) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = shader->vk_res_vh[i].buf;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, NULL);
        binding += 1;
    }
    for (i = 0; i < analyzer->res_th_count; i++) {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.imageView = get_vk_texture_view(shader->vk_res_th[i].texture);
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, NULL);
        binding += 1;
    }
    for (i = 0; i < analyzer->res_sh_count; i++) {
        VkDescriptorImageInfo samplerInfo = {};
        samplerInfo.sampler = shader->vk_res_sh[i].sampler;

        VkWriteDescriptorSet descriptorWrite = {};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descSet;
        descriptorWrite.dstBinding = binding;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &samplerInfo;

        vkUpdateDescriptorSets(dev, 1, &descriptorWrite, 0, NULL);
        binding += 1;
    }
}
