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

#include "SC4DirectoryEnumerator.h"
#include "GZStringConvert.h"
#include "PathUtil.h"
#include "StringViewUtil.h"
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
	cRZBaseString CreateUtf8FilePath(const std::wstring& root, const std::wstring_view& fileName)
	{
		const std::wstring nativePath = PathUtil::Combine(PathUtil::RemoveExtendedPathPrefix(root), fileName);

		return GZStringConvert::FromUtf16(nativePath);
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

		const std::wstring_view extension = PathUtil::GetExtension(fileName);

		if (!extension.empty())
		{
			result = boost::istarts_with(extension, ".SC4"sv);
		}
		else
		{
			// Files without an extension are treated as potential .SC4* files, there
			// are released plugins that don't have a file extension.
			// For example, Bosham Church by mintoes
			//
			// If the file is not a DBPF file, it will fail the signature check that the game
			// performs when loading DBPF files and the plugin will log it as an error.
			result = true;
		}

		return result;
	}

	typedef bool(*FileNamePredicate)(const std::wstring_view& fileName);

	std::wstring GetAllFilesSearchPattern(const std::wstring& directory, bool topLevelDirectory)
	{
		std::wstring searchDirectory;

		if (PathUtil::MustAddExtendedPathPrefix(directory))
		{
			searchDirectory = PathUtil::AddExtendedPathPrefix(directory);

			if (topLevelDirectory)
			{
				// The extended path must be normalized because the OS won't do it for us.
				// We only do this for the top-level directory, because if it is normalized
				// any sub directories will also be valid.
				searchDirectory = PathUtil::Normalize(searchDirectory);
			}
		}
		else
		{
			searchDirectory = directory;
		}

		return PathUtil::Combine(searchDirectory, L"*"sv);
	}

	void NativeScanDirectoryRecursive(
		const std::filesystem::path& directory,
		bool topLevelDirectory,
		std::vector<cRZBaseString>& files,
		FileNamePredicate Predicate)
	{
		std::vector<std::filesystem::path> subFolders;

		WIN32_FIND_DATAW findData{};

		const std::filesystem::path searchPattern = GetAllFilesSearchPattern(directory, topLevelDirectory);

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
						subFolders.push_back(PathUtil::Combine(directory, std::wstring_view(findData.cFileName)));
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
			NativeScanDirectoryRecursive(path, false, files, Predicate);
		}
	}
}

void SC4DirectoryEnumerator::ScanDirectoryForDatFilesRecursive(const cIGZString& root, std::vector<cRZBaseString>& output)
{
	NativeScanDirectoryRecursive(GZStringConvert::ToUtf16(root), true, output, DatFilesPredicate);
}

void SC4DirectoryEnumerator::ScanDirectoryForLooseSC4FilesRecursive(const cIGZString& root, std::vector<cRZBaseString>& output)
{
	NativeScanDirectoryRecursive(GZStringConvert::ToUtf16(root), true, output, SC4FilesPredicate);
}
