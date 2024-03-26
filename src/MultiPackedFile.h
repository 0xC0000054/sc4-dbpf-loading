#pragma once
#include "cIGZPersistDBSegment.h"
#include "cIGZPersistDBSegmentMultiPackedFiles.h"
#include "cRZBaseString.h"
#include "PersistResourceKeyHash.h"
#include <unordered_map>
#include <vector>
#include <Windows.h>

class cIGZCOM;
class cIGZPersistResourceManager;

class MultiPackedFile final : public cIGZPersistDBSegment, public cIGZPersistDBSegmentMultiPackedFiles
{
public:

	MultiPackedFile();

	~MultiPackedFile();

	bool QueryInterface(uint32_t riid, void** ppvObj) override;

	uint32_t AddRef() override;

	uint32_t Release() override;

	// This overload is used to avoid having to rescan the
	// directory for DAT files.
	bool Open(const std::vector<cRZBaseString>& datFiles);

	// cIGZPersistDBSegment

	bool Init() override;
	bool Shutdown() override;

	bool Open(bool openRead, bool openWrite) override;
	bool IsOpen() const override;
	bool Close() override;
	bool Flush() override;

	void GetPath(cIGZString& path) const override;
	bool SetPath(cIGZString const& path) override;

	bool Lock() override;
	bool Unlock() override;

	uint32_t GetSegmentID() const override;
	bool SetSegmentID(uint32_t const& segmentID) override;

	uint32_t GetRecordCount(cIGZPersistResourceKeyFilter* filter) override;

	bool GetResourceKeyList(cIGZPersistResourceKeyList* list, cIGZPersistResourceKeyFilter* filter) override;
	bool GetResourceKeyList(cIGZPersistResourceKeyList& list) override;

	bool TestForRecord(cGZPersistResourceKey const& key) override;
	uint32_t GetRecordSize(cGZPersistResourceKey const& key) override;
	bool OpenRecord(cGZPersistResourceKey const& key, cIGZPersistDBRecord** unknown2, uint32_t unknown3) override;
	bool CreateNewRecord(cGZPersistResourceKey const& key, cIGZPersistDBRecord** unknown2) override;

	bool CloseRecord(cIGZPersistDBRecord* unknown1) override;
	bool CloseRecord(cIGZPersistDBRecord** unknown1) override;

	bool AbortRecord(cIGZPersistDBRecord* unknown1) override;
	bool AbortRecord(cIGZPersistDBRecord** unknown1) override;

	bool DeleteRecord(cGZPersistResourceKey const& key) override;
	bool ReadRecord(cGZPersistResourceKey const& key, void* buffer, uint32_t& recordSize) override;
	bool WriteRecord(cGZPersistResourceKey const& key, void* buffer, uint32_t recordSize) override;

	bool Init(uint32_t segmentID, cIGZString const& path, bool unknown2) override;

	// cIGZPersistDBSegmentMultiPackedFiles

	void SetPathFilter(cIGZString const&) override;

	int32_t ConsolidateDatabaseRecords(cIGZPersistDBSegment*, cIGZPersistResourceKeyFilter*) override;
	int32_t ConsolidateDatabaseRecords(cIGZString const&, cIGZPersistResourceKeyFilter*) override;

	bool FindDBSegment(cGZPersistResourceKey const&, cIGZPersistDBSegment**) override;
	uint32_t GetSegmentCount() override;
	cIGZPersistDBSegment* GetSegmentByIndex(uint32_t) override;

	void AddedResource(cGZPersistResourceKey const&, cIGZPersistDBSegment*) override;
	void RemovedResource(cGZPersistResourceKey const&, cIGZPersistDBSegment*) override;

private:

	bool SetupGZPersistDBSegment(
		cIGZString const& path,
		cIGZCOM* const pCOM);

	struct EnumResKeyContext
	{
		MultiPackedFile* pThis;
		cIGZPersistDBSegment* parentDBSegment;

		EnumResKeyContext(MultiPackedFile* file, cIGZPersistDBSegment* segment);
	};

	static void EnumResKeyListCallback(cGZPersistResourceKey const& key, void* pContext);

	uint32_t refCount;
	uint32_t segmentID;
	cRZBaseString folderPath;
	bool initialized;
	bool isOpen;
	CRITICAL_SECTION criticalSection;
	std::unordered_map<const cGZPersistResourceKey, cIGZPersistDBSegment*> tgiMap;
	std::vector<cIGZPersistDBSegment*> segments;
};

