///////////////////////////////////////////////////////////////////////////////
//
// This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
// optimizes the DBPF loading.
//
// Copyright (c) 2024 Nicholas Hayes
//
// This file is licensed under terms of the MIT License.
// See LICENSE.txt for more information.
//
///////////////////////////////////////////////////////////////////////////////

#include "version.h"
#include "cRZFileHooks.h"
#include "DebugUtil.h"
#include "Logger.h"
#include "Patcher.h"
#include "SC4VersionDetection.h"
#include "Stopwatch.h"
#include "StringViewUtil.h"
#include "cIGZApp.h"
#include "cIGZCheatCodeManager.h"
#include "cIGZCmdLine.h"
#include "cIGZCOM.h"
#include "cIGZDBSegmentPackedFile.h"
#include "cIGZFrameWork.h"
#include "cIGZPersistDBSegment.h"
#include "cIGZPersistDBSegmentMultiPackedFiles.h"
#include "cIGZPersistResourceKeyFilter.h"
#include "cIGZPersistResourceKeyList.h"
#include "cIGZPersistResourceManager.h"
#include "cRZAutoRefCount.h"
#include "cRZBaseString.h"
#include "cRZCOMDllDirector.h"
#include "cISC4App.h"
#include "GZServPtrs.h"
#include "SC4Preferences.h"

#include <filesystem>
#include <memory>
#include <string>
#include <Windows.h>
#include "wil/resource.h"
#include "wil/win32_helpers.h"

#include "boost/algorithm/string.hpp"
#include "detours/detours.h"

static constexpr uint32_t kDBPFLoadingDirectorID = 0x87A74BF8;

static constexpr std::string_view PluginLogFileName = "SC4DBPFLoading.log";

using namespace std::literals::string_view_literals;

namespace
{
	std::filesystem::path GetDllFolderPath()
	{
		wil::unique_cotaskmem_string modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());

		std::filesystem::path temp(modulePath.get());

