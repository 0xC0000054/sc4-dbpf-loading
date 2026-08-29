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

#include "cRZFileHooks.h"
#include "cRZFileProxy.h"
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
			const DWORD numberOfBytesToRead = (std::min)(0x80000000UL, static_cast<DWORD>(remaining));
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
		cIGZFile::AccessMode accessMode,
		cIGZFile::CreationMode creationMode,
		cIGZFile::ShareMode shareMode);
	static pfn_cRZFile_Open RealOpen = nullptr;

	bool __fastcall HookedOpen(
		cRZFileProxy* pThis,
		void* edxUnused,
		cIGZFile::AccessMode accessMode,
		cIGZFile::CreationMode creationMode,
		cIGZFile::ShareMode shareMode)
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
					case cIGZFile::AccessMode::Read:
						dwDesiredAccess = GENERIC_READ;
						break;
					case cIGZFile::AccessMode::Write:
						dwDesiredAccess = GENERIC_WRITE;
						break;
					case cIGZFile::AccessMode::ReadWrite:
						dwDesiredAccess = GENERIC_READ | GENERIC_WRITE;
						break;
					}

					switch (creationMode)
					{
					case cIGZFile::CreationMode::CreateNew:
						dwCreationDisposition = CREATE_NEW;
						break;
					case cIGZFile::CreationMode::CreateAlways:
						dwCreationDisposition = CREATE_ALWAYS;
						break;
					case cIGZFile::CreationMode::OpenExisting:
						dwCreationDisposition = OPEN_EXISTING;
						break;
					case cIGZFile::CreationMode::OpenAlways:
						dwCreationDisposition = OPEN_ALWAYS;
						break;
					case cIGZFile::CreationMode::TruncateExisting:
						dwCreationDisposition = TRUNCATE_EXISTING;
						break;
					}

					switch (shareMode)
					{
					case cIGZFile::ShareMode::Read:
						dwShareMode = FILE_SHARE_READ;
						break;
					case cIGZFile::ShareMode::ReadWrite:
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
						pThis->isOpen = true;
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

	typedef bool(__thiscall* pfn_cRZFile_ReadWriteWithCount)(cRZFileProxy* pThis, void* outBuffer, uint32_t& byteCount);

	static pfn_cRZFile_ReadWriteWithCount RealReadWithCount = nullptr;

	bool __fastcall HookedReadWithCount(cRZFileProxy* pThis, void* edxUnused, void* outBuffer, uint32_t& byteCount)
	{
		bool result = false;
#if 0
		Logger::GetInstance().WriteLineFormatted(
			LogLevel::Info,
			"HookedReadWithCount: %u bytes requested, position: 0x%08x, name: %s",
			byteCount,
			pThis->currentFilePosition + (pThis->currentFilePosition - pThis->readBufferOffset),
			pThis->nameRZStr.AsIGZString()->ToChar());
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

	static pfn_cRZFile_ReadWriteWithCount RealWriteWithCount = nullptr;

	bool __fastcall HookedWriteWithCount(cRZFileProxy* pThis, void* edxUnused, void* buffer, uint32_t& byteCount)
	{
		bool result = false;
#if 0
		Logger::GetInstance().WriteLineFormatted(
			LogLevel::Info,
			"HookedWriteWithCount: Writing %u bytes, position: 0x%08x, write buffer length: %u, name: %s",
			byteCount,
			pThis->currentFilePosition,
			pThis->writeBufferLength,
			pThis->nameRZStr.AsIGZString()->ToChar());
#endif

		if (pThis->isOpen)
		{
			if (byteCount == 0)
			{
				result = true;
			}
			else
			{
				// If the number of bytes to write is larger than the games buffer size, we will send the
				// buffer directly to the OS.
				// This can significantly reduce the required number of system calls for large writes when
				// compared to the game's standard behavior of copying from a fixed-size buffer in a loop.
				//
				// To minimize complexity and potential compatibility issues, our code only runs when the
				// following conditions are true:
				//
				// 1. The game's write buffer size is greater than 0 and less than the number of bytes to write.
				// 2. The file is at the correct position to start writing.
				// 3. The current write buffer is empty.
				//
				// If any of these conditions are not met, the call will be forwarded to the game's
				// original write method.
				if (byteCount >= pThis->maxWriteBufferSize
					&& pThis->maxWriteBufferSize > 0
					&& pThis->position == pThis->currentFilePosition
					&& pThis->writeBufferLength == 0)
				{
					// Invalidate the read buffer
					pThis->readBufferOffset = 0;
					pThis->readBufferLength = 0;

					DWORD bytesWritten = 0;

					if (WriteFile(pThis->fileHandle, buffer, byteCount, &bytesWritten, nullptr))
					{
						byteCount = static_cast<uint32_t>(bytesWritten);
						pThis->position += bytesWritten;
						result = true;
					}
					else
					{
						// The buffered write loop in cRZFile_FillWriteBuffer ignores
						// the error code from WriteFile, so we recreate that behavior.
						pThis->position = SetFilePointer(pThis->fileHandle, 0, nullptr, FILE_CURRENT);
					}

					pThis->writeBufferOffset = pThis->position;
					pThis->writeBufferLength = 0;
				}
				else
				{
					result = RealWriteWithCount(pThis, buffer, byteCount);
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
		RealReadWithCount = reinterpret_cast<pfn_cRZFile_ReadWriteWithCount>(0x9192A9);
		RealWriteWithCount = reinterpret_cast<pfn_cRZFile_ReadWriteWithCount>(0x9193e3);

		DetourRestoreAfterWith();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)RealOpen, HookedOpen);
		DetourAttach(&(PVOID&)RealReadWithCount, HookedReadWithCount);
		DetourAttach(&(PVOID&)RealWriteWithCount, HookedWriteWithCount);
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
