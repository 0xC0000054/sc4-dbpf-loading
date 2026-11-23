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

#include "SetupResourcesPatch.h"
#include "DatMultiPackedFile.h"
#include "SC4PluginMultiPackedFile.h"
#include "Logger.h"
#include "Patcher.h"
#include "SC4DirectoryEnumerator.h"
#include "Stopwatch.h"
#include "StringConversion.h"
#include "cIGZApp.h"
#include "cIGZCOM.h"
#include "cIGZFrameWork.h"
#include "cIGZDBSegmentPackedFile.h"
#include "cIGZPersistDBSegment.h"
#include "cIGZPersistResourceManager.h"
#include "cIGZRegistry.h"
#include "cISC4App.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "cRZCOMDllDirector.h"
#include "GZServPtrs.h"
#include <ShlObj_core.h>

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
			try
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
			catch (const std::exception& e)
			{
				Logger::GetInstance().WriteLineFormatted(
					LogLevel::Error,
					"Error loading plugins from %s: %s",
					folder.ToChar(),
					e.what());
			}
		}
	}

	void AddMultiPackedFileToResourceManager(const cIGZString& path, cIGZPersistResourceManager* pRM)
	{
		try
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
		catch (const std::exception& e)
		{
			Logger::GetInstance().WriteLineFormatted(
				LogLevel::Error,
				"Error loading plugins from %s: %s",
				path.ToChar(),
				e.what());
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
		try
		{
			std::vector<cRZBaseString> datFiles = SC4DirectoryEnumerator::GetDatFiles(directory);

			for (const cRZBaseString& file : datFiles)
			{
				SetupGZPersistDBSegment(file, pCOM, pRM);
			}
		}
		catch (const std::exception& e)
		{
			Logger::GetInstance().WriteLineFormatted(
				LogLevel::Error,
				"Error loading plugins from %s: %s",
				directory.ToChar(),
				e.what());
		}
	}

	void SetupPluginsDirectory(const cIGZString& path, cIGZPersistResourceManager* pRM)
	{
		try
		{
			const std::wstring utf16Path = StringConversion::ToUtf16(path);

			// Create the directory if it doesn't exist.
			if (GetFileAttributesW(utf16Path.c_str()) == INVALID_FILE_ATTRIBUTES)
			{
				// SHCreateDirectoryExW handles multi-level directory creation,
				// which is necessary for user plugins in the -UserDir case.
				SHCreateDirectoryExW(nullptr, utf16Path.c_str(), nullptr);
			}

			std::vector<cRZBaseString> datFiles;
			std::vector<cRZBaseString> sc4Files;

			SC4DirectoryEnumerator::GetDBPFFilesRecurseSubdirectories(utf16Path, datFiles, sc4Files);

			// SC4Desc, SC4Lot, and SC4Model plug-ins are always loaded before DAT files.
			AddMultiPackedFileToResourceManager<SC4PluginMultiPackedFile>(path, sc4Files, pRM);
			AddMultiPackedFileToResourceManager<DatMultiPackedFile>(path, datFiles, pRM);
		}
		catch (const std::exception& e)
		{
			Logger::GetInstance().WriteLineFormatted(
				LogLevel::Error,
				"Error loading plugins from %s: %s",
				path.ToChar(),
				e.what());
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

	bool SetupResources(void* pThis)
	{
		bool result = false;

		try
		{
			cISC4App* pISC4App = reinterpret_cast<cISC4App*>(static_cast<uint8_t*>(pThis) + 0xc);

			if (pISC4App->GetDebugFunctionalityEnabled())
			{
				// Replicate the steps the game uses to set up its internal development environment.

				uint8_t* pGZApp = static_cast<uint8_t*>(pThis) + 0x10;
				cIGZApp* pIGZApp = reinterpret_cast<cIGZApp*>(pGZApp + 0x1c);

				// cSC4App::LoadRegistry loads and parses <SC4 Root>\Sys\Resources.ini.

				pIGZApp->LoadRegistry();

				cIGZRegistryPtr pRegistry;

				// cSC4App::CreateDBSegmentCallback
				cIGZRegistry::EnumCallback pCallback = reinterpret_cast<cIGZRegistry::EnumCallback>(0x456ba0);

				// Load the DBSegment entries from Resources.ini.

				char* root = const_cast<char*>("CN_PDBS");
				pRegistry->EnumCategory(root, pCallback, pThis);
			}

			cIGZCOM* const pCOM = RZGetCOMDllDirector()->GZCOM();
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
			pISC4App->GetDataDirectory(installationRoot, -1);

			LoadDATPluginsFromDirectory(installationRoot, pCOM, resMan);

			cRZBaseString installationLanguageFolder;
			pISC4App->GetDataDirectory(installationLanguageFolder, 0);

			LoadDATPluginsFromDirectory(installationLanguageFolder, pCOM, resMan);

			cRZBaseString installationSkuData;
			pISC4App->GetSkuSpecificDirectory(installationSkuData);

			AddMultiPackedFileToResourceManager(installationSkuData, resMan);

			cRZBaseString installationPlugins;
			pISC4App->GetPluginDirectory(installationPlugins);

			SetupPluginsDirectory(installationPlugins, resMan);

			cRZBaseString userPlugins;
			pISC4App->GetUserPluginDirectory(userPlugins);

			SetupPluginsDirectory(userPlugins, resMan);

			// After loading DBPF files, the last setup item is registering
			// the resource manager factory classes.
			SetupResourceFactories(resMan);

			result = resMan->GetSegmentCount() >= 3;
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

	bool TimedSetupResources(void* pThis)
	{
		Stopwatch sw;
		sw.Start();

		bool result = SetupResources(pThis);

		sw.Stop();

		char buffer[256]{};

		std::snprintf(buffer, sizeof(buffer), "Loaded resources in %lld ms", sw.GetElapsed<Stopwatch::TimeFormat::Milliseconds>());

		MessageBoxA(nullptr, buffer, "SC4DBPFLoading", 0);

		return result;
	}

	bool WindowsAPILogSetupResources(void* pThis)
	{
		MessageBoxA(nullptr, "Start your Process Monitor trace and press OK.", "SC4DBPFLoading", 0);

		bool result = SetupResources(pThis);

		MessageBoxA(nullptr, "Stop your Process Monitor trace and press OK.", "SC4DBPFLoading", 0);

		return result;
	}

	bool __fastcall HookedSetupResources(void* pThis, void* edxUnused)
	{
		bool result = false;

		switch (SetupResourcesPatch::resourceLoadingTraceOption)
		{
		case SetupResourcesPatch::ResourceLoadingTraceOption::ShowLoadTime:
			result = TimedSetupResources(pThis);
			break;
		case SetupResourcesPatch::ResourceLoadingTraceOption::WindowsAPILogWait:
			result = WindowsAPILogSetupResources(pThis);
			break;
		case SetupResourcesPatch::ResourceLoadingTraceOption::None:
		case SetupResourcesPatch::ResourceLoadingTraceOption::ListLoadedFiles:
		default:
			result = SetupResources(pThis);
			break;
		}

		return result;
	}
}

SetupResourcesPatch::ResourceLoadingTraceOption SetupResourcesPatch::resourceLoadingTraceOption = SetupResourcesPatch::ResourceLoadingTraceOption::None;

void SetupResourcesPatch::Install()
{
	Logger& logger = Logger::GetInstance();

	try
	{
		Patcher::InstallCallHook(0x44C97E, &HookedSetupResources);
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
