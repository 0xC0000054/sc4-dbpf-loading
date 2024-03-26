#pragma once
#include "cIGZPersistResourceKeyList.h"
#include "PersistResourceKeyHash.h"
#include <vector>

class PersistResourceKeyList : public cIGZPersistResourceKeyList
{
public:
	PersistResourceKeyList();

	~PersistResourceKeyList();

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

	uint32_t refCount;
	std::vector<cGZPersistResourceKey> keys;
};

