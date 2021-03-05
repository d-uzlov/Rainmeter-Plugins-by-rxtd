// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Danil Uzlov

#pragma once

namespace rxtd::rainmeter {
	/**
	 * Wrapper over a rainmeter data handler.
	 * Used for type safety only.
	 */
	class DataHandle {
		friend class Rainmeter;

		void* handle{};

	public:
		DataHandle() = default;

	private:
		explicit DataHandle(void* handle) : handle(handle) { }

	public:
		[[nodiscard]]
		void* getRawHandle() const {
			return handle;
		}
	};
}
