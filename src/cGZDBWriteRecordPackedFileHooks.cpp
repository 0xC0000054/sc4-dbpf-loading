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

#include "cGZDBWriteRecordPackedFileHooks.h"
#include "cRZFileProxy.h"
#include "Patcher.h"
#include "PathUtil.h"

namespace
{
	typedef cRZFileProxy* (__thiscall* pfn_cRZFile_ctor_IGZString)(cRZFileProxy* pThis, cIGZString const& path);
	static const pfn_cRZFile_ctor_IGZString Real_cRZFile_ctor_IGZString = reinterpret_cast<pfn_cRZFile_ctor_IGZString>(0x918c00);

	cRZFileProxy* __fastcall Hooked_cRZFile_ctor_IGZString(cRZFileProxy* pThis, void* edxUnused, cIGZString const& path)
	{
		cRZFileProxy* initializedClass = Real_cRZFile_ctor_IGZString(pThis, path);

		const std::string_view fileName = PathUtil::GetFileName(std::string_view(path.ToChar(), path.Strlen()));

		// We only set the larger buffer size for the Network Index Subfile as it uses sequential writes.
		// Subfiles that use the game's COM serializer such as the prop subfile (Record_2977aa47) do not benefit
		// from the larger buffer size due to the seeking that is required to write the serialized data header,
		// each seek operation will flush the write buffer to disk.
		if (fileName.starts_with("Record_6a0f82b2"))
		{
			// By default, the game uses read and write buffers that are 512 bytes.
			// Because this code path only gets hit for files that are at least 200,000 bytes
			// in size, we increase the write buffer to 16384 bytes.
			// The 16384 byte size was chosen to match the behavior of the cGZDBWriteRecordPackedFile
			// constructor overload that copies from an existing DBPF record.
			initializedClass->maxWriteBufferSize = 16384;
		}

		return initializedClass;
	}
}

void cGZDBWriteRecordPackedFileHooks::Install()
{
	constexpr size_t SetFieldVoid_cRZFile_ctor_Inject = 0x970fa4;

	Patcher::InstallCallHook(SetFieldVoid_cRZFile_ctor_Inject, &Hooked_cRZFile_ctor_IGZString);
}