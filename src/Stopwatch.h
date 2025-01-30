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

#include <stdint.h>

class Stopwatch
{
public:

	Stopwatch() noexcept;

	int64_t ElapsedMilliseconds() const;

	int64_t ElapsedSeconds() const;

	int64_t ElapsedMinutes() const;

	bool IsRunning() const;

	void Reset();

	void Restart();

	void Start();

	void Stop();

private:

	int64_t GetElapsedTicks() const;

	const int64_t tickFrequency;
	int64_t elapsed;
	int64_t startTimeStamp;
	bool isRunning;
};

