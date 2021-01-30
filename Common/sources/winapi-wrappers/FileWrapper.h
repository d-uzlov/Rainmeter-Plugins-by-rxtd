/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	//
	// Kinda useless class that provides RAII for Win32 file API
	// Was created to use windows-specific file options.
	// TODO: remove
	//
	class FileWrapper : NonMovableBase {
		void* fileHandle{};

	public:
		FileWrapper(const wchar_t* path);

		~FileWrapper();

		[[nodiscard]]
		bool isValid() const;
		void write(const void* data, index count);

		[[nodiscard]]
		static string getAbsolutePath(string folder, sview currentPath);

		static void createDirectories(string path);

	private:
		void close();
	};
}
