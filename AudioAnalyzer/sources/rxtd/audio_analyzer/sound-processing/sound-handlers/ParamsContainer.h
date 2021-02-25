/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::audio_analyzer::handler {
	class ParamsContainer {
		std::any erasedParams;

	public:
		template<typename T>
		T& clear() {
			erasedParams = T{};
			return cast<T>();
		}

		template<typename T>
		T& cast() {
			return *std::any_cast<T>(&erasedParams);
		}

		template<typename T>
		const T& cast() const {
			return *std::any_cast<T>(&erasedParams);
		}

		// prevent automatic type conversions
		ParamsContainer() = default;
	};
}
