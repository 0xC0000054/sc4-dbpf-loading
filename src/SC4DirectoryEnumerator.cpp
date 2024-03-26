#include "SC4DirectoryEnumerator.h"
#include "GZStringConvert.h"
#include "StringViewUtil.h"
#include <stdexcept>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/result.h"
#include "wil/filesystem.h"
#include "wil/win32_helpers.h"
#include "boost/algorithm/string.hpp"

using namespace std::string_view_literals;

static const wchar_t* const AllFilesSearchPattern = L"*";
static const wchar_t* const DatFilesSearchPattern = L"*.DAT";

static const std::wstring_view DatFileExtension = L".DAT"sv;
static const std::wstring_view Sc4FileExtension = L".SC4"sv;

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
}

SC4DirectoryEnumerator::SC4DirectoryEnumerator(
	const std::filesystem::path& root,
	SC4DirectoryEnumeratorOptions options)
{
	Init(root, options);
}

SC4DirectoryEnumerator::SC4DirectoryEnumerator(const cIGZString& path, SC4DirectoryEnumeratorOptions options)
{
	const std::filesystem::path nativePath = GZStringConvert::ToFileSystemPath(path);

	Init(nativePath, options);
}

const std::vector<cRZBaseString>& SC4DirectoryEnumerator::DatFiles() const
{
	return datFiles;
}

const std::vector<cRZBaseString>& SC4DirectoryEnumerator::Sc4Files() const
{
	return sc4Files;
}

void SC4DirectoryEnumerator::Init(const std::filesystem::path& path, SC4DirectoryEnumeratorOptions options)
{
	bool includeSc4Files = (options & SC4DirectoryEnumeratorOptions::OnlyDatFiles) == SC4DirectoryEnumeratorOptions::None;

	if ((options & SC4DirectoryEnumeratorOptions::TopDirectoryOnly) == SC4DirectoryEnumeratorOptions::TopDirectoryOnly)
	{
		if (includeSc4Files)
		{
			ScanDirectory(path);
		}
		else
		{
			ScanDirectoryForDatFiles(path);
		}
	}
	else
	{
		if (includeSc4Files)
		{
			ScanDirectoryRecursive(path);
		}
		else
		{
			ScanDirectoryForDatFilesRecursive(path);
		}
	}
}

void SC4DirectoryEnumerator::ScanDirectory(const std::filesystem::path& root)
{
	WIN32_FIND_DATAW findData{};

	const std::filesystem::path searchPattern = AppendFileName(root, AllFilesSearchPattern);

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
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				std::wstring_view fileName(findData.cFileName);

				size_t periodIndex = fileName.find_last_of(L".");

				if (periodIndex != std::wstring::npos)
				{
					std::wstring_view extension = fileName.substr(periodIndex);

					if (StringViewUtil::EqualsIgnoreCase(extension, DatFileExtension))
					{
						datFiles.push_back(CreateUtf8FilePath(root, findData.cFileName));
					}
					else if (StringViewUtil::StartsWithIgnoreCase(extension, Sc4FileExtension))
					{
						// The file has a .SC4* extension, .SC4, .SC4Desc, .SC4Lot, etc.
						sc4Files.push_back(CreateUtf8FilePath(root, findData.cFileName));
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
}

void SC4DirectoryEnumerator::ScanDirectoryForDatFiles(const std::filesystem::path& root)
{
	WIN32_FIND_DATAW findData{};

	const std::filesystem::path searchPattern = AppendFileName(root, DatFilesSearchPattern);

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
			if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				datFiles.push_back(CreateUtf8FilePath(root, findData.cFileName));
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
}

void SC4DirectoryEnumerator::ScanDirectoryRecursive(const std::filesystem::path& root)
{
	std::vector<std::filesystem::path> subFolders;

	WIN32_FIND_DATAW findData{};

	const std::filesystem::path searchPattern = AppendFileName(root, AllFilesSearchPattern);

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
					subFolders.push_back(AppendFileName(root, findData.cFileName));
				}
			}
			else
			{
				std::wstring_view fileName(findData.cFileName);

				size_t periodIndex = fileName.find_last_of(L".");

				if (periodIndex != std::wstring::npos)
				{
					std::wstring_view extension = fileName.substr(periodIndex);

					if (StringViewUtil::EqualsIgnoreCase(extension, DatFileExtension))
					{
						datFiles.push_back(CreateUtf8FilePath(root, findData.cFileName));
					}
					else if (StringViewUtil::StartsWithIgnoreCase(extension, Sc4FileExtension))
					{
						// The file has a .SC4* extension, .SC4, .SC4Desc, .SC4Lot, etc.
						sc4Files.push_back(CreateUtf8FilePath(root, findData.cFileName));
					}
				}
			}
		}
		while (FindNextFileW(findHandle.get(), &findData));

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
		ScanDirectoryRecursive(path);
	}
}

void SC4DirectoryEnumerator::ScanDirectoryForDatFilesRecursive(const std::filesystem::path& root)
{
	std::vector<std::filesystem::path> subFolders;

	WIN32_FIND_DATAW findData{};

	const std::filesystem::path searchPattern = AppendFileName(root, AllFilesSearchPattern);

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
					subFolders.push_back(AppendFileName(root, findData.cFileName));
				}
			}
			else
			{
				if (boost::iends_with(std::wstring_view(findData.cFileName), DatFileExtension))
				{
					datFiles.push_back(CreateUtf8FilePath(root, findData.cFileName));
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
		ScanDirectoryRecursive(path);
	}
}

