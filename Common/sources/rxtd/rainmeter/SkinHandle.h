/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::rainmeter {
	/**
	 * Wrapper over a rainmeter skin handler.
	 * Used for type safety only.
	 */
	class SkinHandle {
		friend class Rainmeter;

		void* handle{};

	public:
		SkinHandle() = default;
	
	private:
		explicit SkinHandle(void* handle) : handle(handle) { }

	public:
		[[nodiscard]]
		bool operator<(SkinHandle other) const {
			return handle < other.handle;
		}

		[[nodiscard]]
		void* getRawHandle() const {
			return handle;
		}
	};
}
