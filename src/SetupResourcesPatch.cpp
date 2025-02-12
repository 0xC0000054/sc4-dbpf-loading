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

#include "SetupResourcesPatch.h"
#include "DatMultiPackedFile.h"
#include "LooseSC4PluginMultiPackedFile.h"
#include "Logger.h"
#include "Patcher.h"
#include "SC4DirectoryEnumerator.h"
#include "Stopwatch.h"
#include "cIGZCOM.h"
#include "cIGZFrameWork.h"
#include "cIGZDBSegmentPackedFile.h"
#include "cIGZPersistDBSegment.h"
#include "cIGZPersistResourceManager.h"
#include "cISC4App.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "cRZCOMDllDirector.h"
#include "GZServPtrs.h"

namespace
{
	template <class T>
	void AddMultiPackedFileToResourceManager(
		const cIGZString& folder,
		const std::vector<cRZBaseString>& files,
		cIGZPersistResourceManager* pRM)
	{
		static_assert(std::is_base_of<BaseMultiPackedFile, T>::value, "T must extend BaseMultiPackedFile");

		if (!files.empty())
		{
			cRZAutoRefCount<BaseMultiPackedFile> multiPackedFile(
				new T(),
				cRZAutoRefCount<BaseMultiPackedFile>::kAddRef);

			if (multiPackedFile->Init())
			{
				bool result = false;

				if (multiPackedFile->SetPath(folder))
				{
					if (multiPackedFile->Open(files))
					{
						result = pRM->RegisterDBSegment(*multiPackedFile->AsIGZPersistDBSegment());
					}
				}

				if (!result)
				{
					multiPackedFile->Shutdown();
				}
			}
		}
	}

	void AddMultiPackedFileToResourceManager(const cIGZString& path, cIGZPersistResourceManager* pRM)
	{
		cRZAutoRefCount<cIGZPersistDBSegment> multiPackedFile(
			new DatMultiPackedFile(),
			cRZAutoRefCount<cIGZPersistDBSegment>::kAddRef);

		if (multiPackedFile->Init())
		{
			bool result = false;

			if (multiPackedFile->SetPath(path))
			{
				if (multiPackedFile->Open(true, false))
				{
					result = pRM->RegisterDBSegment(*multiPackedFile);
				}
			}

			if (!result)
			{
				multiPackedFile->Shutdown();
			}
		}
	}

	void SetupGZPersistDBSegment(
		cIGZString const& path,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* pRM)
	{
		cRZAutoRefCount<cIGZPersistDBSegment> pSegment;

		if (pCOM->GetClassObject(
			GZCLSID_cGZDBSegmentPackedFile,
			GZIID_cIGZPersistDBSegment,
			pSegment.AsPPVoid()))
		{
			if (pSegment->Init())
			{
				if (pSegment->SetPath(path))
				{
					if (pSegment->Open(true, false))
					{
						pRM->RegisterDBSegment(*pSegment);
					}
				}
			}
		}
	}

	void LoadDATPluginsFromDirectory(
		const cIGZString& directory,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* pRM)
	{
		std::vector<cRZBaseString> datFiles = SC4DirectoryEnumerator::GetDatFiles(directory);

		for (const cRZBaseString& file : datFiles)
		{
			SetupGZPersistDBSegment(file, pCOM, pRM);
		}
	}

	void SetupPluginsDirectory(const cIGZString& path, cIGZPersistResourceManager* pRM)
	{
		std::vector<cRZBaseString> datFiles;
		std::vector<cRZBaseString> sc4Files;

		SC4DirectoryEnumerator::GetDBPFFilesRecurseSubdirectories(path, datFiles, sc4Files);

		// SC4Desc, SC4Lot, and SC4Model plug-ins are always loaded before DAT files.
		AddMultiPackedFileToResourceManager<LooseSC4MultiPackedFile>(path, sc4Files, pRM);
		AddMultiPackedFileToResourceManager<DatMultiPackedFile>(path, datFiles, pRM);
	}

	void SetupResourceFactories(cIGZPersistResourceManager* pRM)
	{
		pRM->RegisterObjectFactory(0xa63df8c, 0xca63e2a3, nullptr);
		pRM->RegisterObjectFactory(0x436eb4, 0x2026960b, nullptr);
		pRM->RegisterObjectFactory(0xc8696797, 0x686aa4b0, nullptr);
		pRM->RegisterObjectFactory(0xa2ffb5d3, 0x856ddbac, nullptr);
		pRM->RegisterObjectFactory(0x3ab50e2a, 0x7ab50e44, nullptr);
		pRM->RegisterObjectFactory(0x3ab50e2a, 0x7ab50e45, nullptr);
		pRM->RegisterObjectFactory(0x69b6f01c, 0x29a5d1ec, nullptr);
		pRM->RegisterObjectFactory(0x69b6f01c, 0x9adcd75, nullptr);
		pRM->RegisterObjectFactory(0xfad0f0b6, 0x5ad0e817, nullptr);
		pRM->RegisterObjectFactory(0x453429b3, 0x6534284a, nullptr);
		pRM->RegisterObjectFactory(0x53429c8, 0x5342861, nullptr);
		pRM->RegisterObjectFactory(0xa83479ea, 0xa83479d3, nullptr);
		pRM->RegisterObjectFactory(0x496678fe, 0x296678f7, nullptr);
		pRM->RegisterObjectFactory(0xea5118b5, 0xea5118b0, nullptr);
		pRM->RegisterObjectFactory(0x42e411c2, 0xa2e3d533, nullptr);
	}

