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

#include "DatMultiPackedFile.h"
#include "SC4DirectoryEnumerator.h"

DatMultiPackedFile::DatMultiPackedFile() : BaseMultiPackedFile(false)
{
}

std::vector<cRZBaseString> DatMultiPackedFile::GetDBPFFiles(const cIGZString& folderPath) const
{
	std::vector<cRZBaseString> files;

	SC4DirectoryEnumerator::ScanDirectoryForDatFilesRecursive(folderPath, files);

	return files;
}

