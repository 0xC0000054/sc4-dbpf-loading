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

#include "QFSDecoderHooks.h"
#include "Logger.h"
#include "QFSDecoder.h"
#include <cstring>
#include <exception>

#define NOMINMAX

#include <Windows.h>
#include "detours/detours.h"

namespace
{
	constexpr uintptr_t DecodeRefAddress = 0x00A6DA3F;

	QFSDecoder::DecodeFunction RealDecodeRef = reinterpret_cast<QFSDecoder::DecodeFunction>(DecodeRefAddress);
}

void QFSDecoderHooks::Install()
{
	Logger& logger = Logger::GetInstance();

	try
	{
		static constexpr uint8_t ExpectedPrologue[] = { 0x55, 0x8B, 0xEC, 0x51, 0x51, 0x8B };

		if (std::memcmp(reinterpret_cast<const void*>(DecodeRefAddress), ExpectedPrologue, sizeof(ExpectedPrologue)) != 0)
		{
			logger.WriteLine(LogLevel::Error, "Failed to install the QFS decoder hook: unexpected function prologue.");
			return;
		}

		const bool useAVX2 = QFSDecoder::CpuSupportsAVX2();

		DetourRestoreAfterWith();

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(
			&(PVOID&)RealDecodeRef,
			useAVX2 ? QFSDecoder::DecodeAVX2 : QFSDecoder::DecodeScalar);

		LONG error = DetourTransactionCommit();

		if (error == NO_ERROR)
		{
			logger.WriteLine(
				LogLevel::Info,
				useAVX2 ? "Installed the AVX2 QFS decoder." : "Installed the scalar QFS decoder.");
		}
		else
		{
			logger.WriteLineFormatted(
				LogLevel::Error,
				"Failed to install the QFS decoder hook, error code=%d",
				error);
		}
	}
	catch (const std::exception& e)
	{
		logger.WriteLineFormatted(
			LogLevel::Error,
			"Failed to install the QFS decoder hook: %s",
			e.what());
	}
}
