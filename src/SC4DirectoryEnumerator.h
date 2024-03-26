#pragma once
#include "cRZBaseString.h"
#include <filesystem>
#include <vector>

enum class SC4DirectoryEnumeratorOptions : uint32_t
{
	// The selected directory and all sub-directories will be searched,
	// .SC4* files will be included.
	None = 0,
	// The selected directory will be searched, sub-directories are ignored.
	TopDirectoryOnly = 1 << 0,
	// Only .DAT files wil be included, .SC4* files are ignored.
	OnlyDatFiles = 1 << 1
};

inline SC4DirectoryEnumeratorOptions operator|(SC4DirectoryEnumeratorOptions lhs, SC4DirectoryEnumeratorOptions rhs)
{
	return static_cast<SC4DirectoryEnumeratorOptions>(
		static_cast<std::underlying_type<SC4DirectoryEnumeratorOptions>::type>(lhs) |
		static_cast<std::underlying_type<SC4DirectoryEnumeratorOptions>::type>(rhs)
		);
}

inline SC4DirectoryEnumeratorOptions operator&(SC4DirectoryEnumeratorOptions lhs, SC4DirectoryEnumeratorOptions rhs)
{
	return static_cast<SC4DirectoryEnumeratorOptions>(
		static_cast<std::underlying_type<SC4DirectoryEnumeratorOptions>::type>(lhs) &
		static_cast<std::underlying_type<SC4DirectoryEnumeratorOptions>::type>(rhs)
		);
}

class SC4DirectoryEnumerator
{
public:
	SC4DirectoryEnumerator(
		const std::filesystem::path& path,
		SC4DirectoryEnumeratorOptions options = SC4DirectoryEnumeratorOptions::None);

	SC4DirectoryEnumerator(
		const cIGZString& path,
		SC4DirectoryEnumeratorOptions options = SC4DirectoryEnumeratorOptions::None);

	const std::vector<cRZBaseString>& DatFiles() const;
	const std::vector<cRZBaseString>& Sc4Files() const;

private:

	void Init(const std::filesystem::path& path, SC4DirectoryEnumeratorOptions options);

	void ScanDirectory(const std::filesystem::path& path);
	void ScanDirectoryForDatFiles(const std::filesystem::path& path);
	void ScanDirectoryRecursive(const std::filesystem::path& root);
	void ScanDirectoryForDatFilesRecursive(const std::filesystem::path& root);

	std::vector<cRZBaseString> datFiles;
	std::vector<cRZBaseString> sc4Files;
};