		return temp.parent_path();
	}

	void DisableResourceLoadDebuggingCode(uint16_t gameVersion)
	{
		Logger& logger = Logger::GetInstance();

		if (gameVersion == 641)
		{
			try
			{
				// The method that scans for plugins on startup (cSC4App::UpdateResources) has some debugging code that
				// always runs when the extra cheats plugin is installed. The extra cheats plugin sets a value to enable
				// the cheat codes and other internal debug functionality that Maxis used when developing the game.
				// This resource debug code appears to have possibly been doing some kind of logging in debug builds of
				// of the game, but it just wastes CPU time in the retail builds.
				//
				// We modify that check to make the game think that the internal debug mode is always disabled by
				// replacing the conditional short jump that is taken when the pointer is null with an unconditional
				// short jump.
				//
				// Original instruction: 0x74 (JZ rel8).
				// New instruction: 0xEB (JMP rel8).
				Patcher::OverwriteMemory(0x4572CE, static_cast<uint8_t>(0xEB));
				logger.WriteLine(LogLevel::Info, "Disabled the built-in DBPF loading debug code.");
			}
			catch (const std::exception& e)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to disable the built-in DBPF loading debug code: %s",
					e.what());
			}
		}
	}

	bool HasDBPFFileExtension(const std::string_view& file)
	{
		// Assume that the file has a DBPF extension.

		bool result = true;

		const size_t periodOffset = file.find_last_of('.');

		if (periodOffset != std::string_view::npos && periodOffset != (file.length() - 1))
		{
			std::string_view ext = file.substr(periodOffset);

			// The non-DAT DBPF plugins all use a file extension starting with .SC4, these files
			// should use one of the following extensions: .SC4, .SC4Desc, .SC4Lot or .SC4Model.
			result = boost::istarts_with(ext, ".SC4"sv);
		}

		return result;
	}

	typedef int32_t(__cdecl *pfn_cGZDBSegmentPackedFile_IsDatabaseFile)(cIGZString const& path, bool scanEntireFile);

	static pfn_cGZDBSegmentPackedFile_IsDatabaseFile RealIsDatabaseFile = nullptr;

	static int32_t __cdecl HookedIsDatabaseFile(cIGZString const& path, bool scanEntireFile)
	{
#ifdef _DEBUG
		//PrintLineToDebugOutput(path.ToChar());
#endif // _DEBUG

		// SC4 checks every file in its plugins folders that does not have a .dat file extension to
		// see if it is a DBPF file, this can include file extensions such as .dll, .zip, etc.
		//
		// As a performance optimization, we check that the file extension starts .SC4, this will
		// restrict the DBPF header check to .SC4, .SC4Desc, .SC4Lot or .SC4Model.
		// SC4 will check the file signature when it opens the file and validates the header.
		//
		// The method treats -1 as false, and any other value as true.

		return HasDBPFFileExtension(path.ToChar()) ? 1 : -1;
	}

	void InstallIsDatabaseFileHook(uint16_t gameVersion)
	{
		if (gameVersion == 641)
		{
			Logger& logger = Logger::GetInstance();

			// The cGZDBSegmentPackedFile::IsDatabaseFile method is only called by the method that scans
			// for plugins on startup (cSC4App::UpdateResources), so we don't need to unhook it after
			// that code runs.
			//
			// Using Detours to hook the original method was simpler than trying to patch the 3
			// locations in cSC4App::UpdateResources where it is called.

			try
			{
				RealIsDatabaseFile = reinterpret_cast<pfn_cGZDBSegmentPackedFile_IsDatabaseFile>(0x9728D5);

				DetourRestoreAfterWith();

				DetourTransactionBegin();
				DetourUpdateThread(GetCurrentThread());
				DetourAttach(&(PVOID&)RealIsDatabaseFile, HookedIsDatabaseFile);
				DetourTransactionCommit();

				logger.WriteLine(LogLevel::Info, "Patched cGZDBSegmentPackedFile::IsDatabaseFile.");
			}
			catch (const std::exception& e)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to patch cGZDBSegmentPackedFile::IsDatabaseFile: %s",
					e.what());
			}
		}
	}

	static int32_t __fastcall HookedFindHeaderRecord(void* thisPtr, void* edxUnused)
	{
		// When SC4 reads a DBPF file and does not find a valid header, it will
		// scan the entire file for the following 16-byte hexadecimal value:
		// 80 9D 88 EC 8F 24 03 6C C9 A6 31 56 5B CF 77 22
		// Any data following this magic value will be treated as a DBPF file.
		//
		// We always tells the game that this magic header is not present.
		// The method treats -1 as false, and any other value as true.

		return -1;
	}

	void InstallDBPFOpenFindHeaderRecordHook(uint16_t gameVersion)
	{
		if (gameVersion == 641)
		{
			Logger& logger = Logger::GetInstance();

			// The cGZDBSegmentPackedFile::FindHeaderRecord method is called when
			// the game opens a DBPF file and the header validation fails.
			// This method scan the entire file for a 16-byte magic signature, and if
			// it is found the data following the signature is loaded as a DBPF file.
			//
			// Our version always tells the game that this magic signature was not found.

			try
			{
				Patcher::InstallCallHook(0x9729e1, &HookedFindHeaderRecord);
				logger.WriteLine(LogLevel::Info, "Patched the DBPF Open header check.");
			}
			catch (const std::exception& e)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to patch the DBPF Open header check: %s",
					e.what());
			}
		}
	}

	typedef int32_t(__cdecl* RZString_Sprintf)(void* thisPtr, const char* format, ...);

	static RZString_Sprintf RealRZStringSprintf = nullptr;

	static int32_t __cdecl Hooked_MissingPluginPackSprintf(
		void* rzStringThisPtr,
		const char* format,
		const char* pluginPackStr,
		uint32_t pluginPackID)
	{
		// SC4's missing plugin format string is "%s %d". The first parameter is a localized
		// string for "Plugin Pack" and the second parameter is the plugin pack ID as a decimal
		// number.
		// Our replacement format string adds a hexadecimal version of the plugin pack ID, which
		// makes it simpler for users as they don't have to convert the decimal string to
		// hexadecimal.

		int32_t result = RealRZStringSprintf(
			rzStringThisPtr,
			"%s 0x%08x",
			pluginPackStr,
			pluginPackID,
			pluginPackID);

		return result;
	}

	void InstallMissingPluginDialogHexPatch(uint16_t gameVersion)
	{
		if (gameVersion == 641)
		{
			Logger& logger = Logger::GetInstance();
			// We wrap the cRZString::Sprintf call that SC4 used to print the missing plugin pack
			// message and replace the format string with one that includes the plugin pack id as
			// a hexadecimal number.

			try
			{
				RealRZStringSprintf = reinterpret_cast<RZString_Sprintf>(0x90F574);
				Patcher::InstallCallHook(0x48C603, &Hooked_MissingPluginPackSprintf);
				logger.WriteLine(LogLevel::Info, "Changed the missing plugin error message to use hexadecimal.");
			}
			catch (const std::exception& e)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to change the missing plugin error message to use hexadecimal: %s",
					e.what());
			}
		}
	}

	enum class ResourceLoadingTraceOption
	{
		// No tracing will be performed.
		None = 0,
		// A message box will be shown with the number of milliseconds the game took to load resources.
		ShowLoadTime,
		// Message boxes will be shown before and after the resource loading so that the user can start
		// and stop a program that logs the Windows API calls issued by the game.
		// For example, Sysinternals Process Monitor.
		WindowsAPILogWait,
		// Writes a list of the loaded fies to the plugin's log file.
		ListLoadedFiles,
	};

	static ResourceLoadingTraceOption resourceLoadingTraceOption = ResourceLoadingTraceOption::None;

	typedef bool(__thiscall* pfn_cSC4App_SetupResources)(void* pSC4App);

	static pfn_cSC4App_SetupResources RealSetupResources = reinterpret_cast<pfn_cSC4App_SetupResources>(0x4572B0);

	bool TimedSetupResources(void* pSC4App)
	{
		Stopwatch sw;
		sw.Start();

		bool result = RealSetupResources(pSC4App);

		sw.Stop();

		char buffer[256]{};

		std::snprintf(buffer, sizeof(buffer), "Loaded resources in %lld ms", sw.ElapsedMilliseconds());

		MessageBoxA(nullptr, buffer, "SC4DBPFLoading", 0);

		return result;
	}

	bool WindowsAPILogSetupResources(void* pSC4App)
	{
		MessageBoxA(nullptr, "Start your Process Monitor trace and press OK.", "SC4DBPFLoading", 0);

		bool result = RealSetupResources(pSC4App);

		MessageBoxA(nullptr, "Stop your Process Monitor trace and press OK.", "SC4DBPFLoading", 0);

		return result;
	}

	bool __fastcall HookedSetupResources(void* pSC4App, void* edxUnused)
	{
		bool result = false;

		switch (resourceLoadingTraceOption)
		{
		case ResourceLoadingTraceOption::ShowLoadTime:
			result = TimedSetupResources(pSC4App);
			break;
		case ResourceLoadingTraceOption::WindowsAPILogWait:
			result = WindowsAPILogSetupResources(pSC4App);
			break;
		case ResourceLoadingTraceOption::None:
		case ResourceLoadingTraceOption::ListLoadedFiles:
		default:
			result = RealSetupResources(pSC4App);
			break;
		}

		return result;
	}

	void InstallSC4AppSetupResourcesHook(uint16_t gameVersion)
	{
		if (gameVersion == 641)
		{
			Logger& logger = Logger::GetInstance();

			try
			{
				Patcher::InstallCallHook(0x44C97E, &HookedSetupResources);
				logger.WriteLine(LogLevel::Info, "Installed the cSC4App::SetupResources hook.");
			}
			catch (const std::exception& e)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to install the cSC4App::SetupResources hook: %s",
					e.what());
			}
		}
	}

	void InstallMemoryPatches()
	{
		Logger& logger = Logger::GetInstance();

		const uint16_t gameVersion = SC4VersionDetection::GetInstance().GetGameVersion();

		if (gameVersion == 641)
		{
			DisableResourceLoadDebuggingCode(gameVersion);
			InstallIsDatabaseFileHook(gameVersion);
			InstallDBPFOpenFindHeaderRecordHook(gameVersion);
			InstallMissingPluginDialogHexPatch(gameVersion);
			cRZFileHooks::Install(gameVersion);

			switch (resourceLoadingTraceOption)
			{
			case ResourceLoadingTraceOption::ShowLoadTime:
			case ResourceLoadingTraceOption::WindowsAPILogWait:
				InstallSC4AppSetupResourcesHook(gameVersion);
				break;
			}
		}
		else
		{
			logger.WriteLineFormatted(
				LogLevel::Error,
				"Unable to install the memory patches. Requires "
				"game version 641, found game version %d.",
				gameVersion);
		}
	}

