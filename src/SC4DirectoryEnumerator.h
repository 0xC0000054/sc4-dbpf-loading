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
#include <vector>

namespace SC4DirectoryEnumerator
{
	std::vector<cRZBaseString> GetDatFiles(const cIGZString& directory);
	std::vector<cRZBaseString> GetDatFilesRecurseSubdirectories(const cIGZString& root);

	void GetDBPFFilesRecurseSubdirectories(
		const cIGZString& root,
		std::vector<cRZBaseString>& datFiles,
		std::vector<cRZBaseString>& sc4Files);

	std::vector<cRZBaseString> GetLooseSC4FilesRecurseSubdirectories(const cIGZString& root);
};