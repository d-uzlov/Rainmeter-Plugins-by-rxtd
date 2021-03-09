// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

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

std::vector<std::pair<SubstringViewInfo, SubstringViewInfo>>
Tokenizer::parseSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t optionDelimiter) {
	index begin = 0;
	index level = 0;
	std::vector<std::pair<SubstringViewInfo, SubstringViewInfo>> result;

	while (true) {

		while (true) {
			begin = view.find_first_not_of(L" \t", begin);
			if (begin == sview::npos) {
				return result;
			}
			if (view[begin] == optionDelimiter) {
				begin++;
			} else {
				break;
			}
		}

		wchar_t nameEndSymbols[] = { L' ', L'\t', optionBegin, optionEnd, optionDelimiter, L'\0' };
		index nameEnd = view.find_first_of(nameEndSymbols, begin);

		if (nameEnd == sview::npos) {
			result.emplace_back(SubstringViewInfo{ begin, view.length() - begin }, SubstringViewInfo{});
			return result;
		}

		if (view[nameEnd] == optionEnd) {
			return {};
		}
		const SubstringViewInfo nameInfo = { begin, nameEnd - begin };

		begin = nameEnd;

		wchar_t argSymbols[] = { optionBegin, optionEnd, optionDelimiter, L'\0' };
		const index nextPos = view.find_first_of(argSymbols, begin);
		if (nextPos == sview::npos) {
			result.emplace_back(nameInfo, SubstringViewInfo{});
			return result;
		}

		if (view[nextPos] == optionDelimiter) {
			result.emplace_back(nameInfo, SubstringViewInfo{});
			begin = nextPos;
			continue;
		}

		SubstringViewInfo argInfo;
		if (view[nextPos] == optionBegin) {
			level++;
			const index argBegin = nextPos + 1;
			begin = argBegin;
			while (level > 0) {
				wchar_t optionBoundSymbols[] = { optionBegin, optionEnd, L'\0' };
				const index boundPos = view.find_first_of(optionBoundSymbols, begin);
				if (boundPos == sview::npos) {
					return {};
				}

				if (view[boundPos] == optionBegin) {
					level++;
				}

				if (view[boundPos] == optionEnd) {
					level--;
				}

				begin = boundPos + 1;
			}

			argInfo = StringUtils::trimInfo(view, { argBegin, begin - argBegin - 1 });
		}

		result.push_back({ nameInfo, argInfo });

		wchar_t afterArgSymbols[] = { L' ', L'\t', L'\0' };
		begin = view.find_first_not_of(afterArgSymbols, begin);
		if (begin == sview::npos) {
			return result;
		}
		if (view[begin] != optionDelimiter) {
			return {};
		}
		begin++;
	}
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
