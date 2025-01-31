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
#include <cstdint>

#ifdef __clang__
#define NAKED_FUN __attribute__((naked))
#else
#define NAKED_FUN __declspec(naked)
#endif

namespace Patcher
{
	void OverwriteMemory(uintptr_t address, uint8_t newValue);
	void OverwriteMemory(uintptr_t address, uintptr_t newValue);

	void InstallHook(uintptr_t targetAddress, void (*pfnFunc)());
	void InstallCallHook(uintptr_t targetAddress, void* pfnFunc);
	void InstallJumpTableHook(uintptr_t targetAddress, void* pfnFunc);
}