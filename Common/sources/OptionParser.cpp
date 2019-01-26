/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "OptionParser.h"
#include "MathExpressionParser.h"
#include <cwctype>

#pragma warning(disable : 4244)

std::vector<rxu::SubstringViewInfo> rxu::OptionParser::Tokenizer::parse(std::wstring_view string,
	wchar_t delimiter) {
	if (string.empty()) {
		return { };
	}

	tempList.clear();

	tokenize(string, delimiter);

	auto list = trimSpaces(string);

	return list;
}

void rxu::OptionParser::Tokenizer::emitToken(const size_t begin, const size_t end) {
	if (end <= begin) {
		return;
	}
	tempList.emplace_back(begin, end - begin);
}

void rxu::OptionParser::Tokenizer::tokenize(std::wstring_view string, wchar_t delimiter) {
	size_t begin = 0u;

	for (size_t i = 0; i < string.length(); ++i) {
		if (string[i] == delimiter) {
			const size_t end = i;
			emitToken(begin, end);
			begin = end + 1;
		}
	}

	emitToken(begin, string.length());
}

std::vector<rxu::SubstringViewInfo> rxu::OptionParser::Tokenizer::trimSpaces(std::wstring_view string) {
	std::vector<SubstringViewInfo> list { };

	for (auto& view : tempList) {
		auto trimmed = svu::trim(string, view);
		if (trimmed.empty()) {
			continue;
		}

		list.emplace_back(trimmed);
	}

	return list;
}



rxu::OptionParser::OptionList::OptionList() = default;

rxu::OptionParser::OptionList::OptionList(std::wstring_view string, std::vector<SubstringViewInfo>&& list) :
	list(std::move(list)) {
	auto data = string.data();
	
	source.resize(string.length() + 1);
	for (unsigned i = 0; i < string.size(); ++i) {
		source[i] = string[i];
	}
	source.back() = L'\0';
}

std::pair<std::vector<wchar_t>, std::vector<rxu::SubstringViewInfo>> rxu::OptionParser::OptionList::consume() && {
	return { std::move(source), std::move(list) };
}

size_t rxu::OptionParser::OptionList::size() const {
	return list.size();
}

bool rxu::OptionParser::OptionList::empty() const {
	return list.empty();
}

std::wstring_view rxu::OptionParser::OptionList::get(size_t index) const {
	return list[index].makeView(source);
}

rxu::OptionParser::Option rxu::OptionParser::OptionList::getOption(size_t index) const {
	return Option { list[index].makeView(source) };
}


rxu::OptionParser::Option::Option() = default;

rxu::OptionParser::Option::Option(std::wstring_view view) : view(view) { }

std::wstring_view rxu::OptionParser::Option::asString(std::wstring_view defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

double rxu::OptionParser::Option::asFloat(double defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}

	return parseNumber(view);
}

int64_t rxu::OptionParser::Option::asInt(int64_t defaultValue) const {
	return asFloat(defaultValue);
}

bool rxu::OptionParser::Option::asBool(bool defaultValue) const {
	return asFloat(defaultValue ? 1.0 : 0.0) != 0.0;
}

rxu::Color rxu::OptionParser::Option::asColor(Color defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}

	Tokenizer tokenizer;
	auto numbers = tokenizer.parse(view, L',');
	const auto count = numbers.size();
	if (count != 3 && count != 4) {
		return { };
	}
	double values[4];
	for (unsigned i = 0; i < count; ++i) {
		values[i] = parseNumber(numbers[i].makeView(view));
	}

	if (count == 3) {
		return { values[0], values[1], values[2], 1.0 };
	}
	return { values[0], values[1], values[2], values[3] };
}

double rxu::OptionParser::Option::parseNumber(std::wstring_view source) {
	MathExpressionParser parser(source);

	parser.parse();

	if (parser.isError()) {
		return 0;
	}

	auto exp = parser.getExpression();
	exp.solve();

	if (exp.type != ExpressionType::NUMBER) {
		return 0;
	}

	return exp.number;
}



rxu::OptionParser::OptionMap::OptionMap() = default;

