#include "PersistResourceKeyList.h"

PersistResourceKeyList::PersistResourceKeyList()
	: refCount(0)
{
}

PersistResourceKeyList::~PersistResourceKeyList()
{
}

bool PersistResourceKeyList::QueryInterface(uint32_t riid, void** ppvObj)
{
	if (riid == GZIID_cIGZPersistResourceKeyList)
	{
		*ppvObj = static_cast<cIGZPersistResourceKeyList*>(this);
		AddRef();

		return true;
	}
	else if (riid == GZIID_cIGZUnknown)
	{
		*ppvObj = static_cast<cIGZUnknown*>(this);
		AddRef();

		return true;
	}

	*ppvObj = nullptr;
	return false;
}

uint32_t PersistResourceKeyList::AddRef()
{
	return ++refCount;
}

uint32_t PersistResourceKeyList::Release()
{
	if (refCount == 1)
	{
		delete this;
		return 0;
	}
	else
	{
		return --refCount;
	}
}

bool PersistResourceKeyList::Insert(cGZPersistResourceKey const& key)
{
	keys.push_back(key);
	return true;
}

bool PersistResourceKeyList::Insert(cIGZPersistResourceKeyList const& list)
{
	EnumKeys(&InsertKeyCallback, this);
	return true;
}

bool PersistResourceKeyList::Erase(cGZPersistResourceKey const& key)
{
	bool result = false;

	for (const auto& entry = keys.begin(); entry != keys.end();)
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
