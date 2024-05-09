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

#include "SC4DirectoryEnumerator.h"
#include "GZStringConvert.h"
#include "StringViewUtil.h"
#include <array>
#include <filesystem>
#include <stdexcept>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/result.h"
#include "wil/filesystem.h"
#include "wil/win32_helpers.h"
#include "boost/algorithm/string.hpp"

using namespace std::string_view_literals;

namespace
{
	std::filesystem::path AppendFileName(const std::filesystem::path& root, const std::wstring_view& fileName)
	{
		std::filesystem::path newPath = root;
		newPath /= fileName;

		return newPath;
	}

	cRZBaseString CreateUtf8FilePath(const std::filesystem::path& root, const std::wstring_view& fileName)
	{
		const std::filesystem::path nativePath = AppendFileName(root, fileName);

		return GZStringConvert::FromFileSystemPath(nativePath);
	}

	void ThrowExceptionForWin32Error(const char* win32MethodName, DWORD error)
	{
		char buffer[1024]{};

		std::snprintf(
			buffer,
			sizeof(buffer),
			"%s failed with error code %u.",
			win32MethodName,
			error);

		throw std::runtime_error(buffer);
	}

	bool DatFilesPredicate(const std::wstring_view& fileName)
	{
		return boost::iends_with(fileName, L".DAT"sv);
	}

	bool SC4FilesPredicate(const std::wstring_view& fileName)
	{
		bool result = false;

		size_t periodIndex = fileName.find_last_of(L'.');

		if (periodIndex != std::wstring::npos)
		{
			std::wstring_view extension = fileName.substr(periodIndex);

			result = boost::istarts_with(extension, ".SC4"sv);
		}

		return result;
	}

	typedef bool(*FileNamePredicate)(const std::wstring_view& fileName);

	// From https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ne-wdm-_file_information_class
	typedef enum _FILE_INFORMATION_CLASS
	{
		FileDirectoryInformation = 1,
		FileFullDirectoryInformation = 2,
		FileBothDirectoryInformation = 3,
		FileBasicInformation = 4,
		FileStandardInformation = 5,
		FileInternalInformation = 6,
		FileEaInformation = 7,
		FileAccessInformation = 8,
		FileNameInformation = 9,
		FileRenameInformation = 10,
		FileLinkInformation = 11,
		FileNamesInformation = 12,
		FileDispositionInformation = 13,
		FilePositionInformation = 14,
		FileFullEaInformation = 15,
		FileModeInformation = 16,
		FileAlignmentInformation = 17,
		FileAllInformation = 18,
		FileAllocationInformation = 19,
		FileEndOfFileInformation = 20,
		FileAlternateNameInformation = 21,
		FileStreamInformation = 22,
		FilePipeInformation = 23,
		FilePipeLocalInformation = 24,
		FilePipeRemoteInformation = 25,
		FileMailslotQueryInformation = 26,
		FileMailslotSetInformation = 27,
		FileCompressionInformation = 28,
		FileObjectIdInformation = 29,
		FileCompletionInformation = 30,
		FileMoveClusterInformation = 31,
		FileQuotaInformation = 32,
		FileReparsePointInformation = 33,
		FileNetworkOpenInformation = 34,
		FileAttributeTagInformation = 35,
		FileTrackingInformation = 36,
		FileIdBothDirectoryInformation = 37,
		FileIdFullDirectoryInformation = 38,
		FileValidDataLengthInformation = 39,
		FileShortNameInformation = 40,
		FileIoCompletionNotificationInformation = 41,
		FileIoStatusBlockRangeInformation = 42,
		FileIoPriorityHintInformation = 43,
		FileSfioReserveInformation = 44,
		FileSfioVolumeInformation = 45,
		FileHardLinkInformation = 46,
		FileProcessIdsUsingFileInformation = 47,
		FileNormalizedNameInformation = 48,
		FileNetworkPhysicalNameInformation = 49,
		FileIdGlobalTxDirectoryInformation = 50,
		FileIsRemoteDeviceInformation = 51,
		FileUnusedInformation = 52,
		FileNumaNodeInformation = 53,
		FileStandardLinkInformation = 54,
		FileRemoteProtocolInformation = 55,
		FileRenameInformationBypassAccessCheck = 56,
		FileLinkInformationBypassAccessCheck = 57,
		FileVolumeNameInformation = 58,
		FileIdInformation = 59,
		FileIdExtdDirectoryInformation = 60,
		FileReplaceCompletionInformation = 61,
		FileHardLinkFullIdInformation = 62,
		FileIdExtdBothDirectoryInformation = 63,
		FileDispositionInformationEx = 64,
		FileRenameInformationEx = 65,
		FileRenameInformationExBypassAccessCheck = 66,
		FileDesiredStorageClassInformation = 67,
		FileStatInformation = 68,
		FileMemoryPartitionInformation = 69,
		FileStatLxInformation = 70,
		FileCaseSensitiveInformation = 71,
		FileLinkInformationEx = 72,
		FileLinkInformationExBypassAccessCheck = 73,
		FileStorageReserveIdInformation = 74,
		FileCaseSensitiveInformationForceAccessCheck = 75,
		FileKnownFolderInformation = 76,
		FileStatBasicInformation = 77,
		FileId64ExtdDirectoryInformation = 78,
		FileId64ExtdBothDirectoryInformation = 79,
		FileIdAllExtdDirectoryInformation = 80,
		FileIdAllExtdBothDirectoryInformation = 81,
		FileMaximumInformation
	} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

