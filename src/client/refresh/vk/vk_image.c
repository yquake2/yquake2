/*
Copyright (C) 1997-2001 Id Software, Inc.
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

image_t		vktextures[MAX_VKTEXTURES];
int		numvktextures;
// texture for storing raw image data (cinematics, endscreens, etc.)
qvktexture_t vk_rawTexture = QVVKTEXTURE_INIT;

static byte			 intensitytable[256];
static unsigned char overbrightable[256];

static cvar_t	*intensity;
extern cvar_t	*vk_mip_nearfilter;

unsigned	d_8to24table[256];

// default global texture and lightmap samplers
qvksampler_t vk_current_sampler = S_MIPMAP_LINEAR;
qvksampler_t vk_current_lmap_sampler = S_MIPMAP_LINEAR;

// internal helper
static VkImageAspectFlags getDepthStencilAspect(VkFormat depthFormat)
{
	switch (depthFormat)
	{
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D16_UNORM_S8_UINT:
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	default:
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	}
}

// internal helper
static void transitionImageLayout(const VkCommandBuffer *cmdBuffer, const VkQueue *queue, const qvktexture_t *texture, const VkImageLayout oldLayout, const VkImageLayout newLayout)
{
	VkPipelineStageFlags srcStage = 0;
	VkPipelineStageFlags dstStage = 0;

	VkImageMemoryBarrier imgBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = NULL,
		.oldLayout = oldLayout,
		.newLayout = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = texture->resource.image,
		.subresourceRange.baseMipLevel = 0, // no mip mapping levels
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
		.subresourceRange.levelCount = texture->mipLevels
	};

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		imgBarrier.subresourceRange.aspectMask = getDepthStencilAspect(texture->format);
	}
	else
	{
		imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imgBarrier.srcAccessMask = 0;
		imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	// transiton that may occur when updating existing image
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imgBarrier.srcAccessMask = 0;
		imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		if (vk_device.transferQueue == vk_device.gfxQueue)
		{
			imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else
		{
			if (vk_device.transferQueue == *queue)
			{
				// if the image is exclusively shared, start queue ownership transfer process (release) - only for VK_SHARING_MODE_EXCLUSIVE
				imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imgBarrier.dstAccessMask = 0;
				imgBarrier.srcQueueFamilyIndex = vk_device.transferFamilyIndex;
				imgBarrier.dstQueueFamilyIndex = vk_device.gfxFamilyIndex;
				srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			}
			else
			{
				// continuing queue transfer (acquisition) - this will only happen for VK_SHARING_MODE_EXCLUSIVE images
				if (texture->sharingMode == VK_SHARING_MODE_EXCLUSIVE)
				{
					imgBarrier.srcAccessMask = 0;
					imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					imgBarrier.srcQueueFamilyIndex = vk_device.transferFamilyIndex;
					imgBarrier.dstQueueFamilyIndex = vk_device.gfxFamilyIndex;
					srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
				else
				{
					imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
			}
		}
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		imgBarrier.srcAccessMask = 0;
		imgBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		imgBarrier.srcAccessMask = 0;
		imgBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else
	{
		assert(0 && !"Invalid image stage!");
	}

	vkCmdPipelineBarrier(*cmdBuffer, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &imgBarrier);
}

// internal helper
static void generateMipmaps(const VkCommandBuffer *cmdBuffer, const qvktexture_t *texture, uint32_t width, uint32_t height)
{
	int32_t mipWidth = width;
	int32_t mipHeight = height;
	VkFilter mipFilter = vk_mip_nearfilter->value > 0 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;

	VkImageMemoryBarrier imgBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = NULL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = texture->resource.image,
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.levelCount = 1,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1
	};

	// copy rescaled mip data between consecutive levels (each higher level is half the size of the previous level)
	for (uint32_t i = 1; i < texture->mipLevels; ++i)
	{
		imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imgBarrier.subresourceRange.baseMipLevel = i - 1;

		vkCmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &imgBarrier);

		VkImageBlit blit = {
			.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.srcSubresource.mipLevel = i - 1,
			.srcSubresource.baseArrayLayer = 0,
			.srcSubresource.layerCount = 1,
			.srcOffsets[0] = { 0, 0, 0 },
			.srcOffsets[1] = { mipWidth, mipHeight, 1 },
			.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.dstSubresource.mipLevel = i,
			.dstSubresource.baseArrayLayer = 0,
			.dstSubresource.layerCount = 1,
			.dstOffsets[0] = { 0, 0, 0 },
			.dstOffsets[1] = { mipWidth > 1 ? mipWidth >> 1 : 1,
							  mipHeight > 1 ? mipHeight >> 1 : 1, 1 } // each mip level is half the size of the previous level
		};

		// src image == dst image, because we're blitting between different mip levels of the same image
		vkCmdBlitImage(*cmdBuffer, texture->resource.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			texture->resource.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, mipFilter);

		imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		vkCmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imgBarrier);

		// avoid zero-sized mip levels
		if (mipWidth > 1)  mipWidth >>= 1;
		if (mipHeight > 1) mipHeight >>= 1;
	}

	imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgBarrier.subresourceRange.baseMipLevel = texture->mipLevels - 1;

	vkCmdPipelineBarrier(*cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &imgBarrier);
}

// internal helper
static void createTextureImage(qvktexture_t *dstTex, const unsigned char *data, uint32_t width, uint32_t height)
{
	int unifiedTransferAndGfx = vk_device.transferQueue == vk_device.gfxQueue ? 1 : 0;
	// assuming 32bit images
	uint32_t imageSize = width * height * 4;

	VkBuffer staging_buffer;
	VkCommandBuffer command_buffer;
	uint32_t staging_offset;
	void *imgData = QVk_GetStagingBuffer(imageSize, 4, &command_buffer, &staging_buffer, &staging_offset);
	if (!imgData)
		Sys_Error("%s: Staging buffers is smaller than image: %d.\n", __func__, imageSize);

	memcpy(imgData, data, (size_t)imageSize);

	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	// set extra image usage flag if we're dealing with mipmapped image - will need it for copying data between mip levels
	if (dstTex->mipLevels > 1)
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VK_VERIFY(QVk_CreateImage(width, height, dstTex->format, VK_IMAGE_TILING_OPTIMAL, imageUsage, dstTex));

	transitionImageLayout(&command_buffer, &vk_device.transferQueue, dstTex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	// copy buffer to image
	VkBufferImageCopy region = {
		.bufferOffset = staging_offset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.imageSubresource.mipLevel = 0,
		.imageSubresource.baseArrayLayer = 0,
		.imageSubresource.layerCount = 1,
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { width, height, 1 }
	};

	vkCmdCopyBufferToImage(command_buffer, staging_buffer, dstTex->resource.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	if (dstTex->mipLevels > 1)
	{
		// vkCmdBlitImage requires a queue with GRAPHICS_BIT present
		generateMipmaps(&command_buffer, dstTex, width, height);
	}
	else
	{
		// for non-unified transfer and graphics, this step begins queue ownership transfer to graphics queue (for exclusive sharing only)
		if (unifiedTransferAndGfx || dstTex->sharingMode == VK_SHARING_MODE_EXCLUSIVE)
			transitionImageLayout(&command_buffer, &vk_device.transferQueue, dstTex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if (!unifiedTransferAndGfx)
		{
			transitionImageLayout(&command_buffer, &vk_device.gfxQueue, dstTex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}
}

VkResult QVk_CreateImageView(const VkImage *image, VkImageAspectFlags aspectFlags, VkImageView *imageView, VkFormat format, uint32_t mipLevels)
{
	VkImageViewCreateInfo ivCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.image = *image,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = format,
		.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
		.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
		.subresourceRange.aspectMask = aspectFlags,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.layerCount = 1,
		.subresourceRange.levelCount = mipLevels
	};

	return vkCreateImageView(vk_device.logical, &ivCreateInfo, NULL, imageView);
}

VkResult QVk_CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, qvktexture_t *texture)
{
	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.extent.width = width,
		.extent.height = height,
		.extent.depth = 1,
		.mipLevels = texture->mipLevels,
		.arrayLayers = 1,
		.format = format,
		.tiling = tiling,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.samples = texture->sampleCount,
		.flags = 0
	};

	uint32_t queueFamilies[] = { (uint32_t)vk_device.gfxFamilyIndex, (uint32_t)vk_device.transferFamilyIndex };
	if (vk_device.gfxFamilyIndex != vk_device.transferFamilyIndex)
	{
		imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		imageInfo.queueFamilyIndexCount = 2;
		imageInfo.pQueueFamilyIndices = queueFamilies;
	}

	texture->sharingMode = imageInfo.sharingMode;
	return image_create(
		&texture->resource, imageInfo,
		/*mem_properties*/ 0,
		/*mem_preferences*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void QVk_CreateDepthBuffer(VkSampleCountFlagBits sampleCount, qvktexture_t *depthBuffer)
{
	depthBuffer->format = QVk_FindDepthFormat();
	depthBuffer->sampleCount = sampleCount;

	VK_VERIFY(QVk_CreateImage(vk_swapchain.extent.width, vk_swapchain.extent.height, depthBuffer->format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthBuffer));
	VK_VERIFY(QVk_CreateImageView(&depthBuffer->resource.image, getDepthStencilAspect(depthBuffer->format), &depthBuffer->imageView, depthBuffer->format, depthBuffer->mipLevels));
}

void QVk_CreateColorBuffer(VkSampleCountFlagBits sampleCount, qvktexture_t *colorBuffer, int extraFlags)
{
	colorBuffer->format = vk_swapchain.format;
	colorBuffer->sampleCount = sampleCount;

	VK_VERIFY(QVk_CreateImage(vk_swapchain.extent.width, vk_swapchain.extent.height, colorBuffer->format, VK_IMAGE_TILING_OPTIMAL, extraFlags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, colorBuffer));
	VK_VERIFY(QVk_CreateImageView(&colorBuffer->resource.image, VK_IMAGE_ASPECT_COLOR_BIT, &colorBuffer->imageView, colorBuffer->format, colorBuffer->mipLevels));
}

void QVk_CreateTexture(qvktexture_t *texture, const unsigned char *data, uint32_t width, uint32_t height, qvksampler_t samplerType)
{
	createTextureImage(texture, data, width, height);
	VK_VERIFY(QVk_CreateImageView(&texture->resource.image, VK_IMAGE_ASPECT_COLOR_BIT, &texture->imageView, texture->format, texture->mipLevels));

	// create descriptor set for the texture
	VkDescriptorSetAllocateInfo dsAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = NULL,
		.descriptorPool = vk_descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &vk_samplerDescSetLayout
	};

	VK_VERIFY(vkAllocateDescriptorSets(vk_device.logical, &dsAllocInfo, &texture->descriptorSet));

	// attach sampler
	QVk_UpdateTextureSampler(texture, samplerType);
}

void QVk_UpdateTextureData(qvktexture_t *texture, const unsigned char *data, uint32_t offset_x, uint32_t offset_y, uint32_t width, uint32_t height)
{
	int unifiedTransferAndGfx = vk_device.transferQueue == vk_device.gfxQueue ? 1 : 0;
	// assuming 32bit images
	uint32_t imageSize = width * height * 4;

	VkBuffer staging_buffer;
	VkCommandBuffer command_buffer;
	uint32_t staging_offset;
	void *imgData = QVk_GetStagingBuffer(imageSize, 4, &command_buffer, &staging_buffer, &staging_offset);
	if (!imgData)
		Sys_Error("%s: Staging buffers is smaller than image: %d.\n", __func__, imageSize);

	memcpy(imgData, data, (size_t)imageSize);

	transitionImageLayout(&command_buffer, &vk_device.transferQueue, texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	// copy buffer to image
	VkBufferImageCopy region = {
		.bufferOffset = staging_offset,
		.bufferRowLength = 0,
		.bufferImageHeight = 0,
		.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.imageSubresource.mipLevel = 0,
		.imageSubresource.baseArrayLayer = 0,
		.imageSubresource.layerCount = 1,
		.imageOffset = { offset_x, offset_y, 0 },
		.imageExtent = { width, height, 1 }
	};

	vkCmdCopyBufferToImage(command_buffer, staging_buffer, texture->resource.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	if (texture->mipLevels > 1)
	{
		// vkCmdBlitImage requires a queue with GRAPHICS_BIT present
		generateMipmaps(&command_buffer, texture, width, height);
	}
	else
	{
		// for non-unified transfer and graphics, this step begins queue ownership transfer to graphics queue (for exclusive sharing only)
		if (unifiedTransferAndGfx || texture->sharingMode == VK_SHARING_MODE_EXCLUSIVE)
			transitionImageLayout(&command_buffer, &vk_device.transferQueue, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if (!unifiedTransferAndGfx)
		{
			transitionImageLayout(&command_buffer, &vk_device.gfxQueue, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}
}

static void QVk_ReleaseTexture(qvktexture_t *texture)
{
	QVk_SubmitStagingBuffers();
	if (vk_device.logical != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(vk_device.logical);
	}

	if (texture->imageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(vk_device.logical, texture->imageView, NULL);
		texture->imageView = VK_NULL_HANDLE;
	}

	if (texture->resource.image != VK_NULL_HANDLE)
		image_destroy(&texture->resource);

	if (texture->descriptorSet != VK_NULL_HANDLE)
		vkFreeDescriptorSets(vk_device.logical, vk_descriptorPool, 1, &texture->descriptorSet);

	texture->descriptorSet = VK_NULL_HANDLE;
}

void QVk_ReadPixels(uint8_t *dstBuffer, uint32_t width, uint32_t height)
{
	BufferResource_t buff;
	uint8_t *pMappedData;
	VkCommandBuffer cmdBuffer;
	extern int vk_activeBufferIdx;

	VkBufferCreateInfo bcInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = width * height * 4,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL,
	};

	VK_VERIFY(buffer_create(&buff, bcInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT));

	cmdBuffer = QVk_CreateCommandBuffer(&vk_commandPool[vk_activeBufferIdx], VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	VK_VERIFY(QVk_BeginCommand(&cmdBuffer));

	// transition the current swapchain image to be a source of data transfer to our buffer
	VkImageMemoryBarrier imgBarrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = NULL,
		.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = vk_swapchain.images[vk_activeBufferIdx],
		.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel = 0,
		.subresourceRange.baseArrayLayer = 0,
		.subresourceRange.layerCount = 1,
		.subresourceRange.levelCount = 1
	};

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &imgBarrier);

	VkBufferImageCopy region = {
		.bufferOffset = 0,
		.bufferRowLength = width,
		.bufferImageHeight = height,
		.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.imageSubresource.mipLevel = 0,
		.imageSubresource.baseArrayLayer = 0,
		.imageSubresource.layerCount = 1,
		.imageOffset = { 0, 0, 0 },
		.imageExtent = { width, height, 1 }
	};

	// copy the swapchain image
	vkCmdCopyImageToBuffer(cmdBuffer, vk_swapchain.images[vk_activeBufferIdx], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buff.buffer, 1, &region);
	VK_VERIFY(vkDeviceWaitIdle(vk_device.logical));
	QVk_SubmitCommand(&cmdBuffer, &vk_device.gfxQueue);

	// store image in destination buffer
	pMappedData = buffer_map(&buff);
	memcpy(dstBuffer, pMappedData, width * height * 4);
	buffer_unmap(&buff);

	buffer_destroy(&buff);
}

/*
===============
Vk_ImageList_f
===============
*/
void	Vk_ImageList_f (void)
{
	int		i;
	image_t	*image;
	int		texels;

	R_Printf(PRINT_ALL, "------------------\n");
	texels = 0;

	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (image->vk_texture.resource.image == VK_NULL_HANDLE)
			continue;
		texels += image->upload_width*image->upload_height;
		switch (image->type)
		{
		case it_skin:
			R_Printf(PRINT_ALL, "M");
			break;
		case it_sprite:
			R_Printf(PRINT_ALL, "S");
			break;
		case it_wall:
			R_Printf(PRINT_ALL, "W");
			break;
		case it_pic:
			R_Printf(PRINT_ALL, "P");
			break;
		default:
			R_Printf(PRINT_ALL, " ");
			break;
		}

		R_Printf(PRINT_ALL, " %4i %4i RGB: %s (%dx%d)\n",
			image->upload_width, image->upload_height, image->name,
			image->width, image->height);
	}
	R_Printf(PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
}

