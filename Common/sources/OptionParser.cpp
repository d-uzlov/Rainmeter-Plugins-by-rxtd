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

#include "undef.h"

using namespace utils;

std::vector<SubstringViewInfo> OptionParser::Tokenizer::parse(sview string, wchar_t delimiter) {
	if (string.empty()) {
		return { };
	}

	tempList.clear();

	tokenize(string, delimiter);

	auto list = trimSpaces(string);

	return list;
}

void OptionParser::Tokenizer::emitToken(const index begin, const index end) {
	if (end <= begin) {
		return;
	}
	tempList.emplace_back(begin, end - begin);
}

void OptionParser::Tokenizer::tokenize(sview string, wchar_t delimiter) {
	index begin = 0;

	for (index i = 0; i < index(string.length()); ++i) {
		if (string[i] == delimiter) {
			const index end = i;
			emitToken(begin, end);
			begin = end + 1;
		}
	}

	emitToken(begin, string.length());
}

std::vector<SubstringViewInfo> OptionParser::Tokenizer::trimSpaces(sview string) {
	std::vector<SubstringViewInfo> list{ };

	for (auto& view : tempList) {
		auto trimmed = svu::trimInfo(string, view);
		if (trimmed.empty()) {
			continue;
		}

		list.emplace_back(trimmed);
	}

	return list;
}


OptionParser::OptionList::OptionList() = default;

OptionParser::OptionList::OptionList(sview view, std::vector<SubstringViewInfo>&& list) :
	source(view.begin(), view.end()),
	list(std::move(list)) {
	source.push_back(L'\0');
}

std::pair<std::vector<wchar_t>, std::vector<SubstringViewInfo>> OptionParser::OptionList::consume() && {
	return { std::move(source), std::move(list) };
}

index OptionParser::OptionList::size() const {
	return list.size();
}

bool OptionParser::OptionList::empty() const {
	return list.empty();
}

sview OptionParser::OptionList::get(index ind) const {
	return list[ind].makeView(source);
}

isview OptionParser::OptionList::getCI(index ind) const {
	return get(ind) % ciView();
}

OptionParser::Option OptionParser::OptionList::getOption(index ind) const {
	return Option{ list[ind].makeView(source) };
}

OptionParser::OptionList::iterator::iterator(const OptionList& container, index _index):
	container(container),
	ind(_index) {
}

OptionParser::OptionList::iterator& OptionParser::OptionList::iterator::operator++() {
	ind++;
	return *this;
}

bool OptionParser::OptionList::iterator::operator!=(const iterator& other) const {
	return &container != &other.container || ind != other.ind;
}

sview OptionParser::OptionList::iterator::operator*() const {
	return container.get(ind);
}

OptionParser::OptionList::iterator OptionParser::OptionList::begin() const {
	return { *this, 0 };
}

OptionParser::OptionList::iterator OptionParser::OptionList::end() const {
	return { *this, size() };
}

OptionParser::OptionList::CaseInsensitiveView::
CaseInsensitiveView(const OptionList& basicStringViews): container(basicStringViews) {
}

OptionParser::OptionList::CaseInsensitiveView::iterator::iterator(const OptionList& container, index _index):
	container(container),
	ind(_index) {
}

OptionParser::OptionList::CaseInsensitiveView::iterator& OptionParser::OptionList::CaseInsensitiveView::iterator
::operator++() {
	ind++;
	return *this;
}

bool OptionParser::OptionList::CaseInsensitiveView::iterator::operator!=(const iterator& other) const {
	return &container != &other.container || ind != other.ind;
}

isview OptionParser::OptionList::CaseInsensitiveView::iterator::operator*() const {
	return container.getCI(ind);
}

OptionParser::OptionList::CaseInsensitiveView::iterator OptionParser::OptionList::CaseInsensitiveView::
begin() const {
	return { container, 0 };
}

OptionParser::OptionList::CaseInsensitiveView::iterator OptionParser::OptionList::CaseInsensitiveView::
end() const {
	return { container, container.size() };
}

OptionParser::OptionList::CaseInsensitiveView OptionParser::OptionList::viewCI() const {
	return CaseInsensitiveView{ *this };
}

OptionParser::Option::Option(sview view) : view(view) {
}

sview OptionParser::Option::asString(sview defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

isview OptionParser::Option::asIString(isview defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}
	return view % ciView();

}

double OptionParser::Option::asFloat(double defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}

	return parseNumber(view);
}

Color OptionParser::Option::asColor(Color defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}

	Tokenizer tokenizer;
	auto numbers = tokenizer.parse(view, L',');
	const auto count = index(numbers.size());
	if (count != 3 && count != 4) {
		return { };
	}
	float values[4];
	for (index i = 0; i < count; ++i) {
		values[i] = parseNumber(numbers[i].makeView(view));
	}

	if (count == 3) {
		return { values[0], values[1], values[2], 1.0 };
	}
	return { values[0], values[1], values[2], values[3] };
}

OptionParser::OptionSeparated OptionParser::Option::breakFirst(wchar_t separator) const {
	for (int i = 0; i < view.size(); ++i) {
		if (view[i] == separator) {
			auto first = Option{ sview(view.data(), i) };
			auto rest = Option{ sview(view.data() + i + 1, view.size() - i - 1) };
			return OptionSeparated{ first, rest };
		}
	}
	return {
		*this,
		{ }
	};
}

