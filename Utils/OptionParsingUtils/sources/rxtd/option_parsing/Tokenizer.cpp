// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019 Danil Uzlov

#include "Tokenizer.h"

#include "OptionParser.h"

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

std::vector<std::tuple<SubstringViewInfo, SubstringViewInfo, SubstringViewInfo>>
Tokenizer::parseSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t optionDelimiter, bool allowPostfix, const Logger& cl) {
	index begin = 0;
	index level = 0;
	std::vector<std::tuple<SubstringViewInfo, SubstringViewInfo, SubstringViewInfo>> result;

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
			result.push_back({ SubstringViewInfo{ begin, view.length() - begin }, SubstringViewInfo{}, SubstringViewInfo{} });
			return result;
		}

		const SubstringViewInfo nameInfo = { begin, nameEnd - begin };

		begin = view.find_first_not_of(L" \t", nameEnd);
		SubstringViewInfo argInfo;
		if (view[begin] == optionEnd) {
			cl.error(L"unexpected closing delimiter '{}', after '{}', before '{}'", optionEnd, view.substr(0, begin), view.substr(begin));
			throw OptionParser::Exception{};
		} else if (view[begin] == optionDelimiter) {
			result.push_back({ nameInfo, SubstringViewInfo{}, SubstringViewInfo{} });
			continue;
		} else if (view[begin] == optionBegin) {
			level++;
			const index argBegin = begin + 1;
			begin = argBegin;
			while (level > 0) {
				wchar_t optionBoundSymbols[] = { optionBegin, optionEnd, L'\0' };
				const index boundPos = view.find_first_of(optionBoundSymbols, begin);
				if (boundPos == sview::npos) {
					cl.error(L"unexpected end of string: {}", view);
					throw OptionParser::Exception{};
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
		} else {
			cl.error(L"unexpected symbol '{}', after '{}', before '{}'", view[begin], view.substr(0, begin), view.substr(begin));
			throw OptionParser::Exception{};
		}

		if (allowPostfix) {
			wchar_t afterArgSymbols[] = { optionBegin, optionEnd, optionDelimiter, L'\0' };
			auto delimPos = view.find_first_of(afterArgSymbols, begin);

			if (delimPos == sview::npos) {
				auto postfixInfo = StringUtils::trimInfo(view, { begin, view.length() - begin });
				result.push_back({ nameInfo, argInfo, postfixInfo });
				return result;
			}
			
			auto postfixInfo = StringUtils::trimInfo(view, { begin, delimPos - begin });
			result.push_back({ nameInfo, argInfo, postfixInfo });
			if (view[delimPos] != optionDelimiter) {
				cl.error(
					L"unexpected symbol '{}': expected end of string or option delimiter '{}', in place: after '{}', before '{}'",
					view[delimPos],
					optionDelimiter,
					view.substr(0, delimPos),
					view.substr(delimPos)
				);
				throw OptionParser::Exception{};
			}

			begin = delimPos + 1;
		} else {
			result.push_back({ nameInfo, argInfo, {} });

			wchar_t afterArgSymbols[] = { L' ', L'\t', L'\0' };
			begin = view.find_first_not_of(afterArgSymbols, begin);
			if (begin == sview::npos) {
				return result;
			}
			if (view[begin] != optionDelimiter) {
				cl.error(
					L"unexpected symbol '{}': expected end of string or option delimiter '{}', in place: after '{}', before '{}'",
					view[begin],
					optionDelimiter,
					view.substr(0, begin),
					view.substr(begin)
				);
				throw OptionParser::Exception{};
			}

			begin++;
		}

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
