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
#include "Logger.h"
#include "Patcher.h"
#include "SC4DirectoryEnumerator.h"
#include "cIGZString.h"

namespace
{
	static void* RZStringVtable = reinterpret_cast<void*>(0xA80810);

	class cRZString
	{
	public:
		cIGZString* AsIGZString()
		{
			return reinterpret_cast<cIGZString*>(this);
		}

		void* vtable;
		intptr_t stdStringField1;
		intptr_t stdStringField2;
		intptr_t stdStringField3;
		uint32_t refCount;
	};

	typedef cRZString* (__thiscall* cRZString_ctor_cIGZString)(cRZString* pThis, cIGZString const& str);

	static const cRZString_ctor_cIGZString RZString_ctor_cIGZString = reinterpret_cast<cRZString_ctor_cIGZString>(0x40AE00);

	enum cRZString_dtor_Options : uint8_t
	{
		RZStrDtorOptionClassIsStackVariable = 0,
		RZStrDtorOptionClassIsHeapAllocated = 1,
		RZStrDtorOptionUnknown = 2,
	};

	typedef cRZString* (__thiscall* cRZString_dtor)(cRZString* pThis, cRZString_dtor_Options options);

	static const cRZString_dtor RZString_dtor = reinterpret_cast<cRZString_dtor>(0x44BF10);

	typedef void(__thiscall* std_list_cRZString_insert)(void* pList, cRZString const& str);

	static const std_list_cRZString_insert ListInsert = reinterpret_cast<std_list_cRZString_insert>(0x4580E0);

	static bool __fastcall ScanDirectoryForLooseSC4Plugins(
		void* rzDirectoryThisPtr,
		void* edxUnused,
		cIGZString* rootDir,
		void* rzStringList,
		bool includeCurrentDirFiles)
	{
		// This method replaces the cRZDirectory::ReadDirectoryFileEntriesIntoStringListRecursive call
		// that the game uses to scan for the loose .SC4* plugins.
		//
		// Unlike the original method, we check the file extension as part of the file scanning code.
		// This ensures that we only perform the UTF-16 to UTF-8 string conversion for items with a .SC4*
		// file extension, and it also reduces the amount of memory that needs to be allocated for the output
		// lists.

		bool result = false;

		try
		{
			std::vector<cRZBaseString> sc4Files;

			SC4DirectoryEnumerator::ScanDirectoryForLooseSC4FilesRecursive(*rootDir, sc4Files);

			for (const cRZBaseString& path : sc4Files)
			{
				// The cRZString class is a reimplementation of the game's internal cRZString class.
				// We need to do this for compatibility with the game's std::list<cRZString>::insert method
				// that we call to insert the item into this method's output list.
				//
				// We allocate our class on the stack and call the game's cRZString constructor and destructor.

				cRZString rzStr{};
				RZString_ctor_cIGZString(&rzStr, path);

#ifdef _DEBUG
				const char* const path2 = rzStr.AsIGZString()->ToChar();
#endif // _DEBUG


				// Insert the copied string into the output list.
				ListInsert(rzStringList, rzStr);

				RZString_dtor(&rzStr, RZStrDtorOptionClassIsStackVariable);
			}

			result = true;
		}
		catch (const std::exception& e)
		{
			Logger::GetInstance().WriteLine(LogLevel::Error, e.what());
			result = false;
		}

		return result;
	}

	static int32_t __cdecl HookedIsDatabaseFile(cIGZString const& path, bool scanEntireFile)
	{
#ifdef _DEBUG
		//PrintLineToDebugOutput(path.ToChar());
#endif // _DEBUG

		// SC4 checks every file in its plugins folders that does not have a .dat file extension to
		// see if it is a DBPF file.
		// Because our replacement directory scanning code already filters out any files that
		// don't have a .SC4* file extension, we just return true.
		//
		// The method treats -1 as false, and any other value as true.

		return 1;
	}

	static constexpr uintptr_t SC4InstallationPluginDirectoryScan_Inject = 0x457A2F;
	static constexpr uintptr_t SC4InstallationPluginSkipFileExtensionCheck_Inject = 0x457A4B;
	static constexpr uintptr_t SC4InstallationPluginSkipFileExtensionCheck_Continue = 0x457A6A;
	static constexpr uintptr_t SC4InstallationPluginIsDatabaseFile_Inject = 0x457A9F;

	static void NAKED_FUN SC4InstallationPluginSkipFileExtensionCheck()
	{
		__asm
		{
			// Cleanup the stack parameters for the cRZFile::GetPathExtension call we are replacing.
			add esp, 8;
			// Skip the remaining file extension check code.
			push SC4InstallationPluginSkipFileExtensionCheck_Continue;
			ret;
		}
	}

	static constexpr uintptr_t UserPluginDirectoryScan_Inject = 0x457CAE;
	static constexpr uintptr_t UserPluginSkipFileExtensionCheck_Inject = 0x457CCA;
	static constexpr uintptr_t UserPluginSkipFileExtensionCheck_Continue = 0x457CE9;
	static constexpr uintptr_t UserPluginIsDatabaseFile_Inject = 0x457D1E;

	static void NAKED_FUN UserPluginSkipFileExtensionCheck()
	{
		__asm
		{
			// Cleanup the stack parameters for the cRZFile::GetPathExtension call we are replacing.
			add esp, 8;
			// Skip the remaining file extension check code.
			push UserPluginSkipFileExtensionCheck_Continue;
			ret;
		}
	}

	void InstallSC4InstallationPluginsDirScanPatch()
	{
		Patcher::InstallCallHook(SC4InstallationPluginDirectoryScan_Inject, &ScanDirectoryForLooseSC4Plugins);
		Patcher::InstallHook(SC4InstallationPluginSkipFileExtensionCheck_Inject, &SC4InstallationPluginSkipFileExtensionCheck);
		Patcher::InstallCallHook(SC4InstallationPluginIsDatabaseFile_Inject, &HookedIsDatabaseFile);
	}

	void InstallUserDirScanPatch()
	{
		Patcher::InstallCallHook(UserPluginDirectoryScan_Inject, &ScanDirectoryForLooseSC4Plugins);
		Patcher::InstallHook(UserPluginSkipFileExtensionCheck_Inject, &UserPluginSkipFileExtensionCheck);
		Patcher::InstallCallHook(UserPluginIsDatabaseFile_Inject, &HookedIsDatabaseFile);
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