typedef struct
{
	char *name;
	qvksampler_t samplerType;
} vkmode_t;

static vkmode_t modes[] = {
	{"VK_NEAREST", S_NEAREST },
	{"VK_LINEAR", S_LINEAR },
	{"VK_MIPMAP_NEAREST", S_MIPMAP_NEAREST },
	{"VK_MIPMAP_LINEAR", S_MIPMAP_LINEAR }
};

#define NUM_VK_MODES (sizeof(modes) / sizeof (vkmode_t))

/*
 ===============
 Vk_TextureMode
 ===============
 */
void Vk_TextureMode( char *string )
{
	int		i,j;
	image_t	*image;
	static char prev_mode[32] = { "VK_MIPMAP_LINEAR" };

	for (i = 0; i < NUM_VK_MODES; i++)
	{
		if (!Q_stricmp(modes[i].name, string))
			break;
	}

	if (i == NUM_VK_MODES)
	{
		R_Printf(PRINT_ALL, "bad filter name (valid values: VK_NEAREST, VK_LINEAR, VK_MIPMAP_NEAREST, VK_MIPMAP_LINEAR)\n");
		ri.Cvar_Set("vk_texturemode", prev_mode);
		return;
	}

	memcpy(prev_mode, string, strlen(string));
	prev_mode[strlen(string)] = '\0';

	i += vk_aniso->value > 0 ? NUM_VK_MODES : 0;
	vk_current_sampler = i;

	vkDeviceWaitIdle(vk_device.logical);
	for (j = 0, image = vktextures; j < numvktextures; j++, image++)
	{
		// skip console characters - we want them unfiltered at all times
		if (image->vk_texture.resource.image != VK_NULL_HANDLE && Q_stricmp(image->name, "pics/conchars.pcx"))
			QVk_UpdateTextureSampler(&image->vk_texture, i);
	}

	if (vk_rawTexture.resource.image != VK_NULL_HANDLE)
		QVk_UpdateTextureSampler(&vk_rawTexture, i);
}

