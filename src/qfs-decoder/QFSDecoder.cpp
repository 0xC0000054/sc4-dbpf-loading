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

#include "QFSDecoder.h"
#include <intrin.h>

// This file must not be compiled with /arch:AVX2, the AVX2 decoder is in QFSDecoderAVX2.cpp.

uint32_t __cdecl QFSDecoder::DecodeScalar(const uint8_t* input, int* consumed, uint8_t* output)
{
	if (!input)
	{
		if (consumed) *consumed = 0;
		return 0;
	}

	const uint8_t* source = input + 2;
	if (*input & 1) source = input + 5;

	const uint32_t outputSize =
		(uint32_t(source[0]) << 16) |
		(uint32_t(source[1]) << 8) |
		uint32_t(source[2]);
	source += 3;
	uint8_t* destination = output;

	for (;;)
	{
		const uint8_t code = *source;
		const uint8_t* next = source + 1;

		if (!(code & 0x80))
		{
			const uint8_t b2 = source[1];
			const uint32_t literals = code & 3;
			for (uint32_t i = 0; i != literals; ++i) *destination++ = source[i + 2];
			source += literals + 2;
			uint8_t* back = destination - 1 - (uint32_t(b2) + (code & 0x60) * 8);
			uint8_t* end = destination + ((code & 0x1c) >> 2) + 3;
			while (destination != end) *destination++ = *back++;
			continue;
		}

		if (!(code & 0x40))
		{
			const uint8_t b2 = source[1];
			const uint8_t b3 = source[2];
			const uint32_t literals = b2 >> 6;
			for (uint32_t i = 0; i != literals; ++i) *destination++ = source[i + 3];
			source += literals + 3;
			uint8_t* back = destination - 1 - ((uint32_t(b2 & 0x3f) << 8) + b3);
			uint8_t* end = destination + (code & 0x3f) + 4;
			while (destination != end) *destination++ = *back++;
			continue;
		}

		if (!(code & 0x20))
		{
			const uint8_t b2 = source[1];
			const uint8_t b3 = source[2];
			const uint8_t b4 = source[3];
			const uint32_t literals = code & 3;
			for (uint32_t i = 0; i != literals; ++i) *destination++ = source[i + 4];
			source += literals + 4;
			uint8_t* back = destination - 1 -
				(((uint32_t(code & 0x10) >> 4) << 16) + (uint32_t(b2) << 8) + b3);
			uint8_t* end = destination + ((code & 0x0c) * 0x40) + 5 + b4;
			while (destination != end) *destination++ = *back++;
			continue;
		}

		const uint32_t literalCount = (code & 0x1f) * 4 + 4;
		if (literalCount > 0x70)
		{
			for (uint32_t i = 0; i != (code & 3); ++i) *destination++ = *next++;
			source = next;
			break;
		}

		for (uint32_t i = 0; i != literalCount; ++i) *destination++ = next[i];
		source = next + literalCount;
	}

	if (consumed) *consumed = static_cast<int>(source - input);
	return outputSize;
}

bool QFSDecoder::CpuSupportsAVX2()
{
	// /arch:AVX2 allows the compiler to use any of the Haswell instructions, so we check
	// for the same features as Intel's 4th generation Core detection sample.
	constexpr int FMA = 1 << 12;
	constexpr int MOVBE = 1 << 22;
	constexpr int OSXSAVE = 1 << 27;
	constexpr int AVX = 1 << 28;
	constexpr int F16C = 1 << 29;
	constexpr int Leaf1Features = FMA | MOVBE | OSXSAVE | AVX | F16C;

	constexpr int BMI1 = 1 << 3;
	constexpr int AVX2 = 1 << 5;
	constexpr int BMI2 = 1 << 8;
	constexpr int Leaf7Features = BMI1 | AVX2 | BMI2;

	constexpr int LZCNT = 1 << 5;

	// The OS has to save the XMM and YMM registers.
	constexpr unsigned long long XCR0SSEAndAVXState = 0x6;

	int cpuInfo[4] = {};

	__cpuid(cpuInfo, 0);
	if (cpuInfo[0] < 7)
	{
		return false;
	}

	__cpuid(cpuInfo, 1);
	if ((cpuInfo[2] & Leaf1Features) != Leaf1Features
		|| (_xgetbv(0) & XCR0SSEAndAVXState) != XCR0SSEAndAVXState)
	{
		return false;
	}

	__cpuidex(cpuInfo, 7, 0);
	if ((cpuInfo[1] & Leaf7Features) != Leaf7Features)
	{
		return false;
	}

	__cpuid(cpuInfo, static_cast<int>(0x80000000));
	if (static_cast<uint32_t>(cpuInfo[0]) < 0x80000001)
	{
		return false;
	}

	__cpuid(cpuInfo, static_cast<int>(0x80000001));
	return (cpuInfo[2] & LZCNT) != 0;
}
