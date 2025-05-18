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

#include "Logger.h"
#include <Windows.h>

namespace
{
#ifdef _DEBUG
	void PrintLineToDebugOutput(const char* line)
	{
		OutputDebugStringA(line);
		OutputDebugStringA("\n");
	}
#endif // _DEBUG
}

Logger& Logger::GetInstance()
{
	static Logger logger;

	return logger;
}

Logger::Logger()
	: initialized(false),
	  logFile(),
	  logLevel(LogLevel::Error)
{
}

Logger::~Logger()
{
	initialized = false;
}

void Logger::Init(std::filesystem::path logFilePath, LogLevel level)
{
	if (!initialized)
	{
		initialized = true;

		// Open the log file in binary mode to allow UTF-8 text to be written without modification.
		// UTF-8 is the native encoding of SC4.
		logFile.open(logFilePath, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
		logLevel = level;
	}
}

bool Logger::IsEnabled(LogLevel level) const
{
	return logLevel >= level;
}

void Logger::WriteLogFileHeader(const char* const text)
{
	if (initialized && logFile)
	{
		logFile << text << std::endl;
	}
}

void Logger::WriteLine(LogLevel level, const char* const message)
{
	if (!IsEnabled(level))
	{
		return;
	}

	WriteLineCore(message);
}

void Logger::WriteLineFormatted(LogLevel level, const char* const format, ...)
{
	if (!IsEnabled(level))
	{
		return;
	}

	va_list args;
	va_start(args, format);

	va_list argsCopy;
	va_copy(argsCopy, args);

	int formattedStringLength = std::vsnprintf(nullptr, 0, format, argsCopy);

	va_end(argsCopy);

	if (formattedStringLength > 0)
	{
		size_t formattedStringLengthWithNull = static_cast<size_t>(formattedStringLength) + 1;

		constexpr size_t stackBufferSize = 1024;

		if (formattedStringLengthWithNull >= stackBufferSize)
		{
			std::unique_ptr<char[]> buffer = std::make_unique_for_overwrite<char[]>(formattedStringLengthWithNull);

			std::vsnprintf(buffer.get(), formattedStringLengthWithNull, format, args);

			WriteLineCore(buffer.get());
		}
		else
		{
			char buffer[stackBufferSize]{};

			std::vsnprintf(buffer, stackBufferSize, format, args);

			WriteLineCore(buffer);
		}
	}

	va_end(args);
}

void Logger::WriteLineCore(const char* const message)
{
	if (initialized && logFile)
	{
#ifdef _DEBUG
		PrintLineToDebugOutput(message);
#endif // _DEBUG

		logFile << message << '\n';
	}
}