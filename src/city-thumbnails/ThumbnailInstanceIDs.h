/*
 * This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
 * optimizes the DBPF loading.
 *
 * Copyright (C) 2024, 2025, 2026 Nicholas Hayes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, under
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include <cstdint>

static constexpr uint32_t kGimexImageTypeID = 0x8a2482b9;
static constexpr uint32_t kLTEXTTypeID = 0x2026960b;

static constexpr uint32_t kThumbnailGroupID = 0x4a2482bb;

struct ThumbnailInstanceID
{
	// The instance id of the thumbnail image.  
	uint32_t imageInstanceID;
	// The instance id of the LTEXT that the game uses to detect the image
	// file format from the file extension.
	// This LTEXT contains Temp.png in version 641, but the game may have
	// used formats other than PNG at some point.
	uint32_t formatLTEXTInstanceID;

	constexpr ThumbnailInstanceID(uint32_t imageIID, uint32_t formatLTEXTIID)
		: imageInstanceID(imageIID), formatLTEXTInstanceID(formatLTEXTIID)
	{
	}
};

namespace ThumbnailInstanceIDs
{
	static constexpr ThumbnailInstanceID CityImage(0, 1);
	static constexpr ThumbnailInstanceID CityMask(2, 3);
	static constexpr ThumbnailInstanceID RegionTransportImage(4, 5);
	static constexpr ThumbnailInstanceID RegionTransportMask(6, 7);
}
