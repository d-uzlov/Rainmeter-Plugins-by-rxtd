// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "Option.h"
#include "OptionBase.h"

namespace rxtd::option_parsing {
	//
	// Parses its contents in the form:
	// <name><nameDelimiter><value> <optionDelimiter> <name><nameDelimiter><value> <optionDelimiter> ...
	//
	class OptionMap : public OptionBase {
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
		OptionMap(SourceType&& source, wchar_t optionDelimiter, wchar_t nameDelimiter);

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
