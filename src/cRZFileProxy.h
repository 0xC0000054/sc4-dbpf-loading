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
#include "cIGZFile.h"
#include "cIGZString.h"
#include <Windows.h>

class cRZString
{
public:
	const cIGZString* AsIGZString() const
	{
		return reinterpret_cast<const cIGZString*>(this);
	}
private:
	void* vtable;
	intptr_t stdStringFields[3];
	uint32_t refCount;
};

class cRZFileProxy
{
public:
	void* vtable;
	cRZString nameRZStr;
	bool isOpen;
	HANDLE fileHandle;
	cIGZFile::AccessMode accessMode;
	cIGZFile::CreationMode creationMode;
	cIGZFile::ShareMode shareMode;
	intptr_t unknown2;
	uint32_t fileIOError;
	intptr_t unknown3[5];
	uint32_t currentFilePosition;
	uint32_t position;
	uint32_t maxReadBufferSize;
	void* pReadBuffer;
	intptr_t unknown4[2];
	uint32_t readBufferOffset;
	uint32_t readBufferLength;
	uint32_t maxWriteBufferSize;
	void* pWriteBuffer;
	intptr_t unknown5[2];
	uint32_t writeBufferOffset;
	uint32_t writeBufferLength;
};

static_assert(offsetof(cRZFileProxy, isOpen) == 0x18);
static_assert(offsetof(cRZFileProxy, fileHandle) == 0x1C);
static_assert(offsetof(cRZFileProxy, fileIOError) == 0x30);
static_assert(offsetof(cRZFileProxy, currentFilePosition) == 0x48);
static_assert(offsetof(cRZFileProxy, readBufferOffset) == 0x60);
static_assert(offsetof(cRZFileProxy, readBufferLength) == 0x64);
static_assert(offsetof(cRZFileProxy, writeBufferOffset) == 0x78);