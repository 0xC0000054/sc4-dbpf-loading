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
#include "BaseMultiPackedFile.h"

// A cIGZPersistDBSegmentMultiPackedFiles implementation for .DAT files
// that are loaded from the specified root folder and any sub folders.
class DatMultiPackedFile final : public BaseMultiPackedFile
{
public:
	DatMultiPackedFile();

protected:
	std::vector<cRZBaseString> GetDBPFFiles(const cIGZString& path) const override;
};

