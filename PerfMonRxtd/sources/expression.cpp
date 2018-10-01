/* Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <cwctype>
#include <algorithm>

#include "utils.h"
#include "expressions.h"

#undef max

void pmrexp::ExpressionTreeNode::simplify() {
	if (type == ExpressionType::NUMBER ||
		type == ExpressionType::REF) {
		return;
	}

	bool isConst = true;
	for (ExpressionTreeNode& node : nodes) {
		node.simplify();
		if (node.type != ExpressionType::NUMBER) {
			isConst = false;
		}
	}
	if (!isConst) {
		return;
	}

	switch (type) {
	case ExpressionType::SUM:
	{
		double value = 0;
		for (ExpressionTreeNode& node : nodes) {
			value += node.number;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::NUMBER;
		return;
	}
	case ExpressionType::DIFF:
	{
		double value = nodes[0].number;
		for (unsigned int i = 1; i < nodes.size(); i++) {
			value -= nodes[i].number;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::NUMBER;
		return;
	}
	case ExpressionType::INVERSE:
	{
		number = -nodes[0].number;
		nodes.clear();
		type = ExpressionType::NUMBER;
		return;
	}
	case ExpressionType::MULT:
	{
		double value = 1;
		for (ExpressionTreeNode& node : nodes) {
			value *= node.number;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::NUMBER;
		return;
	}
	case ExpressionType::DIV:
	{
		double value = nodes[0].number;
		for (unsigned int i = 1; i < nodes.size(); i++) {
			value /= nodes[i].number;
		}
		nodes.clear();
		number = value;
		type = ExpressionType::NUMBER;
		return;
	}
	case ExpressionType::POWER:
	{
		double value = nodes[0].number;
		value = std::pow(value, nodes[1].number);
		nodes.clear();
		number = value;
		type = ExpressionType::NUMBER;
		return;
	}
	default:;
	}
}
int pmrexp::ExpressionTreeNode::maxExpRef() const {
	int max = -1;
	if (type == ExpressionType::REF && ref.type == ReferenceType::EXPRESSION) {
		max = ref.counter;
	} else {
		for (const ExpressionTreeNode& node : nodes) {
			max = std::max(max, node.maxExpRef());
		}
	}
	return max;
}

int pmrexp::ExpressionTreeNode::maxRUERef() const {
	int max = -1;
	if (type == ExpressionType::REF && ref.type == ReferenceType::ROLLUP_EXPRESSION) {
		max = ref.counter;
	} else {
		for (const ExpressionTreeNode& node : nodes) {
			max = std::max(max, node.maxRUERef());
		}
	}
	return max;
}

void pmrexp::ExpressionTreeNode::processRefs(void(*handler)(reference&)) {
	if (type == ExpressionType::REF) {
		handler(ref);
	} else {
		for (auto& node : nodes) {
			node.processRefs(handler);
		}
	}
}

pmrexp::Lexer::Lexer(std::wstring source)
	: source(std::move(source)) {

};

pmrexp::Lexeme pmrexp::Lexer::next() {
	skipSpaces();
	if (isSymbol(source[position])) {
		const wchar_t value = source[position];
		LexemeType type = LexemeType::UNKNOWN;
		position++;
		switch (value) {
		case L'(':
			type = LexemeType::PAR_OPEN;
			break;
		case L')':
			type = LexemeType::PAR_CLOSE;
			break;
		case L'[':
			type = LexemeType::BR_OPEN;
			break;
		case L']':
			type = LexemeType::BR_CLOSE;
			break;
		case L'+':
			type = LexemeType::PLUS;
			break;
		case L'-':
			type = LexemeType::MINUS;
			break;
		case L'*':
			type = LexemeType::MULT;
			break;
		case L'/':
			type = LexemeType::DIV;
			break;
		case L'^':
			type = LexemeType::POWER;
			break;
		case L'\0':
			type = LexemeType::END;
			break;
		case L'.':
			type = LexemeType::DOT;
			break;
		case L'#':
			type = LexemeType::HASH;
			break;
		default:
			type = LexemeType::UNKNOWN;
			break;
		}
		return Lexeme(type, std::to_wstring(value));
	}
	if (std::iswdigit(source[position])) {
		return Lexeme(LexemeType::NUMBER, readNumber());
	}
	if (std::iswalpha(source[position])) {
		return Lexeme(LexemeType::WORD, readWord());
	}
	Lexeme result(LexemeType::UNKNOWN, std::to_wstring(source[position]));
	position++;
	return result;
}

std::wstring pmrexp::Lexer::getUntil(const wchar_t stop1, const wchar_t stop2) {
	const int startPos = position;
	int i = 0;
	while (true) {
		const wchar_t c1 = source[static_cast<long long>(position) + i];
		if (c1 != stop1 && c1 != stop2 && c1 != L'\0') {
			i++;
		} else {
			break;
		}
	}
	position += i;
	return source.substr(startPos, i);
}

void pmrexp::Lexer::skipSpaces() {
	while (std::iswspace(source[position])) {
		position++;
	}
}

bool pmrexp::Lexer::isSymbol(const wchar_t c) {
	return
		c == L'\0' ||
		c == L'(' ||
		c == L')' ||
		c == L'+' ||
		c == L'-' ||
		c == L'*' ||
		c == L'/' ||
		c == L'^' ||
		c == L'[' ||
		c == L']' ||
		c == L'.' ||
		c == L'#';
}

std::wstring pmrexp::Lexer::readWord() {
	const int startPos = position;
	int i = 0;
	while (true) {
		const wchar_t c1 = source[static_cast<long long>(position) + i];
		if (std::iswalpha(c1)) {
			i++;
		} else {
			break;
		}
	}
	position += i;
	return source.substr(startPos, i);
}
std::wstring pmrexp::Lexer::readNumber() {
	const int startPos = position;
	int i = 0;
	while (true) {
		const wchar_t c1 = source[static_cast<long long>(position) + i];
		if (std::iswdigit(c1)) {
			i++;
		} else {
			break;
		}
	}
	position += i;
	return source.substr(startPos, i);
}

pmrexp::Parser::Parser(std::wstring source) : lexer(std::move(source)) {
	readNext();
}
void pmrexp::Parser::parse() {
	result = parseExpression();
	if (next.type != LexemeType::END) {
		error = true;
	}
}
bool pmrexp::Parser::isError() const {
	return error;
}
pmrexp::ExpressionTreeNode pmrexp::Parser::getExpression() const {
	return result;
}
void pmrexp::Parser::readNext() {
	next = lexer.next();
	if (next.type == LexemeType::UNKNOWN) {
		error = true;
	}
}
void pmrexp::Parser::toUpper(std::wstring& s) {
	for (wchar_t& c : s) {
		c = towupper(c);
	}
}

pmrexp::ExpressionTreeNode pmrexp::Parser::parseExpression() {
	ExpressionTreeNode result = parseTerm();
	if (error) {
		return ExpressionTreeNode();
	}
	while (next.type == LexemeType::PLUS || next.type == LexemeType::MINUS) {
		if (next.type == LexemeType::PLUS) {
			ExpressionTreeNode sumResult;
			sumResult.type = ExpressionType::SUM;
			sumResult.nodes.push_back(result);
			while (next.type == LexemeType::PLUS) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseTerm();
				if (error) {
					return ExpressionTreeNode();
				}
				sumResult.nodes.push_back(result);
			}
			result = std::move(sumResult);
		}
		if (next.type == LexemeType::MINUS) {
			ExpressionTreeNode diffResult;
			diffResult.type = ExpressionType::DIFF;
			diffResult.nodes.push_back(result);
			while (next.type == LexemeType::MINUS) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseTerm();
				if (error) {
					return ExpressionTreeNode();
				}
				diffResult.nodes.push_back(result);
			}
			result = std::move(diffResult);
		}
	}
	return result;
}

pmrexp::ExpressionTreeNode pmrexp::Parser::parseTerm() {
	ExpressionTreeNode result = parseFactor();
	if (error) {
		return ExpressionTreeNode();
	}
	while (next.type == LexemeType::MULT || next.type == LexemeType::DIV) {
		if (next.type == LexemeType::MULT) {
			ExpressionTreeNode multResult;
			multResult.type = ExpressionType::MULT;
			multResult.nodes.push_back(result);
			while (next.type == LexemeType::MULT) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseFactor();
				if (error) {
					return ExpressionTreeNode();
				}
				multResult.nodes.push_back(result);
			}
			result = std::move(multResult);
		}
		if (next.type == LexemeType::DIV) {
			ExpressionTreeNode divResult;
			divResult.type = ExpressionType::DIV;
			divResult.nodes.push_back(result);
			while (next.type == LexemeType::DIV) {
				readNext();
				if (error) {
					return ExpressionTreeNode();
				}
				result = parseFactor();
				if (error) {
					return ExpressionTreeNode();
				}
				divResult.nodes.push_back(result);
			}
			result = std::move(divResult);
		}
	}
	return result;
}

pmrexp::ExpressionTreeNode pmrexp::Parser::parseFactor() {
	ExpressionTreeNode power = parsePower();
	if (error) {
		return ExpressionTreeNode();
	}
	if (next.type == LexemeType::POWER) {
		ExpressionTreeNode res;
		res.type = ExpressionType::POWER;
		res.nodes.push_back(power);
		// while (next.type == POWER) {
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		power = parseFactor();
		if (error) {
			return ExpressionTreeNode();
		}
		res.nodes.push_back(power);
		// }
		return res;
	}
	return power;
}

pmrexp::ExpressionTreeNode pmrexp::Parser::parsePower() {
	if (next.type == LexemeType::MINUS) {
		ExpressionTreeNode res;
		res.type = ExpressionType::INVERSE;
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		const ExpressionTreeNode atom = parseAtom();
		if (error) {
			return ExpressionTreeNode();
		}
		res.nodes.push_back(atom);
		return res;
	}
	return parseAtom();
}

pmrexp::ExpressionTreeNode pmrexp::Parser::parseAtom() {
	if (next.type == LexemeType::PAR_OPEN) {
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		ExpressionTreeNode res = parseExpression();
		if (next.type != LexemeType::PAR_CLOSE) {
			error = true;
			return ExpressionTreeNode();
		}
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		return res;
	}
	if (next.type == LexemeType::NUMBER) {
		ExpressionTreeNode res;
		res.type = ExpressionType::NUMBER;
		const double i = std::stoi(next.value);
		double m = 0;
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		if (next.type == LexemeType::DOT) {
			readNext();
			if (error) {
				return ExpressionTreeNode();
			}
			if (next.type != LexemeType::NUMBER) {
				error = true;
				return ExpressionTreeNode();
			}
			m = std::stod(L"0." + next.value);
			readNext();
			if (error) {
				return ExpressionTreeNode();
			}
		}
		res.number = i + m;
		return res;
	}
	if (next.type == LexemeType::WORD) {
		ExpressionTreeNode res;
		res.type = ExpressionType::REF;
		res.ref = parseReference();
		if (error) {
			return ExpressionTreeNode();
		}
		return res;
	}
	error = true;
	return ExpressionTreeNode();
}

pmrexp::reference pmrexp::Parser::parseReference() {
	if (next.type != LexemeType::WORD) {
		error = true;
		return reference();
	}
	reference ref;
	std::wstring name = next.value;
	toUpper(name);
	if (name == L"COUNTERRAW" || name == L"CR") {
		ref.type = ReferenceType::COUNTER_RAW;
	} else if (name == L"COUNTERFORMATED" || name == L"CF") {
		ref.type = ReferenceType::COUNTER_FORMATTED;
	} else if (name == L"EXPRESSION" || name == L"E") {
		ref.type = ReferenceType::EXPRESSION;
	} else if (name == L"ROLLUPEXPRESSION" || name == L"R") {
		ref.type = ReferenceType::ROLLUP_EXPRESSION;
	} else if (name == L"COUNT" || name == L"C") {
		ref.type = ReferenceType::COUNT;
	} else {
		error = true;
		return reference();
	}
	readNext();
	if (error) {
		return reference();
	}
	if (ref.type != ReferenceType::COUNT) {
		if (next.type != LexemeType::NUMBER) {
			error = true;
			return reference();
		}
		ref.counter = std::stoi(next.value);
		readNext();
		if (error) {
			return reference();
		}
	}
	if (next.type == LexemeType::BR_OPEN) {
		ref.name = lexer.getUntil(L'#', L']');
		ref.named = !ref.name.empty();
		if (ref.named) {
			if (ref.name[0] == L'\\') {
				std::wstring::size_type indexOfFirstNonFlag = ref.name.find_first_of(L' ');
				if (indexOfFirstNonFlag == std::string::npos) {
					indexOfFirstNonFlag = ref.name.length();
				}
				std::wstring flags = ref.name.substr(1, indexOfFirstNonFlag - 1);
				toUpper(flags);

				if (flags.find(L'D') != std::string::npos) {
					ref.discarded = true;
				}
				if (flags.find(L'O') != std::string::npos) {
					ref.useOrigName = true;
				}
				if (flags.find(L'T') != std::string::npos) {
					ref.total = true;
				}
				ref.name = ref.name.substr(indexOfFirstNonFlag);
			}
			ref.name = trimSpaces(ref.name);
			const auto len = ref.name.size();
			if (len >= 2 && ref.name[0] == L'*' && ref.name[len - 1] == L'*') {
				ref.name = ref.name.substr(1, len - 2);
				ref.namePartialMatch = true;
			}
		}
		readNext();
		if (error) {
			return reference();
		}
		if (next.type != LexemeType::BR_CLOSE) {
			error = true;
			return reference();
		}
		readNext();
		if (error) {
			return reference();
		}
	}
	if (next.type == LexemeType::WORD) {
		std::wstring suffix = next.value;
		toUpper(suffix);
		if (suffix == L"SUM" || suffix == L"S") {
			ref.rollupFunction = pmre::RollupFunction::SUM;
		} else if (suffix == L"AVG" || suffix == L"A") {
			ref.rollupFunction = pmre::RollupFunction::AVERAGE;
		} else if (suffix == L"MIN" || next.value == L"m") {
			ref.rollupFunction = pmre::RollupFunction::MINIMUM;
		} else if (suffix == L"MAX" || next.value == L"M") {
			ref.rollupFunction = pmre::RollupFunction::MAXIMUM;
		} else if (suffix == L"COUNT" || suffix == L"C") {
			// handling deprecated rollup function
			ref.type = ReferenceType::COUNT;
		} else if (suffix == L"FIRST" || suffix == L"F") {
			ref.rollupFunction = pmre::RollupFunction::FIRST;
		}
		readNext();
		if (error) {
			return reference();
		}
	}
	return ref;
}

