/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "OptionBase.h"
#include "Option.h"

namespace rxtd::utils {
	class OptionMap : public OptionBase<OptionMap> {
	public:
		struct MapOptionInfo {
			sview view{};
			mutable bool touched = false;

			MapOptionInfo() = default;

			MapOptionInfo(sview value) : view(value) { }
		};

	private:
		wchar_t optionDelimiter{};
		wchar_t nameDelimiter{};

		std::map<isview, MapOptionInfo> params{};

	public:
		OptionMap() = default;
		OptionMap(sview view, wchar_t optionDelimiter, wchar_t nameDelimiter);
		OptionMap(std::vector<wchar_t>&& source, wchar_t optionDelimiter, wchar_t nameDelimiter);

		[[nodiscard]]
		GhostOption get(isview name) const &;

		[[nodiscard]]
		Option get(isview name) const && {
			return get(name);
		}

		// returns true if option with such name exists, even if it is empty
		[[nodiscard]]
		bool has(isview name) const;

		[[nodiscard]]
		std::vector<isview> getListOfUntouched() const;

		OptionMap& own() {
			OptionBase::own();
			parseParams();
			return *this;
		}

	private:
		void parseParams();

		[[nodiscard]]
		const MapOptionInfo* find(isview name) const;
	};
}