/*
 ===============
 Vk_LmapTextureMode
 ===============
 */
void Vk_LmapTextureMode( char *string )
{
	int		i,j;
	static char prev_mode[32] = { "VK_MIPMAP_LINEAR" };

	for (i = 0; i < NUM_VK_MODES; i++)
	{
		if (!Q_stricmp(modes[i].name, string))
			break;
	}

	if (i == NUM_VK_MODES)
	{
		R_Printf(PRINT_ALL, "bad filter name (valid values: VK_NEAREST, VK_LINEAR, VK_MIPMAP_NEAREST, VK_MIPMAP_LINEAR)\n");
		ri.Cvar_Set("vk_lmaptexturemode", prev_mode);
		return;
	}

	memcpy(prev_mode, string, strlen(string));
	prev_mode[strlen(string)] = '\0';

	i += vk_aniso->value > 0 ? NUM_VK_MODES : 0;
	vk_current_lmap_sampler = i;

	vkDeviceWaitIdle(vk_device.logical);
	for (j = 0; j < MAX_LIGHTMAPS*2; j++)
	{
		if (vk_state.lightmap_textures[j].resource.image != VK_NULL_HANDLE)
			QVk_UpdateTextureSampler(&vk_state.lightmap_textures[j], i);
	}
}

/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short x, y;
} floodfill_t;

