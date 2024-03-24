#include "cRZFileHooks.h"
#include "Logger.h"
#include "Patcher.h"
#include <Windows.h>
#include <stdexcept>

namespace
{


	static constexpr uintptr_t ReadWithCount_Inject = 0x9192F1;
	static constexpr uintptr_t ReadWithCount_Return_Jump = 0x9193DC;
	static constexpr uintptr_t ReadWithCount_ClearBufferLoop_Jump = 0x919386;
	static constexpr uintptr_t ReadWithCount_CopyDataFromBuffer_Jump = 0x919304;

	static constexpr int kFileHandleFieldOffset = 0x1C;
	static constexpr int kIOErrorFieldOffset = 0x30;
	static constexpr int kCurrentFilePositionFieldOffset = 0x48;
	static constexpr int kPositionFieldOffset = 0x4C;
	static constexpr int kMaxReadBufferSizeFieldOffset = 0x50;
	static constexpr int kCurrentReadBufferOffsetFieldOffset = 0x60;
	static constexpr int kCurrentReadBufferLengthFieldOffset = 0x64;

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

	class cRZFileProxy
	{
	public:
		void* vtable;
		void* baseVTable;
		intptr_t unknown1[4];
		int isOpen;
		HANDLE fileHandle;
		intptr_t unknown2[4];
		uint32_t fileIOError;
		intptr_t unknown3[5];
		uint32_t currentFileOffset;
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
		uint32_t writeBufferSize;
	};

	static_assert(offsetof(cRZFileProxy, isOpen) == 0x18);
	static_assert(offsetof(cRZFileProxy, fileHandle) == 0x1C);
	static_assert(offsetof(cRZFileProxy, fileIOError) == 0x30);
	static_assert(offsetof(cRZFileProxy, currentFileOffset) == 0x48);
	static_assert(offsetof(cRZFileProxy, readBufferOffset) == 0x60);
	static_assert(offsetof(cRZFileProxy, readBufferLength) == 0x64);
	static_assert(offsetof(cRZFileProxy, writeBufferOffset) == 0x78);

	static void NAKED_FUN Hook_cRZFile_ReadWithCount()
	{
		static cRZFileProxy* pThis;
		static DWORD numBytesToRead;

		__asm
		{
			mov pThis, esi;
			// The EBX register is used to by the calling code to
			// store  the number of bytes to read.
			mov ebx, dword ptr[edi];
			mov dword ptr[numBytesToRead], ebx;
			pushad;
		}

		// The original code will only read directly from the file into the output buffer
		// when readBufferSize equals 0, in all other cases it will read from the file in
		// chunks that are at most equal to readBufferSize.
		//
		// We change that logic so that it will read directly from the file into the
		// output buffer when the requested read size is greater than or equal to the
		// read buffer size and the existing read buffer is empty or invalid.
		if (pThis->currentFileOffset < pThis->readBufferOffset || (pThis->readBufferOffset + pThis->readBufferLength) <= pThis->currentFileOffset)
		{
			if (numBytesToRead >= pThis->maxReadBufferSize)
			{
				static HANDLE fileHandle;
				static void* outputBuffer;
				static DWORD position;

				__asm
				{
					// Invalidate the existing read buffer offset and length.
					mov dword ptr[esi + kCurrentReadBufferOffsetFieldOffset], 0;
					mov dword ptr[esi + kCurrentReadBufferLengthFieldOffset], 0;
					// Get the file handle.
					push edx;
					mov edx, dword ptr[esi + kFileHandleFieldOffset];
					mov dword ptr[fileHandle], edx;
					// Get the game's current file position
					mov edx, dword ptr[esi + kPositionFieldOffset];
					mov dword ptr[position], edx;
					// Get the output buffer pointer.
					mov edx, dword ptr[ebp + 0x8];
					mov dword ptr[outputBuffer], edx;
					pop edx;
				}

				// TODO: finish this

				if (ReadFileBlocking(fileHandle, static_cast<BYTE*>(outputBuffer), numBytesToRead))
				{
					position = position + numBytesToRead;

					__asm
					{
						// Update the position and file position.
						// The popad instruction below will restore the registers before we return.
						mov edx, dword ptr[position];
						mov dword ptr[edi + kPositionFieldOffset], edx;
						mov dword ptr[edi + kCurrentFilePositionFieldOffset], edx;
						// Jump to the end of the method.
						// The method result is in AL, we don't need to update it because
						// it was set to 1/true before the game jumped into this method.
						popad;
						push ReadWithCount_Return_Jump;
						ret;
					}
				}
				else
				{
					DWORD errorCode = GetRZFileErrorCode();
					DWORD currentFileOffset = SetFilePointer(fileHandle, 0, nullptr, FILE_CURRENT);
					__asm
					{
						// Update the position and file position.
						// The popad instruction below will restore the registers before we return.
						mov edx, dword ptr[currentFileOffset];
						mov dword ptr[edi + kPositionFieldOffset], edx;
						mov dword ptr[edi + kCurrentFilePositionFieldOffset], edx;
						// Set the error code.
						mov edx, dword ptr[errorCode];
						mov dword ptr[edi + kIOErrorFieldOffset], edx;
						// Jump to the end of the method.
						// The method result is in AL, we XOR it with itself to
						// set it to 0/false.
						popad;
						xor al, al;
						push ReadWithCount_Return_Jump;
						ret;
					}
				}
			}

			// If the requested number of bytes is smaller than SC4's read buffer, jump to the
			// code that clears the buffer and refills it.
			_asm
			{
				popad;
				push ReadWithCount_ClearBufferLoop_Jump;
				ret;
			}
		}

		// The read buffer has valid data, continue with the original code.
		_asm
		{
			popad;
			push ReadWithCount_CopyDataFromBuffer_Jump;
			ret;
		}
	}

	void IncreaseRZFileDefaultBufferSize()
	{
		// The RZFile constructor sets its default read and write buffer sizes to 512 bytes.
		// As this is very small, we change it to 4096 bytes.
		// This will significantly reduce the number of system calls that SC4 has to make
		// when reading a file.
		// This change is applied to 3 different cRZFile constructor overloads.
		//
		// Original instruction: e8 0x200  (MOV EAX,0x200)
		// New instruction:      e8 0x1000 (MOV EAX,0x1000)

		constexpr uint32_t kNewBufferSize = 0x1000;

		// cRZFile()
		Patcher::OverwriteMemory(0x918BD3, kNewBufferSize);
		// cRZFile(char*)
		Patcher::OverwriteMemory(0x919AA9, kNewBufferSize);
		// cRZFile(cIGZString const&)
		Patcher::OverwriteMemory(0x918C41, kNewBufferSize);
	}

	void InstallReadWithCountHook()
	{
		Patcher::InstallHook(ReadWithCount_Inject, &Hook_cRZFile_ReadWithCount);
	}
}


void cRZFileHooks::Install(uint16_t gameVersion)
{
	if (gameVersion == 641)
	{
		Logger& logger = Logger::GetInstance();

		try
		{
			IncreaseRZFileDefaultBufferSize();
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