#ifdef _DEBUG
	typedef bool(_cdecl *pfn_DoesDirectoryExist)(cIGZString const& path);

	static pfn_DoesDirectoryExist RealDoesDirectoryExist = reinterpret_cast<pfn_DoesDirectoryExist>(0x91B6CA);

	static bool __cdecl HookedDoesDiectoryExist(cIGZString const& path)
	{
		DebugUtil::PrintLineToDebugOutput(path.ToChar());

		bool result = RealDoesDirectoryExist(path);

		return result;
	}

	void InstallDoesDirectoryExistHook()
	{
		DetourRestoreAfterWith();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)RealDoesDirectoryExist, HookedDoesDiectoryExist);
		DetourTransactionCommit();
	}

	void RemoveDoesDirectoryExistHook()
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)RealDoesDirectoryExist, HookedDoesDiectoryExist);
		DetourTransactionCommit();
	}

	cRZBaseString GetSC4InstallFolderFilePath(cIGZFrameWork* const pFramework, const char* const fileName)
	{
		cRZBaseString path;

		cIGZApp* const pApp = pFramework->Application();

		cRZAutoRefCount<cISC4App> pSC4App;

		if (pApp->QueryInterface(GZIID_cISC4App, pSC4App.AsPPVoid()))
		{
			cRZBaseString installDir;

			pSC4App->GetDataDirectory(installDir, -1);

			path.Append(installDir);

			if (path.Strlen() > 0)
			{
				char lastPathChar = path.Data()[path.Strlen() - 1];

				if (lastPathChar != '\\' && lastPathChar != '/')
				{
					path.Append("\\", 1);
				}
			}

			path.Append(fileName, strlen(fileName));
		}

		return path;
	}

	void Trace_GZDBSegmentPackedFile_GetResourceKeys(cIGZCOM* pCOM)
	{
		const char* const fileName = "SimCity_1.dat";
		cRZBaseString path = GetSC4InstallFolderFilePath(pCOM->FrameWork(), fileName);

		cRZAutoRefCount<cIGZDBSegmentPackedFile> packedFile;

		if (pCOM->GetClassObject(GZCLSID_cGZDBSegmentPackedFile, GZIID_cIGZDBSegmentPackedFile, packedFile.AsPPVoid()))
		{
			if (packedFile->Init())
			{
				if (packedFile->SetPath(path))
				{
					cIGZPersistDBSegment* pSegment = packedFile->AsIGZPersistDBSegment();

					if (pSegment)
					{
						Stopwatch sw;
						sw.Start();

						bool result = pSegment->Open(true, false);

						sw.Stop();

						DebugUtil::PrintLineToDebugOutputFormatted("%s opened in %lld ms", fileName, sw.ElapsedMilliseconds());

						if (result)
						{
							cRZAutoRefCount<cIGZPersistResourceKeyList> pList;

							if (pCOM->GetClassObject(GZCLSID_cIGZPersistResourceKeyList, GZIID_cIGZPersistResourceKeyList, pList.AsPPVoid()))
							{
								if (pSegment->GetResourceKeyList(pList, nullptr))
								{
									DebugUtil::PrintLineToDebugOutputFormatted("%u resource keys", pList->Size());
								}
							}
						}
					}
				}

			}
		}
	}
