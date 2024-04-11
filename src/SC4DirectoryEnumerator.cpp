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
#include <filesystem>
#include <stdexcept>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/result.h"
#include "wil/filesystem.h"
#include "wil/win32_helpers.h"
#include "boost/algorithm/string.hpp"

using namespace std::string_view_literals;

static const wchar_t* const AllFilesSearchPattern = L"*";

static const std::wstring_view DatFileExtension = L".DAT"sv;

namespace
{
	std::filesystem::path AppendFileName(const std::filesystem::path& root, const wchar_t* const fileName)
	{
		std::filesystem::path newPath = root;
		newPath /= fileName;

		return newPath;
	}

	cRZBaseString CreateUtf8FilePath(const std::filesystem::path& root, const wchar_t* const fileName)
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

	void NativeScanDirectoryForDatFilesRecursive(
		const std::filesystem::path& directory,
		std::vector<cRZBaseString>& datFiles)
	{
		std::vector<std::filesystem::path> subFolders;

		WIN32_FIND_DATAW findData{};

		const std::filesystem::path searchPattern = AppendFileName(directory, AllFilesSearchPattern);

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
						subFolders.push_back(AppendFileName(directory, findData.cFileName));
					}
				}
				else
				{
					if (boost::iends_with(std::wstring_view(findData.cFileName), DatFileExtension))
					{
						datFiles.push_back(CreateUtf8FilePath(directory, findData.cFileName));
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
			NativeScanDirectoryForDatFilesRecursive(path, datFiles);
		}
	}

	void NativeScanDirectoryForSC4FilesRecursive(
		const std::filesystem::path& directory,
		std::vector<cRZBaseString>& sc4Files)
	{
		std::vector<std::filesystem::path> subFolders;

		WIN32_FIND_DATAW findData{};

		const std::filesystem::path searchPattern = AppendFileName(directory, AllFilesSearchPattern);

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
						subFolders.push_back(AppendFileName(directory, findData.cFileName));
					}
				}
				else
				{
					const std::wstring_view fileName(findData.cFileName);

					size_t periodIndex = fileName.find_last_of(L".");

					if (periodIndex != std::wstring::npos)
					{
						std::wstring_view extension = fileName.substr(periodIndex);

						if (boost::istarts_with(extension, ".SC4"sv))
						{
							sc4Files.push_back(CreateUtf8FilePath(directory, findData.cFileName));
						}
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
			NativeScanDirectoryForSC4FilesRecursive(path, sc4Files);
		}
	}
}

void SC4DirectoryEnumerator::ScanDirectoryForDatFilesRecursive(const cIGZString& root, std::vector<cRZBaseString>& output)
{
	NativeScanDirectoryForDatFilesRecursive(GZStringConvert::ToFileSystemPath(root), output);
}

void SC4DirectoryEnumerator::ScanDirectoryForLooseSC4FilesRecursive(const cIGZString& root, std::vector<cRZBaseString>& output)
{
	NativeScanDirectoryForSC4FilesRecursive(GZStringConvert::ToFileSystemPath(root), output);
}
