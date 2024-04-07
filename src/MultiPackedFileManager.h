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

#pragma once
#include "MultiPackedFile.h"
#include <memory>
#include <vector>

class MultiPackedFileManager
{
public:

	MultiPackedFileManager();

	cIGZUnknown* CreateMultiPackedFile();

private:

	std::vector<std::unique_ptr<MultiPackedFile>> files;
};

