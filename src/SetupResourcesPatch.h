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

#pragma once

namespace SetupResourcesPatch
{
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

	void Install(ResourceLoadingTraceOption traceOption);
}