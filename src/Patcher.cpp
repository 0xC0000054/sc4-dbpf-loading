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

#include "Patcher.h"
#include <Windows.h>
#include "wil/resource.h"
#include "wil/win32_helpers.h"

void Patcher::OverwriteMemory(uintptr_t address, uint8_t newValue)
{
	DWORD oldProtect;
	// Allow the executable memory to be written to.
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
		reinterpret_cast<LPVOID>(address),
		sizeof(newValue),
		PAGE_EXECUTE_READWRITE,
		&oldProtect));

	// Patch the memory at the specified address.
	*((uint8_t*)address) = newValue;
}

void Patcher::OverwriteMemory(uintptr_t address, uintptr_t newValue)
{
	DWORD oldProtect;
	// Allow the executable memory to be written to.
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
		reinterpret_cast<LPVOID>(address),
		sizeof(newValue),
		PAGE_EXECUTE_READWRITE,
		&oldProtect));

	// Patch the memory at the specified address.
	*((uintptr_t*)address) = newValue;
}

void Patcher::InstallHook(uintptr_t targetAddress, void (*pfnFunc)())
{
	// Allow the executable memory to be written to.
	DWORD oldProtect = 0;
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
		reinterpret_cast<LPVOID>(targetAddress),
		5,
		PAGE_EXECUTE_READWRITE,
		&oldProtect));

	// Patch the memory at the specified address.
	*((uint8_t*)targetAddress) = 0xE9;
	*((uintptr_t*)(targetAddress + 1)) = reinterpret_cast<uintptr_t>(pfnFunc) - targetAddress - 5;
}


void Patcher::InstallCallHook(uintptr_t targetAddress, void* pfnFunc)
{
	// Allow the executable memory to be written to.
	DWORD oldProtect = 0;
	THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
		reinterpret_cast<LPVOID>(targetAddress),
		5,
		PAGE_EXECUTE_READWRITE,
		&oldProtect));

	// Patch the memory at the specified address.
	*((uint8_t*)targetAddress) = 0xE8;
	*((uintptr_t*)(targetAddress + 1)) = ((uintptr_t)pfnFunc) - targetAddress - 5;
}
