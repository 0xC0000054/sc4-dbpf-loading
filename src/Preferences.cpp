/*
 * This file is part of sc4-dbpf-loading, a DLL Plugin for SimCity 4 that
 * optimizes the DBPF loading.
 *
 * Copyright (C) 2024, 2025, 2026 Nicholas Hayes
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

#include "Preferences.h"
#include "FileSystem.h"
#include "IniReader.h"
#include "Logger.h"
#include <fstream>

Preferences& Preferences::GetInstance()
{
	static Preferences preferences;

	return preferences;
}

Preferences::Preferences()
	: largeWritePatch(false)
{
	Load();
}

void Preferences::Load()
{
	try
	{
		std::ifstream stream(FileSystem::GetDllIniFilePath(), std::ifstream::in);

		if (stream)
		{
			IniReader iniReader(stream);

			const auto settingsSectionOptional = iniReader.get_section_optional("Settings");

			if (settingsSectionOptional.has_value())
			{
				const IniSection& section = settingsSectionOptional.value();

				largeWritePatch = section.get_converted_value<bool>("LargeWritePatch", false);
			}
		}
	}
	catch (const std::exception& e)
	{
		Logger::GetInstance().WriteLineFormatted(
			LogLevel::Error,
			"Error reading the preferences file: %s",
			e.what());
	}
}

bool Preferences::LargeWritePatch() const
{
    return largeWritePatch;
}
