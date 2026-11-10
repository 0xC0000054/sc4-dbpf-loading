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

#include "ShellLinkHelper.h"
#include "ShObjIdl_core.h"
#include "Windows.h"
#include "wil/resource.h"

namespace
{
	bool Load(IShellLinkW* pShellLinkW, LPCOLESTR filePath)
	{
		IPersistFile* pPersistFile = nullptr;

		if (SUCCEEDED(pShellLinkW->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&pPersistFile))))
		{
			auto cleanupIPersistFile = wil::scope_exit([&]
				{
					pPersistFile->Release();
				});

			return SUCCEEDED(pPersistFile->Load(filePath, STGM_READ));
		}

		return false;
	}
}

bool ShellLinkHelper::GetTargetPath(const std::wstring& linkPath, std::wstring& linkTargetPath)
{
	if (SUCCEEDED(CoInitialize(nullptr)))
	{
		auto shutdownCOM = wil::scope_exit([&]
			{
				CoUninitialize();
			});

		IShellLinkW* pShellLinkW = nullptr;

		if (SUCCEEDED(CoCreateInstance(
			CLSID_ShellLink,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_IShellLinkW,
			reinterpret_cast<void**>(&pShellLinkW))))
		{
			auto cleanupIShellLink = wil::scope_exit([&]
				{
					pShellLinkW->Release();
				});

			if (Load(pShellLinkW, linkPath.c_str()))
			{
				if (SUCCEEDED(pShellLinkW->Resolve(nullptr, SLR_NO_UI)))
				{
					constexpr int bufferSize = MAX_PATH + 2;
					wchar_t buffer[bufferSize]{};

					// We check for S_OK instead of using the SUCCEEDED macro because
					// GetPath can return S_FALSE if the target path is an empty string.

					if (pShellLinkW->GetPath(buffer, bufferSize - 1, nullptr, 0) == S_OK)
					{
						linkTargetPath.assign(buffer);
						return true;
					}
				}
			}
		}
	}

	return false;
}
