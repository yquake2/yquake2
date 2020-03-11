/*
Copyright (C) 2018 Christoph Schied

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "header/vk_util.h"
#include "header/vk_local.h"

#include <assert.h>

static uint32_t
get_memory_type(uint32_t mem_req_type_bits, VkMemoryPropertyFlags mem_prop)
{
	for(uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
		if(mem_req_type_bits & (1 << i)) {
			if((vk_device.mem_properties.memoryTypes[i].propertyFlags & mem_prop) == mem_prop)
				return i;
		}
	}
	return -1;
}

VkResult
buffer_create(BufferResource_t *buf,
		VkDeviceSize size,
		VkBufferCreateInfo buf_create_info,
		VkMemoryPropertyFlags mem_properties,
		VkMemoryPropertyFlags mem_preferences)
{
	assert(size > 0);
	assert(buf);
	VkResult result = VK_SUCCESS;
	int memory_index;

	buf->size = size;
	buf->is_mapped = 0;

	result = vkCreateBuffer(vk_device.logical, &buf_create_info, NULL, &buf->buffer);
	if(result != VK_SUCCESS) {
		R_Printf(PRINT_ALL, "%s:%d: VkResult verification: %s\n",
			__func__, __LINE__, QVk_GetError(result));
		goto fail_buffer;
	}
	assert(buf->buffer != VK_NULL_HANDLE);

	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(vk_device.logical, buf->buffer, &mem_reqs);

	memory_index = get_memory_type(mem_reqs.memoryTypeBits, mem_properties | mem_preferences);
	if (memory_index == -1)
	{
		memory_index = get_memory_type(mem_reqs.memoryTypeBits, mem_properties);
	}
	if (memory_index == -1)
	{
		R_Printf(PRINT_ALL, "%s:%d: Have found required memory type.\n",
			__func__, __LINE__);
		goto fail_mem_alloc;
	}

	VkMemoryAllocateInfo mem_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = get_memory_type(mem_reqs.memoryTypeBits, mem_properties)
	};

	result = vkAllocateMemory(vk_device.logical, &mem_alloc_info, NULL, &buf->memory);
	if(result != VK_SUCCESS) {
		R_Printf(PRINT_ALL, "%s:%d: VkResult verification: %s\n",
			__func__, __LINE__, QVk_GetError(result));
		goto fail_mem_alloc;
	}

	assert(buf->memory != VK_NULL_HANDLE);

	result = vkBindBufferMemory(vk_device.logical, buf->buffer, buf->memory, 0);
	if(result != VK_SUCCESS) {
		R_Printf(PRINT_ALL, "%s:%d: VkResult verification: %s\n",
			__func__, __LINE__, QVk_GetError(result));
		goto fail_bind_buf_memory;
	}

	return VK_SUCCESS;

fail_bind_buf_memory:
	vkFreeMemory(vk_device.logical, buf->memory, NULL);
fail_mem_alloc:
	vkDestroyBuffer(vk_device.logical, buf->buffer, NULL);
fail_buffer:
	buf->buffer = VK_NULL_HANDLE;
	buf->memory = VK_NULL_HANDLE;
	buf->size   = 0;
	return result;
}

VkResult
image_create(ImageResource_t *img,
		VkImageCreateInfo img_create_info,
		VkMemoryPropertyFlags mem_properties,
		VkMemoryPropertyFlags mem_preferences)
{
	assert(img);
	VkResult result = VK_SUCCESS;
	int memory_index;

	result = vkCreateImage(vk_device.logical, &img_create_info, NULL, &img->image);
	if(result != VK_SUCCESS) {
		R_Printf(PRINT_ALL, "%s:%d: VkResult verification: %s\n",
			__func__, __LINE__, QVk_GetError(result));
		goto fail_buffer;
	}
	assert(img->image != VK_NULL_HANDLE);

	VkMemoryRequirements mem_reqs;
	vkGetImageMemoryRequirements(vk_device.logical, img->image, &mem_reqs);
	img->size = mem_reqs.size;

	memory_index = get_memory_type(mem_reqs.memoryTypeBits, mem_properties | mem_preferences);
	if (memory_index == -1)
	{
		memory_index = get_memory_type(mem_reqs.memoryTypeBits, mem_properties);
	}
	if (memory_index == -1)
	{
		R_Printf(PRINT_ALL, "%s:%d: Have found required memory type.\n",
			__func__, __LINE__);
		goto fail_mem_alloc;
	}

	VkMemoryAllocateInfo mem_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_reqs.size,
		.memoryTypeIndex = get_memory_type(mem_reqs.memoryTypeBits, mem_properties)
	};

	result = vkAllocateMemory(vk_device.logical, &mem_alloc_info, NULL, &img->memory);
	if(result != VK_SUCCESS) {
		R_Printf(PRINT_ALL, "%s:%d: VkResult verification: %s\n",
			__func__, __LINE__, QVk_GetError(result));
		goto fail_mem_alloc;
	}

	assert(img->memory != VK_NULL_HANDLE);

	result = vkBindImageMemory(vk_device.logical, img->image, img->memory, 0);
	if(result != VK_SUCCESS) {
		R_Printf(PRINT_ALL, "%s:%d: VkResult verification: %s\n",
			__func__, __LINE__, QVk_GetError(result));
		goto fail_bind_buf_memory;
	}

	return VK_SUCCESS;

fail_bind_buf_memory:
	vkFreeMemory(vk_device.logical, img->memory, NULL);
fail_mem_alloc:
	vkDestroyImage(vk_device.logical, img->image, NULL);
fail_buffer:
	img->image = VK_NULL_HANDLE;
	img->memory = VK_NULL_HANDLE;
	img->size   = 0;
	return result;
}

VkResult
buffer_destroy(BufferResource_t *buf)
{
	assert(!buf->is_mapped);
	if(buf->memory != VK_NULL_HANDLE)
		vkFreeMemory(vk_device.logical, buf->memory, NULL);
	if(buf->buffer != VK_NULL_HANDLE)
	vkDestroyBuffer(vk_device.logical, buf->buffer, NULL);
	buf->buffer = VK_NULL_HANDLE;
	buf->memory = VK_NULL_HANDLE;
	buf->size   = 0;

	return VK_SUCCESS;
}

VkResult
image_destroy(ImageResource_t *img)
{
	if(img->memory != VK_NULL_HANDLE)
		vkFreeMemory(vk_device.logical, img->memory, NULL);
	if(img->image != VK_NULL_HANDLE)
	vkDestroyImage(vk_device.logical, img->image, NULL);
	img->image = VK_NULL_HANDLE;
	img->memory = VK_NULL_HANDLE;
	img->size   = 0;

	return VK_SUCCESS;
}

VkResult
buffer_flush(BufferResource_t *buf)
{
	VkResult result = VK_SUCCESS;

	VkMappedMemoryRange ranges[1] = {{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = buf->memory,
		.offset = 0,
		.size = buf->size
	}};
	result = vkFlushMappedMemoryRanges(vk_device.logical, 1, ranges);
	return result;
}

VkResult
buffer_invalidate(BufferResource_t *buf)
{
	VkResult result = VK_SUCCESS;

	VkMappedMemoryRange ranges[1] = {{
		.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		.memory = buf->memory,
		.offset = 0,
		.size = buf->size
	}};
	result = vkInvalidateMappedMemoryRanges(vk_device.logical, 1, ranges);
	return result;

}

void *
buffer_map(BufferResource_t *buf)
{
	assert(!buf->is_mapped);
	buf->is_mapped = 1;
	void *ret = NULL;
	assert(buf->memory != VK_NULL_HANDLE);
	assert(buf->size > 0);
	VK_VERIFY(vkMapMemory(vk_device.logical, buf->memory,
		0 /*offset*/, buf->size, 0 /*flags*/, &ret));
	return ret;
}

void
buffer_unmap(BufferResource_t *buf)
{
	assert(buf->is_mapped);
	buf->is_mapped = 0;
	vkUnmapMemory(vk_device.logical, buf->memory);
}
