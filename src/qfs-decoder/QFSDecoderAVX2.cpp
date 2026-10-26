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
#include <cstring>
#include <immintrin.h>

// This file is compiled with /arch:AVX2. QFSDecoder::CpuSupportsAVX2 must be checked before
// calling anything in it, and it should not include headers with inline functions that the
// rest of the DLL also uses, because the linker can pick the AVX2 copy of those.
//
// Most of the FSH matches in the plugins we profiled are 3 to 15 bytes, so short matches are
// copied inline and only matches over 31 bytes go through CopyLongMatchAVX2.

namespace
{
	__declspec(noinline) void CopyForwardAVX2(
		uint8_t* destination,
		const uint8_t* source,
		size_t count) noexcept
	{
		while (count >= 32)
		{
			const __m256i value = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(source));
			_mm256_storeu_si256(reinterpret_cast<__m256i*>(destination), value);
			source += 32;
			destination += 32;
			count -= 32;
		}

		if (count >= 16)
		{
			const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source));
			_mm_storeu_si128(reinterpret_cast<__m128i*>(destination), value);
			source += 16;
			destination += 16;
			count -= 16;
		}

		if (count) std::memcpy(destination, source, count);
	}

	__declspec(noinline) void CopyLongMatchAVX2(
		uint8_t* destination,
		const uint8_t* source,
		size_t count,
		size_t distance) noexcept
	{
		if (distance >= 32)
		{
			CopyForwardAVX2(destination, source, count);
			return;
		}

		size_t seeded = 0;
		while (seeded < count && seeded < distance)
		{
			destination[seeded] = source[seeded];
			++seeded;
		}

		size_t copied = seeded;
		while (copied < count)
		{
			const size_t chunk = (copied < count - copied) ? copied : count - copied;
			std::memcpy(destination + copied, destination, chunk);
			copied += chunk;
		}
	}

	__forceinline void CopyLiterals0To3(
		uint8_t*& destination,
		const uint8_t*& source,
		size_t count) noexcept
	{
		switch (count)
		{
		case 3: destination[2] = source[2]; [[fallthrough]];
		case 2: destination[1] = source[1]; [[fallthrough]];
		case 1: destination[0] = source[0];
		default: break;
		}
		destination += count;
		source += count;
	}

	__forceinline void CopyShortNonOverlap(
		uint8_t* destination,
		const uint8_t* source,
		size_t count) noexcept
	{
		while (count >= 16)
		{
			const __m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source));
			_mm_storeu_si128(reinterpret_cast<__m128i*>(destination), value);
			source += 16;
			destination += 16;
			count -= 16;
		}

		if (count >= 8)
		{
			const __m128i value = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(source));
			_mm_storel_epi64(reinterpret_cast<__m128i*>(destination), value);
			source += 8;
			destination += 8;
			count -= 8;
		}

		if (count >= 4)
		{
			uint32_t value;
			std::memcpy(&value, source, sizeof(value));
			std::memcpy(destination, &value, sizeof(value));
			source += 4;
			destination += 4;
			count -= 4;
		}

		switch (count)
		{
		case 3: destination[2] = source[2]; [[fallthrough]];
		case 2: destination[1] = source[1]; [[fallthrough]];
		case 1: destination[0] = source[0];
		default: break;
		}
	}

	__forceinline void CopyMatch(
		uint8_t* destination,
		const uint8_t* source,
		size_t count,
		size_t distance) noexcept
	{
		if (count <= 31)
		{
			if (distance >= count)
			{
				CopyShortNonOverlap(destination, source, count);
			}
			else
			{
				while (count--) *destination++ = *source++;
			}
			return;
		}

		CopyLongMatchAVX2(destination, source, count, distance);
	}
}

uint32_t __cdecl QFSDecoder::DecodeAVX2(const uint8_t* input, int* consumed, uint8_t* output)
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
		const uint8_t code = *source++;

		if (!(code & 0x80))
		{
			const uint8_t b2 = *source++;
			const size_t literals = code & 3;
			CopyLiterals0To3(destination, source, literals);
			const size_t length = ((code & 0x1c) >> 2) + 3;
			const size_t distance = size_t(b2) + (code & 0x60) * 8 + 1;
			CopyMatch(destination, destination - distance, length, distance);
			destination += length;
			continue;
		}

		if (!(code & 0x40))
		{
			const uint8_t b2 = *source++;
			const uint8_t b3 = *source++;
			const size_t literals = b2 >> 6;
			CopyLiterals0To3(destination, source, literals);
			const size_t length = (code & 0x3f) + 4;
			const size_t distance = ((size_t(b2 & 0x3f) << 8) | b3) + 1;
			CopyMatch(destination, destination - distance, length, distance);
			destination += length;
			continue;
		}

		if (!(code & 0x20))
		{
			const uint8_t b2 = *source++;
			const uint8_t b3 = *source++;
			const uint8_t b4 = *source++;
			const size_t literals = code & 3;
			CopyLiterals0To3(destination, source, literals);
			const size_t length = ((code & 0x0c) << 6) + b4 + 5;
			const size_t distance =
				((((size_t(code & 0x10) >> 4) << 16) | (size_t(b2) << 8) | b3) + 1);
			CopyMatch(destination, destination - distance, length, distance);
			destination += length;
			continue;
		}

		const size_t literals = (code & 0x1f) * 4 + 4;
		if (literals > 0x70)
		{
			const size_t tail = code & 3;
			CopyLiterals0To3(destination, source, tail);
			break;
		}

		std::memcpy(destination, source, literals);
		destination += literals;
		source += literals;
	}

	if (consumed) *consumed = static_cast<int>(source - input);
	return outputSize;
}
