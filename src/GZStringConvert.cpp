#include "GZStringConvert.h"
#include <Windows.h>

namespace
{
	void ThrowExceptionForWin32Error(const char* win32MethodName, DWORD error)
	{
		char buffer[1024]{};

		std::snprintf(
			buffer,
			sizeof(buffer),
			"%s failed with error code %u.",
			win32MethodName,
			error);

		throw std::runtime_error(buffer);
	}
}

cRZBaseString GZStringConvert::FromUtf16(const std::wstring& str)
{
	cRZBaseString result;

	const wchar_t* const utf16Chars = str.data();
	const int utf16Length = static_cast<int>(str.length());

	if (utf16Length > 0)
	{
		const int utf8Length = WideCharToMultiByte(
			CP_UTF8,
			0,
			utf16Chars,
			utf16Length,
			nullptr,
			0,
			nullptr,
			nullptr);

		if (utf8Length == 0)
		{
			DWORD lastError = GetLastError();
			ThrowExceptionForWin32Error("WideCharToMultiByte", lastError);
		}

		result.Resize(static_cast<uint32_t>(utf16Length));

		const int convertResult = WideCharToMultiByte(
			CP_UTF8,
			0,
			utf16Chars,
			utf16Length,
			result.Data(),
			utf8Length,
			nullptr,
			nullptr);

		if (convertResult == 0)
		{
			DWORD lastError = GetLastError();
			ThrowExceptionForWin32Error("WideCharToMultiByte", lastError);
		}
	}

	return result;
}

cRZBaseString GZStringConvert::FromFileSystemPath(const std::filesystem::path& path)
{
	return FromUtf16(path.native());
}

std::wstring GZStringConvert::ToUtf16(const cIGZString& str)
{
	std::wstring result;

	const char* const utf8Chars = str.Data();
	const int utf8Length = static_cast<int>(str.Strlen());

	if (utf8Length > 0)
	{
		const int utf16Length = MultiByteToWideChar(
			CP_UTF8,
			0,
			utf8Chars,
			utf8Length,
			nullptr,
			0);

		if (utf16Length == 0)
		{
			DWORD lastError = GetLastError();
			ThrowExceptionForWin32Error("MultiByteToWideChar", lastError);
		}

		result.resize(static_cast<size_t>(utf16Length), L'\0');

		const int convertResult = MultiByteToWideChar(
			CP_UTF8,
			0,
			utf8Chars,
			utf8Length,
			result.data(),
			utf16Length);

		if (convertResult == 0)
		{
			DWORD lastError = GetLastError();
			ThrowExceptionForWin32Error("MultiByteToWideChar", lastError);
		}
	}

	return result;
}

std::filesystem::path GZStringConvert::ToFileSystemPath(const cIGZString& str)
{
	std::wstring utf16Str = ToUtf16(str);
	std::filesystem::path path(std::move(utf16Str));

	return path;
}
