/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "OptionParser.h"
#include "MathExpressionParser.h"

#include "undef.h"

using namespace utils;

std::vector<SubstringViewInfo> Tokenizer::parse(sview string, wchar_t delimiter) {
	// this method is guarantied to return non - empty views

	if (string.empty()) {
		return { };
	}

	tempList.clear();

	tokenize(string, delimiter);

	trimSpaces(string);

	return tempList;
}

void Tokenizer::emitToken(const index begin, const index end) {
	if (end <= begin) {
		return;
	}
	tempList.emplace_back(begin, end - begin);
}

void Tokenizer::tokenize(sview string, wchar_t delimiter) {
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

void Tokenizer::trimSpaces(sview string) {
	for (auto& view : tempList) {
		view = StringUtils::trimInfo(string, view);
	}

	tempList.erase(
		std::remove_if(tempList.begin(), tempList.end(), [](SubstringViewInfo svi) {return svi.empty(); }),
		tempList.end()
	);
}


OptionList::OptionList(sview view, std::vector<SubstringViewInfo>&& list) :
	_view(view),
	list(std::move(list)) {
}

std::pair<sview, std::vector<SubstringViewInfo>> OptionList::consume() && {
	return { getView(), std::move(list) };
}

index OptionList::size() const {
	return list.size();
}

bool OptionList::empty() const {
	return list.empty();
}

sview OptionList::get(index ind) const {
	return list[ind].makeView(getView());
}

isview OptionList::getCI(index ind) const {
	return get(ind) % ciView();
}

Option OptionList::getOption(index ind) const {
	return Option{ list[ind].makeView(getView()) };
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
	source.resize(_view.size());
	std::copy(_view.begin(), _view.end(), source.begin());

	return *this;
}

sview OptionList::getView() const {
	if (source.empty()) {
		return _view;
	}
	return sview { source.data(), source.size() };
}

Option::Option(sview view) : _view(view) {
}

sview Option::asString(sview defaultValue) const {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view;
}

isview Option::asIString(isview defaultValue) const {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}
	return view % ciView();

}

double Option::asFloat(double defaultValue) const {
	sview view = getView();
	if (view.empty()) {
		return defaultValue;
	}

	return parseNumber(view);
}

Color Option::asColor(Color defaultValue) const {
	sview view = getView();

	if (view.empty()) {
		return defaultValue;
	}

	Tokenizer tokenizer;
	auto numbers = tokenizer.parse(view, L',');
	const auto count = index(numbers.size());
	if (count != 3 && count != 4) {
		return defaultValue;
	}

	float values[4];
	values[3] = 1.0;
	for (index i = 0; i < count; ++i) {
		values[i] = parseNumber(numbers[i].makeView(view));
	}

	return { values[0], values[1], values[2], values[3] };
}

OptionSeparated Option::breakFirst(wchar_t separator) const {
	sview view = getView();

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
	Tokenizer tokenizer;

	sview view = getView();

	auto list = tokenizer.parse(view, optionDelimiter);

	std::map<SubstringViewInfo, SubstringViewInfo> paramsInfo { };
	for (const auto& viewInfo : list) {
		const auto delimiterPlace = viewInfo.makeView(view).find_first_of(nameDelimiter);
		if (delimiterPlace == sview::npos) {
			// tokenizer.parse is guarantied to return non-empty views
			paramsInfo[viewInfo] = { };
			continue;
		}

		auto name = StringUtils::trimInfo(view, viewInfo.substr(0, delimiterPlace));
		if (name.empty()) {
			continue;
		}

		auto value = StringUtils::trimInfo(view, viewInfo.substr(delimiterPlace + 1));

		paramsInfo[name] = value;
	}

	return { view, std::move(paramsInfo) };
}

OptionList Option::asList(wchar_t delimiter) const {
	Tokenizer tokenizer;

	auto list = tokenizer.parse(getView(), delimiter);
	return { getView(), std::move(list) };
}

Option Option::own() {
	source.resize(_view.size());
	std::copy(_view.begin(), _view.end(), source.begin());

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

sview Option::getView() const {
	if (source.empty()) {
		return _view;
	}
	return sview { source.data(), source.size() };
}


OptionMap::OptionMap() = default;

OptionMap::OptionMap(sview source, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo) :
	_view(source),
	paramsInfo(std::move(paramsInfo)) {

	fillParams();
}

Option OptionMap::getUntouched(sview name) const {
	const auto optionInfoPtr = find(name % ciView());
	if (optionInfoPtr == nullptr) {
		return { };
	}

	return Option{ optionInfoPtr->substringInfo.makeView(getView()) };
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
	return Option{ optionInfoPtr->substringInfo.makeView(getView()) };
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
	source.resize(_view.size());
	std::copy(_view.begin(), _view.end(), source.begin());

	return *this;
}

void OptionMap::fillParams() const {
	params.clear();
	for (auto [nameInfo, valueInfo] : paramsInfo) {
		params[nameInfo.makeView(getView()) % ciView()] = valueInfo;
	}
}

OptionMap::MapOptionInfo* OptionMap::find(isview name) const {
	if (params.empty() && !paramsInfo.empty()) {
		fillParams();
	}
	const auto iter = params.find(name);
	if (iter == params.end()) {
		return nullptr;
	}
	return &iter->second;
}

sview OptionMap::getView() const {
	if (source.empty()) {
		return _view;
	}
	return sview { source.data(), source.size() };
}

Option OptionParser::parse(sview string) {
	return Option { string };
}
