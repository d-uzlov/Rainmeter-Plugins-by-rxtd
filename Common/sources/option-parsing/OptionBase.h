/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	template<typename T>
	class OptionBase {
		// There can be two states of this object:
		// 1. Owning source. Then field source is filled and field view points to it.
		// 2. Non-owning. Then field source is empty, and field view point to some user-managed memory.

		sview view;

		// source is a vector and not a string,
		// because small string optimization kills string views on move
		std::vector<wchar_t> source;

	protected:
		explicit OptionBase() = default;

		OptionBase(sview view) : view(view) { }

		OptionBase(std::vector<wchar_t>&& source) : source(std::move(source)) {
			view = makeView();
		}

	public:
		T& own() {
			static_assert(std::is_base_of<OptionBase<T>, T>::value, "Descendants of OptionBase must use itself as a template parameter.");

			if (!isOwningSource()) {
				source.resize(view.size());
				std::copy(view.begin(), view.end(), source.begin());
				view = makeView();
			}

			return static_cast<T&>(*this);
		}

		[[nodiscard]]
		bool isOwningSource() const {
			return !source.empty();
		}

	protected:
		[[nodiscard]]
		sview getView() const {
			return view;
		}

		[[nodiscard]]
		std::vector<wchar_t>&& consumeSource() && {
			return std::move(source);
		}

	private:
		[[nodiscard]]
		sview makeView() const {
			return sview{ source.data(), source.size() };
		}

	};
}
