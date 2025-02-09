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

#include "LooseSC4PluginMultiPackedFile.h"
#include "SC4DirectoryEnumerator.h"


LooseSC4MultiPackedFile::LooseSC4MultiPackedFile() : BaseMultiPackedFile(true)
{
}

std::vector<cRZBaseString> LooseSC4MultiPackedFile::GetDBPFFiles(const cIGZString& folderPath) const
{
	return SC4DirectoryEnumerator::GetLooseSC4FilesRecurseSubdirectories(folderPath);
}
