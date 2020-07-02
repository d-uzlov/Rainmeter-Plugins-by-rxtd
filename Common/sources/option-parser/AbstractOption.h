/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	template<typename T>
	class AbstractOption {
		// View of string containing data for this option
		// There are a view and a source, because source may be empty while view points to data in some other object
		// It's a user's responsibility to manage memory for this
		sview _view;
		// ↓↓ yes, this must be a vector and not a string,
		// ↓↓ because small string optimization kills string views on move
		std::vector<wchar_t> source;

	public:
		explicit AbstractOption() = default;
		AbstractOption(sview view, std::vector<wchar_t> &&source) : _view(view), source(std::move(source)) { }

		T& own() {
			if (source.empty()) {
				source.resize(_view.size());
				std::copy(_view.begin(), _view.end(), source.begin());
				_view = { };
			}

			return static_cast<T&>(*this);
		}

	protected:
		void setView(sview view) {
			_view = view;
			source.resize(0);
		}

		sview getView() const {
			if (source.empty()) {
				return _view;
			}

			return sview { source.data(), source.size() };
		}

		std::vector<wchar_t>&& consumeSource() && {
			return std::move(source);
		}
	};
}
