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
#include "SC4PluginMultiPackedFile.h"
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

#include <Windows.h>
#include "wil/resource.h"

namespace
{
	template <class T>
	bool OpenMultiPackedFile(const cIGZString& folder, cRZAutoRefCount<T>& multiPackedFile)
	{
		static_assert(std::is_base_of<BaseMultiPackedFile, T>::value, "T must extend BaseMultiPackedFile");

		multiPackedFile = new T();

		if (multiPackedFile->Init())
		{
			if (multiPackedFile->SetPath(folder))
			{
				if (multiPackedFile->Open(true, false))
				{
					return true;
				}
			}

			multiPackedFile->Shutdown();
		}

		multiPackedFile.Reset();
		return false;
	}

	template <class T>
	class BackgroundThreadPluginScanner
	{
	public:
		static_assert(std::is_base_of<BaseMultiPackedFile, T>::value, "T must extend BaseMultiPackedFile");

		BackgroundThreadPluginScanner(const cIGZString& folder)
			: folder(folder), multiPackedFile(), event(wil::EventOptions::ManualReset)
		{
			THROW_IF_WIN32_BOOL_FALSE(QueueUserWorkItem(&ThreadProcStatic, this, 0));
		}

		T* GetMultiPackedFile()
		{
			return multiPackedFile;
		}

		void Wait()
		{
			event.wait();
		}

	private:
		static DWORD WINAPI ThreadProcStatic(LPVOID lpParameter)
		{
			static_cast<BackgroundThreadPluginScanner*>(lpParameter)->ThreadProc();
			return 0;
		}

		void ThreadProc()
		{
			OpenMultiPackedFile(folder, multiPackedFile);
			event.SetEvent();
		}

		cRZBaseString folder;
		cRZAutoRefCount<T> multiPackedFile;
		wil::unique_event event;
	};

	bool AddMultiPackedFileToResourceManager(
		BaseMultiPackedFile& baseMultiPackedFile,
		cIGZPersistResourceManager* pRM)
	{
		bool result = false;

		if (baseMultiPackedFile.IsOpen())
		{
			result = pRM->RegisterDBSegment(*baseMultiPackedFile.AsIGZPersistDBSegment());
		}

		return result;
	}

	template <class T>
	void AddMultiPackedFileToResourceManager(
		const cIGZString& folder,
		cIGZPersistResourceManager* pRM)
	{
		static_assert(std::is_base_of<BaseMultiPackedFile, T>::value, "T must extend BaseMultiPackedFile");

		cRZAutoRefCount<T> multiPackedFile;

		if (OpenMultiPackedFile<T>(folder, multiPackedFile))
		{
			if (!AddMultiPackedFileToResourceManager(*multiPackedFile, pRM))
			{
				multiPackedFile->Close();
				multiPackedFile->Shutdown();
			}
		}
	}

	template <class T>
	void AddMultiPackedFileToResourceManager(
		BackgroundThreadPluginScanner<T>& backgroundPluginScanner,
		cIGZPersistResourceManager* pRM)
	{
		static_assert(std::is_base_of<BaseMultiPackedFile, T>::value, "T must extend BaseMultiPackedFile");

		// Block until the background thread finishes loading the file.
		backgroundPluginScanner.Wait();

		T* multiPackedFile = backgroundPluginScanner.GetMultiPackedFile();

		if (multiPackedFile)
		{
			if (!AddMultiPackedFileToResourceManager(*multiPackedFile, pRM))
			{
				multiPackedFile->Close();
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
			// The user plugins folder (typically Documents/SimCity 4/Plugins) is scanned on
			// background threads while the main thread loads the other plugins.

			cRZBaseString userPlugins;
			pSC4App->GetUserPluginDirectory(userPlugins);

			BackgroundThreadPluginScanner<SC4PluginMultiPackedFile> userPluginsSC4Files(userPlugins);
			BackgroundThreadPluginScanner<DatMultiPackedFile> userPluginsDATFiles(userPlugins);

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

			AddMultiPackedFileToResourceManager<DatMultiPackedFile>(installationSkuData, resMan);

			cRZBaseString installationPlugins;
			pSC4App->GetPluginDirectory(installationPlugins);

			// SC4Desc, SC4Lot, and SC4Model plug-ins are always loaded before DAT files.
			AddMultiPackedFileToResourceManager<SC4PluginMultiPackedFile>(installationPlugins, resMan);
			AddMultiPackedFileToResourceManager<DatMultiPackedFile>(installationPlugins, resMan);

			// Register the user plugins that were loaded in the background.
			// SC4Desc, SC4Lot, and SC4Model plug-ins are always loaded before DAT files.

			AddMultiPackedFileToResourceManager<SC4PluginMultiPackedFile>(userPluginsSC4Files, resMan);
			AddMultiPackedFileToResourceManager<DatMultiPackedFile>(userPluginsDATFiles, resMan);

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