/* must be a power of 2 */
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP(off, dx, dy)	\
	{ \
		if (pos[off] == fillcolor) \
		{ \
			pos[off] = 255;	\
			fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
			inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
		} \
		else if (pos[off] != 255) \
		{ \
			fdc = pos[off];	\
		} \
	}

/*
 * Fill background pixels so mipmapping doesn't have haloes
 */
static void
FloodFillSkin(byte *skin, int skinwidth, int skinheight)
{
	byte fillcolor = *skin; /* assume this is the pixel to fill */
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0;
	int filledcolor = 0;
	int i;

	/* attempt to find opaque black */
	for (i = 0; i < 256; ++i)
	{
		if (LittleLong(d_8to24table[i]) == (255 << 0)) /* alpha 1.0 */
		{
			filledcolor = i;
			break;
		}
	}

	/* can't fill to filled color or to transparent color (used as visited marker) */
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int x = fifo[outpt].x, y = fifo[outpt].y;
		int fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)
		{
			FLOODFILL_STEP(-1, -1, 0);
		}

		if (x < skinwidth - 1)
		{
			FLOODFILL_STEP(1, 1, 0);
		}

		if (y > 0)
		{
			FLOODFILL_STEP(-skinwidth, 0, -1);
		}

		if (y < skinheight - 1)
		{
			FLOODFILL_STEP(skinwidth, 0, 1);
		}

		skin[x + skinwidth * y] = fdc;
	}
}


