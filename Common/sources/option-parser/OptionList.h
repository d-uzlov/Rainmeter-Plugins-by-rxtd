/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "StringUtils.h"
#include "AbstractOption.h"
#include "Option.h"

namespace rxtd::utils {
	// List of string.
	class OptionList : public AbstractOption<OptionList> {
		std::vector<SubstringViewInfo> list;

	public:
		OptionList() = default;

		OptionList(sview view, std::vector<wchar_t>&& source, std::vector<SubstringViewInfo>&& list) :
			AbstractOption(view, std::move(source)),
			list(std::move(list)) {
		}

		// Allows you to steal inner resources.
		std::pair<sview, std::vector<SubstringViewInfo>> consume() && {
			return { getView(), std::move(list) };
		}

		// Count of elements in list.
		index size() const {
			return list.size();
		}

		// Alias to "size() == 0".
		bool empty() const {
			return list.empty();
		}

		// Parseable view of Nth option.
		GhostOption get(index ind) const & {
			return GhostOption { list[ind].makeView(getView()) };
		}

		Option get(index ind) const && {
			return get(ind);
		}

		class ghost_iterator {
			const OptionList& container;
			index ind;

		public:
			ghost_iterator(const OptionList& container, index _index) :
				container(container),
				ind(_index) {
			}

			ghost_iterator& operator++() {
				ind++;
				return *this;
			}

			bool operator !=(const ghost_iterator& other) const {
				return &container != &other.container || ind != other.ind;
			}

			GhostOption operator*() const {
				return container.get(ind);
			}
		};

		ghost_iterator begin() const & {
			return { *this, 0 };
		}

		ghost_iterator end() const & {
			return { *this, size() };
		}

		class iterator {
			const OptionList& container;
			index ind;

		public:
			iterator(const OptionList& container, index _index) :
				container(container),
				ind(_index) {
			}

			iterator& operator++() {
				ind++;
				return *this;
			}

			bool operator !=(const iterator& other) const {
				return &container != &other.container || ind != other.ind;
			}

			Option operator*() const {
				return container.get(ind);
			}
		};

		iterator begin() const && {
			return { *this, 0 };
		}

		iterator end() const && {
			return { *this, size() };
		}
	};
}
