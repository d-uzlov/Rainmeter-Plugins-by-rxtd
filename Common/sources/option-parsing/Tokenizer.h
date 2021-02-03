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

namespace rxtd::common::options {
	class Tokenizer {
	public:
		using SubstringViewInfo = utils::SubstringViewInfo;
		
		[[nodiscard]]
		static std::vector<SubstringViewInfo> parse(sview view, wchar_t delimiter);
		[[nodiscard]]
		static std::vector<std::vector<SubstringViewInfo>>
		parseSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t optionDelimiter);

	private:
		static void emitToken(std::vector<SubstringViewInfo>& list, index begin, index end);

		static void tokenize(std::vector<SubstringViewInfo>& list, sview string, wchar_t delimiter);

		static void trimSpaces(std::vector<SubstringViewInfo>& list, sview string);
	};
}
