/*
 * This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
 * optimizes the DBPF loading.
 *
 * Copyright (C) 2026 Casper Van Gheluwe
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

#include "DBPFIndexHashPatch.h"
#include <cstring>
#include <exception>
#include <intrin.h>
#include <iterator>
#include <Windows.h>
#include "Logger.h"
#include "Patcher.h"
#include "wil/resource.h"
#include "wil/win32_helpers.h"

// cGZDBSegmentPackedFile stores its index and DIR records in hash tables that use the
// instance ID as the hash. S3D models and their textures reuse a small set of instances (0x00030000
// plus the zoom and rotation), which gives very long bucket chains in large plugin files and makes
// loading them quadratic.
//
// The bucket is only calculated in a few places, so we replace all of them with a hash of the full
// TGI. They all have to use the same hash, otherwise lookups will miss records.

namespace
{
	// This hash function has chained multiplies that make every bit of the type, group and instance affect
	// the high bits of the hash, and the bucket is taken from those bits with a multiply instead of a modulo.
	// On my 11GB plugins folder it distributes as well as memo's PersistResourceKeyHash or MSVC's default FNV-1a
	// at roughly a third to half of their cost, and only about 1 ns more than the original (bad) hash.
	uint32_t __fastcall GetBucketIndex(const uint32_t* tgi, uint32_t bucketCount)
	{
		uint32_t hash = tgi[2] * 0x9E3779B1;
		hash ^= tgi[1];
		hash *= 0x85EBCA77;
		hash ^= tgi[0];
		hash *= 0xC2B2AE3D;

		return static_cast<uint32_t>(__emulu(hash, bucketCount) >> 32);
	}

	// These hooks replace the instance load and the modulo on each of the call sites.
	// The bucket is returned in EDX. The game code overwrites EAX after each of the patched sites.

	// Index and DIR find, EDI is the key.
	static void NAKED_FUN FindKeyBucketHook()
	{
		__asm
		{
			sar esi, 2
			push ecx
			mov ecx, edi
			mov edx, esi
			call GetBucketIndex // (fastcall)
			mov edx, eax
			pop ecx
			ret
		}
	}

	// Index and DIR insert, the value is the second argument.
	static void NAKED_FUN InsertValueArg2BucketHook()
	{
		__asm
		{
			sar ecx, 2
			push ecx
			mov edx, ecx
			mov ecx, dword ptr [ebp + 0xC]
			call GetBucketIndex // (fastcall)
			mov edx, eax
			pop ecx
			ret
		}
	}

	// TGI map insert, the value is the first argument.
	static void NAKED_FUN InsertValueArg1BucketHook()
	{
		__asm
		{
			sar ecx, 2
			push ecx
			mov edx, ecx
			mov ecx, dword ptr [ebp + 8]
			call GetBucketIndex // (fastcall)
			mov edx, eax
			pop ecx
			ret
		}
	}

	// TGI map insert, ECX is the key.
	static void NAKED_FUN InsertKeyBucketHook()
	{
		__asm
		{
			sar edi, 2
			push ecx
			mov edx, edi
			call GetBucketIndex // (fastcall)
			mov edx, eax
			pop ecx
			ret
		}
	}

	// resize, ECX is the node and the new bucket count is at ebp-4.
	static void NAKED_FUN ResizeNodeBucketHook()
	{
		__asm
		{
			push ecx
			mov edx, dword ptr [ebp - 4]
			add ecx, 4
			call GetBucketIndex // (fastcall)
			mov edx, eax
			pop ecx
			ret
		}
	}

	// erase, EDI is the node.
	static void NAKED_FUN EraseNodeBucketHook()
	{
		__asm
		{
			sar ecx, 2
			push ecx
			mov edx, ecx
			lea ecx, [edi + 4]
			call GetBucketIndex // (fastcall)
			mov edx, eax
			pop ecx
			ret
		}
	}

	// iterator increment, ECX is the iterator and its first field is the node.
	static void NAKED_FUN IteratorNodeBucketHook()
	{
		__asm
		{
			sar edi, 2
			push ecx
			mov ecx, dword ptr [ecx]
			add ecx, 4
			mov edx, edi
			call GetBucketIndex // (fastcall)
			mov edx, eax
			pop ecx
			ret
		}
	}

	struct BucketPatch
	{
		uintptr_t address;
		uint8_t length;
		uint8_t expectedBytes[10];
		void (*pfnHook)();
	};

	// Sorted by address.
	static const BucketPatch BucketPatches[] =
	{
		{ 0x0096EEB5, 10, { 0x8B, 0x47, 0x08, 0xC1, 0xFE, 0x02, 0x33, 0xD2, 0xF7, 0xF6 }, &FindKeyBucketHook },   // index find
		{ 0x0096F21D, 7, { 0x33, 0xD2, 0xC1, 0xF9, 0x02, 0xF7, 0xF1 }, &InsertValueArg2BucketHook },              // index insert
		{ 0x0096F66F, 8, { 0x8B, 0x41, 0x0C, 0x33, 0xD2, 0xF7, 0x75, 0xFC }, &ResizeNodeBucketHook },             // resize
		{ 0x0097C055, 10, { 0x8B, 0x47, 0x08, 0xC1, 0xFE, 0x02, 0x33, 0xD2, 0xF7, 0xF6 }, &FindKeyBucketHook },   // DIR find
		{ 0x0097C0D9, 7, { 0x33, 0xD2, 0xC1, 0xF9, 0x02, 0xF7, 0xF1 }, &InsertValueArg2BucketHook },              // DIR insert
		{ 0x0097C160, 10, { 0x8B, 0x47, 0x0C, 0xC1, 0xF9, 0x02, 0x33, 0xD2, 0xF7, 0xF1 }, &EraseNodeBucketHook }, // erase
		{ 0x0097C1FA, 7, { 0x33, 0xD2, 0xC1, 0xFF, 0x02, 0xF7, 0xF7 }, &IteratorNodeBucketHook },                 // iterator increment
		{ 0x0097E4CC, 7, { 0x33, 0xD2, 0xC1, 0xF9, 0x02, 0xF7, 0xF1 }, &InsertValueArg1BucketHook },              // TGI map insert
		{ 0x0097E56B, 10, { 0x8B, 0x41, 0x08, 0xC1, 0xFF, 0x02, 0x33, 0xD2, 0xF7, 0xF7 }, &InsertKeyBucketHook }, // TGI map insert
	};
}

void DBPFIndexHashPatch::Install()
{
	Logger& logger = Logger::GetInstance();

	try
	{
		for (const BucketPatch& patch : BucketPatches)
		{
			if (std::memcmp(reinterpret_cast<const void*>(patch.address), patch.expectedBytes, patch.length) != 0)
			{
				logger.WriteLineFormatted(
					LogLevel::Error,
					"Failed to install the DBPF index hash patch: unexpected code at 0x%08X.",
					patch.address);
				return;
			}
		}

		// Make all of the patch sites writable first, so we can't end up with a partially installed patch.
		const BucketPatch& lastPatch = BucketPatches[std::size(BucketPatches) - 1];
		const uintptr_t start = BucketPatches[0].address;
		const size_t length = lastPatch.address + lastPatch.length - start;

		DWORD oldProtect = 0;
		THROW_IF_WIN32_BOOL_FALSE(VirtualProtect(
			reinterpret_cast<LPVOID>(start),
			length,
			PAGE_EXECUTE_READWRITE,
			&oldProtect));

		for (const BucketPatch& patch : BucketPatches)
		{
			Patcher::InstallCallHook(patch.address, patch.pfnHook);

			for (uintptr_t address = patch.address + 5; address < patch.address + patch.length; address++)
			{
				Patcher::OverwriteMemory(address, static_cast<uint8_t>(0x90));
			}
		}

		logger.WriteLine(LogLevel::Info, "Installed the DBPF index hash patch.");
	}
	catch (const std::exception& e)
	{
		logger.WriteLineFormatted(
			LogLevel::Error,
			"Failed to install the DBPF index hash patch: %s",
			e.what());
	}
}
