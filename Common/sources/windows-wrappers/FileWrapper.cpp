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
#include <filesystem>

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
}

FileWrapper::~FileWrapper() {
	close();
}

bool FileWrapper::isValid() const {
	return fileHandle != INVALID_HANDLE_VALUE;
}

void FileWrapper::write(const void *data, index count) {
	if (!isValid()) {
		return;
	}

	DWORD bytesWritten;
	const bool success = WriteFile(fileHandle, data,
		DWORD(count),  // number of bytes to write
		&bytesWritten, nullptr);

	if (!success || index(bytesWritten) != count) {
		close();
	}
}

string FileWrapper::getAbsolutePath(string folder, sview currentPath) {
	std::filesystem::path path { folder };
	if (!path.is_absolute()) {
		folder = currentPath;
		folder += folder;
	}

	folder = std::filesystem::absolute(folder).wstring();
	folder = LR"(\\?\)" + folder;

	if (folder[folder.size() - 1] != L'\\') {
		folder += L'\\';
	}

	return folder;
}

void FileWrapper::createDirectories(string path) {
	auto pos = path.find(L':') + 1; // either npos+1 == 0 or index of first meaningful symbol

	while (true) {
		const auto nextPos = path.find(L'\\', pos);

		if (nextPos == string::npos) {
			break;
		}

		path[nextPos] = L'\0';
		CreateDirectoryW(path.c_str(), nullptr);
		path[nextPos] = L'\\';

		pos = nextPos + 1;
	}
}

void FileWrapper::close() {
	if (fileHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(fileHandle);
		fileHandle = INVALID_HANDLE_VALUE;
	}
}
