// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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
