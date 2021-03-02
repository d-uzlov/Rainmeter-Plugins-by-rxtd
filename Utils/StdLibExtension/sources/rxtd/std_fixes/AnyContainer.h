/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::std_fixes {
	class AnyContainer {
		std::any erasedData;

	public:
		template<typename T>
		T& clear() {
			erasedData = T{};
			return cast<T>();
		}

		template<typename T>
		[[nodiscard]]
		T& cast() {
			return *std::any_cast<T>(&erasedData);
		}

		template<typename T>
		[[nodiscard]]
		const T& cast() const {
			return *std::any_cast<T>(&erasedData);
		}

		AnyContainer() = default;

		template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, AnyContainer>>>
		AnyContainer(T&& value) : erasedData(std::move(value)) {}
	};
}
