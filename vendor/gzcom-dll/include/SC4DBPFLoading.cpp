#include "SC4DBPFLoading.h"
#include "GZStringConvert.h"
#include "Logger.h"
#include "MultiPackedFile.h"
#include "SC4DirectoryEnumerator.h"

#include "cIGZApp.h"
#include "cIGZCOM.h"
#include "cIGZFrameWork.h"
#include "cIGZDBSegmentPackedFile.h"
#include "cIGZPersistDBSegment.h"
#include "cIGZPersistResourceManager.h"
#include "cIGZString.h"
#include "cISC4App.h"
#include "cRZAutoRefCount.h"
#include "cRZCOMDllDirector.h"
#include "GZServPtrs.h"
#include <functional>
#include <stdexcept>
#include <Windows.h>
#include "wil/filesystem.h"

namespace
{
	bool DirectoryExists(const std::filesystem::path& path)
	{
		bool result = false;

		WIN32_FILE_ATTRIBUTE_DATA data{};

		if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
		{
			result = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		}

		return result;
	}

	void SetupPackedFileSegment(
		const cIGZString& path,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* const pResMan)
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
						pResMan->RegisterDBSegment(*pSegment);
					}
				}

				pSegment->Shutdown();
			}
		}
	}

	void SetupPackedFileSegments(
		const std::vector<cRZBaseString>& segmentFilePaths,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* const pResMan)
	{
		for (const cRZBaseString& path : segmentFilePaths)
		{
			SetupPackedFileSegment(path, pCOM, pResMan);
		}
	}

	void SetupMultiPackedFileSegment(
		const cIGZString& folderPath,
		const std::vector<cRZBaseString>& datFilePaths,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* const pResMan)
	{
		cRZAutoRefCount<MultiPackedFile> multiPackedFile(new MultiPackedFile(), cRZAutoRefCount<MultiPackedFile>::kAddRef);

		if (multiPackedFile->Init())
		{
			if (multiPackedFile->SetPath(folderPath))
			{
				if (multiPackedFile->Open(datFilePaths))
				{
					pResMan->RegisterDBSegment(*multiPackedFile);
				}
			}

			multiPackedFile->Shutdown();
		}
	}

	void SetupMultiPackedFileSegment(
		const cIGZString& folderPath,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* const pResMan)
	{
		cRZAutoRefCount<MultiPackedFile> multiPackedFile(new MultiPackedFile(), cRZAutoRefCount<MultiPackedFile>::kAddRef);

		if (multiPackedFile->Init())
		{
			if (multiPackedFile->SetPath(folderPath))
			{
				if (multiPackedFile->Open(true, false))
				{
					pResMan->RegisterDBSegment(*multiPackedFile);
				}
			}

			multiPackedFile->Shutdown();
		}
	}

	void RegisterDefaultObjectFactories(cIGZPersistResourceManager* const pResMan)
	{
		pResMan->RegisterObjectFactory(0x0a63df8c, 0xca63e2a3, nullptr);
		pResMan->RegisterObjectFactory(0x00436eb4, 0x2026960b, nullptr);
		pResMan->RegisterObjectFactory(0xc8696797, 0x686aa4b0, nullptr);
		pResMan->RegisterObjectFactory(0xa2ffb5d3, 0x856ddbac, nullptr);
		pResMan->RegisterObjectFactory(0x3ab50e2a, 0x7ab50e44, nullptr);
		pResMan->RegisterObjectFactory(0x3ab50e2a, 0x7ab50e45, nullptr);
		pResMan->RegisterObjectFactory(0x69b6f01c, 0x29a5d1ec, nullptr);
		pResMan->RegisterObjectFactory(0x69b6f01c, 0x09adcd75, nullptr);
		pResMan->RegisterObjectFactory(0xfad0f0b6, 0x5ad0e817, nullptr);
		pResMan->RegisterObjectFactory(0x453429b3, 0x6534284a, nullptr);
		pResMan->RegisterObjectFactory(0x053429c8, 0x05342861, nullptr);
		pResMan->RegisterObjectFactory(0xa83479ea, 0xa83479d3, nullptr);
		pResMan->RegisterObjectFactory(0x496678fe, 0x296678f7, nullptr);
		pResMan->RegisterObjectFactory(0xea5118b5, 0xea5118b0, nullptr);
		pResMan->RegisterObjectFactory(0x42e411c2, 0xa2e3d533, nullptr);
	}

	void LoadDatFilesTopDirectoryOnly(
		const cIGZString& folderPath,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* const pResMan)
	{
		SC4DirectoryEnumerator enumerator(
			folderPath,
			SC4DirectoryEnumeratorOptions::TopDirectoryOnly | SC4DirectoryEnumeratorOptions::OnlyDatFiles);

		SetupPackedFileSegments(enumerator.DatFiles(), pCOM, pResMan);
	}

	void LoadPluginsFolder(
		const cIGZString& folderPath,
		cIGZCOM* const pCOM,
		cIGZPersistResourceManager* const pResMan)
	{
		const std::filesystem::path nativePath = GZStringConvert::ToFileSystemPath(folderPath);

		// SC4 will create the <SC4 install folder>\Plugins and <User folder>\Plugins if they
		// don't exist, so we preserve that behavior.
		if (!DirectoryExists(nativePath))
		{
			std::filesystem::create_directories(nativePath);
		}

		SC4DirectoryEnumerator enumerator(nativePath);

		// The .SC4* files (.SC4Desc, .SC4Lot, etc.) are loaded first.
		SetupPackedFileSegments(enumerator.Sc4Files(), pCOM, pResMan);

		// The .DAT files are loaded second as a multi-packed file.
		SetupMultiPackedFileSegment(folderPath, enumerator.DatFiles(), pCOM, pResMan);
	}
}

