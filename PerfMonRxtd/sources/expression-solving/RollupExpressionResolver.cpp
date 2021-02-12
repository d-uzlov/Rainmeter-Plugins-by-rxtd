/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "RollupExpressionResolver.h"

#include "expressions/ASTSolver.h"
#include "option-parsing/OptionList.h"

using namespace perfmon;

RollupExpressionResolver::RollupExpressionResolver(Logger log, const InstanceManager& instanceManager, ExpressionSolver& simpleExpressionSolver) :
	log(std::move(log)), instanceManager(instanceManager), simpleExpressionSolver(simpleExpressionSolver) {}

index RollupExpressionResolver::getExpressionsCount() const {
	return index(expressions.size());
}

void RollupExpressionResolver::resetCaches() {
	totalCaches.reset();
}

RollupExpressionResolver::ASTSolver RollupExpressionResolver::parseExpressionTree(sview expressionString, sview loggerName, index loggerIndex) {
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

void RollupExpressionResolver::parseExpressions(std::vector<ASTSolver>& vector, OptionList list, sview loggerName) {
	vector.resize(list.size());
	for (index i = 0; i < index(list.size()); ++i) {
		vector[i] = parseExpressionTree(list.get(i).asString(), loggerName, i);
	}
}

void RollupExpressionResolver::checkExpressionIndices() {
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

						if (ref.type == Reference::Type::EXPRESSION && ref.counter >= simpleExpressionSolver.getExpressionsCount()) {
							log.error(L"RollupExpression {}: invalid reference: Expression {} doesn't exist", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::ROLLUP_EXPRESSION && ref.counter >= i) {
							log.error(L"RollupExpression {} can't reference RollupExpression {}", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::EXPRESSION || ref.type == Reference::Type::ROLLUP_EXPRESSION) {
							if (!ref.useOrigName) {
								CharUpperW(&ref.name[0]);
							}
						}
					}
				}
			);
		} catch (std::runtime_error&) {
			expressions[i] = {};
		}
	}
}

void RollupExpressionResolver::setExpressions(OptionList expressionsList) {
	parseExpressions(expressions, expressionsList, L"RollupExpression");

	checkExpressionIndices();
}

double RollupExpressionResolver::calculateExpressionRollup(
	index expressionIndex,
	const RollupFunction rollupFunction,
	const InstanceInfo& instance
) const {
	return doRollup(
		rollupFunction, instance.indices, instance.vectorIndices, [&](Indices item) {
			return simpleExpressionSolver.solveExpression(expressionIndex, item);
		}
	);
}

double RollupExpressionResolver::resolveReference(const Reference& ref, const InstanceInfo& instance) const {
	if (ref.total) {
		switch (ref.type) {
		case Reference::Type::COUNTER_RAW: return calculateAndCacheTotal(TotalSource::eRAW_COUNTER, ref.counter, ref.rollupFunction);
		case Reference::Type::COUNTER_FORMATTED: return calculateAndCacheTotal(TotalSource::eFORMATTED_COUNTER, ref.counter, ref.rollupFunction);
		case Reference::Type::EXPRESSION: return calculateAndCacheTotal(TotalSource::eEXPRESSION, ref.counter, ref.rollupFunction);
		case Reference::Type::ROLLUP_EXPRESSION: return calculateAndCacheTotal(TotalSource::eROLLUP_EXPRESSION, ref.counter, ref.rollupFunction);
		case Reference::Type::COUNT: return calculateRollupCountTotal(ref.rollupFunction);
		}
		return 0.0;
	}

	if (ref.named) {
		const auto instancePtr = instanceManager.findInstanceByName(ref, true);
		if (instancePtr == nullptr) {
			return 0.0;
		}

		instance = *instancePtr;
	}

	switch (ref.type) {
	case Reference::Type::COUNTER_RAW:
		return doRollup(
			ref.rollupFunction, instance.indices, instance.vectorIndices, [&](Indices item) {
				return getRaw(ref.counter, item);
			}
		);
	case Reference::Type::COUNTER_FORMATTED:
		return doRollup(
			ref.rollupFunction, instance.indices, instance.vectorIndices, [&](Indices item) {
				return getFormatted(ref.counter, item);
			}
		);
	case Reference::Type::EXPRESSION:
		return doRollup(
			ref.rollupFunction, instance.indices, instance.vectorIndices, [&](Indices item) {
				return simpleExpressionSolver.solveExpression(ref.counter, item);
			}
		);
	case Reference::Type::ROLLUP_EXPRESSION: return solveExpression(ref.counter, instance);
	case Reference::Type::COUNT: return double(instance.vectorIndices.size() + 1);
	}

	log.error(L"unexpected reference type in resolveReference(): {}", ref.type);
	return 0.0;
}

double RollupExpressionResolver::getRaw(index counterIndex, Indices originalIndexes) const {
	return instanceManager.calculateRaw(counterIndex, originalIndexes);
}

double RollupExpressionResolver::getFormatted(index counterIndex, Indices originalIndexes) const {
	return instanceManager.calculateFormatted(counterIndex, originalIndexes);
}

double RollupExpressionResolver::calculateRollupCountTotal(const RollupFunction rollupFunction) const {
	switch (rollupFunction) {
	case RollupFunction::eSUM: return static_cast<double>(instanceManager.getRollupInstances().size());
	case RollupFunction::eAVERAGE:
		return static_cast<double>(instanceManager.getInstances().size())
			/ double(instanceManager.getRollupInstances().size());
	case RollupFunction::eMINIMUM: {
		index min = std::numeric_limits<index>::max();
		for (const auto& item : instanceManager.getRollupInstances()) {
			index val = index(item.vectorIndices.size()) + 1;
			min = std::min(min, val);
		}
		return static_cast<double>(min);
	}
	case RollupFunction::eMAXIMUM: {
		index max = 0;
		for (const auto& item : instanceManager.getRollupInstances()) {
			index val = index(item.vectorIndices.size()) + 1;
			max = std::max(max, val);
		}
		return static_cast<double>(max);
	}
	case RollupFunction::eFIRST:
		if (!instanceManager.getRollupInstances().empty()) {
			return static_cast<double>(instanceManager.getRollupInstances()[0].vectorIndices.size() + 1);
		}
		return 0.0;
	}

	return 0.0;
}

double RollupExpressionResolver::solveExpression(index expressionIndex, const InstanceInfo& instanceInfo) const {
	ReferenceResolver referenceResolver{ *this, instanceInfo };
	return expressions[expressionIndex].solve(&referenceResolver);
}
