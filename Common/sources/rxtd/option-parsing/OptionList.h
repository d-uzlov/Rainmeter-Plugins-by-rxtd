/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "Option.h"
#include "OptionBase.h"
#include "rxtd/StringUtils.h"

namespace rxtd::common::options {
	//
	// Parses its contents in the form:
	// <value> <delimiter> <value> <delimiter> ...
	//
	class OptionList : public OptionBase {
		using SubstringViewInfo = utils::SubstringViewInfo;

		std::vector<SubstringViewInfo> list{};

	public:
		OptionList() = default;

		OptionList(sview view, std::vector<SubstringViewInfo>&& list) :
			OptionBase(view), list(std::move(list)) { }

		OptionList(SourceType&& source, std::vector<SubstringViewInfo>&& list) :
			OptionBase(std::move(source)), list(std::move(list)) { }

		// Allows you to steal inner resources.
		[[nodiscard]]
		std::pair<sview, std::vector<SubstringViewInfo>> consume() && {
			return { getView(), std::move(list) };
		}

		// Count of elements in list.
		[[nodiscard]]
		index size() const {
			return list.size();
		}

		// Alias to "size() == 0".
		[[nodiscard]]
		bool empty() const {
			return list.empty();
		}

		// Returns Nth element
		[[nodiscard]]
		GhostOption get(index ind) const & {
			if (ind >= index(list.size())) {
				return {};
			}
			return GhostOption{ list[ind].makeView(getView()) };
		}

		// Returns Nth element
		[[nodiscard]]
		Option get(index ind) const && {
			return get(ind);
		}

		template<typename T>
		class iterator {
			const OptionList& container;
			index ind;

		public:
			iterator(const OptionList& container, index _index) :
				container(container),
				ind(_index) { }

			iterator& operator++() {
				ind++;
				return *this;
			}

			bool operator !=(const iterator& other) const {
				return &container != &other.container || ind != other.ind;
			}

			[[nodiscard]]
			T operator*() const {
				return container.get(ind);
			}
		};

		[[nodiscard]]
		iterator<GhostOption> begin() const & {
			return { *this, 0 };
		}

		[[nodiscard]]
		iterator<GhostOption> end() const & {
			return { *this, size() };
		}

		[[nodiscard]]
		iterator<Option> begin() const && {
			return { *this, 0 };
		}

		[[nodiscard]]
		iterator<Option> end() const && {
			return { *this, size() };
		}
	};
}