#endif // _DEBUG
}

class DBPFLoadingDllDirector : public cRZCOMDllDirector
{
public:

	DBPFLoadingDllDirector()
	{
		std::filesystem::path dllFolderPath = GetDllFolderPath();

		std::filesystem::path logFilePath = dllFolderPath;
		logFilePath /= PluginLogFileName;

		Logger& logger = Logger::GetInstance();
		logger.Init(logFilePath, LogLevel::Error, false);
		logger.WriteLogFileHeader("SC4DBPFLoading v" PLUGIN_VERSION_STR);
	}

private:

	uint32_t GetDirectorID() const
	{
		return kDBPFLoadingDirectorID;
	}

	bool OnStart(cIGZCOM* pCOM)
	{
		cIGZFrameWork* const pFramework = pCOM->FrameWork();
		cIGZCmdLine* const pCmdLine = pFramework->CommandLine();

		resourceLoadingTraceOption = ResourceLoadingTraceOption::None;

		cRZBaseString value;

		if (pCmdLine->IsSwitchPresent(cRZBaseString("StartupDBPFLoadTrace"), value, true))
		{
			std::string_view valueAsStringView = value.ToChar();

			if (StringViewUtil::EqualsIgnoreCase(valueAsStringView, "ShowLoadTime"sv))
			{
				resourceLoadingTraceOption = ResourceLoadingTraceOption::ShowLoadTime;
			}
			else if (StringViewUtil::EqualsIgnoreCase(valueAsStringView, "WinAPI"sv))
			{
				resourceLoadingTraceOption = ResourceLoadingTraceOption::WindowsAPILogWait;
			}
			else if (StringViewUtil::EqualsIgnoreCase(valueAsStringView, "ListLoadedFiles"sv))
			{
				resourceLoadingTraceOption = ResourceLoadingTraceOption::ListLoadedFiles;
			}
		}

		InstallMemoryPatches();

		//Trace_GZDBSegmentPackedFile_GetResourceKeys(pCOM);

		if (resourceLoadingTraceOption == ResourceLoadingTraceOption::ListLoadedFiles)
		{
			if (pFramework->GetState() < cIGZFrameWork::kStatePreAppInit)
			{
				pFramework->AddHook(this);
			}
			else
			{
				PreAppInit();
			}
		}

		return true;
	}

