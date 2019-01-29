/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "FileWrapper.h"
#include "my-windows.h"

#include "undef.h"

using namespace utils;

FileWrapper::FileWrapper(const wchar_t *path) {
	fileHandle = CreateFileW(path,
		0
		| GENERIC_WRITE
		,
		0
		| FILE_SHARE_READ
		// | FILE_SHARE_DELETE
		, nullptr, CREATE_ALWAYS,
		0
		// |FILE_ATTRIBUTE_NORMAL
		// | FILE_ATTRIBUTE_TEMPORARY
		, nullptr);
	if (fileHandle == INVALID_HANDLE_VALUE) {
		valid = false;
		return;
	}
}

FileWrapper::~FileWrapper() {
	if (fileHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(fileHandle);
		fileHandle = INVALID_HANDLE_VALUE;
	}
}

bool FileWrapper::isValid() const {
	return valid;
}

void FileWrapper::write(std::byte * data, index count) {
	if (!valid) {
		return;
	}

	DWORD bytesWritten;
	const bool success = WriteFile(fileHandle, data,
		DWORD(count),  // number of bytes to write
		&bytesWritten, nullptr);

	if (!success || index(bytesWritten) != count) {
		valid = false;
		return;
	}
}

void FileWrapper::createDirectories(sview path) {
	string buffer { path };

	auto pos = buffer.find(L':') + 1; // either npos+1 == 0 or index of first meaningful symbol

	while (true) {
		const auto nextPos = buffer.find(L'\\', pos);

		if (nextPos == string::npos) {
			break;
		}

		buffer[nextPos] = L'\0';
		CreateDirectoryW(buffer.c_str(), nullptr);
		buffer[nextPos] = L'\\';

		pos = nextPos + 1;
	}
	CreateDirectoryW(buffer.c_str(), nullptr);
}
