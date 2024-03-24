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
}