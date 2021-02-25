/*
 * Copyright (C) 2020-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::common::options {
	class OptionBase {
	protected:
		using SourceType = std::vector<wchar_t>;

	private:
		// There can be two states of this object:
		// 1. Owning source. Then field source is filled and field view points to it.
		// 2. Non-owning. Then field source is empty, and field view point to some user-managed memory.

		sview view;

		// source is a vector and not a string,
		// because small string optimization kills string views on move
		SourceType source{};

	protected:
		explicit OptionBase() = default;

		OptionBase(sview view) : view(view) {}

		OptionBase(SourceType&& source) : source(std::move(source)) {
			view = makeView();
		}

	public:
		void own() {
			if (isOwningSource()) {
				return;
			}

			source.resize(view.size());
			std::copy(view.begin(), view.end(), source.begin());
			view = makeView();
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
		SourceType consumeSource() && {
			return std::move(source);
		}

	private:
		[[nodiscard]]
		sview makeView() const {
			return sview{ source.data(), source.size() };
		}
	};
}
