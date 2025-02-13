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
#include "cRZBaseString.h"
#include <filesystem>

// Provides utility functions for cIGZString character set conversion.
// The native cIGZString character set is UTF-8.
namespace GZStringConvert
{
	cRZBaseString FromUtf16(const std::wstring& str);

	cRZBaseString FromFileSystemPath(const std::filesystem::path& path);

	std::wstring ToUtf16(const cIGZString& str);

	std::filesystem::path ToFileSystemPath(const cIGZString& str);
}