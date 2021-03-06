// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2021 Danil Uzlov

#include "RollupExpressionResolver.h"


#include "SimpleExpressionSolver.h"
#include "TotalUtilities.h"
#include "rxtd/expression_parser/ASTSolver.h"
#include "rxtd/option_parsing/OptionList.h"

using rxtd::perfmon::expressions::RollupExpressionResolver;

rxtd::index RollupExpressionResolver::getExpressionsCount() const {
	return static_cast<index>(expressions.size());
}

void RollupExpressionResolver::resetCaches() {
	totalCaches.reset();
}

RollupExpressionResolver::ASTSolver RollupExpressionResolver::parseExpressionTree(sview expressionString, sview loggerName, index loggerIndex) {
	using expression_parser::ASTSolver;

	try {
		parser.parse(expressionString);

		ASTSolver solver{ parser.takeTree() };
		solver.optimize(nullptr);

		return solver;
	} catch (expression_parser::Lexer::Exception& e) {
		log.error(
			L"{} {} can't be parsed: unknown token here: '{}'",
			loggerName, loggerIndex, expressionString.substr(e.getPosition())
		);
	} catch (expression_parser::ASTParser::Exception& e) {
		log.error(
			L"{} {} can't be parsed: {}, at position: '{}'",
			loggerName, loggerIndex, e.getReason(), expressionString.substr(e.getPosition())
		);
	}

	return {};
}

void RollupExpressionResolver::parseExpressions(std::vector<ASTSolver>& vector, OptionList list, sview loggerName) {
	vector.resize(static_cast<size_t>(list.size()));
	for (index i = 0; i < static_cast<index>(list.size()); ++i) {
		vector[static_cast<size_t>(i)] = parseExpressionTree(list.get(i).asString(), loggerName, i);
	}
}

void RollupExpressionResolver::checkExpressionIndices() {
	array_span<ASTSolver> expressionsView = expressions;
	for (index i = 0; i < expressionsView.size(); ++i) {
		try {
			using CustomNode = expression_parser::ast_nodes::CustomTerminalNode;

			expressionsView[i].peekTree().visitNodes(
				[&](const auto& node) {
					using Type = std::decay<decltype(node)>;
					if constexpr (std::is_same<Type, CustomNode>::value) {
						CustomNode& n = node;
						auto& ref = *std::any_cast<Reference>(&n.value);

						if (ref.counter < 0) {
							log.error(L"Expression {}: invalid reference to Expression {}", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::eEXPRESSION && ref.counter >= simpleExpressionSolver.getExpressionsCount()) {
							log.error(L"RollupExpression {}: invalid reference: Expression {} doesn't exist", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::eROLLUP_EXPRESSION && ref.counter >= i) {
							log.error(L"RollupExpression {} can't reference RollupExpression {}", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::eEXPRESSION || ref.type == Reference::Type::eROLLUP_EXPRESSION) {
							if (!ref.useOrigName) {
								std_fixes::StringUtils::makeUppercaseInPlace(ref.namePattern.getName());
							}
						}
					}
				}
			);
		} catch (std::runtime_error&) {
			expressionsView[i] = {};
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
	array_view<Indices> indices
) const {
	return TotalUtilities::calculateTotal(
		indices, rollupFunction, [&](Indices item) {
			return simpleExpressionSolver.solveExpression(expressionIndex, item);
		}
	);
}

double RollupExpressionResolver::resolveReference(const Reference& ref, array_view<Indices> indices) const {
	Reference totalRef;
	totalRef.type = ref.type;
	totalRef.counter = ref.counter;
	totalRef.rollupFunction = ref.rollupFunction;

	if (ref.total) {
		switch (ref.type) {
		case Reference::Type::eCOUNTER_RAW:
		case Reference::Type::eCOUNTER_FORMATTED:
		case Reference::Type::eEXPRESSION: return TotalUtilities::getTotal(
				totalCaches.simpleExpression, rollupInstanceManager.getRollupInstances(), ref.counter, ref.rollupFunction,
				[&](const RollupInstanceInfo& info) {
					return TotalUtilities::calculateTotal(
						array_view<Indices>{ info.indices }, ref.rollupFunction, [&](Indices ind) {
							return simpleExpressionSolver.resolveReference(totalRef, ind);
						}
					);
				}
			);
		case Reference::Type::eROLLUP_EXPRESSION: return TotalUtilities::getTotal(
				totalCaches.expression, rollupInstanceManager.getRollupInstances(), ref.counter, ref.rollupFunction,
				[&](const RollupInstanceInfo& info) { return solveExpression(ref.counter, info.indices); }
			);
		case Reference::Type::eCOUNT: return calculateRollupCountTotal(ref.rollupFunction);
		}
		return 0.0;
	}

	if (!ref.namePattern.isEmpty()) {
		if (ref.discarded) {
			const auto instancePtr = simpleInstanceManager.findSimpleInstanceByName(ref);
			if (instancePtr == nullptr) {
				return 0.0;
			}
			indices = array_view<Indices>{ &instancePtr->indices, 1 };
		} else {
			const auto instancePtr = rollupInstanceManager.findRollupInstanceByName(ref);
			if (instancePtr == nullptr) {
				return 0.0;
			}
			indices = instancePtr->indices;
		}
	}

	switch (ref.type) {
	case Reference::Type::eCOUNTER_RAW:
	case Reference::Type::eCOUNTER_FORMATTED:
	case Reference::Type::eEXPRESSION: return TotalUtilities::calculateTotal(
			indices, ref.rollupFunction, [&](Indices ind) {
				return simpleExpressionSolver.resolveReference(totalRef, ind);
			}
		);
	case Reference::Type::eROLLUP_EXPRESSION: return solveExpression(ref.counter, indices);
	case Reference::Type::eCOUNT: return static_cast<double>(indices.size());
	}

	log.error(L"unexpected reference type in resolveReference(): {}", ref.type);
	return 0.0;
}

double RollupExpressionResolver::calculateRollupCountTotal(const RollupFunction rollupFunction) const {
	auto instances = rollupInstanceManager.getRollupInstances();
	if (instances.empty()) {
		return 0.0;
	}

	switch (rollupFunction) {
	case RollupFunction::eSUM: return static_cast<double>(instances.size());
	case RollupFunction::eAVERAGE:
		return static_cast<double>(simpleInstanceManager.getInstances().size())
			/ static_cast<double>(instances.size());
	case RollupFunction::eMINIMUM: {
		index min = std::numeric_limits<index>::max();
		for (const auto& item : instances) {
			index val = static_cast<index>(item.indices.size()) + 1;
			min = std::min(min, val);
		}
		return static_cast<double>(min);
	}
	case RollupFunction::eMAXIMUM: {
		index max = 0;
		for (const auto& item : instances) {
			index val = static_cast<index>(item.indices.size()) + 1;
			max = std::max(max, val);
		}
		return static_cast<double>(max);
	}
	case RollupFunction::eFIRST:
		return static_cast<double>(instances[0].indices.size() + 1);
	}

	return 0.0;
}

double RollupExpressionResolver::solveExpression(index expressionIndex, array_view<Indices> indices) const {
	ReferenceResolver referenceResolver{ *this, indices };
	return expressions[static_cast<size_t>(expressionIndex)].solve(&referenceResolver);
}
