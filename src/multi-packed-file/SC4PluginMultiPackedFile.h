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

class cIGZCOM;
class cIGZPersistResourceManager;

// A cIGZPersistDBSegmentMultiPackedFiles implementation for .SC4* files (.SC4Desc, .SC4Lot, and .SC4Model)
// that are loaded from the specified root folder and any sub folders.
// This class replaces the game's liner search code with a per-TGI lookup.
class SC4MultiPackedFile final : public BaseMultiPackedFile
{
public:
	SC4MultiPackedFile();

protected:
	std::vector<cRZBaseString> GetDBPFFiles(const cIGZString& folderPath) const override;
};

