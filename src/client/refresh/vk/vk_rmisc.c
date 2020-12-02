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

// vk_misc.c

#include "header/local.h"

/*
==================
RE_InitParticleTexture
==================
*/
static byte	dottexture[8][8] =
{
	{0,0,0,0,0,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,1,1,1,1,0,0,0},
	{0,0,1,1,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};

void RE_InitParticleTexture (void)
{
	int		x,y,i;
	byte	data[8][8][4];

	//
	// particle texture
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = dottexture[x][y]*255;
		}
	}
	r_particletexture = Vk_LoadPic("***particle***", (byte *)data,
		8, 8, 8, 8, it_sprite, 32);

	//
	// particle texture
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			for (i=0 ; i<4 ; i++)
			{
				data[y][x][i] = ((y < 4) && (x < 4)) ? 255 : 0;
			}
		}
	}
	r_squaretexture = Vk_LoadPic("***square***", (byte *)data,
		8, 8, 8, 8, it_sprite, 32);

	//
	// also use this for bad textures, but without alpha
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3]*255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}
	r_notexture = Vk_LoadPic("***r_notexture***", (byte *)data,
		8, 8, 8, 8, it_wall, 32);
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
Vk_ScreenShot_f
==================
*/
void Vk_ScreenShot_f (void)
{
	byte		*buffer;
	int		i;
	size_t		buffSize = vid.width * vid.height * 4;

	if (!vk_device.screenshotSupported)
	{
		R_Printf(PRINT_ALL, "%s: Screenshots are not supported by this GPU.\n", __func__);
		return;
	}

	buffer = malloc(buffSize);

	QVk_ReadPixels(buffer, vid.width, vid.height);

	// swap rgb to bgr
	if (vk_swapchain.format == VK_FORMAT_R8G8B8A8_UNORM ||
	    vk_swapchain.format == VK_FORMAT_R8G8B8A8_SRGB)
	{
		for (i = 0; i < buffSize; i += 4)
		{
			buffer[i + 3] = 255;
		}
	}
	else
	{
		for (i = 0; i < buffSize; i += 4)
		{
			int	temp;

			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
			buffer[i + 3] = 255; // alpha component
		}
	}

	ri.Vid_WriteScreenshot(vid.width, vid.height, 4, buffer);

	free(buffer);
}

/*
** Vk_Strings_f
*/
void Vk_Strings_f(void)
{
	int i = 0;

	uint32_t numDevices = 0;
	int usedDevice = 0;
	VkPhysicalDevice *physicalDevices;
	VkPhysicalDeviceProperties deviceProperties;
	int preferredDevice = (int)vk_device_idx->value;
	int msaa = (int)vk_msaa->value;
	uint32_t driverMajor = VK_VERSION_MAJOR(vk_device.properties.driverVersion);
	uint32_t driverMinor = VK_VERSION_MINOR(vk_device.properties.driverVersion);
	uint32_t driverPatch = VK_VERSION_PATCH(vk_device.properties.driverVersion);

	// NVIDIA driver version decoding scheme
	if (vk_device.properties.vendorID == 0x10DE)
	{
		driverMajor = ((uint32_t)(vk_device.properties.driverVersion) >> 22) & 0x3ff;
		driverMinor = ((uint32_t)(vk_device.properties.driverVersion) >> 14) & 0x0ff;

		uint32_t secondary = ((uint32_t)(vk_device.properties.driverVersion) >> 6) & 0x0ff;
		uint32_t tertiary = vk_device.properties.driverVersion & 0x03f;

		driverPatch = (secondary << 8) | tertiary;
	}

	VK_VERIFY(vkEnumeratePhysicalDevices(vk_instance, &numDevices, NULL));
	if (!numDevices)
	{
		return;
	}
	physicalDevices = malloc(sizeof(VkPhysicalDevice) * numDevices);
	if (!physicalDevices)
	{
		return;
	}
	VK_VERIFY(vkEnumeratePhysicalDevices(vk_instance, &numDevices, physicalDevices));

	if (preferredDevice >= numDevices)
	{
		preferredDevice = -1;
	}

	R_Printf(PRINT_ALL, "------------------------------------\n");
	R_Printf(PRINT_ALL, "Vulkan API: %d.%d\n",  VK_VERSION_MAJOR(vk_config.vk_version),
				VK_VERSION_MINOR(vk_config.vk_version));
	R_Printf(PRINT_ALL, "Header version: %d\n", VK_HEADER_VERSION);
	R_Printf(PRINT_ALL, "Devices found:\n");
	for (i = 0; i < numDevices; ++i)
	{
		qboolean isPreferred = false;

		vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);
		isPreferred = (preferredDevice == i) || (
			preferredDevice < 0 &&
			deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
		);
		if (isPreferred) {
			usedDevice = i;
		}
		R_Printf(PRINT_ALL, "%s#%d: %s\n",
			isPreferred && numDevices > 1 ? "*  " : "   ",
			i, deviceProperties.deviceName);
	}
	free(physicalDevices);

	R_Printf(PRINT_ALL, "Using device #%d:\n", usedDevice);
	R_Printf(PRINT_ALL, "   deviceName: %s\n", vk_device.properties.deviceName);
	R_Printf(PRINT_ALL, "   resolution: %dx%d", vid.width, vid.height);
	if (msaa > 0)
	{
		R_Printf(PRINT_ALL, " (MSAAx%d)\n", 2 << (msaa - 1));
	}
	else
	{
		R_Printf(PRINT_ALL, "\n");
	}
