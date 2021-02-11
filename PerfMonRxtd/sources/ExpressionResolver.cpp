/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ExpressionResolver.h"


#include "expressions/ASTSolver.h"
#include "expressions/GrammarBuilder.h"
#include "option-parsing/OptionList.h"

using namespace perfmon;

bool ExpressionResolver::CacheEntry::operator<(const CacheEntry& other) const {
	return source < other.source
		&& counterIndex < other.counterIndex
		&& rollupFunction < other.rollupFunction;
}

ExpressionResolver::ExpressionResolver(Logger log, const InstanceManager& instanceManager) :
	log(std::move(log)),
	instanceManager(instanceManager) {

	common::expressions::GrammarBuilder builder;
	using Data = common::expressions::OperatorInfo::Data;

	// Previously expressions could only parse: +, -, *, /, ^
	// So let's limit it to these operators

	builder.pushBinary(L"+", [](Data d1, Data d2) { return Data{ d1.value + d2.value }; });
	builder.pushBinary(L"-", [](Data d1, Data d2) { return Data{ d1.value - d2.value }; });

	builder.increasePrecedence();
	builder.pushBinary(L"*", [](Data d1, Data d2) { return Data{ d1.value * d2.value }; });
	builder.pushBinary(L"/", [](Data d1, Data d2) { return Data{ (d1.value == 0.0 ? 0.0 : d1.value / d2.value) }; });

	builder.increasePrecedence();
	builder.pushPrefix(L"+", [](Data d1, Data d2) { return d1; });
	builder.pushPrefix(L"-", [](Data d1, Data d2) { return Data{ -d1.value }; });

	builder.increasePrecedence();
	builder.pushBinary(L"^", [](Data d1, Data d2) { return Data{ std::pow(d1.value, d2.value) }; }, false);

	builder.pushGrouping(L"(", L")", L",");

	parser.setGrammar(std::move(builder).takeResult(), false);
}

index ExpressionResolver::getExpressionsCount() const {
	return index(expressions.size());
}

index ExpressionResolver::getRollupExpressionsCount() const {
	return index(rollupExpressions.size());
}

void ExpressionResolver::resetCaches() {
	totalsCache.clear();
}

