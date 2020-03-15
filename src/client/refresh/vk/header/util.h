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

#ifndef  __VK_UTIL_H__
#define  __VK_UTIL_H__

#include <vulkan/vulkan.h>

typedef struct BufferResource_s {
	VkBuffer buffer;
	VkDeviceMemory memory;
	size_t size;
	int is_mapped;
} BufferResource_t;

typedef struct ImageResource_s {
	VkImage image;
	VkDeviceMemory memory;
	size_t size;
} ImageResource_t;

VkResult buffer_create(BufferResource_t *buf,
		VkDeviceSize size,
		VkBufferCreateInfo buf_create_info,
		VkMemoryPropertyFlags mem_properties,
		VkMemoryPropertyFlags mem_preferences);

VkResult buffer_destroy(BufferResource_t *buf);
void buffer_unmap(BufferResource_t *buf);
void *buffer_map(BufferResource_t *buf);
VkResult buffer_flush(BufferResource_t *buf);
VkResult buffer_invalidate(BufferResource_t *buf);

VkResult image_create(ImageResource_t *img,
		VkImageCreateInfo img_create_info,
		VkMemoryPropertyFlags mem_properties,
		VkMemoryPropertyFlags mem_preferences);
VkResult image_destroy(ImageResource_t *img);

#endif  /*__VK_UTIL_H__*/
