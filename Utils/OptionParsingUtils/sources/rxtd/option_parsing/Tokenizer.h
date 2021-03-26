// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#pragma once
#include "rxtd/Logger.h"
#include "rxtd/std_fixes/StringUtils.h"

namespace rxtd::option_parsing {
	class Tokenizer {
	public:
		using SubstringViewInfo = std_fixes::SubstringViewInfo;

		[[nodiscard]]
		static std::vector<SubstringViewInfo> parse(sview view, wchar_t delimiter);

		// can throw OptionParser::Exception
		[[nodiscard]]
		static std::vector<std::tuple<SubstringViewInfo, SubstringViewInfo, SubstringViewInfo>>
		parseSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t optionDelimiter, bool allowPostfix, const Logger& cl);

	private:
		static void emitToken(std::vector<SubstringViewInfo>& list, index begin, index end);

		static void tokenize(std::vector<SubstringViewInfo>& list, sview string, wchar_t delimiter);

		static void trimSpaces(std::vector<SubstringViewInfo>& list, sview string);
	};
}
