/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	class FileWrapper {
		void *fileHandle { };

	public:
		FileWrapper(const wchar_t* path);

		FileWrapper(FileWrapper&& other) = delete;
		FileWrapper& operator=(FileWrapper&& other) noexcept = delete;
		FileWrapper(const FileWrapper& other) = delete;
		FileWrapper& operator=(const FileWrapper& other) = delete;

		~FileWrapper();

		bool isValid() const;
		void write(const void *data, index count);

		static string getAbsolutePath(string folder, string currentPath);

		static void createDirectories(sview path);

	private:
		void close();
	};
}
