/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <cstdint>
#include <string>

#pragma warning(disable : 6215)
#pragma warning(disable : 6217)

namespace rxu {
	class FileWrapper {
		void *fileHandle { };
		bool valid = true;

	public:
		FileWrapper(const wchar_t* path);

		FileWrapper(FileWrapper&& other) = delete;
		FileWrapper& operator=(FileWrapper&& other) noexcept = delete;
		FileWrapper(const FileWrapper& other) = delete;
		FileWrapper& operator=(const FileWrapper& other) = delete;

		~FileWrapper();

		bool isValid() const;
		void write(uint8_t *data, size_t count);

		static void createDirectories(std::wstring_view path);

	};
}