bool SC4DBPFLoading::SetupResources()
{
	bool result = false;

	cIGZPersistResourceManagerPtr pResMan;

	cIGZFrameWork* const pFramework = RZGetFramework();
	cIGZApp* const pApp = pFramework->Application();

	cRZAutoRefCount<cISC4App> pSC4App;

	if (pApp->QueryInterface(GZIID_cISC4App, pSC4App.AsPPVoid()))
	{
		cIGZCOM* const pCOM = pFramework->GetCOMObject();

		try
		{
			// The first directory loaded is the installation root folder,
			// this is where SimCity_1.dat is located.
			cRZBaseString sc4InstallationRoot;
			pSC4App->GetDataDirectory(sc4InstallationRoot, -1);
			LoadDatFilesTopDirectoryOnly(sc4InstallationRoot, pCOM, pResMan);

			// The second directory loaded is the language-specific folder,
			// this is where SimCityLocale.dat is located.
			cRZBaseString sc4LanguageDir;
			pSC4App->GetDataDirectory(sc4LanguageDir, 0);
			LoadDatFilesTopDirectoryOnly(sc4LanguageDir, pCOM, pResMan);

			// The third directory loaded is the SKU-specific folder.
			cRZBaseString skuDataDir;
			pSC4App->GetSkuSpecificDirectory(skuDataDir);
			SetupMultiPackedFileSegment(skuDataDir, pCOM, pResMan);

			// The fourth directory loaded is the Plugins folder in
			// the installation directory.
			cRZBaseString sc4InstallationPlugins;
			pSC4App->GetPluginDirectory(sc4InstallationPlugins);
			LoadPluginsFolder(sc4InstallationPlugins, pCOM, pResMan);

			// The fifth directory loaded is the Plugins folder in the user
			// directory, this is typically <Documents>\SimCity 4\Plugins.
			cRZBaseString userPlugins;
			pSC4App->GetUserPluginDirectory(userPlugins);
			LoadPluginsFolder(userPlugins, pCOM, pResMan);

			// Finally we register the resource manager factories.
			RegisterDefaultObjectFactories(pResMan);

			result = pResMan->GetSegmentCount() > 0;
		}
		catch (const std::exception& e)
		{
			Logger::GetInstance().WriteLine(LogLevel::Error, e.what());
		}
	}

	return result;
}
