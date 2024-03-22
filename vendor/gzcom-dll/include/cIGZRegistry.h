#pragma once
#include "cIGZUnknown.h"

class cIGZString;

class cIGZRegistry : public cIGZUnknown
{
public:

	virtual bool Register(cIGZString const& unknown1) = 0;
	virtual bool Unregister(cIGZString const& unknown1) = 0;

	virtual bool Lookup(const char* unknown1, cIGZString const& unknown2, cIGZString& unknown3) = 0;
	virtual bool MakeRegistryName(const char* unknown1, cIGZString const& unknown2, cIGZString const& unknown3, cIGZString& unknown4) = 0;

	typedef void (*EnumCategoryFunctionPtr)(cIGZString const&, cIGZString const&, void*);
	virtual bool EnumCategory(const char* category, EnumCategoryFunctionPtr pCallback, void* pContext) = 0;
};