	// From https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_full_dir_info
	typedef struct _FILE_FULL_DIR_INFORMATION
	{
		ULONG         NextEntryOffset;
		ULONG         FileIndex;
		LARGE_INTEGER CreationTime;
		LARGE_INTEGER LastAccessTime;
		LARGE_INTEGER LastWriteTime;
		LARGE_INTEGER ChangeTime;
		LARGE_INTEGER EndOfFile;
		LARGE_INTEGER AllocationSize;
		ULONG         FileAttributes;
		ULONG         FileNameLength;
		ULONG         EaSize;
		WCHAR         FileName[1];
	} FILE_FULL_DIR_INFORMATION, *PFILE_FULL_DIR_INFORMATION;

	// From https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_io_status_block
	typedef struct _IO_STATUS_BLOCK
	{
		union
		{
			NTSTATUS Status;
			PVOID    Pointer;
		};
		ULONG_PTR Information;
	} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

	// From https://learn.microsoft.com/en-us/windows/win32/api/ntdef/ns-ntdef-_unicode_string
	typedef struct _UNICODE_STRING
	{
		USHORT Length;
		USHORT MaximumLength;
		PWSTR  Buffer;
	} UNICODE_STRING, * PUNICODE_STRING;

	using PIO_APC_ROUTINE = PVOID;

	// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntquerydirectoryfile
	typedef NTSTATUS (NTAPI* pfnNtQueryDirectoryFile)(
		HANDLE                 FileHandle,
		HANDLE                 Event,
		PIO_APC_ROUTINE        ApcRoutine,
		PVOID                  ApcContext,
		PIO_STATUS_BLOCK       IoStatusBlock,
		PVOID                  FileInformation,
		ULONG                  Length,
		FILE_INFORMATION_CLASS FileInformationClass,
		BOOLEAN                ReturnSingleEntry,
		PUNICODE_STRING        FileName,
		BOOLEAN                RestartScan
	);

	// https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-rtlntstatustodoserror
	typedef DWORD (NTAPI* pfnRtlNtStatusToDosError)(NTSTATUS Status);

#ifndef STATUS_NO_MORE_FILES
#define STATUS_NO_MORE_FILES 0x80000006
#endif // !STATUS_NO_MORE_FILES

	static const pfnNtQueryDirectoryFile NtQueryDirectoryFile = reinterpret_cast<pfnNtQueryDirectoryFile>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryDirectoryFile"));
	static const pfnRtlNtStatusToDosError RtlNtStatusToDosError = reinterpret_cast<pfnRtlNtStatusToDosError>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlNtStatusToDosError"));

	bool FillDirectoryBuffer(HANDLE directoryHandle, void* buffer, ULONG bufferSize)
	{
		IO_STATUS_BLOCK statusBlock;

		NTSTATUS status = NtQueryDirectoryFile(
			directoryHandle,
			nullptr,
			nullptr,
			nullptr,
			&statusBlock,
			buffer,
			bufferSize,
			FileFullDirectoryInformation,
			FALSE,
			nullptr,
			FALSE);

		bool result = false;

		if (status == STATUS_SUCCESS)
		{
			result = true;
		}
		else
		{
			if (status != STATUS_NO_MORE_FILES)
			{
				ThrowExceptionForWin32Error("NtQueryDirectoryFile", RtlNtStatusToDosError(status));
			}
		}

		return result;
	}

	bool IsNtPathDotOrDotDot(const std::wstring_view& fileName)
	{
		bool result = false;

		switch (fileName.size())
		{
		case 1:
			result = fileName[0] == L'.';
			break;
		case 2:
			result = fileName[0] == L'.' && fileName[1] == L'.';
			break;
		}

		return result;
	}