/*
================
Vk_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void Vk_LightScaleTexture (byte *in, int inwidth, int inheight)
{
	int		i, c;
	byte	*p;

	p = (byte *)in;

	c = inwidth*inheight;
	for (i=0 ; i<c ; i++, p+=4)
	{
		p[0] = overbrightable[intensitytable[p[0]]];
		p[1] = overbrightable[intensitytable[p[1]]];
		p[2] = overbrightable[intensitytable[p[2]]];
	}
}

/*
===============
Vk_Upload32

Returns number of mip levels
===============
*/
static uint32_t Vk_Upload32 (byte *data, int width, int height, imagetype_t type,
							 byte **texBuffer, int *upload_width, int *upload_height)
{
	int	scaled_width, scaled_height;
	int	miplevel = 1;

	*texBuffer = NULL;

	for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
		;
	for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
		;
	if (type != it_pic)
	{
		// let people sample down the world textures for speed
		scaled_width >>= (int)vk_picmip->value;
		scaled_height >>= (int)vk_picmip->value;
	}

	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

	*texBuffer = malloc(scaled_width * scaled_height * 4);
	if (!*texBuffer)
		ri.Sys_Error(ERR_DROP, "%s: too big", __func__);

	*upload_width = scaled_width;
	*upload_height = scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		memcpy(*texBuffer, data, scaled_width * scaled_height * 4);
	}
	else
	{
		ResizeSTB(data, width, height,
				  *texBuffer, scaled_width, scaled_height);
	}

	// world textures
	if (type != it_pic && type != it_sky)
	{
		Vk_LightScaleTexture(*texBuffer, scaled_width, scaled_height);
	}

	while (scaled_width > 1 || scaled_height > 1)
	{
		scaled_width >>= 1;
		scaled_height >>= 1;
		if (scaled_width < 1)
			scaled_width = 1;
		if (scaled_height < 1)
			scaled_height = 1;
		miplevel++;
	}

	return miplevel;
}