	bool SetupResources(cISC4App* pSC4App)
	{
		bool result = false;

		try
		{
			cIGZCOM* const pCOM = RZGetFramework()->GetCOMObject();
			cIGZPersistResourceManagerPtr resMan;

			// SC4 searches directories for DBPF Files in the following order:
			//
			// 1. Installation root
			//    - This is where e.g.the SimCity_x.dat files are loaded.
			// 	  - Only loads.DAT files from the folder, and doesn't include sub-folders.
			// 	2. Installation language sub-folder
			// 	  - The sub-folder is based on the "Language" setting in the Registry.
			// 	  - Only loads .DAT files from the folder, and doesn't include sub-folders.
			// 	3. Installation Sku_data sub-folder
			// 	  - All sub-folders are recursively loaded.
			// 	4. Installation plugins sub-folder
			// 	  - All sub-folders are recursively loaded.
			// 	5. User plugins sub-folder
			// 	  - All sub-folders are recursively loaded.

			cRZBaseString installationRoot;
			pSC4App->GetDataDirectory(installationRoot, -1);

			LoadDATPluginsFromDirectory(installationRoot, pCOM, resMan);

			cRZBaseString installationLanguageFolder;
			pSC4App->GetDataDirectory(installationLanguageFolder, 0);

			LoadDATPluginsFromDirectory(installationLanguageFolder, pCOM, resMan);

			cRZBaseString installationSkuData;
			pSC4App->GetSkuSpecificDirectory(installationSkuData);

			AddMultiPackedFileToResourceManager(installationSkuData, resMan);

			cRZBaseString installationPlugins;
			pSC4App->GetPluginDirectory(installationPlugins);

			SetupPluginsDirectory(installationPlugins, resMan);

			cRZBaseString userPlugins;
			pSC4App->GetUserPluginDirectory(userPlugins);

			SetupPluginsDirectory(userPlugins, resMan);

			// After loading DBPF files, the last setup item is registering
			// the resource manager factory classes.
			SetupResourceFactories(resMan);

			result = resMan->GetSegmentCount() > 0;
		}
		catch (const std::exception& e)
		{
			Logger::GetInstance().WriteLineFormatted(
				LogLevel::Error,
				"SetupResources exception: %s",
				e.what());
			result = false;
		}

		return result;
	}

	bool TimedSetupResources(cISC4App* pSC4App)
	{
		Stopwatch sw;
		sw.Start();

		bool result = SetupResources(pSC4App);

		sw.Stop();

		char buffer[256]{};

		std::snprintf(buffer, sizeof(buffer), "Loaded resources in %lld ms", sw.ElapsedMilliseconds());

		MessageBoxA(nullptr, buffer, "SC4DBPFLoading", 0);

		return result;
	}

	bool WindowsAPILogSetupResources(cISC4App* pSC4App)
	{
		MessageBoxA(nullptr, "Start your Process Monitor trace and press OK.", "SC4DBPFLoading", 0);

		bool result = SetupResources(pSC4App);

		MessageBoxA(nullptr, "Stop your Process Monitor trace and press OK.", "SC4DBPFLoading", 0);

		return result;
	}

	static SetupResourcesPatch::ResourceLoadingTraceOption resourceLoadingTraceOption = SetupResourcesPatch::ResourceLoadingTraceOption::None;

	bool __fastcall HookedSetupResources(void* pThis, void* edxUnused)
	{
		bool result = false;

		cISC4App* pSC4App = reinterpret_cast<cISC4App*>(static_cast<uint8_t*>(pThis) + 0xc);

		switch (resourceLoadingTraceOption)
		{
		case SetupResourcesPatch::ResourceLoadingTraceOption::ShowLoadTime:
			result = TimedSetupResources(pSC4App);
			break;
		case SetupResourcesPatch::ResourceLoadingTraceOption::WindowsAPILogWait:
			result = WindowsAPILogSetupResources(pSC4App);
			break;
		case SetupResourcesPatch::ResourceLoadingTraceOption::None:
		case SetupResourcesPatch::ResourceLoadingTraceOption::ListLoadedFiles:
		default:
			result = SetupResources(pSC4App);
			break;
		}

		return result;
	}
}

void SetupResourcesPatch::Install(ResourceLoadingTraceOption traceOption)
{
	Logger& logger = Logger::GetInstance();

	try
	{
		Patcher::InstallCallHook(0x44C97E, &HookedSetupResources);
		resourceLoadingTraceOption = traceOption;
		logger.WriteLine(LogLevel::Info, "Installed the SetupResources hook.");
	}
	catch (const std::exception& e)
	{
		logger.WriteLineFormatted(
			LogLevel::Error,
			"Failed to install the SetupResources hook: %s",
			e.what());
	}
}
