/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "SimpleExpressionSolver.h"


#include "TotalUtilities.h"
#include "rxtd/option-parsing/OptionList.h"

using namespace rxtd::perfmon::expressions;

SimpleExpressionSolver::SimpleExpressionSolver(Logger log, const SimpleInstanceManager& instanceManager) :
	log(std::move(log)),
	instanceManager(instanceManager) {}

rxtd::index SimpleExpressionSolver::getExpressionsCount() const {
	return index(expressions.size());
}

void SimpleExpressionSolver::resetCache() {
	totalCaches.reset();
}

SimpleExpressionSolver::ASTSolver SimpleExpressionSolver::parseExpression(sview expressionString, sview loggerName, index loggerIndex) {
	using common::expressions::ASTSolver;

	try {
		parser.parse(expressionString);

		ASTSolver solver{ parser.takeTree() };
		solver.optimize(nullptr);

		return solver;
	} catch (common::expressions::Lexer::Exception& e) {
		log.error(
			L"{} {} can't be parsed: unknown token here: '{}'",
			loggerName, loggerIndex, expressionString.substr(e.getPosition())
		);
	} catch (common::expressions::ASTParser::Exception& e) {
		log.error(
			L"{} {} can't be parsed: {}, at position: '{}'",
			loggerName, loggerIndex, e.getReason(), expressionString.substr(e.getPosition())
		);
	}

	return {};
}

void SimpleExpressionSolver::checkExpressionIndices() {
	for (index i = 0; i < index(expressions.size()); ++i) {
		try {
			using CustomNode = common::expressions::ast_nodes::CustomTerminalNode;

			expressions[i].peekTree().visitNodes(
				[&](const auto& node) {
					using Type = std::decay<decltype(node)>;
					if constexpr (std::is_same<Type, CustomNode>::value) {
						CustomNode& n = node;
						auto& ref = *std::any_cast<Reference>(&n.value);

						if (ref.counter < 0) {
							log.error(L"Expression {}: invalid reference to Expression {}", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::eEXPRESSION && ref.counter >= i) {
							log.error(L"Expression {}: invalid reference to Expression {}", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::eROLLUP_EXPRESSION) {
							log.error(L"Expression {}: invalid reference to RollupExpressions", i);
							throw std::runtime_error{ "" };
						}

						if (!ref.useOrigName) {
							utils::StringUtils::makeUppercaseInPlace(ref.namePattern.getName());
						}
					}
				}
			);
		} catch (std::runtime_error&) {
			expressions[i] = {};
		}
	}
}

void SimpleExpressionSolver::setExpressions(OptionList expressionsList) {
	expressions.resize(expressionsList.size());
	for (index i = 0; i < index(expressionsList.size()); ++i) {
		expressions[i] = parseExpression(expressionsList.get(i).asString(), L"Expression", i);
	}

	checkExpressionIndices();
}

double SimpleExpressionSolver::resolveReference(const Reference& ref, Indices indices) const {
	using Type = Reference::Type;

	if (ref.total) {
		switch (ref.type) {
		case Type::eCOUNTER_RAW: return TotalUtilities::getTotal(
				totalCaches.raw, instanceManager.getInstances(), ref.counter, ref.rollupFunction,
				[&](InstanceInfo info) { return instanceManager.calculateRaw(ref.counter, info.indices); }
			);
		case Type::eCOUNTER_FORMATTED: return TotalUtilities::getTotal(
				totalCaches.raw, instanceManager.getInstances(), ref.counter, ref.rollupFunction,
				[&](InstanceInfo info) { return instanceManager.calculateFormatted(ref.counter, info.indices); }
			);
		case Type::eEXPRESSION: return TotalUtilities::getTotal(
				totalCaches.raw, instanceManager.getInstances(), ref.counter, ref.rollupFunction,
				[&](InstanceInfo info) { return solveExpression(ref.counter, info.indices); }
			);
		case Type::eROLLUP_EXPRESSION: return 0.0; // all checks must have been done elsewhere
		case Type::eCOUNT: return ref.rollupFunction == RollupFunction::eSUM ? static_cast<double>(instanceManager.getInstances().size()) : 1.0;
		}
	}

	if (!ref.namePattern.isEmpty()) {
		const auto instancePtr = instanceManager.findSimpleInstanceByName(ref);
		if (instancePtr == nullptr) {
			return 0.0;
		}
		indices = instancePtr->indices;
	}

	switch (ref.type) {
	case Type::eCOUNTER_RAW: return instanceManager.calculateRaw(ref.counter, indices);
	case Type::eCOUNTER_FORMATTED: return instanceManager.calculateFormatted(ref.counter, indices);
	case Type::eEXPRESSION: return solveExpression(ref.counter, indices);
	case Type::eROLLUP_EXPRESSION: return 0.0; // all checks must have been done elsewhere
	case Type::eCOUNT: return 1.0;
	}

	log.error(L"unexpected reference type in SimpleExpressionSolver.resolveReference(): {}", ref.type);
	return 0.0;
}

double SimpleExpressionSolver::solveExpression(index expressionIndex, Indices indices) const {
	ReferenceResolver referenceResolver{ *this, indices };
	return expressions[expressionIndex].solve(&referenceResolver);
}
