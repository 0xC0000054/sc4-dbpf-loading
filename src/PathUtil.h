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

#pragma once
#include <string>

namespace PathUtil
{
	std::wstring AddExtendedPathPrefix(const std::wstring& path);
	std::wstring Combine(const std::wstring& root, const std::wstring_view& segment);
	bool MustAddExtendedPathPrefix(const std::wstring& path);
	std::wstring Normalize(const std::wstring& path);
	std::wstring RemoveExtendedPathPrefix(const std::wstring& path);
}