rxu::OptionParser::OptionMap::OptionMap(std::wstring&& string,
	std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo) :
	stringOriginal(std::move(string)),
	paramsInfo(std::move(paramsInfo)) {
	stringLower = stringOriginal;
	for (auto&c : stringLower) {
		c = std::towlower(c);
	}

	fillParams();
}

rxu::OptionParser::OptionMap::~OptionMap() = default;

rxu::OptionParser::OptionMap::OptionMap(const OptionMap& other) :
	stringLower(other.stringLower),
	stringOriginal(other.stringOriginal),
	paramsInfo(other.paramsInfo) {
	fillParams();
}

rxu::OptionParser::OptionMap::OptionMap(OptionMap&& other) noexcept :
	stringLower(std::move(other.stringLower)),
	stringOriginal(std::move(other.stringOriginal)),
	paramsInfo(std::move(other.paramsInfo)) {
	other.params.clear();

	fillParams();
}

rxu::OptionParser::OptionMap& rxu::OptionParser::OptionMap::operator=(const OptionMap& other) {
	if (this == &other)
		return *this;

	stringLower = other.stringLower;
	stringOriginal = other.stringOriginal;
	paramsInfo = other.paramsInfo;

	fillParams();

	return *this;
}

rxu::OptionParser::OptionMap& rxu::OptionParser::OptionMap::operator=(OptionMap&& other) noexcept {
	if (this == &other)
		return *this;

	stringLower = std::move(other.stringLower);
	stringOriginal = std::move(other.stringOriginal);
	paramsInfo = std::move(other.paramsInfo);
	other.params.clear();

	fillParams();

	return *this;
}

rxu::OptionParser::Option rxu::OptionParser::OptionMap::get(std::wstring_view name) const {
	auto viewInfoOpt = find(name);
	return viewInfoOpt.has_value() ? Option { viewInfoOpt.value().makeView(stringLower) } : Option { };
}

rxu::OptionParser::Option rxu::OptionParser::OptionMap::getCS(std::wstring_view name) const {
	auto viewInfoOpt = find(name);
	return viewInfoOpt.has_value() ? Option { viewInfoOpt.value().makeView(stringOriginal) } : Option { };
}

const std::map<std::wstring_view, rxu::SubstringViewInfo>& rxu::OptionParser::OptionMap::getParams() const {
	return params;
}

void rxu::OptionParser::OptionMap::fillParams() {
	params.clear();
	for (auto& iter : paramsInfo) {
		params[iter.first.makeView(stringLower)] = iter.second;
	}
}

std::optional<rxu::SubstringViewInfo> rxu::OptionParser::OptionMap::find(std::wstring_view name) const {
	nameBuffer = name;
	StringUtils::lowerInplace(nameBuffer);

	const auto iter = params.find(nameBuffer);
	if (iter == params.end()) {
		return std::nullopt;
	}
	return iter->second;
}

rxu::OptionParser::OptionList rxu::OptionParser::asList(std::wstring_view string, wchar_t delimiter) {
	auto list = tokenizer.parse(string, delimiter);
	return { string, std::move(list) };
}

rxu::OptionParser::OptionMap rxu::OptionParser::asMap(std::wstring string, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	auto list = tokenizer.parse(string, optionDelimiter);

	std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };
	for (const auto &viewInfo : list) {
		const auto delimiterPlace = viewInfo.makeView(string).find_first_of(nameDelimiter);
		if (delimiterPlace == std::wstring_view::npos) {
			// tokenizer.parse is guarantied to return non-empty views
			paramsInfo[viewInfo] = { };
			continue;
		}

		auto name = svu::trim(string, viewInfo.substr(0, delimiterPlace));
		if (name.empty()) {
			continue;
		}

		auto value = svu::trim(string, viewInfo.substr(delimiterPlace + 1));

		paramsInfo[name] = value;
	}

	return { std::move(string), std::move(paramsInfo) };
}

rxu::OptionParser::OptionMap rxu::OptionParser::asMap(std::wstring_view string, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	return asMap(std::wstring { string }, optionDelimiter, nameDelimiter);
}

rxu::OptionParser::OptionMap rxu::OptionParser::asMap(const wchar_t* string, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	return asMap(std::wstring { string }, optionDelimiter, nameDelimiter);
}
