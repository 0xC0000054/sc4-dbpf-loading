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
#include "cIGZPersistDBSegment.h"
#include "cIGZPersistDBSegmentMultiPackedFiles.h"
#include "cRZBaseString.h"
#include "cRZUnknown.h"
#include "PersistResourceKeyBoostHash.h"
#include "PersistResourceKeyHash.h"
#include "boost/unordered/unordered_flat_map.hpp"
#include <vector>
#include <Windows.h>

class cIGZCOM;
class PersistResourceKeyList;

class BaseMultiPackedFile : public cRZUnknown, public cIGZPersistDBSegment, public cIGZPersistDBSegmentMultiPackedFiles
{
protected:
	/**
	 * @brief The constructor derived classed must call.
	 * @param enumerateSegmentsLastInFirstOut true if the segments should be enumerated in
	 * last in first out order; otherwise, false to use first in first out order.
	 * cGZPersistResourceManager uses last in first out for its file list.
	 * cGZPersistDBSegmentMultiPackedFiles uses first in first out.
	 */
	BaseMultiPackedFile(bool enumerateSegmentsLastInFirstOut);

public:
	~BaseMultiPackedFile();

	bool QueryInterface(uint32_t riid, void** ppvObj) override;
	uint32_t AddRef() override;
	uint32_t Release() override;

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

	uint32_t GetResourceKeyList(cIGZPersistResourceKeyList* list, cIGZPersistResourceKeyFilter* filter) override;
	bool GetResourceKeyList(cIGZPersistResourceKeyList& list) override;

	bool TestForRecord(cGZPersistResourceKey const& key) override;
	uint32_t GetRecordSize(cGZPersistResourceKey const& key) override;
	bool OpenRecord(cGZPersistResourceKey const& key, cIGZPersistDBRecord** record, cIGZFile::AccessMode accessMode) override;
	bool CreateNewRecord(cGZPersistResourceKey const& key, cIGZPersistDBRecord** record) override;

	bool CloseRecord(cIGZPersistDBRecord* record) override;
	bool CloseRecord(cIGZPersistDBRecord** record) override;

	bool AbortRecord(cIGZPersistDBRecord* record) override;
	bool AbortRecord(cIGZPersistDBRecord** record) override;

	bool DeleteRecord(cGZPersistResourceKey const& key) override;
	uint32_t ReadRecord(cGZPersistResourceKey const& key, void* buffer, uint32_t& recordSize) override;
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

protected:
	virtual std::vector<cRZBaseString> GetDBPFFiles(const cIGZString& folderPath) const = 0;

private:
	bool SetupGZPersistDBSegment(
		cIGZString const& path,
		cIGZCOM* const pCOM,
		PersistResourceKeyList* const pKeyList);

	uint32_t segmentID;
	cRZBaseString folderPath;
	bool enumerateSegmentsLastInFirstOut;
	bool initialized;
	bool isOpen;
	CRITICAL_SECTION criticalSection;
	boost::unordered::unordered_flat_map<const cGZPersistResourceKey, cIGZPersistDBSegment*> tgiMap;
	std::vector<cIGZPersistDBSegment*> segments;
};
