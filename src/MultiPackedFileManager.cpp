///////////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
// optimizes the DBPF loading.
//
// Copyright (c) 2024 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
///////////////////////////////////////////////////////////////////////////////

#include "MultiPackedFileManager.h"

MultiPackedFileManager::MultiPackedFileManager()
{
}

cIGZUnknown* MultiPackedFileManager::CreateMultiPackedFile()
{
	auto& ptr = files.emplace_back(std::make_unique<MultiPackedFile>());

	return static_cast<cIGZPersistDBSegment*>(ptr.get());
}
