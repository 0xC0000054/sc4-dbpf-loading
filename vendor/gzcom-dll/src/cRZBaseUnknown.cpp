#include "cRZBaseUnknown.h"

cRZBaseUnkown::cRZBaseUnkown()
	: refCount(0)
{
}

cRZBaseUnkown::~cRZBaseUnkown()
{
}

bool cRZBaseUnkown::QueryInterface(uint32_t riid, void** ppvObj)
{
	if (riid == GZIID_cIGZUnknown)
	{
		*ppvObj = static_cast<cIGZUnknown*>(this);
		AddRef();

		return true;
	}

	*ppvObj = nullptr;
	return false;
}

uint32_t cRZBaseUnkown::AddRef()
{
	uint32_t localRefCount = refCount + 1;
	refCount = localRefCount;

	return localRefCount;
}

uint32_t cRZBaseUnkown::Release()
{
	uint32_t localRefCount = 0;

	if (refCount > 0)
	{
		localRefCount = refCount - 1;
		refCount = localRefCount;

		if (localRefCount == 0)
		{
			delete this;
		}
	}

	return localRefCount;
}

uint32_t cRZBaseUnkown::RefCount() const
{
	return refCount;
}
