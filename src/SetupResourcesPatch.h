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
		// Writes a list of the loaded files to the plugin's log file.
		ListLoadedFiles,
		// A message box will be shown with the number of milliseconds the game took to get to the region view.
		ShowRegionViewLoadTime,
	};

	extern ResourceLoadingTraceOption resourceLoadingTraceOption;

	void Install();
}