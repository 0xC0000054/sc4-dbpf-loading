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

#include "PathUtil.h"
#include <Windows.h>

static const std::wstring_view extendedPathPrefix = L"\\\\?\\";
static const std::wstring_view extendedUncPathPrefix = L"\\\\?\\UNC\\";

std::wstring PathUtil::AddExtendedPathPrefix(const std::wstring& path)
{
	if (path.starts_with(extendedPathPrefix))
	{
		return path;
	}
	else
	{
		if (path.starts_with(L"\\\\"))
		{
			// Network path

			std::wstring result(extendedUncPathPrefix);
			result.append(path.substr(2));

			return result;
		}
		else
		{
			std::wstring result(extendedPathPrefix);
			result.append(path);

			return result;
		}
	}
}

std::wstring PathUtil::Combine(const std::wstring& root, const std::wstring_view& segment)
{
	if (!root.empty() && !segment.empty())
	{
		std::wstring result(root);

		if (!IsDirectorySeparator(root[root.size() - 1]))
		{
			result.append(1, L'\\');
		}

		result.append(segment);

		return result;
	}

	return root;
}

std::string_view PathUtil::GetFileName(const std::string_view& path)
{
	std::string_view fileName;

	if (path.size() > 0)
	{
		const size_t length = path.size();

		// This loop will exclude file names that start with a period, but that
		// is the desired behavior.

		for (size_t i = length - 1; i != 0; i--)
		{
			const char c = path[i];

			if (IsDirectorySeparator(c) || c == ':')
			{
				fileName = path.substr(i + 1, length - i - 1);
				break;
			}
		}
	}

	return fileName;
}

std::wstring_view PathUtil::GetFileName(const std::wstring_view& path)
{
	std::wstring_view fileName;

	if (path.size() > 0)
	{
		const size_t length = path.size();

		// This loop will exclude file names that start with a period, but that
		// is the desired behavior.

		for (size_t i = length - 1; i != 0; i--)
		{
			const wchar_t c = path[i];

			if (IsDirectorySeparator(c) || c == L':')
			{
				fileName = path.substr(i + 1, length - i - 1);
				break;
			}
		}
	}

	return fileName;
}

std::wstring_view PathUtil::GetExtension(const std::wstring_view& path)
{
	std::wstring_view extension;

	if (path.size() > 0)
	{
		const size_t length = path.size();
		const size_t lastCharacterIndex = length - 1;

		// This loop will exclude file names that start with a period, but that
		// is the desired behavior.

		for (size_t i = lastCharacterIndex; i != 0; i--)
		{
			const wchar_t c = path[i];

			if (c == L'.')
			{
				// Treat a file name ending in a period as having no file extension.
				if (i != lastCharacterIndex)
				{
					extension = path.substr(i, length - i);
				}
				break;
			}
			else if (IsDirectorySeparator(c))
			{
				break;
			}
		}
	}

	return extension;
}

bool PathUtil::IsDirectorySeparator(char value)
{
	return value == '\\' || value == '/';
}

bool PathUtil::IsDirectorySeparator(wchar_t value)
{
	return value == L'\\' || value == L'/';
}

bool PathUtil::MustAddExtendedPathPrefix(const std::wstring& path)
{
	return path.size() >= MAX_PATH && !path.starts_with(extendedPathPrefix);
}

std::wstring PathUtil::Normalize(const std::wstring& path)
{
	// With the extended path format, we have to make the OS normalize the path.
	// It will not do so when opening the file.

	DWORD normalizedPathLengthWithNull = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);

	THROW_LAST_ERROR_IF(normalizedPathLengthWithNull == 0);

	std::wstring normalizedPath;
	normalizedPath.resize(normalizedPathLengthWithNull);

	DWORD normalizedPathLength2 = GetFullPathNameW(
		path.c_str(),
		normalizedPathLengthWithNull,
		normalizedPath.data(),
		nullptr);

	THROW_LAST_ERROR_IF(normalizedPathLength2 == 0);

	// Strip the null terminator.
	normalizedPath.resize(normalizedPathLength2);

	return normalizedPath;
}

std::wstring PathUtil::RemoveExtendedPathPrefix(const std::wstring& path)
{
	if (path.starts_with(extendedUncPathPrefix))
	{
		std::wstring result(L"\\\\");

		result.append(std::wstring_view(path).substr(extendedUncPathPrefix.size()));
	}
	else if (path.starts_with(extendedPathPrefix))
	{
		std::wstring result(std::wstring_view(path).substr(extendedPathPrefix.size()));

		return result;
	}

	return path;
}
