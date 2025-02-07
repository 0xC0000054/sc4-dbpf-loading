///////////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
// optimizes the DBPF loading.
//
// Copyright (c) 2024, 2025 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
///////////////////////////////////////////////////////////////////////////////

#include "cRZFileHooks.h"
#include "cIGZString.h"
#include "DebugUtil.h"
#include "GZStringConvert.h"
#include "Logger.h"
#include "Patcher.h"
#include "PathUtil.h"
#include <stdexcept>

#define NOMINMAX

#include <Windows.h>
#include "detours/detours.h"

namespace
{
	bool ReadFileBlocking(HANDLE hFile, uint8_t* buffer, uint32_t byteCount, uint32_t& bytesRead)
	{
		uint32_t remaining = byteCount;

		bytesRead = 0;

		while (remaining > 0)
		{
			const DWORD numberOfBytesToRead = std::min(0x80000000UL, static_cast<DWORD>(remaining));
			DWORD numberOfBytesRead = 0;

			if (!ReadFile(hFile, buffer + bytesRead, numberOfBytesToRead, &numberOfBytesRead, nullptr))
			{
				return false;
			}

			if (numberOfBytesRead == 0)
			{
				break;
			}

			bytesRead += static_cast<uint32_t>(numberOfBytesRead);
			remaining -= static_cast<uint32_t>(numberOfBytesRead);
		}

		return true;
	}

	std::wstring GetUtf16FilePath(const cIGZString& utf8Path)
	{
		std::wstring utf16Path = GZStringConvert::ToUtf16(utf8Path);

		if (PathUtil::MustAddExtendedPathPrefix(utf16Path))
		{
			// The extended path must be normalized because the OS won't do it for us.
			utf16Path = PathUtil::Normalize(PathUtil::AddExtendedPathPrefix(utf16Path));
		}

		return utf16Path;
	}

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

	enum class RZFileAccessMode : uint32_t
	{
		None = 0,
		Read = 1,
		Write = 2,
		ReadWrite = Read | Write,
	};
	DEFINE_ENUM_FLAG_OPERATORS(RZFileAccessMode);

	enum class RZFileCreationMode : uint32_t
	{
		CreateNew = 0,
		CreateAlways = 1,
		OpenExisting = 2,
		OpenAlways = 3,
		TruncateExisting = 4,
	};

	enum class RZFileShareMode : uint32_t
	{
		None = 0,
		Read = 1,
		ReadWrite = 2,
	};

	class cRZFileProxy
	{
	public:
		void* vtable;
		cRZString nameRZStr;
		bool isOpen;
		HANDLE fileHandle;
		RZFileAccessMode accessMode;
		RZFileCreationMode creationMode;
		RZFileShareMode shareMode;
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

	void SetRZFileErrorCode(cRZFileProxy* pThis, DWORD lastError)
	{
		switch (lastError)
		{
		case ERROR_ACCESS_DENIED:
			pThis->fileIOError = 0x20000002;
			break;
		case ERROR_ALREADY_EXISTS:
			pThis->fileIOError = 0x20000001;
			break;
		default:
			pThis->fileIOError = lastError;
			break;
		}
	}

	typedef bool(__thiscall *pfn_cRZFile_Open)(
		cRZFileProxy* pThis,
		RZFileAccessMode accessMode,
		RZFileCreationMode creationMode,
		RZFileShareMode shareMode);
	static pfn_cRZFile_Open RealOpen = nullptr;

