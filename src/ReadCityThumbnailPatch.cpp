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

#include "ReadCityThumbnailPatch.h"
#include "cGZPersistResourceKey.h"
#include "cIGZBuffer.h"
#include "cIGZCOM.h"
#include "cIGZGimexFactory.h"
#include "cIGZPersistBufferResource.h"
#include "cIGZPersistDBRecord.h"
#include "cIGZPersistDBSegment.h"
#include "cIGZPersistDBSerialRecord.h"
#include "cIGZPersistResourceManager.h"
#include "cISC4App.h"
#include "cRZAutoRefCount.h"
#include "cRZCOMDllDirector.h"
#include "GZServPtrs.h"
#include "Logger.h"
#include "Patcher.h"
#include <array>
#include <memory>

namespace
{
	bool ReadCityThumbnail(
		cIGZGimexFactory* pGimexFactory,
		cIGZPersistDBSegment* pCityDBSegment,
		uint32_t imageIID,
		cIGZBuffer** ppBuffer,
		cGZBufferColorType::eColorType colorType)
	{
		bool result = false;

		const cGZPersistResourceKey key(0x8a2482b9, 0x4a2482bb, imageIID);

		if (pCityDBSegment->TestForRecord(key))
		{
			// The game only loads thumbnails that are below this size.
			constexpr uint32_t kMaxRecordSize = 5000000;

			const uint32_t recordSize = pCityDBSegment->GetRecordSize(key);

			if (recordSize < kMaxRecordSize)
			{
				// We could also use the index of the PNG handler directly, but this
				// produces the same format detection behavior as what the original
				// code did with the file extension of the temporary file.

				constexpr uint32_t kGimexDetectFormatFromHeaderSignature = 0x32;

				cRZAutoRefCount<cIGZPersistDBRecord> pRecord;

				if (pCityDBSegment->OpenRecord(key, pRecord.AsPPObj(), cIGZFile::AccessMode::Read))
				{
					cRZAutoRefCount<cIGZPersistDBSerialRecord> pSerialRecord;

					if (pRecord->QueryInterface(GZIID_cIGZPersistDBSerialRecord, pSerialRecord.AsPPVoid()))
					{
						result = pGimexFactory->CreateFromSerialRecord(
							pSerialRecord,
							kGimexDetectFormatFromHeaderSignature,
							ppBuffer,
							colorType);
					}
				}
			}
		}

		return result;
	}

	bool HookedSC4ReadCityRegionViewThumbnails(
		cIGZPersistDBSegment* pCitySaveSegment,
		cIGZBuffer** ppCityImageBuffer,
		cIGZBuffer** ppCityMaskBuffer,
		cIGZBuffer** ppRegionalTransportImageBuffer,
		cIGZBuffer** ppRegionalTransportMaskBuffer)
	{
		*ppCityImageBuffer = nullptr;
		*ppCityMaskBuffer = nullptr;
		
		if (ppRegionalTransportImageBuffer)
		{
			*ppRegionalTransportImageBuffer = nullptr;
		}

		if (ppRegionalTransportMaskBuffer)
		{
			*ppRegionalTransportMaskBuffer = nullptr;
		}

		bool result = false;

		cRZAutoRefCount<cIGZGimexFactory> pGF;

		if (GZCOM()->GetClassObject(GZCLSID_cIGZGimexFactory, GZIID_cIGZGimexFactory, pGF.AsPPVoid()))
		{
			// The odd numbered items are LTEXT records with "Temp.png".
			// The original game code used the file extension of that
			// string to let Gimex detect which format it needed to use
			// when opening the temporary file.

			constexpr uint32_t kCityImageIID = 0;
			constexpr uint32_t kCityMaskIID = 2;
			constexpr uint32_t kRegionalTransportImageIID = 4;
			constexpr uint32_t kRegionalTransportMaskIID = 6;

			cRZAutoRefCount<cIGZBuffer> tempCityImageBuffer;
			cRZAutoRefCount<cIGZBuffer> tempCityMaskBuffer;

			if (ReadCityThumbnail(pGF, pCitySaveSegment, kCityImageIID, tempCityImageBuffer.AsPPObj(), cGZBufferColorType::A8R8G8B8))
			{
				if (ReadCityThumbnail(pGF, pCitySaveSegment, kCityMaskIID, tempCityMaskBuffer.AsPPObj(), cGZBufferColorType::A8R8G8B8))
				{
					if (tempCityImageBuffer)
					{
						tempCityImageBuffer->AddRef();
						*ppCityImageBuffer = tempCityImageBuffer;
					}
					 
					if (tempCityMaskBuffer)
					{
						tempCityMaskBuffer->AddRef();
						*ppCityMaskBuffer = tempCityMaskBuffer;
					}

					// The original game code does not check if the optional
					// regional transport images were successfully loaded.

					if (ppRegionalTransportImageBuffer != nullptr)
					{
						cRZAutoRefCount<cIGZBuffer> tempRegionTransportImageBuffer;

						if (ReadCityThumbnail(
							pGF,
							pCitySaveSegment,
							kRegionalTransportImageIID,
							tempRegionTransportImageBuffer.AsPPObj(),
							cGZBufferColorType::A8R8G8B8))
						{
							if (tempRegionTransportImageBuffer)
							{
								tempRegionTransportImageBuffer->AddRef();
								*ppRegionalTransportImageBuffer = tempRegionTransportImageBuffer;
							}
						}
					}

					if (ppRegionalTransportMaskBuffer != nullptr)
					{			
						cRZAutoRefCount<cIGZBuffer> tempRegionTransportMaskBuffer;

						if (ReadCityThumbnail(
							pGF,
							pCitySaveSegment,
							kRegionalTransportMaskIID,
							tempRegionTransportMaskBuffer.AsPPObj(),
							cGZBufferColorType::A8R8G8B8))
						{
							if (tempRegionTransportMaskBuffer)
							{
								tempRegionTransportMaskBuffer->AddRef();
								*ppRegionalTransportMaskBuffer = tempRegionTransportMaskBuffer;
							}
						}
					}

					result = true;
				}
			}
		}


		return result;
	}
}

void ReadCityThumbnailPatch::Install()
{
	static constexpr std::array<uint8_t, 5> expectedBytes = { 0xe8, 0x0b, 0xfa, 0xff, 0xff };
	constexpr uintptr_t SC4ReadCityRegionViewThumbnails_Inject = 0x5dda70;

	Logger& logger = Logger::GetInstance();

	if (std::memcmp(reinterpret_cast<const void*>(SC4ReadCityRegionViewThumbnails_Inject), expectedBytes.data(), expectedBytes.size()) == 0)
	{
		Patcher::InstallCallHook(SC4ReadCityRegionViewThumbnails_Inject, &HookedSC4ReadCityRegionViewThumbnails);
		logger.WriteLine(LogLevel::Info, "Installed the read city thumbnail patch.");
	}
	else
	{
		logger.WriteLine(LogLevel::Error, "Failed to install the read city thumbnail patch, the expected patch target bytes don't match.");
	}
}
