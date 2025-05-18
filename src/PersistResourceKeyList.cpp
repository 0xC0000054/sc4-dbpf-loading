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

	return cRZBaseUnkown::QueryInterface(riid, ppvObj);
}

uint32_t PersistResourceKeyList::AddRef()
{
	return cRZBaseUnkown::AddRef();
}

uint32_t PersistResourceKeyList::Release()
{
	return cRZBaseUnkown::Release();
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