#ifndef __linux__
	// Intel on Windows and MacOS (Linux uses semver for Mesa drivers)
	if (vk_device.properties.vendorID == 0x8086)
	{
		R_Printf(PRINT_ALL, "   driverVersion: %d (0x%X)\n",
			vk_device.properties.driverVersion,
			vk_device.properties.driverVersion);
	}
	else
#endif
	{
		R_Printf(PRINT_ALL, "   driverVersion: %d.%d.%d (0x%X)\n",
			driverMajor, driverMinor, driverPatch,
			vk_device.properties.driverVersion);
	}

	R_Printf(PRINT_ALL, "   apiVersion: %d.%d.%d\n",
		VK_VERSION_MAJOR(vk_device.properties.apiVersion),
		VK_VERSION_MINOR(vk_device.properties.apiVersion),
		VK_VERSION_PATCH(vk_device.properties.apiVersion));
	R_Printf(PRINT_ALL, "   deviceID: %d\n", vk_device.properties.deviceID);
	R_Printf(PRINT_ALL, "   vendorID: 0x%X (%s)\n",
		vk_device.properties.vendorID, vk_config.vendor_name);
	R_Printf(PRINT_ALL, "   deviceType: %s\n", vk_config.device_type);
	R_Printf(PRINT_ALL, "   gfx/present/transfer: %d/%d/%d\n",
		vk_device.gfxFamilyIndex,
		vk_device.presentFamilyIndex,
		vk_device.transferFamilyIndex);
	R_Printf(PRINT_ALL, "Present mode: %s\n", vk_config.present_mode);
	R_Printf(PRINT_ALL, "Swapchain image format: %d\n", vk_swapchain.format);

	R_Printf(PRINT_ALL, "Supported present modes: ");
	i = 0;
	while(vk_config.supported_present_modes[i])
	{
		R_Printf(PRINT_ALL, "%s ", vk_config.supported_present_modes[i++]);
	}
	R_Printf(PRINT_ALL, "\n");

	R_Printf(PRINT_ALL, "Enabled extensions: ");
	i = 0;
	while(vk_config.extensions[i])
	{
		R_Printf(PRINT_ALL, "%s ", vk_config.extensions[i++]);
	}
	R_Printf(PRINT_ALL, "\n");

	R_Printf(PRINT_ALL, "Enabled layers: ");
	i = 0;
	while(vk_config.layers[i])
	{
		R_Printf(PRINT_ALL, "%s ", vk_config.layers[i++]);
	}
	R_Printf(PRINT_ALL, "\n");
}

/*
** Vk_Mem_f
*/
void Vk_Mem_f(void)
{
	R_Printf(PRINT_ALL, "\nDynamic buffer stats: \n");
	R_Printf(PRINT_ALL, "Vertex : %u/%ukB (%.1f%% max: %ukB)\n",
		vk_config.vertex_buffer_usage / 1024,
		vk_config.vertex_buffer_size / 1024,
		100.f * vk_config.vertex_buffer_usage / vk_config.vertex_buffer_size,
		vk_config.vertex_buffer_max_usage / 1024);

	R_Printf(PRINT_ALL, "Index  : %u/%uB (%.1f%% max: %uB)\n",
		vk_config.index_buffer_usage,
		vk_config.index_buffer_size,
		100.f * vk_config.index_buffer_usage / vk_config.index_buffer_size,
		vk_config.index_buffer_max_usage);
	R_Printf(PRINT_ALL, "Uniform: %u/%ukB (%.1f%% max: %ukB)\n",
		vk_config.uniform_buffer_usage / 1024,
		vk_config.uniform_buffer_size / 1024,
		100.f * vk_config.uniform_buffer_usage / vk_config.uniform_buffer_size,
		vk_config.uniform_buffer_max_usage / 1024);
	R_Printf(PRINT_ALL, "Tri fan: %u/%u (%.1f%% max: %u)\n",
		vk_config.triangle_fan_index_usage,
		vk_config.triangle_fan_index_count,
		100.f * vk_config.triangle_fan_index_usage / vk_config.triangle_fan_index_count,
		vk_config.triangle_fan_index_max_usage);
}
