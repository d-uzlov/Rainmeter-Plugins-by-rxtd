/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ExpressionSolver.h"


#include "TotalUtilities.h"
#include "option-parsing/OptionList.h"

using namespace rxtd::perfmon;

ExpressionSolver::ExpressionSolver(Logger log, const InstanceManager& instanceManager) :
	log(std::move(log)),
	instanceManager(instanceManager) {}

rxtd::index ExpressionSolver::getExpressionsCount() const {
	return index(expressions.size());
}

void ExpressionSolver::resetCache() {
	totalCaches.reset();
}

ExpressionSolver::ASTSolver ExpressionSolver::parseExpression(sview expressionString, sview loggerName, index loggerIndex) {
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

void ExpressionSolver::checkExpressionIndices() {
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

						if (ref.type == Reference::Type::EXPRESSION && ref.counter >= i) {
							log.error(L"Expression {}: invalid reference to Expression {}", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::ROLLUP_EXPRESSION) {
							log.error(L"Expression {}: invalid reference to RollupExpressions", i);
							throw std::runtime_error{ "" };
						}

						if (!ref.useOrigName) {
							CharUpperW(&ref.name[0]);
						}
					}
				}
			);
		} catch (std::runtime_error&) {
			expressions[i] = {};
		}
	}
}

void ExpressionSolver::setExpressions(OptionList expressionsList) {
	expressions.resize(expressionsList.size());
	for (index i = 0; i < index(expressionsList.size()); ++i) {
		expressions[i] = parseExpression(expressionsList.get(i).asString(), L"Expression", i);
	}

	checkExpressionIndices();
}

double ExpressionSolver::resolveReference(const Reference& ref, Indices indices) const {
	using Type = Reference::Type;

	if (ref.total) {
		switch (ref.type) {
		case Type::COUNTER_RAW: return TotalUtilities::getTotal(
				totalCaches.raw, instanceManager.getInstances(), ref.counter, ref.rollupFunction,
				[&](InstanceInfo info) { return instanceManager.calculateRaw(ref.counter, info.indices); }
			);
		case Type::COUNTER_FORMATTED: return TotalUtilities::getTotal(
				totalCaches.raw, instanceManager.getInstances(), ref.counter, ref.rollupFunction,
				[&](InstanceInfo info) { return instanceManager.calculateFormatted(ref.counter, info.indices); }
			);
		case Type::EXPRESSION: return TotalUtilities::getTotal(
				totalCaches.raw, instanceManager.getInstances(), ref.counter, ref.rollupFunction,
				[&](InstanceInfo info) { return solveExpression(ref.counter, info.indices); }
			);
		case Type::ROLLUP_EXPRESSION: return 0.0; // todo throw
		case Type::COUNT: return ref.rollupFunction == RollupFunction::eSUM ? static_cast<double>(instanceManager.getInstances().size()) : 1.0;
		}
	}

	if (ref.named) {
		const auto instancePtr = instanceManager.findSimpleInstanceByName(ref);
		if (instancePtr == nullptr) {
			return 0.0;
		}
		indices = instancePtr->indices;
	}

	switch (ref.type) {
	case Type::COUNTER_RAW: return instanceManager.calculateRaw(ref.counter, indices);
	case Type::COUNTER_FORMATTED: return instanceManager.calculateFormatted(ref.counter, indices);
	case Type::EXPRESSION: return solveExpression(ref.counter, indices);
	case Type::ROLLUP_EXPRESSION: return 0.0; // todo throw
	case Type::COUNT: return 1.0;
	}

	log.error(L"unexpected reference type in resolveReference(): {}", ref.type);
	return 0.0; // todo throw
}

double ExpressionSolver::solveExpression(index expressionIndex, Indices indices) const {
	ReferenceResolver referenceResolver{ *this, indices };
	return expressions[expressionIndex].solve(&referenceResolver);
}
