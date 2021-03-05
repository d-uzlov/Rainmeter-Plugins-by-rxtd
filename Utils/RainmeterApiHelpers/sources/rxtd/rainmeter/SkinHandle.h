// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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
