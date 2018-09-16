/* Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "expressions.h"
#include <windows.h>
#include <cwctype>
#include <algorithm>
#include "PerfMonRXTD.h"

void pmrexp::ExpressionTreeNode::simplify() {
	if (type == EXP_TYPE_NUMBER ||
		type == EXP_TYPE_REF) {
		return;
	}

	bool isConst = true;
	for (ExpressionTreeNode& node : nodes) {
		node.simplify();
		if (node.type != EXP_TYPE_NUMBER) {
			isConst = false;
			break;
		}
	}
	if (isConst) {
		switch (type) {
		case EXP_TYPE_SUM:
		{
			double value = 0;
			for (ExpressionTreeNode& node : nodes) {
				value += node.number;
			}
			nodes.clear();
			number = value;
			type = EXP_TYPE_NUMBER;
			return;
		}
		case EXP_TYPE_DIFF:
		{
			double value = nodes[0].number;
			for (unsigned int i = 1; i < nodes.size(); i++) {
				value -= nodes[i].number;
			}
			nodes.clear();
			number = value;
			type = EXP_TYPE_NUMBER;
			return;
		}
		case EXP_TYPE_INVERSE:
		{
			number = -nodes[0].number;
			nodes.clear();
			type = EXP_TYPE_NUMBER;
			return;
		}
		case EXP_TYPE_MULT:
		{
			double value = 1;
			for (ExpressionTreeNode& node : nodes) {
				value *= node.number;
			}
			nodes.clear();
			number = value;
			type = EXP_TYPE_NUMBER;
			return;
		}
		case EXP_TYPE_DIV:
		{
			double value = nodes[0].number;
			for (unsigned int i = 1; i < nodes.size(); i++) {
				value /= nodes[i].number;
			}
			nodes.clear();
			number = value;
			type = EXP_TYPE_NUMBER;
			return;
		}
		case EXP_TYPE_POWER:
		{
			double value = nodes[0].number;
			value = std::pow(value, nodes[1].number);
			nodes.clear();
			number = value;
			type = EXP_TYPE_NUMBER;
			return;
		}
		default:;
		}
	}
}
#undef max
int pmrexp::ExpressionTreeNode::maxExpRef() {
	int max = -1;
	if (type == EXP_TYPE_REF && ref.type == REF_TYPE_EXPRESSION) {
		max = ref.counter;
	} else {
		for (ExpressionTreeNode& node : nodes) {
			max = std::max(max, node.maxExpRef());
		}
	}
	return max;
}

int pmrexp::ExpressionTreeNode::maxRUERef() {
	int max = -1;
	if (type == EXP_TYPE_REF && ref.type == REF_TYPE_ROLLUP_EXPRESSION) {
		max = ref.counter;
	} else {
		for (ExpressionTreeNode& node : nodes) {
			max = std::max(max, node.maxRUERef());
		}
	}
	return max;
}

void pmrexp::ExpressionTreeNode::processRefs(void(* handler)(reference&)) {
	if (type == EXP_TYPE_REF) {
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
	Lexeme result;
	if (isSymbol(source[position])) {
		result.value = source[position];
		switch (source[position]) {
		case L'(':
			result.type = LEXEME_TYPE_PAR_OPEN;
			break;
		case L')':
			result.type = LEXEME_TYPE_PAR_CLOSE;
			break;
		case L'[':
			result.type = LEXEME_TYPE_BR_OPEN;
			break;
		case L']':
			result.type = LEXEME_TYPE_BR_CLOSE;
			break;
		case L'+':
			result.type = LEXEME_TYPE_PLUS;
			break;
		case L'-':
			result.type = LEXEME_TYPE_MINUS;
			break;
		case L'*':
			result.type = LEXEME_TYPE_MULT;
			break;
		case L'/':
			result.type = LEXEME_TYPE_DIV;
			break;
		case L'^':
			result.type = LEXEME_TYPE_POWER;
			return result;
		case L'\0':
			result.type = LEXEME_TYPE_END;
			break;
		case L'.':
			result.type = LEXEME_TYPE_DOT;
			break;
		case L'#':
			result.type = LEXEME_TYPE_HASH;
			break;
		default:
			result.type = LEXEME_TYPE_UNKNOWN;
			break;
		}
		position++;
		return result;
	}
	if (std::iswdigit(source[position])) {
		result.type = LEXEME_TYPE_NUMBER;
		result.value = readNumber();
		return result;
	}
	if (std::iswalpha(source[position])) {
		result.type = LEXEME_TYPE_WORD;
		result.value = readWord();
		return result;
	}
	result.type = LEXEME_TYPE_UNKNOWN;
	result.value = source[position];
	position++;
	return result;
}

std::wstring pmrexp::Lexer::getUntil(wchar_t stop1, wchar_t stop2) {
	const int startPos = position;
	int i = 0;
	while (true) {
		const WCHAR c1 = source[static_cast<long long>(position) + i];
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

bool pmrexp::Lexer::isSymbol(wchar_t c) {
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
		const WCHAR c1 = source[static_cast<long long>(position) + i];
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
		const WCHAR c1 = source[static_cast<long long>(position) + i];
		if (std::iswdigit(c1)) {
			i++;
		} else {
			break;
		}
	}
	position += i;
	return source.substr(startPos, i);
}

pmrexp::Parser::Parser(const std::wstring& source) : source(source), lexer(source) {
	readNext();
}
void pmrexp::Parser::parse() {
	result = parseExpression();
	if (next.type != LEXEME_TYPE_END) {
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
	if (next.type == LEXEME_TYPE_UNKNOWN) {
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
	while(next.type == LEXEME_TYPE_PLUS || next.type == LEXEME_TYPE_MINUS) {
		if (next.type == LEXEME_TYPE_PLUS) {
			ExpressionTreeNode sumResult;
			sumResult.type = EXP_TYPE_SUM;
			sumResult.nodes.push_back(result);
			while (next.type == LEXEME_TYPE_PLUS) {
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
		if (next.type == LEXEME_TYPE_MINUS) {
			ExpressionTreeNode diffResult;
			diffResult.type = EXP_TYPE_DIFF;
			diffResult.nodes.push_back(result);
			while (next.type == LEXEME_TYPE_MINUS) {
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
	while(next.type == LEXEME_TYPE_MULT || next.type == LEXEME_TYPE_DIV) {
		if (next.type == LEXEME_TYPE_MULT) {
			ExpressionTreeNode multResult;
			multResult.type = EXP_TYPE_MULT;
			multResult.nodes.push_back(result);
			while (next.type == LEXEME_TYPE_MULT) {
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
		if (next.type == LEXEME_TYPE_DIV) {
			ExpressionTreeNode divResult;
			divResult.type = EXP_TYPE_DIV;
			divResult.nodes.push_back(result);
			while (next.type == LEXEME_TYPE_DIV) {
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
	if (next.type == LEXEME_TYPE_POWER) {
		ExpressionTreeNode res;
		res.type = EXP_TYPE_POWER;
		res.nodes.push_back(power);
		// while (next.type == LEXEME_TYPE_POWER) {
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
	if (next.type == LEXEME_TYPE_MINUS) {
		ExpressionTreeNode res;
		res.type = EXP_TYPE_INVERSE;
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
	if (next.type == LEXEME_TYPE_PAR_OPEN) {
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		ExpressionTreeNode res = parseExpression();
		if (next.type != LEXEME_TYPE_PAR_CLOSE) {
			error = true;
			return ExpressionTreeNode();
		}
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		return res;
	}
	if (next.type == LEXEME_TYPE_NUMBER) {
		ExpressionTreeNode res;
		res.type = EXP_TYPE_NUMBER;
		const double i = std::stoi(next.value);
		double m = 0;
		readNext();
		if (error) {
			return ExpressionTreeNode();
		}
		if (next.type == LEXEME_TYPE_DOT) {
			readNext();
			if (error) {
				return ExpressionTreeNode();
			}
			if (next.type != LEXEME_TYPE_NUMBER) {
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
	if (next.type == LEXEME_TYPE_WORD) {
		ExpressionTreeNode res;
		res.type = EXP_TYPE_REF;
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
	if (next.type != LEXEME_TYPE_WORD) {
		error = true;
		return reference();
	}
	reference ref;
	std::wstring name = next.value;
	std::wstring shortName;
	toUpper(name);
	if (name == L"COUNTERRAW" || name == L"CR") {
		ref.type = REF_TYPE_COUNTER_RAW;
		shortName = L"r";
	} else if (name == L"COUNTERFORMATED" || name == L"CF") {
		ref.type = REF_TYPE_COUNTER_FORMATTED;
		shortName = L"f";
	} else if (name == L"EXPRESSION" || name == L"E") {
		ref.type = REF_TYPE_EXPRESSION;
		shortName = L"e";
	} else if (name == L"ROLLUPEXPRESSION" || name == L"R") {
		ref.type = REF_TYPE_ROLLUP_EXPRESSION;
		shortName = L"R";
	} else {
		error = true;
		return reference();
	}
	readNext();
	if (error) {
		return reference();
	}
	if (next.type != LEXEME_TYPE_NUMBER) {
		error = true;
		return reference();
	}
	ref.counter = std::stoi(next.value);
	readNext();
	if (error) {
		return reference();
	}
	if (next.type == LEXEME_TYPE_BR_OPEN) {
		ref.name = lexer.getUntil(L'#', L']');
		ref.named = !ref.name.empty();
		if (ref.named) {
			if (ref.name[0] == L'\\') {
				std::wstring::size_type b = ref.name.find_first_of(L' ');
				if (b != static_cast<decltype(b)>(-1)) {
					std::wstring flags = ref.name.substr(1, b - 1);
					CharUpperW(&flags[0]);

					if (flags.find(L'P') != std::string::npos && flags.find(L'D') != std::string::npos) {
						if (flags.find(L'P') < flags.find(L'D')) {
							ref.searchPlace = pmre::NSP_PASSED_DISCARDED;
						} else {
							ref.searchPlace = pmre::NSP_DISCARDED_PASSED;
						}
					} else if (flags.find(L'P') != std::string::npos) {
						ref.searchPlace = pmre::NSP_PASSED;
					} else if (flags.find(L'D') != std::string::npos) {
						ref.searchPlace = pmre::NSP_DISCARDED;
					}
					if (flags.find(L'O') != std::string::npos) {
						ref.useOrigName = true;
					}

					ref.name = ref.name.substr(b);
				}
			}
			ref.name = pmr::trimSpaces(ref.name);
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
		if (next.type == LEXEME_TYPE_HASH) {
			readNext();
			if (error) {
				return reference();
			}
			ExpressionTreeNode n = parseExpression();
			if (error) {
				return reference();
			}
			n.simplify();
			if (n.type == EXP_TYPE_NUMBER) {
				ref.number = std::lround(n.number);
			} else {
				error = true;
				return reference();
			}
		}
		if (next.type != LEXEME_TYPE_BR_CLOSE) {
			error = true;
			return reference();
		}
		readNext();
		if (error) {
			return reference();
		}
	}
	std::wstring shortSuffix;
	if (next.type == LEXEME_TYPE_WORD) {
		std::wstring suffix = next.value;
		toUpper(suffix);
		if (suffix == L"SUM" || suffix == L"S") {
			ref.rollupFunction = pmre::ROLLUP_SUM;
			shortSuffix = L"s";
		} else if (suffix == L"AVG" || suffix == L"A") {
			ref.rollupFunction = pmre::ROLLUP_AVERAGE;
			shortSuffix = L"a";
		} else if (suffix == L"MIN" || next.value == L"m") {
			ref.rollupFunction = pmre::ROLLUP_MINIMUM;
			shortSuffix = L"m";
		} else if (suffix == L"MAX" || next.value == L"M") {
			ref.rollupFunction = pmre::ROLLUP_MAXIMUM;
			shortSuffix = L"M";
		} else if (suffix == L"COUNT" || suffix == L"C") {
			ref.rollupFunction = pmre::ROLLUP_COUNT;
			shortSuffix = L"c";
		}
		readNext();
		if (error) {
			return reference();
		}
	}
	ref.uniqueName = shortName
		+ std::to_wstring(ref.searchPlace)
		+ (ref.named
			? (std::wstring(L"[") + (ref.useOrigName ? L"\\" : L"") + std::to_wstring(ref.counter) + ref.name 
				// + L"#" + std::to_wstring(ref.number)
				)
			: L"")
		+ shortSuffix;
	return ref;
}

