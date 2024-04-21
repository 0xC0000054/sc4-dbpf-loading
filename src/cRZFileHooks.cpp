///////////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
// optimizes the DBPF loading.
//
// Copyright (c) 2024 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
///////////////////////////////////////////////////////////////////////////////

#include "cRZFileHooks.h"
#include "cIGZString.h"
#include "DebugUtil.h"
#include "Logger.h"
#include "Patcher.h"
#include <stdexcept>

#define NOMINMAX

#include <Windows.h>
#include "detours/detours.h"

namespace
{
	DWORD GetRZFileErrorCode()
	{
		DWORD lastError = GetLastError();

		switch (lastError)
		{
		case ERROR_ACCESS_DENIED:
			return 0x20000002;
		case ERROR_ALREADY_EXISTS:
			return 0x20000001;
		default:
			return lastError;
		}
	}

	class cRZString
	{
		void* vtable;
		intptr_t stdStringFields[3];
		uint32_t refCount;
	};

	class cRZFileProxy
	{
	public:
		void* vtable;
		cRZString nameRZStr;
		int isOpen;
		HANDLE fileHandle;
		uint32_t accessMode;
		uint32_t creationMode;
		uint32_t shareMode;
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

	typedef bool(__thiscall* pfn_cRZFile_ReadWithCount)(cRZFileProxy* pThis, void* outBuffer, uint32_t& byteCount);

	static pfn_cRZFile_ReadWithCount RealReadWithCount = nullptr;

	static bool __fastcall HookedReadWithCount(cRZFileProxy* pThis, void* edxUnused, void* outBuffer, uint32_t& byteCount)
	{
		bool result = false;
#if 0
		DebugUtil::PrintLineToDebugOutputFormatted(
			"HookedReadWithCount: %u bytes requested, position: %u, name: %s",
			byteCount,
			pThis->currentFilePosition + (pThis->currentFilePosition - pThis->readBufferOffset),
			reinterpret_cast<cIGZString*>(&pThis->nameRZStr)->ToChar());
#endif

		if (pThis->isOpen)
		{
			if (byteCount == 0)
			{
				result = true;
			}
			else
			{
				// If the requested number of bytes is larger than the games buffer size, we will attempt
				// to fill the buffer with as much data as the OS can provide per call.
				// This can significantly reduce the required number of system calls for large reads when
				// compared to the game's standard behavior of copying from a fixed-size buffer in a loop.
				//
				// To minimize complexity and potential compatibility issues, our code only runs when the
				// following conditions are true:
				//
				// 1. The game's read buffer size is greater than 0 and less than the requested read size.
				// 2. The file is at the correct position to start reading.
				// 3. The game's existing read buffer is empty.
				// 4. The write buffer is empty.
				//
				// If any of these conditions are not met, the call will be forwarded to the game's
				// original read method.
				if (byteCount >= pThis->maxReadBufferSize
					&& pThis->maxReadBufferSize > 0
					&& pThis->position == pThis->currentFilePosition
					&& (pThis->currentFilePosition < pThis->readBufferOffset || (pThis->readBufferOffset + pThis->readBufferLength) <= pThis->currentFilePosition)
					&& pThis->writeBufferLength == 0)
				{
					DWORD bytesRead = 0;
					if (ReadFile(pThis->fileHandle, outBuffer, static_cast<DWORD>(byteCount), &bytesRead, nullptr))
					{
						byteCount = static_cast<uint32_t>(bytesRead);
						pThis->position += byteCount;
						result = true;
					}
					else
					{
						pThis->fileIOError = GetRZFileErrorCode();
						pThis->position = SetFilePointer(pThis->fileHandle, 0, nullptr, FILE_CURRENT);
					}
					pThis->currentFilePosition = pThis->position;
				}
				else
				{
					result = RealReadWithCount(pThis, outBuffer, byteCount);
				}
			}
		}

		return result;
	}

	void InstallReadWithCountHook()
	{
		RealReadWithCount = reinterpret_cast<pfn_cRZFile_ReadWithCount>(0x9192A9);

		DetourRestoreAfterWith();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)RealReadWithCount, HookedReadWithCount);
		DetourTransactionCommit();
	}
}


void cRZFileHooks::Install()
{
	Logger& logger = Logger::GetInstance();

	try
	{
		InstallReadWithCountHook();

		logger.WriteLine(LogLevel::Info, "Installed the cRZFile hooks.");
	}
	catch (const std::exception& e)
	{
		logger.WriteLineFormatted(
			LogLevel::Error,
			"Failed to install the cRZFile hooks: %s",
			e.what());
	}
}
