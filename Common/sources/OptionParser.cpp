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

std::vector<SubstringViewInfo> Tokenizer::parse(sview view, wchar_t delimiter) {
	// this method is guarantied to return non - empty views

	if (view.empty()) {
		return { };
	}

	std::vector<SubstringViewInfo> tempList { };

	tokenize(tempList, view, delimiter);

	trimSpaces(tempList, view);

	return tempList;
}

std::vector<std::vector<SubstringViewInfo>> Tokenizer::parseSequence(sview view, wchar_t optionBegin,
	wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter) {
	enum class State {
		eSEARCH,
		eOPTION,
		eSWALLOW,
	};
	auto state = State::eSEARCH;

	std::vector<std::vector<SubstringViewInfo>> result;
	std::vector<SubstringViewInfo> description;
	index begin = view.find_first_not_of(L" \t");
	for (index i = begin; i < index(view.length()); ++i) {
		const auto symbol = view[i];

		if (symbol == optionBegin) {
			if (state != State::eOPTION) {
				return { };
			}
			state = State::eSWALLOW;

			const index end = i;
			emitToken(description, begin, end);
			begin = end + 1;
			continue;
		}
		if (symbol == optionEnd) {
			if (state != State::eSWALLOW) {
				return { };
			}
			state = State::eOPTION;

			const index end = i;
			emitToken(description, begin, end);
			begin = end + 1;
			continue;
		}
		if (symbol == paramDelimiter) {
			if (state != State::eSWALLOW) {
				return { };
			}

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
			description = { };
			state = State::eSEARCH;
			continue;
		}
		if (state == State::eSEARCH) {
			// beginning of option name
			state = State::eOPTION;
			begin = i;
		}
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

	for (index i = 0; i < index(string.length()); ++i) {
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
		std::remove_if(list.begin(), list.end(), [](SubstringViewInfo svi) {return svi.empty(); }),
		list.end()
	);
}


OptionList::OptionList(sview view, std::vector<SubstringViewInfo>&& list) :
	AbstractOption(view),
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

Option OptionList::get(index ind) const {
	return Option { list[ind].makeView(getView()) };
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
	return container.get(ind);
}

OptionList::iterator OptionList::begin() const {
	return { *this, 0 };
}

OptionList::iterator OptionList::end() const {
	return { *this, size() };
}

Option::Option(sview view) : AbstractOption(view) {
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

	const auto delimiterPlace = view.find_first_of(separator);
	if (delimiterPlace == sview::npos) {
		return { *this, { } };
	}

	auto first = Option { sview(view.data(), delimiterPlace) };
	auto rest = Option { sview(view.data() + delimiterPlace + 1, view.size() - delimiterPlace - 1) };
	return OptionSeparated { first, rest };
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

OptionSequence Option::asSequence(wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter) const {
	return { getView(), optionBegin, optionEnd, paramDelimiter, optionDelimiter };
}

bool Option::empty() const {
	return getView().empty();
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

OptionSequence::OptionSequence(sview view, wchar_t optionBegin, wchar_t optionEnd, wchar_t paramDelimiter, wchar_t optionDelimiter) : AbstractOption(view) {
	list = Tokenizer::parseSequence(view, optionBegin, optionEnd, paramDelimiter, optionDelimiter);
}

OptionSequence::iterator::iterator(sview view, const std::vector<std::vector<SubstringViewInfo>>& list, index _index) : 
	view(view), list(list), ind(_index) {
}

OptionSequence::iterator& OptionSequence::iterator::operator++() {
	ind++;
	return *this;
}

bool OptionSequence::iterator::operator!=(const iterator& other) const {
	return &list != &other.list || ind != other.ind;
}

OptionList OptionSequence::iterator::operator*() const {
	auto list_ = list[ind];
	return { view, std::move(list_) };
}

OptionSequence::iterator OptionSequence::begin() const {
	return { getView(), list, 0 };
}

OptionSequence::iterator OptionSequence::end() const {
	return { getView(), list, index(list.size()) };
}

void OptionSequence::emitToken(std::vector<SubstringViewInfo>& description, index begin, index end) {
	if (end <= begin) {
		return;
	}
	description.emplace_back(begin, end - begin);
}

OptionMap::OptionMap(sview source, std::map<SubstringViewInfo, SubstringViewInfo>&& paramsInfo) :
	AbstractOption(source),
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

Option OptionParser::parse(sview string) {
	return Option { string };
}
