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

#include "LooseSC4PluginScanPatch.h"
#include "SC4PluginMultiPackedFile.h"
#include "Logger.h"
#include "Patcher.h"
#include "cIGZCOM.h"
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
	void LoadSC4FilesFromDirectory(const cIGZString& rootDir)
	{
		bool result = false;

		cRZAutoRefCount<cIGZPersistDBSegment> looseSC4MultiPackedFile(
			new SC4PluginMultiPackedFile(),
			cRZAutoRefCount<cIGZPersistDBSegment>::kAddRef);

		if (looseSC4MultiPackedFile->Init())
		{
			if (looseSC4MultiPackedFile->SetPath(rootDir))
			{
				if (looseSC4MultiPackedFile->Open(true, false))
				{
					cIGZPersistResourceManagerPtr resMan;

					if (resMan)
					{
						result = resMan->RegisterDBSegment(*looseSC4MultiPackedFile);
					}
				}
			}

			if (!result)
			{
				looseSC4MultiPackedFile->Shutdown();
			}
		}
	}

	static constexpr uintptr_t SC4InstallationPluginDirectoryScan_Inject = 0x457a07;
	static constexpr uintptr_t SC4InstallationPluginDirectoryScan_Continue = 0x457B50;

	static void NAKED_FUN LoadSC4FilesForInstallationPluginsHook()
	{
		__asm
		{
			// We do not need to preserve any registers.
			lea eax, [esp + 0x3c]
			push eax // directory path
			call LoadSC4FilesFromDirectory // (cdecl)
			add esp, 4
			mov eax, SC4InstallationPluginDirectoryScan_Continue
			jmp eax
		}
	}

	static constexpr uintptr_t UserPluginDirectoryScan_Inject = 0x457C86;
	static constexpr uintptr_t UserPluginDirectoryScan_Continue = 0x457DCF;

	static void NAKED_FUN LoadSC4FilesForUserPluginsHook()
	{
		__asm
		{
			// We do not need to preserve any registers.
			lea eax, [esp + 0x3c]
			push eax // directory path
			call LoadSC4FilesFromDirectory // (cdecl)
			add esp, 4
			mov eax, UserPluginDirectoryScan_Continue
			jmp eax
		}
	}

	void InstallSC4InstallationPluginsDirScanPatch()
	{
		Patcher::InstallHook(SC4InstallationPluginDirectoryScan_Inject, &LoadSC4FilesForInstallationPluginsHook);
	}

	void InstallUserDirScanPatch()
	{
		Patcher::InstallHook(UserPluginDirectoryScan_Inject, &LoadSC4FilesForUserPluginsHook);
	}
}

void LooseSC4PluginScanPatch::Install()
{
	Logger& logger = Logger::GetInstance();

	try
	{
		InstallSC4InstallationPluginsDirScanPatch();
		InstallUserDirScanPatch();

		logger.WriteLine(LogLevel::Info, "Installed the .SC4* plugin scan patch.");
	}
	catch (const std::exception& e)
	{
		logger.WriteLineFormatted(
			LogLevel::Error,
			"Failed to install the .SC4* plugin scan patch: %s",
			e.what());
	}
}