	bool __fastcall HookedOpen(
		cRZFileProxy* pThis,
		void* edxUnused,
		RZFileAccessMode accessMode,
		RZFileCreationMode creationMode,
		RZFileShareMode shareMode)
	{
		bool result = false;

		if (pThis->isOpen)
		{
			result = true;
		}
		else
		{
			const cIGZString* utf8FilePath = pThis->nameRZStr.AsIGZString();

			if (utf8FilePath->Strlen() > 0)
			{
				try
				{
					const std::wstring utf16Path = GetUtf16FilePath(*utf8FilePath);

					DWORD dwDesiredAccess = 0;
					DWORD dwShareMode = 0;
					DWORD dwCreationDisposition = 0;

					switch (accessMode)
					{
					case RZFileAccessMode::Read:
						dwDesiredAccess = GENERIC_READ;
						break;
					case RZFileAccessMode::Write:
						dwDesiredAccess = GENERIC_WRITE;
						break;
					case RZFileAccessMode::ReadWrite:
						dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
						break;
					}

					switch (creationMode)
					{
					case RZFileCreationMode::CreateNew:
						dwCreationDisposition = CREATE_NEW;
						break;
					case RZFileCreationMode::CreateAlways:
						dwCreationDisposition = CREATE_ALWAYS;
						break;
					case RZFileCreationMode::OpenExisting:
						dwCreationDisposition = OPEN_EXISTING;
						break;
					case RZFileCreationMode::OpenAlways:
						dwCreationDisposition = OPEN_ALWAYS;
						break;
					case RZFileCreationMode::TruncateExisting:
						dwCreationDisposition = TRUNCATE_EXISTING;
						break;
					}

					switch (shareMode)
					{
					case RZFileShareMode::Read:
						dwShareMode = FILE_SHARE_READ;
						break;
					case RZFileShareMode::ReadWrite:
						dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
						break;
					}

					HANDLE hFile = CreateFileW(
						utf16Path.c_str(),
						dwDesiredAccess,
						dwShareMode,
						nullptr,
						dwCreationDisposition,
						0,
						nullptr);

					if (hFile == INVALID_HANDLE_VALUE)
					{
						SetRZFileErrorCode(pThis, GetLastError());
					}
					else
					{
						pThis->fileHandle = hFile;
						pThis->isOpen = 1;
						pThis->accessMode = accessMode;
						pThis->creationMode = creationMode;
						pThis->shareMode = shareMode;
						pThis->readBufferOffset = 0;
						pThis->readBufferLength = 0;
						pThis->writeBufferOffset = 0;
						pThis->writeBufferLength = 0;

						DWORD currentFilePosition = SetFilePointer(hFile, 0, 0, FILE_CURRENT);
						pThis->currentFilePosition = currentFilePosition;
						pThis->position = currentFilePosition;
						result = true;
					}
				}
				catch (const std::bad_alloc&)
				{
					SetRZFileErrorCode(pThis, ERROR_OUTOFMEMORY);
				}
				catch (const std::exception&)
				{
					SetRZFileErrorCode(pThis, ERROR_FILENAME_EXCED_RANGE);
				}
			}
		}

		return result;
	}

	typedef bool(__thiscall* pfn_cRZFile_ReadWithCount)(cRZFileProxy* pThis, void* outBuffer, uint32_t& byteCount);

	static pfn_cRZFile_ReadWithCount RealReadWithCount = nullptr;

	bool __fastcall HookedReadWithCount(cRZFileProxy* pThis, void* edxUnused, void* outBuffer, uint32_t& byteCount)
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
					uint32_t bytesRead = 0;
					if (ReadFileBlocking(pThis->fileHandle, static_cast<uint8_t*>(outBuffer), byteCount, bytesRead))
					{
						byteCount = bytesRead;
						pThis->position += bytesRead;
						result = true;
					}
					else
					{
						SetRZFileErrorCode(pThis, GetLastError());
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
}


void cRZFileHooks::Install()
{
	Logger& logger = Logger::GetInstance();

	try
	{
		RealOpen = reinterpret_cast<pfn_cRZFile_Open>(0x919B00);
		RealReadWithCount = reinterpret_cast<pfn_cRZFile_ReadWithCount>(0x9192A9);

		DetourRestoreAfterWith();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)RealOpen, HookedOpen);
		DetourAttach(&(PVOID&)RealReadWithCount, HookedReadWithCount);
		LONG error = DetourTransactionCommit();

		if (error == NO_ERROR)
		{
			logger.WriteLine(LogLevel::Info, "Installed the cRZFile hooks.");
		}
		else
		{
			logger.WriteLineFormatted(
				LogLevel::Error,
				"Failed to install the cRZFile hooks, error code=%d",
				error);
		}
	}
	catch (const std::exception& e)
	{
		logger.WriteLineFormatted(
			LogLevel::Error,
			"Failed to install the cRZFile hooks: %s",
			e.what());
	}
}