/*
===============
Vk_Upload8

Returns number of mip levels
===============
*/

static uint32_t Vk_Upload8 (byte *data, int width, int height, imagetype_t type,
							byte **texBuffer, int *upload_width, int *upload_height)
{
	unsigned	*trans;
	int			i, s;
	int 		miplevel;

	s = width * height;

	trans = malloc(s * sizeof(*trans));
	if (!trans)
		ri.Sys_Error(ERR_DROP, "%s: too large", __func__);

	for (i = 0; i < s; i++)
	{
		int	p;

		p = data[i];
		trans[i] = d_8to24table[p];
	}

	if (type != it_sky && type != it_wall)
	{
		for (i = 0; i < s; i++)
		{
			int     p;

			p = data[i];

			if (p == 255)
			{	// transparent, so scan around for another color
				// to avoid alpha fringes
				// FIXME: do a full flood fill so mips work...
				if (i > width && data[i - width] != 255)
					p = data[i - width];
				else if (i < s - width && data[i + width] != 255)
					p = data[i + width];
				else if (i > 0 && data[i - 1] != 255)
					p = data[i - 1];
				else if (i < s - 1 && data[i + 1] != 255)
					p = data[i + 1];
				else
					p = 0;
				// copy rgb components
				((byte *)&trans[i])[0] = ((byte *)&d_8to24table[p])[0];
				((byte *)&trans[i])[1] = ((byte *)&d_8to24table[p])[1];
				((byte *)&trans[i])[2] = ((byte *)&d_8to24table[p])[2];
			}
		}
	}

	// optimize 8bit images only when we forced such logic
	if (vk_retexturing->value >= 2)
	{
		SmoothColorImage(trans, s, s >> 7);
	}

	miplevel = Vk_Upload32((byte *)trans, width, height, type, texBuffer, upload_width, upload_height);
	free(trans);
	return miplevel;
}


