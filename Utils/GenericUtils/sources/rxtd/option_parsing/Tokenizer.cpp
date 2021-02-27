/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Tokenizer.h"

using rxtd::option_parsing::Tokenizer;
using rxtd::std_fixes::SubstringViewInfo;
using rxtd::std_fixes::StringUtils;

std::vector<SubstringViewInfo> Tokenizer::parse(sview view, wchar_t delimiter) {
	// this method is guarantied to return non empty views

	if (view.empty()) {
		return {};
	}

	std::vector<SubstringViewInfo> tempList{};

	tokenize(tempList, view, delimiter);

	trimSpaces(tempList, view);

	return tempList;
}

std::vector<std::vector<SubstringViewInfo>>
Tokenizer::parseSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t optionDelimiter) {
	enum class State {
		eSEARCH,
		eOPTION,
		eSWALLOW,
	};
	auto state = State::eSEARCH;

	std::vector<std::vector<SubstringViewInfo>> result;
	std::vector<SubstringViewInfo> description;
	index begin = view.find_first_not_of(L" \t");
	if (begin == static_cast<index>(sview::npos)) {
		return {};
	}

	for (index i = begin; i < static_cast<index>(view.length()); ++i) {
		const auto symbol = view[i];

		if (symbol == optionBegin) {
			if (state != State::eOPTION) {
				return {};
			}
			state = State::eSWALLOW;

			const index end = i;
			emitToken(description, begin, end);
			begin = end + 1;
			continue;
		}
		if (symbol == optionEnd) {
			if (state != State::eSWALLOW) {
				return {};
			}
			state = State::eOPTION;

			const index end = i;
			emitToken(description, begin, end);
			begin = end + 1;
			continue;
		}
		if (symbol == optionDelimiter) {
			if (state != State::eOPTION) {
				continue;
			}
			const index end = i;
			emitToken(description, begin, end);
			begin = end + 1;

			trimSpaces(description, view);
			result.emplace_back(std::move(description));
			description = {};
			state = State::eSEARCH;
			continue;
		}
		if (state == State::eSEARCH) {
			// beginning of option name
			state = State::eOPTION;
			begin = i;
		}
	}
	if (state == State::eOPTION) {
		const index end = view.length();
		emitToken(description, begin, end);
	}

	if (!description.empty()) {
		trimSpaces(description, view);
		result.emplace_back(std::move(description));
	}

	return result;
}

void Tokenizer::emitToken(std::vector<SubstringViewInfo>& list, const index begin, const index end) {
	if (end <= begin) {
		return;
	}
	list.emplace_back(begin, end - begin);
}

void Tokenizer::tokenize(std::vector<SubstringViewInfo>& list, sview string, wchar_t delimiter) {
	index begin = 0;

	for (index i = 0; i < static_cast<index>(string.length()); ++i) {
		if (string[i] == delimiter) {
			const index end = i;
			emitToken(list, begin, end);
			begin = end + 1;
		}
	}

	emitToken(list, begin, string.length());
}

void Tokenizer::trimSpaces(std::vector<SubstringViewInfo>& list, sview string) {
	for (auto& view : list) {
		view = StringUtils::trimInfo(string, view);
	}

	list.erase(
		std::remove_if(
			list.begin(), list.end(), [](SubstringViewInfo svi) {
				return svi.empty();
			}
		),
		list.end()
	);
}
