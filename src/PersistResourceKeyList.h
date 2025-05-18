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
#include "cIGZPersistResourceKeyList.h"
#include "cRZBaseUnknown.h"
#include "PersistResourceKeyHash.h"
#include <vector>

class PersistResourceKeyList final : public cRZBaseUnkown, public cIGZPersistResourceKeyList
{
public:
	using container = std::vector<cGZPersistResourceKey>;

	PersistResourceKeyList();

	~PersistResourceKeyList();

	const container& GetKeys() const;

	// cIGZPersistResourceKeyList

	bool QueryInterface(uint32_t riid, void** ppvObj) override;

	uint32_t AddRef() override;

	uint32_t Release() override;

	bool Insert(cGZPersistResourceKey const& key) override;
	bool Insert(cIGZPersistResourceKeyList const& list) override;

	bool Erase(cGZPersistResourceKey const& key) override;
	bool EraseAll() override;

	void EnumKeys(EnumKeysFunctionPtr pCallback, void* pContext) const override;

	bool IsPresent(cGZPersistResourceKey const& key) const override;
	uint32_t Size() const override;
	const cGZPersistResourceKey& GetKey(uint32_t index) const override;

private:
	static void InsertKeyCallback(cGZPersistResourceKey const& key, void* pContext);

	container keys;
};

