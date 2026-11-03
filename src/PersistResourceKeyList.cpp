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

#include "PersistResourceKeyList.h"

PersistResourceKeyList::PersistResourceKeyList()
{
}

PersistResourceKeyList::~PersistResourceKeyList()
{
}

const PersistResourceKeyList::container& PersistResourceKeyList::GetKeys() const
{
	return keys;
}

bool PersistResourceKeyList::QueryInterface(uint32_t riid, void** ppvObj)
{
	if (riid == GZIID_cIGZPersistResourceKeyList)
	{
		*ppvObj = static_cast<cIGZPersistResourceKeyList*>(this);
		AddRef();

		return true;
	}

	return cRZUnknown::QueryInterface(riid, ppvObj);
}

uint32_t PersistResourceKeyList::AddRef()
{
	return cRZUnknown::AddRef();
}

uint32_t PersistResourceKeyList::Release()
{
	return cRZUnknown::Release();
}

bool PersistResourceKeyList::Insert(cGZPersistResourceKey const& key)
{
	keys.push_back(key);
	return true;
}

bool PersistResourceKeyList::Insert(cIGZPersistResourceKeyList const& list)
{
	list.EnumKeys(&InsertKeyCallback, this);
	return true;
}

bool PersistResourceKeyList::Erase(cGZPersistResourceKey const& key)
{
	bool result = false;

	for (auto entry = keys.begin(); entry != keys.end(); entry++)
	{
		if (entry->instance == key.instance
			&& entry->group == key.group
			&& entry->type == key.type)
		{
			keys.erase(entry);
			result = true;
			break;
		}
	}

	return result;
}

bool PersistResourceKeyList::EraseAll()
{
	keys.clear();
	return true;
}

void PersistResourceKeyList::EnumKeys(EnumKeysFunctionPtr pCallback, void* pContext) const
{
	if (pCallback)
	{
		for (const auto& key : keys)
		{
			pCallback(key, pContext);
		}
	}
}

bool PersistResourceKeyList::IsPresent(cGZPersistResourceKey const& key) const
{
	bool result = false;

	for (const auto& entry : keys)
	{
		if (entry.instance == key.instance
			&& entry.group == key.group
			&& entry.type == key.type)
		{
			result = true;
			break;
		}
	}

	return result;
}

uint32_t PersistResourceKeyList::Size() const
{
	return static_cast<uint32_t>(keys.size());
}

const cGZPersistResourceKey& PersistResourceKeyList::GetKey(uint32_t index) const
{
	return keys[index];
}

void PersistResourceKeyList::InsertKeyCallback(cGZPersistResourceKey const& key, void* pContext)
{
	PersistResourceKeyList* pThis = static_cast<PersistResourceKeyList*>(pContext);

	pThis->keys.push_back(key);
}
