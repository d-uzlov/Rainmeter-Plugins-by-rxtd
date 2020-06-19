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


OptionList::OptionList(sview view, std::vector<SubstringViewInfo>&& list) :
	view(view),
	list(std::move(list)) {
}

std::pair<sview, std::vector<SubstringViewInfo>> OptionList::consume() && {
	return { view, std::move(list) };
}

index OptionList::size() const {
	return list.size();
}

bool OptionList::empty() const {
	return list.empty();
}

sview OptionList::get(index ind) const {
	return list[ind].makeView(view);
}

isview OptionList::getCI(index ind) const {
	return get(ind) % ciView();
}

Option OptionList::getOption(index ind) const {
	return Option{ list[ind].makeView(view) };
}

OptionList::iterator::iterator(const OptionList& container, index _index):
	container(container),
	ind(_index) {
}

OptionList::iterator& OptionList::iterator::operator++() {
	ind++;
	return *this;
}

bool OptionList::iterator::operator!=(const iterator& other) const {
	return &container != &other.container || ind != other.ind;
}

Option OptionList::iterator::operator*() const {
	return container.getOption(ind);
}

OptionList::iterator OptionList::begin() const {
	return { *this, 0 };
}

OptionList::iterator OptionList::end() const {
	return { *this, size() };
}

OptionList OptionList::own() {
	source = view;
	view = source;

	return *this;
}

Option::Option(sview view) : view(view) {
}

sview Option::asString(sview defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

isview Option::asIString(isview defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}
	return view % ciView();

}

double Option::asFloat(double defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}

	return parseNumber(view);
}

Color Option::asColor(Color defaultValue) const {
	if (view.empty()) {
		return defaultValue;
	}

	OptionParser::Tokenizer tokenizer;
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

OptionSeparated Option::breakFirst(wchar_t separator) const {
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

OptionMap Option::asMap(wchar_t optionDelimiter, wchar_t nameDelimiter) const {
	OptionParser::Tokenizer tokenizer;

	auto list = tokenizer.parse(view, optionDelimiter);

	std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };
	for (const auto& viewInfo : list) {
		const auto delimiterPlace = viewInfo.makeView(view).find_first_of(nameDelimiter);
		if (delimiterPlace == sview::npos) {
			// tokenizer.parse is guarantied to return non-empty views
			paramsInfo[viewInfo] = { };
			continue;
		}

		auto name = svu::trimInfo(view, viewInfo.substr(0, delimiterPlace));
		if (name.empty()) {
			continue;
		}

		auto value = svu::trimInfo(view, viewInfo.substr(delimiterPlace + 1));

		paramsInfo[name] = value;
	}

	return { view, std::move(paramsInfo) };
}

OptionList Option::asList(wchar_t delimiter) const {
	OptionParser::Tokenizer tokenizer;

	auto list = tokenizer.parse(view, delimiter);
	return { view, std::move(list) };
}

Option Option::own() {
	source = view;
	view = source;

	return *this;
}

double Option::parseNumber(sview source) {
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


OptionMap::OptionMap() = default;

OptionMap::OptionMap(sview source, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo) :
	view(source),
	paramsInfo(std::move(paramsInfo)) {

	fillParams();
}

OptionMap::~OptionMap() = default;

OptionMap::OptionMap(const OptionMap& other) :
	view(other.view),
	paramsInfo(other.paramsInfo) {
	fillParams();
}

OptionMap::OptionMap(OptionMap&& other) noexcept : 
	view(other.view) , paramsInfo(std::move(other.paramsInfo)) {
	other.params.clear();

	fillParams();
}

OptionMap& OptionMap::operator=(const OptionMap& other) {
	if (this == &other)
		return *this;

	view = other.view;
	paramsInfo = other.paramsInfo;

	fillParams();

	return *this;
}

OptionMap& OptionMap::operator=(OptionMap&& other)
noexcept {
	if (this == &other)
		return *this;

	view = other.view;
	paramsInfo = std::move(other.paramsInfo);
	other.params.clear();

	fillParams();

	return *this;
}

Option OptionMap::getUntouched(sview name) const {
	const auto optionInfoPtr = find(name % ciView());
	if (optionInfoPtr == nullptr) {
		return { };
	}

	return Option{ optionInfoPtr->substringInfo.makeView(view) };
}

Option OptionMap::get(sview name) const {
	return get(name % ciView());
}

Option OptionMap::get(isview name) const {
	const auto optionInfoPtr = find(name);
	if (optionInfoPtr == nullptr) {
		return { };
	}

	optionInfoPtr->touched = true;
	return Option{ optionInfoPtr->substringInfo.makeView(view) };
}

Option OptionMap::get(const wchar_t* name) const {
	return get(isview{ name });
}

bool OptionMap::has(sview name) const {
	return has(name % ciView());
}

bool OptionMap::has(isview name) const {
	const auto optionInfoPtr = find(name);
	return optionInfoPtr != nullptr;
}

bool OptionMap::has(const wchar_t* name) const {
	return has(isview{ name });
}

const std::map<isview, OptionMap::MapOptionInfo>& OptionMap::getParams() const {
	return params;
}

std::vector<isview> OptionMap::getListOfUntouched() const {
	std::vector<isview> result;

	for (auto [name, valueInfo] : params) {
		if (valueInfo.touched) {
			continue;
		}

		result.emplace_back(name);
	}

	return result;
}

OptionMap OptionMap::own() {
	source = view;
	view = source;

	return *this;
}

void OptionMap::fillParams() {
	params.clear();
	for (auto [nameInfo, valueInfo] : paramsInfo) {
		params[nameInfo.makeView(view) % ciView()] = valueInfo;
	}
}

OptionMap::MapOptionInfo* OptionMap::find(isview name) const {
	const auto iter = params.find(name);
	if (iter == params.end()) {
		return nullptr;
	}
	return &iter->second;
}

void OptionParser::setSource(string&& string) {
	if (!source.empty()) {
		throw std::exception("Can't reinitialize option parser");
	}

	source = std::move(string);
}

void OptionParser::setSource(istring&& string) {
	setSource(string % csView());
}

void OptionParser::setSource(sview string) {
	setSource(string % own());
}

void OptionParser::setSource(isview string) {
	setSource(string % csView());
}

void OptionParser::setSource(const wchar_t* string) {
	setSource(sview{ string });
}

Option OptionParser::parse() const {
	return Option{ sview{ source } };
}

Option OptionParser::parse(sview string) {
	return Option { string }.own();
}