ExpressionResolver::ASTSolver ExpressionResolver::parseExpressionTree(sview expressionString, sview loggerName, index loggerIndex) {
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

void ExpressionResolver::parseExpressions(std::vector<ASTSolver>& vector, OptionList list, sview loggerName) {
	vector.resize(list.size());
	for (index i = 0; i < index(list.size()); ++i) {
		vector[i] = parseExpressionTree(list.get(i).asString(), loggerName, i);
	}
}

void ExpressionResolver::checkExpressionIndices() {
	for (index i = 0; i < index(expressions.size()); ++i) {
		try {
			using CustomNode = common::expressions::ast_nodes::CustomTerminalNode;

			expressions[i].peekTree().visitNodes(
				[&](const auto& node) {
					using Type = std::decay<decltype(node)>;
					if constexpr (std::is_same<Type, CustomNode>::value) {
						CustomNode& n = node;
						auto& ref = *std::any_cast<Reference>(&n.value);

						if (ref.type == Reference::Type::EXPRESSION && ref.counter >= i) {
							log.error(L"Expression {}: invalid reference to Expression {}", i, ref.counter);
							throw std::runtime_error{ "" };
						}

						if (ref.type == Reference::Type::ROLLUP_EXPRESSION) {
							log.error(L"Expression {}: invalid reference to RollupExpressions", i);
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

	for (index i = 0; i < index(rollupExpressions.size()); ++i) {
		try {
			using CustomNode = common::expressions::ast_nodes::CustomTerminalNode;

			rollupExpressions[i].peekTree().visitNodes(
				[&](const auto& node) {
					using Type = std::decay<decltype(node)>;
					if constexpr (std::is_same<Type, CustomNode>::value) {
						CustomNode& n = node;
						auto& ref = *std::any_cast<Reference>(&n.value);

						if (ref.type == Reference::Type::EXPRESSION && ref.counter >= index(expressions.size())) {
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
			rollupExpressions[i] = {};
		}
	}
}

void ExpressionResolver::setExpressions(
	OptionList expressionsList,
	OptionList rollupExpressionsList
) {
	// TODO check count of expressions

	parseExpressions(expressions, expressionsList, L"Expression");
	parseExpressions(rollupExpressions, rollupExpressionsList, L"RollupExpression");

	checkExpressionIndices();
}

double ExpressionResolver::getValue(const Reference& ref, const InstanceInfo* instancePtr, Logger& logger) const {
	const auto rollup = instanceManager.isRollup();

	if (ref.total) {
		switch (ref.type) {
		case Reference::Type::COUNTER_RAW: return calculateAndCacheTotal(TotalSource::eRAW_COUNTER, ref.counter, ref.rollupFunction);
		case Reference::Type::COUNTER_FORMATTED: return calculateAndCacheTotal(TotalSource::eFORMATTED_COUNTER, ref.counter, ref.rollupFunction);
		case Reference::Type::EXPRESSION: return calculateAndCacheTotal(TotalSource::eEXPRESSION, ref.counter, ref.rollupFunction);
		case Reference::Type::ROLLUP_EXPRESSION: return calculateAndCacheTotal(TotalSource::eROLLUP_EXPRESSION, ref.counter, ref.rollupFunction);
		case Reference::Type::COUNT: {
			if (rollup) {
				return calculateRollupCountTotal(ref.rollupFunction);
			} else {
				return calculateCountTotal(ref.rollupFunction);
			}
		}
		}
		return 0.0;
	}

	if (instancePtr == nullptr) {
		return 0.0;
	}
	auto& instance = *instancePtr;

	// only needed for EXPRESSION and ROLLUP_EXPRESSION
	expressionCurrentItem = instancePtr;

	if (rollup) {
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
		case Reference::Type::EXPRESSION: return calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		case Reference::Type::ROLLUP_EXPRESSION: return calculateExpression < &ExpressionResolver::resolveRollupReference > (rollupExpressions[ref.counter]);
		case Reference::Type::COUNT: return static_cast<double>(instancePtr->vectorIndices.size() + 1);
		}
	} else {
		switch (ref.type) {
		case Reference::Type::COUNTER_RAW: return getRaw(ref.counter, instancePtr->indices);
		case Reference::Type::COUNTER_FORMATTED: return getFormatted(ref.counter, instancePtr->indices);
		case Reference::Type::EXPRESSION: return calculateExpression < &ExpressionResolver::resolveReference > (expressions[ref.counter]);
		case Reference::Type::ROLLUP_EXPRESSION: return 0.0; // todo throw
		case Reference::Type::COUNT: return 1.0;
		}
	}

	// todo
	// if (!indexIsInBounds(ref.counter, 0, instanceManager.getCountersCount() - 1)) {
	// 	logger.error(L"Trying to get a non-existing counter {}", ref.counter);
	// 	return 0.0;
	// }

	return 0.0;
}

double ExpressionResolver::getRaw(index counterIndex, Indices originalIndexes) const {
	return static_cast<double>(instanceManager.calculateRaw(counterIndex, originalIndexes));
}

double ExpressionResolver::getFormatted(index counterIndex, Indices originalIndexes) const {
	return instanceManager.calculateFormatted(counterIndex, originalIndexes);
}

double ExpressionResolver::calculateTotal(const TotalSource source, index counterIndex, const RollupFunction rollupFunction) const {
	switch (source) {
	case TotalSource::eRAW_COUNTER: return calculateTotal<&ExpressionResolver::getRaw>(rollupFunction, counterIndex);
	case TotalSource::eFORMATTED_COUNTER: return calculateTotal<&ExpressionResolver::getFormatted>(
			rollupFunction, counterIndex
		);
	case TotalSource::eEXPRESSION: return calculateExpressionTotal<&ExpressionResolver::calculateExpression<&
			ExpressionResolver::resolveReference>>(rollupFunction, expressions[counterIndex], false);
	case TotalSource::eROLLUP_EXPRESSION: return calculateExpressionTotal<&ExpressionResolver::calculateExpression<&
			ExpressionResolver::resolveRollupReference>>(rollupFunction, expressions[counterIndex], true);
	}
	return 0.0;
}

double ExpressionResolver::calculateAndCacheTotal(TotalSource source, index counterIndex, RollupFunction rollupFunction) const {
	auto& totalOpt = totalsCache[{ source, counterIndex, rollupFunction }];
	if (totalOpt.has_value()) {
		// already calculated
		return totalOpt.value();
	}

	totalOpt = calculateTotal(source, counterIndex, rollupFunction);

	return totalOpt.value();
}

double ExpressionResolver::resolveReference(const Reference& ref) const {
	using Type = Reference::Type;

	if (ref.type == Type::COUNTER_FORMATTED && !instanceManager.canGetFormatted()) {
		return 0.0;
	}

	if (ref.total) {
		switch (ref.type) {
		case Type::COUNTER_RAW: return calculateAndCacheTotal(TotalSource::eRAW_COUNTER, ref.counter, ref.rollupFunction);
		case Type::COUNTER_FORMATTED: return calculateAndCacheTotal(TotalSource::eFORMATTED_COUNTER, ref.counter, ref.rollupFunction);
		case Type::EXPRESSION: return calculateAndCacheTotal(TotalSource::eEXPRESSION, ref.counter, ref.rollupFunction);
		case Type::ROLLUP_EXPRESSION: break; // todo
		case Type::COUNT: return calculateCountTotal(ref.rollupFunction);
		}
	}

	if (ref.named) {
		const auto instance = instanceManager.findInstanceByName(ref, false);
		if (instance == nullptr) {
			return 0.0;
		}

		switch (ref.type) {
		case Type::COUNTER_RAW: return static_cast<double>(getRaw(ref.counter, instance->indices));
		case Type::COUNTER_FORMATTED: return getFormatted(ref.counter, instance->indices);
		case Type::EXPRESSION: {
			const InstanceInfo* const savedInstance = expressionCurrentItem;
			expressionCurrentItem = instance;
			const double res = calculateExpression < &ExpressionResolver::resolveReference > (expressions[ref.counter]);
			expressionCurrentItem = savedInstance;
			return res;
		}
		case Type::ROLLUP_EXPRESSION: break; // todo
		case Type::COUNT: return 1.0;
		}
	}


	switch (ref.type) {
	case Type::COUNTER_RAW: return static_cast<double>(getRaw(ref.counter, expressionCurrentItem->indices));
	case Type::COUNTER_FORMATTED: return getFormatted(ref.counter, expressionCurrentItem->indices);
	case Type::EXPRESSION: return calculateExpression < &ExpressionResolver::resolveReference > (expressions[ref.counter]);
	case Type::ROLLUP_EXPRESSION: break; // todo
	case Type::COUNT: return 1.0;
	}

	log.error(L"unexpected reference type in resolveReference(): {}", ref.type);
	return 0.0; // todo throw
}

double ExpressionResolver::calculateExpressionRollup(
	const ExpressionTreeNode& expression,
	const RollupFunction rollupFunction
) const {
	const InstanceInfo& instance = *expressionCurrentItem;
	InstanceInfo tmp;
	// we only need InstanceKeyItem::originalIndexes member to solve usual expressions 
	// but let's use whole InstanceKeyItem
	expressionCurrentItem = &tmp;

	const auto value = doRollup(
		rollupFunction, instance.indices, instance.vectorIndices, [&](Indices item) {
			tmp.indices = item;
			return calculateExpression < &ExpressionResolver::resolveReference > (expression);
		}
	);

	expressionCurrentItem = &instance;
	return value;
}

double ExpressionResolver::calculateCountTotal(const RollupFunction rollupFunction) const {
	switch (rollupFunction) {
	case RollupFunction::eSUM: return static_cast<double>(instanceManager.getInstances().size());
	case RollupFunction::eAVERAGE:
	case RollupFunction::eMINIMUM:
	case RollupFunction::eMAXIMUM:
	case RollupFunction::eFIRST: return 1.0;
	}
	return 0.0;
}

double ExpressionResolver::calculateRollupCountTotal(const RollupFunction rollupFunction) const {
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

double ExpressionResolver::resolveRollupReference(const Reference& ref) const {
	if (ref.type == Reference::Type::COUNTER_FORMATTED && !instanceManager.canGetFormatted()) {
		return 0.0;
	}

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

	if (!ref.named) {
		auto& instance = *expressionCurrentItem;
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
		case Reference::Type::EXPRESSION: return calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		case Reference::Type::ROLLUP_EXPRESSION: return calculateExpression < &ExpressionResolver::resolveRollupReference > (rollupExpressions[ref.counter]);
		case Reference::Type::COUNT: return static_cast<double>(expressionCurrentItem->vectorIndices.size() + 1);
		}
		return 0.0;
	}

	const auto instancePtr = instanceManager.findInstanceByName(ref, true);
	if (instancePtr == nullptr) {
		return 0.0;
	}

	auto& instance = *instancePtr;

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
	case Reference::Type::EXPRESSION: {
		const InstanceInfo* const savedInstance = expressionCurrentItem;
		expressionCurrentItem = instancePtr;
		double res = calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		expressionCurrentItem = savedInstance;
		return res;
	}
	case Reference::Type::ROLLUP_EXPRESSION: {
		const InstanceInfo* const savedInstance = expressionCurrentItem;
		expressionCurrentItem = instancePtr;
		double res = calculateExpression < &ExpressionResolver::resolveRollupReference > (rollupExpressions[ref.counter]);
		expressionCurrentItem = savedInstance;
		return res;
	}
	case Reference::Type::COUNT: return double(instancePtr->vectorIndices.size() + 1);
	}

	log.error(L"unexpected reference type in resolveReference(): {}", ref.type);
	return 0.0;
}

template<double (ExpressionResolver::* calculateValueFunction)(index counterIndex, Indices originalIndexes) const>
double ExpressionResolver::calculateTotal(RollupFunction rollupType, index counterIndex) const {
	auto vectorInstanceKeys = instanceManager.getInstances();

	return doTotal(
		rollupType, vectorInstanceKeys, [&](InstanceInfo item) {
			return (this->*calculateValueFunction)(counterIndex, item.indices);
		}
	);
}

template<double(ExpressionResolver::* calculateExpressionFunction)(const ExpressionTreeNode& expression) const>
double ExpressionResolver::calculateExpressionTotal(
	const RollupFunction rollupType,
	const ExpressionTreeNode& expression, bool rollup
) const {
	auto vectorInstanceKeys = rollup ? instanceManager.getRollupInstances() : instanceManager.getInstances();

	return doTotal(
		rollupType, vectorInstanceKeys, [&](InstanceInfo item) {
			expressionCurrentItem = &item;
			return (this->*calculateExpressionFunction)(expression);
		}
	);
}

double ExpressionResolver::solveExpression(index expressionIndex) const {
	ReferenceResolver referenceResolver{ *this, expressionCurrentItem, log };

	return expressions[expressionIndex].solve(&referenceResolver);
}

bool ExpressionResolver::indexIsInBounds(index ind, index min, index max) {
	return ind >= min && ind <= max;
}