/*
================
Vk_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *
Vk_LoadPic(char *name, byte *pic, int width, int realwidth,
	   int height, int realheight, imagetype_t type,
	   int bits)
{
	image_t		*image;
	byte		*texBuffer;
	int		upload_width, upload_height;

	qboolean nolerp = false;

	if (vk_nolerp_list != NULL && vk_nolerp_list->string != NULL)
	{
		nolerp = strstr(vk_nolerp_list->string, name) != NULL;
	}

	{
		int		i;
		// find a free image_t
		for (i = 0, image = vktextures; i<numvktextures; i++, image++)
		{
			if (image->vk_texture.resource.image == VK_NULL_HANDLE)
				break;
		}
		if (i == numvktextures)
		{
			if (numvktextures == MAX_VKTEXTURES)
				ri.Sys_Error(ERR_DROP, "%s: MAX_VKTEXTURES", __func__);
			numvktextures++;
		}
		image = &vktextures[i];
	}

	if (strlen(name) >= sizeof(image->name))
		ri.Sys_Error(ERR_DROP, "%s: \"%s\" is too long", __func__, name);
	strcpy(image->name, name);
	image->registration_sequence = registration_sequence;
	// zero-clear Vulkan texture handle
	QVVKTEXTURE_CLEAR(image->vk_texture);
	image->width = realwidth;
	image->height = realheight;
	image->type = type;

	if (type == it_skin && bits == 8)
		FloodFillSkin(pic, width, height);

	if (bits == 8)
	{
		// resize 8bit images only when we forced such logic
		if (vk_retexturing->value >= 2)
		{
			byte *image_converted = malloc(width * height * 4);
			scale2x(pic, image_converted, width, height);
			image->vk_texture.mipLevels = Vk_Upload8(image_converted, width * 2, height * 2, image->type, &texBuffer, &upload_width, &upload_height);
			free(image_converted);
		}
		else
		{
			image->vk_texture.mipLevels = Vk_Upload8(pic, width, height, image->type, &texBuffer, &upload_width, &upload_height);
		}
	}
	else
		image->vk_texture.mipLevels = Vk_Upload32(pic, width, height, image->type, &texBuffer, &upload_width, &upload_height);

	image->upload_width = upload_width;		// after power of 2 and scales
	image->upload_height = upload_height;

	QVk_CreateTexture(&image->vk_texture, (unsigned char*)texBuffer,
		image->upload_width, image->upload_height,
		nolerp ? S_NEAREST : vk_current_sampler);
	QVk_DebugSetObjectName((uint64_t)image->vk_texture.resource.image,
		VK_OBJECT_TYPE_IMAGE, va("Image: %s", name));
	QVk_DebugSetObjectName((uint64_t)image->vk_texture.imageView,
		VK_OBJECT_TYPE_IMAGE_VIEW, va("Image View: %s", name));
	QVk_DebugSetObjectName((uint64_t)image->vk_texture.descriptorSet,
		VK_OBJECT_TYPE_DESCRIPTOR_SET, va("Descriptor Set: %s", name));
	QVk_DebugSetObjectName((uint64_t)image->vk_texture.resource.memory,
		VK_OBJECT_TYPE_DEVICE_MEMORY, "Memory: game textures");

	if (texBuffer)
	{
		free(texBuffer);
	}
	return image;
}


/*
================
Vk_LoadWal
================
*/
static image_t *Vk_LoadWal (char *name, imagetype_t type)
{
	miptex_t	*mt;
	int			width, height, ofs;
	image_t		*image;

	ri.FS_LoadFile (name, (void **)&mt);
	if (!mt)
	{
		R_Printf(PRINT_ALL, "%s: can't load %s\n", __func__, name);
		return r_notexture;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);

	image = Vk_LoadPic(name, (byte *)mt + ofs,
			   width, width,
			   height, height,
			   type, 8);

	ri.FS_FreeFile ((void *)mt);

	return image;
}

static image_t*
Vk_LoadHiColorImage(char *name, const char* namewe, const char *ext, imagetype_t type)
{
	image_t	*image = NULL;
	byte *pic = NULL;
	int realwidth = 0, realheight = 0;
	int width = 0, height = 0;

	if (strcmp(ext, "pcx") == 0)
	{
		/* Get size of the original texture */
		GetPCXInfo(name, &realwidth, &realheight);
	}
	else if (strcmp(ext, "wal") == 0)
	{
		/* Get size of the original texture */
		GetWalInfo(name, &realwidth, &realheight);
	}

	/* try to load a tga, png or jpg (in that order/priority) */
	if (  LoadSTB(namewe, "tga", &pic, &width, &height)
	   || LoadSTB(namewe, "png", &pic, &width, &height)
	   || LoadSTB(namewe, "jpg", &pic, &width, &height) )
	{
		if (width >= realwidth && height >= realheight)
		{
			if (realheight == 0 || realwidth == 0)
			{
				realheight = height;
				realwidth = width;
			}

			image = Vk_LoadPic(name, pic,
					   width, realwidth,
					   height, realheight,
					   type, 32);
		}
	}

	if (pic)
	{
		free(pic);
	}

	return image;
}

static image_t*
Vk_LoadImage(char *name, const char* namewe, const char *ext, imagetype_t type)
{
	image_t	*image = NULL;

	// with retexturing
	if (vk_retexturing->value)
	{
		image = Vk_LoadHiColorImage(name, namewe, ext, type);
	}

	if (!image)
	{
		byte	*pic;
		int	width, height;

		//
		// load the pic from disk
		//
		pic = NULL;
		if (!strcmp(ext, "pcx"))
		{
			byte	*palette;
			palette = NULL;

			LoadPCX (name, &pic, &palette, &width, &height);
			if (!pic)
				return NULL;
			image = Vk_LoadPic(name, pic,
					   width, width,
					   height, height,
					   type, 8);

			if (palette)
				free(palette);
		}
		else if (!strcmp(ext, "wal"))
		{
			image = Vk_LoadWal (name, type);
		}
		else if (!strcmp(ext, "tga"))
		{
			if (!LoadSTB (namewe, "tga", &pic, &width, &height))
				return NULL;
			if (!pic)
				return NULL;
			image = Vk_LoadPic(name, pic,
					   width, width,
					   height, height,
					   type, 32);
		}

		if (pic)
			free(pic);
	}

	return image;
}