	void ScanDirectoryRecursiveNt(
		const std::filesystem::path& directory,
		std::vector<cRZBaseString>& files,
		FileNamePredicate Predicate,
		void* buffer,
		ULONG bufferSize)
	{
		std::vector<std::filesystem::path> subFolders;

		wil::unique_hfile directoryHandle(CreateFileW(
			directory.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS,
			nullptr));

		if (directoryHandle)
		{
			while (FillDirectoryBuffer(directoryHandle.get(), buffer, bufferSize))
			{
				wil::next_entry_offset_iterator<FILE_FULL_DIR_INFORMATION> entries(static_cast<FILE_FULL_DIR_INFORMATION*>(buffer));

				for (const auto& entry : entries)
				{
					const std::wstring_view fileName(entry.FileName, entry.FileNameLength / sizeof(WCHAR));

					if ((entry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
					{
						if (!IsNtPathDotOrDotDot(fileName))
						{
							subFolders.push_back(AppendFileName(directory, fileName));
						}
					}
					else
					{
						if (Predicate(fileName))
						{
							files.push_back(CreateUtf8FilePath(directory, fileName));
						}
					}
				}
			}
		}
		else
		{
			DWORD lastError = GetLastError();

			if (lastError != ERROR_FILE_NOT_FOUND
				&& lastError != ERROR_PATH_NOT_FOUND
				&& lastError != ERROR_ACCESS_DENIED)
			{
				ThrowExceptionForWin32Error("CreateFileW", lastError);
			}
		}

		// Recursively search the sub-directories.
		for (const auto& path : subFolders)
		{
			ScanDirectoryRecursiveNt(path, files, Predicate, buffer, bufferSize);
		}
	}

	void ScanDirectoryRecursiveWin32(
		const std::filesystem::path& directory,
		std::vector<cRZBaseString>& files,
		FileNamePredicate Predicate)
	{
		std::vector<std::filesystem::path> subFolders;

		WIN32_FIND_DATAW findData{};

		const std::filesystem::path searchPattern = AppendFileName(directory, L"*"sv);

		wil::unique_hfind findHandle(FindFirstFileExW(
			searchPattern.c_str(),
			FindExInfoBasic,
			&findData,
			FindExSearchNameMatch,
			nullptr,
			0));

		if (findHandle)
		{
			do
			{
				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				{
					if (!wil::path_is_dot_or_dotdot(findData.cFileName))
					{
						subFolders.push_back(AppendFileName(directory, std::wstring_view(findData.cFileName)));
					}
				}
				else
				{
					const std::wstring_view fileName(findData.cFileName);

					if (Predicate(fileName))
					{
						files.push_back(CreateUtf8FilePath(directory, fileName));
					}
				}
			} while (FindNextFileW(findHandle.get(), &findData));

			DWORD lastError = GetLastError();

			if (lastError != ERROR_SUCCESS && lastError != ERROR_NO_MORE_FILES)
			{
				ThrowExceptionForWin32Error("FindNextFileW", lastError);
			}
		}
		else
		{
			DWORD lastError = GetLastError();

			if (lastError != ERROR_SUCCESS && lastError != ERROR_NO_MORE_FILES)
			{
				ThrowExceptionForWin32Error("FindFirstFileExW", lastError);
			}
		}

		// Recursively search the sub-directories.
		for (const auto& path : subFolders)
		{
			ScanDirectoryRecursiveWin32(path, files, Predicate);
		}
	}

	void NativeScanDirectoryRecursive(
		const std::filesystem::path& directory,
		std::vector<cRZBaseString>& files,
		FileNamePredicate Predicate)
	{
		if (NtQueryDirectoryFile && RtlNtStatusToDosError)
		{
			// We use the Windows NT native file APIs if available.
			// This can improve performance for directories with a very large number of files and allows
			// us to reuse the read buffer for each directory that we enumerate.

			std::array<uint8_t, 8192> buffer{};
			ScanDirectoryRecursiveNt(
				directory,
				files,
				Predicate,
				buffer.data(),
				static_cast<ULONG>(buffer.size()));
		}
		else
		{
			// If the Windows NT native file APIs are not available, we fall back
			// to using the Win32 FindFirstFile and FindNextFile APis.

			ScanDirectoryRecursiveWin32(directory, files, Predicate);
		}
	}
}

void SC4DirectoryEnumerator::ScanDirectoryForDatFilesRecursive(const cIGZString& root, std::vector<cRZBaseString>& output)
{
	NativeScanDirectoryRecursive(GZStringConvert::ToFileSystemPath(root), output, DatFilesPredicate);
}

void SC4DirectoryEnumerator::ScanDirectoryForLooseSC4FilesRecursive(const cIGZString& root, std::vector<cRZBaseString>& output)
{
	NativeScanDirectoryRecursive(GZStringConvert::ToFileSystemPath(root), output, SC4FilesPredicate);
}
