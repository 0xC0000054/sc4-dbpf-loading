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

#include "PathUtil.h"
#include <stdexcept>
#include <Windows.h>
#include "boost/algorithm/string.hpp"

namespace
{
	const std::wstring_view extendedPathPrefix = L"\\\\?\\";
	const std::wstring_view extendedUncPathPrefix = L"\\\\?\\UNC\\";

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

		const wchar_t lastChar = root[root.size() - 1];

		if (lastChar != L'\\' && lastChar != L'/')
		{
			result.append(1, L'\\');
		}

		result.append(segment);

		return result;
	}

	return root;
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

	if (normalizedPathLengthWithNull == 0)
	{
		ThrowExceptionForWin32Error("GetFullPathNameW", GetLastError());
	}

	std::wstring normalizedPath;
	normalizedPath.resize(normalizedPathLengthWithNull);

	DWORD normalizedPathLength2 = GetFullPathNameW(
		path.c_str(),
		normalizedPathLengthWithNull,
		normalizedPath.data(),
		nullptr);

	if (normalizedPathLength2 == 0)
	{
		ThrowExceptionForWin32Error("GetFullPathNameW", GetLastError());
	}

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