/*
===============
Vk_FindImage

Finds or loads the given image
===============
*/
image_t	*Vk_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int	i, len;
	char *ptr;
	char namewe[256];
	const char* ext;

	if (!name)
	{
		return NULL;
	}

	ext = COM_FileExtension(name);
	if(!ext[0])
	{
		/* file has no extension */
		return NULL;
	}

	len = strlen(name);

	/* Remove the extension */
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - (strlen(ext) + 1));

	if (len < 5)
	{
		return NULL;
	}

	/* fix backslashes */
	while ((ptr = strchr(name, '\\')))
	{
		*ptr = '/';
	}

	// look for it
	for (i=0, image=vktextures ; i<numvktextures ; i++,image++)
	{
		if (!strcmp(name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	//
	// load the pic from disk
	//
	return Vk_LoadImage(name, namewe, ext, type);
}


/*
===============
RE_RegisterSkin
===============
*/
struct image_s *RE_RegisterSkin (char *name)
{
	return Vk_FindImage (name, it_skin);
}


/*
================
Vk_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void Vk_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_particletexture->registration_sequence = registration_sequence;
	r_squaretexture->registration_sequence = registration_sequence;

	for (i = 0, image = vktextures; i < numvktextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence)
			continue;		// used this sequence
		if (!image->registration_sequence)
			continue;		// free image_t slot
		if (image->type == it_pic)
			continue;		// don't free pics
		// free it
		QVk_ReleaseTexture(&image->vk_texture);
		memset(image, 0, sizeof(*image));
	}

	// free all unused blocks
	vulkan_memory_free_unused();
}


/*
===============
Draw_GetPalette
===============
*/
static int Draw_GetPalette (void)
{
	int		i;
	byte	*pic, *pal;
	int		width, height;

	// get the palette

	LoadPCX ("pics/colormap.pcx", &pic, &pal, &width, &height);
	if (!pal)
		ri.Sys_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");

	for (i=0 ; i<256 ; i++)
	{
		unsigned	v;
		int	r, g, b;

		r = pal[i*3+0];
		g = pal[i*3+1];
		b = pal[i*3+2];

		v = (255<<24) + (r<<0) + (g<<8) + (b<<16);
		d_8to24table[i] = LittleLong(v);
	}

	d_8to24table[255] &= LittleLong(0xffffff);	// 255 is transparent

	free (pic);
	free (pal);

	return 0;
}


/*
===============
Vk_InitImages
===============
*/
void	Vk_InitImages (void)
{
	int	i;
	float	overbright;

	registration_sequence = 1;

	// init intensity conversions
	intensity = ri.Cvar_Get("vk_intensity", "2", 0);

	if (intensity->value <= 1)
		ri.Cvar_Set("vk_intensity", "1");

	vk_state.inverse_intensity = 1 / intensity->value;

	for (i = 0; i<256; i++)
	{
		int	j;

		j = i * intensity->value;
		if (j > 255)
			j = 255;
		intensitytable[i] = j;
	}

	Draw_GetPalette();

	overbright = vk_overbrightbits->value;

	if(overbright < 0.5)
		overbright = 0.5;

	if(overbright > 4.0)
		overbright = 4.0;

	for (i=0 ; i<256 ; i++) {
		int inf;

		inf = i * overbright;

		if (inf < 0)
			inf = 0;
		if (inf > 255)
			inf = 255;

		overbrightable[i] = inf;
	}
}

/*
===============
Vk_ShutdownImages
===============
*/
void	Vk_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i = 0, image = vktextures; i<numvktextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot

		QVk_ReleaseTexture(&image->vk_texture);
		memset(image, 0, sizeof(*image));
	}

	QVk_ReleaseTexture(&vk_rawTexture);

	for(i = 0; i < MAX_LIGHTMAPS*2; i++)
		QVk_ReleaseTexture(&vk_state.lightmap_textures[i]);
}

