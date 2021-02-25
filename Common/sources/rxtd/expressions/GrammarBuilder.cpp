/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "GrammarBuilder.h"

using namespace rxtd::common::expressions;

GrammarDescription GrammarBuilder::makeSimpleMath() {
	GrammarBuilder builder;
	using Data = GrammarDescription::NodeData;

	// Previously Option could only parse: +, -, *, /, ^
	// So let's limit it to these operators

	builder.pushBinary(L"+", [](Data d1, Data d2) { return Data{ d1.getValue() + d2.getValue() }; });
	builder.pushBinary(L"-", [](Data d1, Data d2) { return Data{ (d1.getValue() - d2.getValue()) }; });

	builder.increasePrecedence();
	builder.pushBinary(L"*", [](Data d1, Data d2) { return Data{ (d1.getValue() * d2.getValue()) }; });
	builder.pushBinary(L"/", [](Data d1, Data d2) { return Data{ ((d1.getValue() == 0.0 ? 0.0 : d1.getValue() / d2.getValue())) }; });

	builder.increasePrecedence();
	builder.pushPrefix(L"+", [](Data d1, Data d2) { return d1; });
	builder.pushPrefix(L"-", [](Data d1, Data d2) { return Data{ (-d1.getValue()) }; });

	builder.increasePrecedence();
	builder.pushBinary(L"^", [](Data d1, Data d2) { return Data{ (std::pow(d1.getValue(), d2.getValue())) }; }, false);

	builder.pushGrouping(L"(", L")", L",");

	return std::move(builder).takeResult();
}

void GrammarBuilder::pushBinary(sview opValue, SolveFunction solveFunc, bool leftToRightAssociativity) {
	if (auto [iter, inserted] = binaryOperators.insert(opValue);
		!inserted) {
		throw OperatorRepeatException{ opValue };
	}

	if (groupingOperatorsFirst.count(opValue) || groupingOperatorsSecond.count(opValue) != 0) {
		throw OperatorRepeatException{ opValue };
	}

	push(opValue, OperatorInfo::Type::eBINARY, solveFunc, leftToRightAssociativity);
}

void GrammarBuilder::pushPrefix(sview opValue, SolveFunction solveFunc, bool leftToRightAssociativity) {
	if (auto [iter, inserted] = prefixOperators.insert(opValue);
		!inserted) {
		throw OperatorRepeatException{ opValue };
	}

	if (groupingOperatorsFirst.count(opValue) || groupingOperatorsSecond.count(opValue) != 0) {
		throw OperatorRepeatException{ opValue };
	}

	push(opValue, OperatorInfo::Type::ePREFIX, solveFunc, leftToRightAssociativity);
}

void GrammarBuilder::pushPostfix(sview opValue, SolveFunction solveFunc, bool leftToRightAssociativity) {
	if (auto [iter, inserted] = postfixOperators.insert(opValue);
		!inserted) {
		throw OperatorRepeatException{ opValue };
	}

	if (groupingOperatorsFirst.count(opValue) || groupingOperatorsSecond.count(opValue) != 0) {
		throw OperatorRepeatException{ opValue };
	}

	push(opValue, OperatorInfo::Type::ePOSTFIX, solveFunc, leftToRightAssociativity);
}

void GrammarBuilder::pushGrouping(sview first, sview second, sview separator) {
	if (prefixOperators.count(first) != 0 || postfixOperators.count(first) != 0 || binaryOperators.count(first) != 0) {
		throw OperatorRepeatException{ first };
	}

	if (prefixOperators.count(second) != 0 || postfixOperators.count(second) != 0 || binaryOperators.count(second) != 0) {
		throw OperatorRepeatException{ second };
	}

	if (auto [iter, inserted] = groupingOperatorsFirst.insert(first);
		!inserted) {
		throw OperatorRepeatException{ first };
	}

	// It's allowed to have different grouping operators
	// with the same closing part.
	// Closing parts are only used to make sure that
	// other operators don't collide with the closing parts.
	groupingOperatorsSecond.insert(second);

	// separator doesn't need to be checked

	grammar.groupingOperators.emplace_back(first, second, separator);
}

void GrammarBuilder::push(sview opValue, OperatorInfo::Type type, SolveFunction solveFunc, bool leftToRightAssociativity) {
	const PrecedenceType rightPrecedence = leftToRightAssociativity ? currentPrecedence + 1 : currentPrecedence;

	PrecedenceType nextPrecedence;
	if (leftToRightAssociativity == true || type == OperatorInfo::Type::ePOSTFIX) {
		nextPrecedence = currentPrecedence;
	} else {
		nextPrecedence = currentPrecedence - 1;
	}

	auto value = OperatorInfo{ { opValue, solveFunc }, type, currentPrecedence, rightPrecedence, nextPrecedence };

	grammar.operators.emplace_back(value);
}
