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

#pragma once
#include <cstdint>

namespace QFSDecoder
{
	// Both decoders have the same signature as the game's QFS decode function.
	typedef uint32_t(__cdecl* DecodeFunction)(const uint8_t* input, int* consumed, uint8_t* output);

	uint32_t __cdecl DecodeScalar(const uint8_t* input, int* consumed, uint8_t* output);
	uint32_t __cdecl DecodeAVX2(const uint8_t* input, int* consumed, uint8_t* output);

	bool CpuSupportsAVX2();
}
