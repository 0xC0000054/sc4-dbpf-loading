/*
 * This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
 * optimizes the DBPF loading.
 *
 * Copyright (C) 2024, 2025, 2026 Nicholas Hayes
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, under
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "cIGZPersistResourceKeyList.h"
#include "cRZBaseUnknown.h"
#include "PersistResourceKeyHash.h"
#include <vector>

class PersistResourceKeyList final : public cRZBaseUnknown, public cIGZPersistResourceKeyList
{
public:
	// MSVC's std::deque allocates a separate block for each element larger than 8 bytes.
	// BaseMultiPackedFile reuses one list for every file, so a vector only has to grow
	// for the largest file, and EraseAll keeps its capacity.
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