	bool PreAppInit()
	{
		//InstallDoesDirectoryExistHook();

		return true;
	}

	bool PostAppInit()
	{
		//RemoveDoesDirectoryExistHook();

		if (resourceLoadingTraceOption == ResourceLoadingTraceOption::ListLoadedFiles)
		{
			cIGZPersistResourceManagerPtr pResMan;

			if (pResMan)
			{
				Logger& logger = Logger::GetInstance();

				uint32_t segmentCount = pResMan->GetSegmentCount();

				// We log the segments in reverse order so that the earliest values are shown first.
				// The resource manager adds new segments to the start of the list instead of the end.
				for (int64_t i = static_cast<int64_t>(segmentCount) - 1; i >= 0; i--)
				{
					cIGZPersistDBSegment* segment = pResMan->GetSegmentByIndex(static_cast<uint32_t>(i));

					if (segment)
					{
						cRZAutoRefCount<cIGZPersistDBSegmentMultiPackedFiles> multiPackedFile;

						if (segment->QueryInterface(GZIID_cIGZPersistDBSegmentMultiPackedFiles, multiPackedFile.AsPPVoid()))
						{
							// Multi-packed files are used as a container for the DAT files that are loaded from a
							// directory and its sub-directories.
							//
							// This design was likely used to allow DAT files in sub-directories to override items
							// from the directories that are loaded earlier.
							// Internally it appears use a combination of a dictionary/unordered_map that maps each TGI
							// to the DAT that contains it, and a list of segments (DAT files) for GetSegmentByIndex.

							uint32_t multiPackSegmentCount = multiPackedFile->GetSegmentCount();

							// The multi-packed files are already in the correct order, with new items added to
							// the end of the list.
							for (uint32_t j = 0; j < multiPackSegmentCount; j++)
							{
								cIGZPersistDBSegment* multiPackedSegment = multiPackedFile->GetSegmentByIndex(j);

								if (multiPackedSegment)
								{
									cRZBaseString path;

									multiPackedSegment->GetPath(path);

									logger.WriteLine(LogLevel::Info, path.ToChar());
								}
							}
						}
						else
						{
							cRZBaseString path;

							segment->GetPath(path);

							logger.WriteLine(LogLevel::Info, path.ToChar());
						}
					}
				}
			}
		}

#if 0
		cIGZFrameWork* const pFramework = RZGetFramework();

		cIGZApp* const pApp = pFramework->Application();

		cRZAutoRefCount<cISC4App> pSC4App;

		if (pApp->QueryInterface(GZIID_cISC4App, pSC4App.AsPPVoid()))
		{
			cRZBaseString a, b, c, d, e, f, g, h, i, j, k, l, m, n;

			pSC4App->GetAppDirectory(a);         // The folder containing SimCity 4.exe.
			pSC4App->GetDataDirectory(b, -1);    // SC4 install root
			pSC4App->GetDataDirectory(c, 0);     // SC4 install language folder
			pSC4App->GetPluginDirectory(d);      // SC4 install plugins folder
			pSC4App->GetSkuSpecificDirectory(e); // SKU data

			// The following folders are all located in <Documents>\SimCity 4 by default.
			pSC4App->GetUserDataDirectory(f); // Root folder, home to SimCity 4.cfg.
			pSC4App->GetUserPluginDirectory(g);
			pSC4App->GetRegionsDirectory(h);
			pSC4App->GetMySimDirectory(i);
			pSC4App->GetAlbumDirectory(j);
			pSC4App->GetHTTPCacheDirectory(k);
			pSC4App->GetTempDirectory(l);
			pSC4App->GetExceptionReportsDirectory(m);
			pSC4App->GetTestScriptDirectory(n);
		}
#endif
		return true;
	}

};

cRZCOMDllDirector* RZGetCOMDllDirector() {
	static DBPFLoadingDllDirector sDirector;
	return &sDirector;
}