/*
Copyright (C) 2018-2019 Krzysztof Kondrak

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "header/local.h"

// internal helper
static void
copyBuffer(const VkBuffer * src, VkBuffer * dst, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = QVk_CreateCommandBuffer(&vk_transferCommandPool,
								VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	QVk_BeginCommand(&commandBuffer);

	VkBufferCopy copyRegion = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = size
	};
	vkCmdCopyBuffer(commandBuffer, *src, *dst, 1, &copyRegion);

	QVk_SubmitCommand(&commandBuffer, &vk_device.transferQueue);
	vkFreeCommandBuffers(vk_device.logical, vk_transferCommandPool, 1,
						 &commandBuffer);
}

// internal helper
static void
createStagedBuffer(const void *data, VkDeviceSize size, qvkbuffer_t * dstBuffer,
				   qvkbufferopts_t bufferOpts)
{
	qvkstagingbuffer_t *stgBuffer;
	stgBuffer = (qvkstagingbuffer_t *) malloc(sizeof(qvkstagingbuffer_t));
	VK_VERIFY(QVk_CreateStagingBuffer(size, stgBuffer,
			   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			   VK_MEMORY_PROPERTY_HOST_CACHED_BIT));

	if (data)
	{
		void *dst;
		// staging buffers in vkQuake2 are required to be host coherent,
		// so no flushing/invalidation is involved
		dst = buffer_map(&stgBuffer->resource);
		memcpy(dst, data, (size_t) size);
		buffer_unmap(&stgBuffer->resource);
	}

	VK_VERIFY(QVk_CreateBuffer(size, dstBuffer, bufferOpts));
	copyBuffer(&stgBuffer->resource.buffer, &dstBuffer->resource.buffer, size);

	QVk_FreeStagingBuffer(stgBuffer);
	free(stgBuffer);
}

VkResult
QVk_CreateBuffer(VkDeviceSize size, qvkbuffer_t *dstBuffer,
				 const qvkbufferopts_t options)
{
	VkBufferCreateInfo bcInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = size,
		.usage = options.usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
	};

	// separate transfer queue makes sense only if the buffer is targetted
	// for being transfered to GPU, so ignore it if it's CPU-only
	uint32_t queueFamilies[] = {
		(uint32_t)vk_device.gfxFamilyIndex,
		(uint32_t)vk_device.transferFamilyIndex
	};

	if (vk_device.gfxFamilyIndex != vk_device.transferFamilyIndex)
	{
		bcInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bcInfo.queueFamilyIndexCount = 2;
		bcInfo.pQueueFamilyIndices = queueFamilies;
	}

	dstBuffer->currentOffset = 0;
	return buffer_create(&dstBuffer->resource, bcInfo,
						 options.reqMemFlags, options.prefMemFlags);
}

void
QVk_FreeBuffer(qvkbuffer_t *buffer)
{
	buffer_destroy(&buffer->resource);
	buffer->currentOffset = 0;
}

void
QVk_FreeStagingBuffer(qvkstagingbuffer_t *buffer)
{
	buffer_destroy(&buffer->resource);
	buffer->currentOffset = 0;
}

VkResult
QVk_CreateStagingBuffer(VkDeviceSize size, qvkstagingbuffer_t *dstBuffer,
						VkMemoryPropertyFlags reqMemFlags,
						VkMemoryPropertyFlags prefMemFlags)
{
	VkBufferCreateInfo bcInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = ROUNDUP(size, 1024),
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
	};

	reqMemFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	dstBuffer->currentOffset = 0;
	return buffer_create(&dstBuffer->resource, bcInfo, reqMemFlags,
						 prefMemFlags);
}

VkResult
QVk_CreateUniformBuffer(VkDeviceSize size, qvkbuffer_t *dstBuffer,
						VkMemoryPropertyFlags reqMemFlags,
						VkMemoryPropertyFlags prefMemFlags)
{
	qvkbufferopts_t dstOpts = {
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.reqMemFlags = reqMemFlags,
		.prefMemFlags = prefMemFlags,
	};

	dstOpts.reqMemFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	if((vk_device.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ||
	   (dstOpts.prefMemFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
	{
		dstOpts.prefMemFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	return QVk_CreateBuffer(size, dstBuffer, dstOpts);
}

void
QVk_CreateVertexBuffer(const void *data, VkDeviceSize size,
					   qvkbuffer_t *dstBuffer,
					   VkMemoryPropertyFlags reqMemFlags,
					   VkMemoryPropertyFlags prefMemFlags)
{
	qvkbufferopts_t dstOpts = {
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.reqMemFlags = reqMemFlags,
		.prefMemFlags = prefMemFlags,
	};

	if((vk_device.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ||
	   (dstOpts.prefMemFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
	{
		dstOpts.prefMemFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	createStagedBuffer(data, size, dstBuffer, dstOpts);
}

void
QVk_CreateIndexBuffer(const void *data, VkDeviceSize size,
					  qvkbuffer_t *dstBuffer,
					  VkMemoryPropertyFlags reqMemFlags,
					  VkMemoryPropertyFlags prefMemFlags)
{
	qvkbufferopts_t dstOpts = {
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.reqMemFlags = reqMemFlags,
		.prefMemFlags = prefMemFlags,
	};

	if((vk_device.properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ||
	   (dstOpts.prefMemFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
	{
		dstOpts.prefMemFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}

	createStagedBuffer(data, size, dstBuffer, dstOpts);
}
