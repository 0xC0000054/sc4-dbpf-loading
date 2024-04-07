#include "cRZFileHooks.h"
#include "cIGZString.h"
#include "Logger.h"
#include "Patcher.h"
#include <stdexcept>

#define NOMINMAX

#include <Windows.h>
#include "detours/detours.h"

namespace
{
	void PrintLineToDebugOutput(const char* const line)
	{
		OutputDebugStringA(line);
		OutputDebugStringA("\n");
	}

	void PrintLineToDebugOutputFormatted(const char* const format, ...)
	{
		va_list args;
		va_start(args, format);

		va_list argsCopy;
		va_copy(argsCopy, args);

		int formattedStringLength = std::vsnprintf(nullptr, 0, format, argsCopy);

		va_end(argsCopy);

		if (formattedStringLength > 0)
		{
			size_t formattedStringLengthWithNull = static_cast<size_t>(formattedStringLength) + 1;

			constexpr size_t stackBufferSize = 1024;

			if (formattedStringLengthWithNull >= stackBufferSize)
			{
				std::unique_ptr<char[]> buffer = std::make_unique_for_overwrite<char[]>(formattedStringLengthWithNull);

				std::vsnprintf(buffer.get(), formattedStringLengthWithNull, format, args);

				PrintLineToDebugOutput(buffer.get());
			}
			else
			{
				char buffer[stackBufferSize]{};

				std::vsnprintf(buffer, stackBufferSize, format, args);

				PrintLineToDebugOutput(buffer);
			}
		}

		va_end(args);
	}

	bool ReadFileBlocking(HANDLE hFile, BYTE* buffer, size_t count)
	{
		size_t offset = 0;
		size_t bytesRemaining = count;

		while (bytesRemaining > 0)
		{
			DWORD numBytesToRead = 0x80000000UL;

			if (count < numBytesToRead)
			{
				numBytesToRead = static_cast<DWORD>(count);
			}

			DWORD bytesRead = 0;

			if (!ReadFile(hFile, buffer, numBytesToRead, &bytesRead, nullptr))
			{
				return false;
			}

			offset += bytesRead;
			bytesRemaining -= bytesRead;
		}

		return true;
	}

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
		intptr_t unknown2[4];
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
		PrintLineToDebugOutputFormatted(
			"HookedReadWithCount: %u bytes requested, position: %u, name: %s",
			byteCount,
			pThis->currentFilePosition + (pThis->currentFilePosition - pThis->readBufferOffset),
			reinterpret_cast<cIGZString*>(&pThis->nameRZStr)->ToChar());
#endif

		if (pThis->isOpen)
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
			//
			// If any of these conditions are not met, the call will be forwarded to the game's
			// original read method.
			if (byteCount >= pThis->maxReadBufferSize
				&& pThis->maxReadBufferSize > 0
				&& pThis->position == pThis->currentFilePosition
				&& (pThis->currentFilePosition < pThis->readBufferOffset || (pThis->readBufferOffset + pThis->readBufferLength) <= pThis->currentFilePosition))
			{
				if (ReadFileBlocking(pThis->fileHandle, static_cast<BYTE*>(outBuffer), byteCount))
				{
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


void cRZFileHooks::Install(uint16_t gameVersion)
{
	if (gameVersion == 641)
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
}