double OptionParser::Option::parseNumber(sview source) {
	MathExpressionParser parser(source);

	parser.parse();

	if (parser.isError()) {
		return 0;
	}

	auto exp = parser.getExpression();
	exp.solve();

	if (exp.type != ExpressionType::eNUMBER) {
		return 0;
	}

	return exp.number;
}


OptionParser::OptionMap::OptionMap() = default;

OptionParser::OptionMap::OptionMap(string&& string,
                                   std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo) :
	source(std::move(string)),
	paramsInfo(std::move(paramsInfo)) {

	fillParams();
}

OptionParser::OptionMap::~OptionMap() = default;

OptionParser::OptionMap::OptionMap(const OptionMap& other) :
	source(other.source),
	paramsInfo(other.paramsInfo) {
	fillParams();
}

OptionParser::OptionMap::OptionMap(OptionMap&& other) noexcept :
	source(std::move(other.source)),
	paramsInfo(std::move(other.paramsInfo)) {
	other.params.clear();

	fillParams();
}

OptionParser::OptionMap& OptionParser::OptionMap::operator=(const OptionMap& other) {
	if (this == &other)
		return *this;

	source = other.source;
	paramsInfo = other.paramsInfo;

	fillParams();

	return *this;
}

OptionParser::OptionMap& OptionParser::OptionMap::operator=(OptionMap&& other) noexcept {
	if (this == &other)
		return *this;

	source = std::move(other.source);
	paramsInfo = std::move(other.paramsInfo);
	other.params.clear();

	fillParams();

	return *this;
}

OptionParser::Option OptionParser::OptionMap::getUntouched(sview name) const {
	const auto optionInfoPtr = find(name % ciView());
	if (optionInfoPtr == nullptr) {
		return { };
	}

	return Option{ optionInfoPtr->substringInfo.makeView(source) };
}

OptionParser::Option OptionParser::OptionMap::get(sview name) const {
	return get(name % ciView());
}

OptionParser::Option OptionParser::OptionMap::get(isview name) const {
	const auto optionInfoPtr = find(name);
	if (optionInfoPtr == nullptr) {
		return { };
	}

	optionInfoPtr->touched = true;
	return Option{ optionInfoPtr->substringInfo.makeView(source) };
}

OptionParser::Option OptionParser::OptionMap::get(const wchar_t* name) const {
	return get(isview{ name });
}

bool OptionParser::OptionMap::has(sview name) const {
	return has(name % ciView());
}

bool OptionParser::OptionMap::has(isview name) const {
	const auto optionInfoPtr = find(name);
	return optionInfoPtr != nullptr;
}

bool OptionParser::OptionMap::has(const wchar_t* name) const {
	return has(isview{ name });
}

const std::map<isview, OptionParser::OptionMap::MapOptionInfo>& OptionParser::OptionMap::getParams() const {
	return params;
}

std::vector<isview> OptionParser::OptionMap::getListOfUntouched() const {
	std::vector<isview> result;

	for (auto [name, valueInfo] : params) {
		if (valueInfo.touched) {
			continue;
		}

		result.emplace_back(name);
	}

	return result;
}

void OptionParser::OptionMap::fillParams() {
	params.clear();
	for (auto [nameInfo, valueInfo] : paramsInfo) {
		params[nameInfo.makeView(source) % ciView()] = valueInfo;
	}
}

OptionParser::OptionMap::MapOptionInfo* OptionParser::OptionMap::find(isview name) const {
	const auto iter = params.find(name);
	if (iter == params.end()) {
		return nullptr;
	}
	return &iter->second;
}

OptionParser::OptionList OptionParser::asList(sview view, wchar_t delimiter) {
	auto list = tokenizer.parse(view, delimiter);
	return { view, std::move(list) };
}

OptionParser::OptionList OptionParser::asList(isview view, wchar_t delimiter) {
	return asList(view % csView(), delimiter);
}

OptionParser::OptionList OptionParser::asList(const wchar_t* str, wchar_t delimiter) {
	return asList(sview{ str }, delimiter);
}

OptionParser::OptionMap OptionParser::asMap(string&& string, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	auto list = tokenizer.parse(string, optionDelimiter);

	std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo{ };
	for (const auto& viewInfo : list) {
		const auto delimiterPlace = viewInfo.makeView(string).find_first_of(nameDelimiter);
		if (delimiterPlace == sview::npos) {
			// tokenizer.parse is guarantied to return non-empty views
			paramsInfo[viewInfo] = { };
			continue;
		}

		auto name = svu::trimInfo(string, viewInfo.substr(0, delimiterPlace));
		if (name.empty()) {
			continue;
		}

		auto value = svu::trimInfo(string, viewInfo.substr(delimiterPlace + 1));

		paramsInfo[name] = value;
	}

	return { std::move(string), std::move(paramsInfo) };
}

OptionParser::OptionMap OptionParser::asMap(istring&& str, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	return asMap(std::move(str) % csString(), optionDelimiter, nameDelimiter);
}

OptionParser::OptionMap OptionParser::asMap(sview view, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	return asMap(string{ view }, optionDelimiter, nameDelimiter);
}

OptionParser::OptionMap OptionParser::asMap(isview str, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	return asMap(str % csView(), optionDelimiter, nameDelimiter);
}

OptionParser::OptionMap OptionParser::asMap(const wchar_t* str, wchar_t optionDelimiter, wchar_t nameDelimiter) {
	return asMap(string{ str }, optionDelimiter, nameDelimiter);
}
