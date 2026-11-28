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
#include "cRZBaseString.h"
#include "cRZCOMDllDirector.h"
#include "GZServPtrs.h"
#include "Logger.h"
#include "Patcher.h"
#include "StringViewUtil.h"
#include "ThumbnailInstanceIDs.h"
#include <array>
#include <memory>

namespace
{
	using namespace std::string_view_literals;

	bool IsPNGThumbnail(cIGZPersistDBSegment* pCityDBSegment, uint32_t formatLTEXTIID)
	{
		bool result = false;

		const cGZPersistResourceKey key(kLTEXTTypeID, kThumbnailGroupID, formatLTEXTIID);

		cRZAutoRefCount<cIGZPersistDBRecord> pRecord;

		if (pCityDBSegment->OpenRecord(key, pRecord.AsPPObj(), cIGZFile::AccessMode::Read))
		{
			cRZAutoRefCount<cIGZPersistDBSerialRecord> pSerialRecord;

			if (pRecord->QueryInterface(GZIID_cIGZPersistDBSerialRecord, pSerialRecord.AsPPVoid()))
			{
				cRZBaseString text;

				if (pSerialRecord->GetFieldString(text))
				{
					const std::string_view textAsStringView(text.Data(), text.Strlen());

					result = StringViewUtil::EndsWithIgnoreCase(textAsStringView, ".png"sv);
				}				
			}
		}

		return result;
	}

	bool ReadCityThumbnail(
		cIGZGimexFactory* pGimexFactory,
		cIGZPersistDBSegment* pCityDBSegment,
		const ThumbnailInstanceID& instanceIDs,
		cIGZBuffer** ppBuffer,
		cGZBufferColorType::eColorType colorType)
	{
		bool result = false;

		const cGZPersistResourceKey key(kGimexImageTypeID, kThumbnailGroupID, instanceIDs.imageInstanceID);

		if (pCityDBSegment->TestForRecord(key))
		{
			// The game only loads thumbnails that are below this size.
			constexpr uint32_t kMaxRecordSize = 5000000;

			const uint32_t recordSize = pCityDBSegment->GetRecordSize(key);

			if (recordSize < kMaxRecordSize)
			{
				cRZAutoRefCount<cIGZPersistDBRecord> pRecord;

				if (pCityDBSegment->OpenRecord(key, pRecord.AsPPObj(), cIGZFile::AccessMode::Read))
				{
					cRZAutoRefCount<cIGZPersistDBSerialRecord> pSerialRecord;

					if (pRecord->QueryInterface(GZIID_cIGZPersistDBSerialRecord, pSerialRecord.AsPPVoid()))
					{
						// PNG is the 4th format in the Windows binary.
						constexpr uint32_t kGimexIndexPNG = 0x4;
						constexpr uint32_t kGimexIndexDetectFormatFromHeaderSignature = 0x32;

						const uint32_t gimexIndex = IsPNGThumbnail(pCityDBSegment, instanceIDs.formatLTEXTInstanceID)
							? kGimexIndexPNG
							: kGimexIndexDetectFormatFromHeaderSignature;

						result = pGimexFactory->CreateFromSerialRecord(
							pSerialRecord,
							gimexIndex,
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
			cRZAutoRefCount<cIGZBuffer> tempCityImageBuffer;
			cRZAutoRefCount<cIGZBuffer> tempCityMaskBuffer;

			if (ReadCityThumbnail(
				pGF,
				pCitySaveSegment,
				ThumbnailInstanceIDs::CityImage,
				tempCityImageBuffer.AsPPObj(),
				cGZBufferColorType::A8R8G8B8))
			{
				if (ReadCityThumbnail(
					pGF,
					pCitySaveSegment,
					ThumbnailInstanceIDs::CityMask,
					tempCityMaskBuffer.AsPPObj(),
					cGZBufferColorType::A8R8G8B8))
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

					if (ppRegionalTransportImageBuffer != nullptr)
					{
						cRZAutoRefCount<cIGZBuffer> tempRegionTransportImageBuffer;

						if (ReadCityThumbnail(
							pGF,
							pCitySaveSegment,
							ThumbnailInstanceIDs::RegionTransportImage,
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
							ThumbnailInstanceIDs::RegionTransportMask,
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

					// The original code returns true even if the
					// regional transport thumbnails failed to load.
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
