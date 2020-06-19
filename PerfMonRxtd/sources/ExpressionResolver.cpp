/*
 * Copyright (C) 2018-2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ExpressionResolver.h"

#include "undef.h"

using namespace perfmon;

bool ExpressionResolver::CacheEntry::operator<(const CacheEntry& other) const {
	return source < other.source
		&& counterIndex < other.counterIndex
		&& rollupFunction < other.rollupFunction;
}

ExpressionResolver::ExpressionResolver(utils::Rainmeter::Logger& log, const InstanceManager& instanceManager) :
	log(log),
	instanceManager(instanceManager) { }

counter_t ExpressionResolver::getExpressionsCount() const {
	return counter_t(expressions.size());
}

counter_t ExpressionResolver::getRollupExpressionsCount() const {
	return counter_t(rollupExpressions.size());
}

void ExpressionResolver::resetCaches() {
	totalsCache.clear();
}

double ExpressionResolver::getValue(const Reference& ref, const InstanceInfo* instance, utils::Rainmeter::Logger& logger) const {
	const auto rollup = instanceManager.isRollup();

	switch (ref.type) {
	case ReferenceType::COUNT:
		if (ref.total) {
			if (rollup) {
				return calculateRollupCountTotal(ref.rollupFunction);
			} else {
				return calculateCountTotal(ref.rollupFunction);
			}
		} else {
			if (instance == nullptr) {
				return 0.0;
			}
			if (rollup) {
				return static_cast<double>(instance->vectorIndices.size() + 1);
			} else {
				return 1.0;
			}
		}

	case ReferenceType::COUNTER_RAW:
		if (!indexIsInBounds(ref.counter, 0, instanceManager.getCountersCount() - 1)) {
			logger.error(L"Trying to get a non-existing counter {}", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::eRAW_COUNTER, ref.counter, ref.rollupFunction);
		}
		if (instance == nullptr) {
			return 0.0;
		}
		if (rollup) {
			return calculateRollup<&ExpressionResolver::getRaw>(ref.rollupFunction, ref.counter, *instance);
		}
		return getRaw(ref.counter, instance->indices);

	case ReferenceType::COUNTER_FORMATTED:
		if (!indexIsInBounds(ref.counter, 0, instanceManager.getCountersCount() - 1)) {
			logger.error(L"Trying to get a non-existing counter {}", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::eFORMATTED_COUNTER, ref.counter, ref.rollupFunction);
		}
		if (instance == nullptr) {
			return 0.0;
		}
		if (rollup) {
			return calculateRollup<&ExpressionResolver::getFormatted>(ref.rollupFunction, ref.counter, *instance);
		}
		return getFormatted(ref.counter, instance->indices);

	case ReferenceType::EXPRESSION:
		if (!indexIsInBounds(ref.counter, 0, index(expressions.size()))) {
			logger.error(L"Trying to get a non-existing expression {}", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::eEXPRESSION, ref.counter, ref.rollupFunction);
		}
		if (instance == nullptr) {
			return 0.0;
		}
		expressionCurrentItem = instance;
		if (rollup) {
			return calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		}
		return calculateExpression<&ExpressionResolver::resolveReference>(expressions[ref.counter]);

	case ReferenceType::ROLLUP_EXPRESSION:
		if (!indexIsInBounds(ref.counter, 0, index(rollupExpressions.size()) - 1)) {
			logger.error(L"Trying to get a non-existing expression {}", ref.counter);
			return 0.0;
		}
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::eROLLUP_EXPRESSION, ref.counter, ref.rollupFunction);
		}
		if (rollup) {
			if (instance == nullptr) {
				return 0.0;
			}
			expressionCurrentItem = instance;
			return calculateExpression<&ExpressionResolver::resolveRollupReference>(rollupExpressions[ref.counter]);
		}
		logger.error(L"RollupExpression can't be evaluated without rollup");
		return 0.0;

	case ReferenceType::UNKNOWN:
		return 0.0;

	default:
		logger.error(L"unexpected refType in getValue(): {}", ref.type);
		return 0.0;
	}
}

void ExpressionResolver::setExpressions(utils::OptionList expressionsList,
	utils::OptionList rollupExpressionsList) { // TODO check count of expressions
	expressions.resize(expressionsList.size());
	for (index i = 0; i < expressionsList.size(); ++i) {
		ExpressionParser parser(expressionsList.get(i));
		parser.parse();
		if (parser.isError()) {
			log.error(L"Expression {} can't be parsed", i);
			ExpressionTreeNode node;
			node.type = ExpressionType::NUMBER;
			node.number = 0;
			expressions[i] = node;
			continue;
		}
		ExpressionTreeNode expression = parser.getExpression();
		expression.simplify();

		const auto maxRef = expression.maxExpRef();
		if (maxRef >= i) {
			log.error(L"Expression {} can't reference Expression {}", i, maxRef);
			ExpressionTreeNode node;
			node.type = ExpressionType::NUMBER;
			node.number = 0;
			expressions[i] = node;
			continue;
		}

		const auto maxRURef = expression.maxExpRef();
		if (maxRURef >= 0) {
			log.error(L"Expressions can't reference RollupExpressions");
			ExpressionTreeNode node;
			node.type = ExpressionType::NUMBER;
			node.number = 0;
			expressions[i] = node;
			continue;
		}

		expression.processRefs([](Reference& ref) {
			if (!ref.useOrigName) {
				CharUpperW(&ref.name[0]);
			}
		});
		expressions[i] = expression;
	}

	rollupExpressions.resize(rollupExpressionsList.size());
	for (index i = 0; i < rollupExpressionsList.size(); ++i) {
		ExpressionParser parser(rollupExpressionsList.get(i));
		parser.parse();
		if (parser.isError()) {
			log.error(L"RollupExpression {} can't be parsed", i);
			ExpressionTreeNode node;
			node.type = ExpressionType::NUMBER;
			node.number = 0;
			expressions[i] = node;
			continue;
		}
		ExpressionTreeNode expression = parser.getExpression();
		expression.simplify();

		const auto maxRef = expression.maxExpRef();
		if (maxRef >= index(expressions.size())) {
			log.error(L"Expression {} doesn't exist", i);
			ExpressionTreeNode node;
			node.type = ExpressionType::NUMBER;
			node.number = 0;
			expressions[i] = node;
			continue;
		}

		const auto maxRURef = expression.maxRUERef();
		if (maxRURef >= i) {
			log.error(L"RollupExpression {} can't reference RollupExpression {}", i, maxRef);
			ExpressionTreeNode node;
			node.type = ExpressionType::NUMBER;
			node.number = 0;
			expressions[i] = node;
			continue;
		}
		expression.processRefs([](Reference& ref) {
			if (!ref.useOrigName) {
				CharUpperW(&ref.name[0]);
			}
		});
		rollupExpressions[i] = expression;
	}
}

double ExpressionResolver::getRaw(counter_t counterIndex, Indices originalIndexes) const {
	return static_cast<double>(instanceManager.calculateRaw(counterIndex, originalIndexes));
}

double ExpressionResolver::getFormatted(counter_t counterIndex, Indices originalIndexes) const {
	return instanceManager.calculateFormatted(counterIndex, originalIndexes);
}

double ExpressionResolver::getRawRollup(RollupFunction rollupType, counter_t counterIndex,
	const InstanceInfo& instance) const {
	return calculateRollup<&ExpressionResolver::getRaw>(rollupType, counterIndex, instance);
}

double ExpressionResolver::getFormattedRollup(RollupFunction rollupType, counter_t counterIndex,
	const InstanceInfo& instance) const {
	return calculateRollup<&ExpressionResolver::getFormatted>(rollupType, counterIndex, instance);
}

double ExpressionResolver::getExpressionRollup(RollupFunction rollupType, counter_t expressionIndex,
	const InstanceInfo& instance) const {
	expressionCurrentItem = &instance;
	return calculateExpressionRollup(expressions[expressionIndex], rollupType);
}

double ExpressionResolver::getExpression(counter_t expressionIndex, const InstanceInfo& instance) const {
	expressionCurrentItem = &instance;
	return calculateExpression<&ExpressionResolver::resolveReference>(expressions[expressionIndex]);
}

double ExpressionResolver::getRollupExpression(counter_t expressionIndex,
	const InstanceInfo& instance) const {
	expressionCurrentItem = &instance;
	return calculateExpression<&ExpressionResolver::resolveRollupReference>(rollupExpressions[expressionIndex]);
}

double ExpressionResolver::calculateTotal(const TotalSource source, counter_t counterIndex, const RollupFunction rollupFunction) const {
	switch (source) {
	case TotalSource::eRAW_COUNTER: return calculateTotal<&ExpressionResolver::getRaw>(rollupFunction, counterIndex);
	case TotalSource::eFORMATTED_COUNTER: return calculateTotal<&ExpressionResolver::getFormatted>(
		rollupFunction, counterIndex);
	case TotalSource::eEXPRESSION: return calculateExpressionTotal<&ExpressionResolver::calculateExpression<&
		ExpressionResolver::resolveReference>>(rollupFunction, expressions[counterIndex], false);
	case TotalSource::eROLLUP_EXPRESSION: return calculateExpressionTotal<&ExpressionResolver::calculateExpression<&
		ExpressionResolver::resolveRollupReference>>(rollupFunction, expressions[counterIndex], true);
	default:
		log.error(L"unexpected TotalSource {}", source);
		return 0.0;
	}
}

double ExpressionResolver::calculateAndCacheTotal(TotalSource source, counter_t counterIndex, RollupFunction rollupFunction) const {
	auto &totalOpt = totalsCache[{ source, counterIndex, rollupFunction }];
	if (totalOpt.has_value()) {
		// already calculated
		return totalOpt.value();
	}

	totalOpt = calculateTotal(source, counterIndex, rollupFunction);

	return totalOpt.value();
}

double ExpressionResolver::resolveReference(const Reference& ref) const {
	if (ref.type == ReferenceType::UNKNOWN) {
		log.error(L"unknown reference being solved");
		return 0.0;
	}
	if (ref.type == ReferenceType::COUNT) {
		if (ref.total) {
			return calculateCountTotal(ref.rollupFunction);
		}
		if (!ref.named) {
			return 1;
		}
		return instanceManager.findInstanceByName(ref, false) != nullptr;
	}
	if (ref.type == ReferenceType::COUNTER_RAW || ref.type == ReferenceType::COUNTER_FORMATTED) {
		if (ref.type == ReferenceType::COUNTER_FORMATTED && !instanceManager.canGetFormatted()) {
			return 0.0;
		}
		if (ref.total) {
			if (ref.type == ReferenceType::COUNTER_RAW) {
				return calculateAndCacheTotal(TotalSource::eRAW_COUNTER, ref.counter, ref.rollupFunction);
			} else {
				return calculateAndCacheTotal(TotalSource::eFORMATTED_COUNTER, ref.counter, ref.rollupFunction);
			}
		}
		if (!ref.named) {
			if (ref.type == ReferenceType::COUNTER_RAW) {
				return static_cast<double>(getRaw(ref.counter, expressionCurrentItem->indices));
			} else {
				return getFormatted(ref.counter, expressionCurrentItem->indices);
			}
		}
		const auto instance = instanceManager.findInstanceByName(ref, false);
		if (instance == nullptr) {
			return 0.0;
		}
		if (ref.type == ReferenceType::COUNTER_RAW) {
			return static_cast<double>(getRaw(ref.counter, instance->indices));
		} else {
			return getFormatted(ref.counter, instance->indices);
		}
	}
	if (ref.type == ReferenceType::EXPRESSION) {
		if (ref.total) {
			return calculateAndCacheTotal(TotalSource::eEXPRESSION, ref.counter, ref.rollupFunction);
		}
		if (!ref.named) {
			return calculateExpression<&ExpressionResolver::resolveReference>(expressions[ref.counter]);
		}
		const auto instance = instanceManager.findInstanceByName(ref, false);
		if (instance == nullptr) {
			return 0.0;
		}
		const InstanceInfo* const savedInstance = expressionCurrentItem;
		expressionCurrentItem = instance;
		const double res = calculateExpression<&ExpressionResolver::resolveReference>(expressions[ref.counter]);
		expressionCurrentItem = savedInstance;
		return res;
	}
	log.error(L"unexpected reference type in resolveReference(): {}", ref.type);
	return 0.0;
}

double ExpressionResolver::calculateExpressionRollup(const ExpressionTreeNode& expression,
	const RollupFunction rollupFunction) const {
	const InstanceInfo& instance = *expressionCurrentItem;
	InstanceInfo tmp; // we only need InstanceKeyItem::originalIndexes member to solve usual expressions 
	// but let's use whole InstanceKeyItem
	expressionCurrentItem = &tmp;
	tmp.indices = instance.indices;
	double value = calculateExpression<&ExpressionResolver::resolveReference>(expression);

	switch (rollupFunction) {
	case RollupFunction::eSUM:
	{
		for (const auto& item : instance.vectorIndices) {
			tmp.indices = item;
			value += calculateExpression<&ExpressionResolver::resolveReference>(expression);
		}
		break;
	}
	case RollupFunction::eAVERAGE:
	{
		for (const auto& item : instance.vectorIndices) {
			tmp.indices = item;
			value += calculateExpression<&ExpressionResolver::resolveReference>(expression);
		}
		value /= (instance.vectorIndices.size() + 1);
		break;
	}
	case RollupFunction::eMINIMUM:
	{
		for (const auto& indexes : instance.vectorIndices) {
			tmp.indices = indexes;
			value = std::min(value, calculateExpression<&ExpressionResolver::resolveReference>(expression));
		}
		break;
	}
	case RollupFunction::eMAXIMUM:
	{
		for (const auto& indexes : instance.vectorIndices) {
			tmp.indices = indexes;
			value = std::max(value, calculateExpression<&ExpressionResolver::resolveReference>(expression));
		}
		break;
	}
	case RollupFunction::eFIRST:
		break;
	default:
		log.error(L"unknown expression type: {}", expression.type);
		value = 0.0;
		break;
	}
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
	default:
		log.error(L"unknown rollup function: {}", rollupFunction);
		return 0.0;
	}
}

double ExpressionResolver::calculateRollupCountTotal(const RollupFunction rollupFunction) const {
	switch (rollupFunction) {
	case RollupFunction::eSUM: return static_cast<double>(instanceManager.getRollupInstances().size());
	case RollupFunction::eAVERAGE: return static_cast<double>(instanceManager.getInstances().size()) / instanceManager
		.getRollupInstances()
		.size();
	case RollupFunction::eMINIMUM:
	{
		index min = std::numeric_limits<index>::max();
		for (const auto& item : instanceManager.getRollupInstances()) {
			index val = index(item.vectorIndices.size()) + 1;
			min = min < val ? min : val;
		}
		return static_cast<double>(min);
	}
	case RollupFunction::eMAXIMUM:
	{
		index max = 0;
		for (const auto& item : instanceManager.getRollupInstances()) {
			index val = index(item.vectorIndices.size()) + 1;
			max = max > val ? max : val;
		}
		return static_cast<double>(max);
	}
	case RollupFunction::eFIRST:
		if (!instanceManager.getRollupInstances().empty()) {
			return static_cast<double>(instanceManager.getRollupInstances()[0].vectorIndices.size() + 1);
		}
		return 0.0;
	default:
		log.error(L"unknown rollup function: {}", rollupFunction);
		return 0.0;
	}
}

double ExpressionResolver::resolveRollupReference(const Reference& ref) const {
	if (ref.type == ReferenceType::UNKNOWN) {
		log.error(L"unknown reference being solved");
		return 0.0;
	}
	if (ref.type == ReferenceType::COUNT) {
		if (ref.total) {
			return calculateRollupCountTotal(ref.rollupFunction);
		}
		if (!ref.named) {
			return static_cast<double>(expressionCurrentItem->vectorIndices.size() + 1);
		}
		const auto instance = instanceManager.findInstanceByName(ref, true);
		return instance == nullptr ? 0 : static_cast<double>(instance->vectorIndices.size() + 1);
	}
	if (ref.type == ReferenceType::COUNTER_RAW || ref.type == ReferenceType::COUNTER_FORMATTED) {
		if (ref.type == ReferenceType::COUNTER_FORMATTED && !instanceManager.canGetFormatted()) {
			return 0.0;
		}
		if (ref.total) {
			if (ref.type == ReferenceType::COUNTER_RAW) {
				return calculateAndCacheTotal(TotalSource::eRAW_COUNTER, ref.counter, ref.rollupFunction);
			} else {
				return calculateAndCacheTotal(TotalSource::eFORMATTED_COUNTER, ref.counter, ref.rollupFunction);
			}
		}
		if (!ref.named) {
			if (ref.type == ReferenceType::COUNTER_RAW) {
				return calculateRollup<&ExpressionResolver::getRaw>(ref.rollupFunction, ref.counter,
					*expressionCurrentItem);
			} else {
				return calculateRollup<&ExpressionResolver::getFormatted>(
					ref.rollupFunction, ref.counter, *expressionCurrentItem);
			}
		}
		const auto instance = instanceManager.findInstanceByName(ref, true);
		if (instance == nullptr) {
			return 0.0;
		}
		if (ref.type == ReferenceType::COUNTER_RAW) {
			return calculateRollup<&ExpressionResolver::getRaw>(ref.rollupFunction, ref.counter, *instance);
		} else {
			return calculateRollup<&ExpressionResolver::getFormatted>(ref.rollupFunction, ref.counter, *instance);
		}
	}
	if (ref.type == ReferenceType::EXPRESSION || ref.type == ReferenceType::ROLLUP_EXPRESSION) {
		if (ref.total) {
			if (ref.type == ReferenceType::EXPRESSION) {
				return calculateAndCacheTotal(TotalSource::eEXPRESSION, ref.counter, ref.rollupFunction);
			} else {
				return calculateAndCacheTotal(TotalSource::eROLLUP_EXPRESSION, ref.counter, ref.rollupFunction);
			}
		}
		if (!ref.named) {
			if (ref.type == ReferenceType::EXPRESSION) {
				return calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
			} else {
				return calculateExpression<&ExpressionResolver::resolveRollupReference>(rollupExpressions[ref.counter]);
			}
		}
		const auto instance = instanceManager.findInstanceByName(ref, true);
		if (instance == nullptr) {
			return 0.0;
		}
		const InstanceInfo* const savedInstance = expressionCurrentItem;
		expressionCurrentItem = instance;
		double res;
		if (ref.type == ReferenceType::EXPRESSION) {
			res = calculateExpressionRollup(expressions[ref.counter], ref.rollupFunction);
		} else {
			res = calculateExpression<&ExpressionResolver::resolveRollupReference>(rollupExpressions[ref.counter]);
		}
		expressionCurrentItem = savedInstance;
		return res;
	}

	log.error(L"unexpected reference type in resolveReference(): {}", ref.type);
	return 0.0;
}

template <double(ExpressionResolver::* calculateValueFunction)(counter_t counterIndex, Indices originalIndexes) const>
double ExpressionResolver::calculateRollup(RollupFunction rollupType, counter_t counterIndex, const InstanceInfo& instance) const {
	const double firstValue = (this->*calculateValueFunction)(counterIndex, instance.indices);
	const auto& indexes = instance.vectorIndices;

	switch (rollupType) {
	case RollupFunction::eSUM:
	case RollupFunction::eAVERAGE:
	{
		auto sum = firstValue;
		for (const auto& item : indexes) {
			sum += (this->*calculateValueFunction)(counterIndex, item);
		}
		if (rollupType == RollupFunction::eSUM) {
			return static_cast<double>(sum);
		}
		if (indexes.empty()) {
			return 0.0;
		}
		return static_cast<double>(sum) / (indexes.size() + 1);
	}
	case RollupFunction::eMINIMUM:
	{
		auto min = firstValue;
		for (const auto& item : indexes) {
			min = std::min(min, (this->*calculateValueFunction)(counterIndex, item));
		}
		return static_cast<double>(min);
	}
	case RollupFunction::eMAXIMUM:
	{
		auto max = firstValue;
		for (const auto& item : indexes) {
			max = std::max(max, (this->*calculateValueFunction)(counterIndex, item));
		}
		return static_cast<double>(max);
	}
	case RollupFunction::eFIRST:
		return static_cast<double>(firstValue);
	default:
		log.error(L"unexpected rollupType {}", rollupType);
		return 0;
	}
}

template <double (ExpressionResolver::* calculateValueFunction)(counter_t counterIndex, Indices originalIndexes) const>
double ExpressionResolver::calculateTotal(RollupFunction rollupType, counter_t counterIndex) const {
	auto& vectorInstanceKeys = instanceManager.getInstances();

	switch (rollupType) {
	case RollupFunction::eSUM:
	case RollupFunction::eAVERAGE:
	{
		if (vectorInstanceKeys.empty()) {
			return 0.0;
		}
		double sum { };
		for (const auto& item : vectorInstanceKeys) {
			sum += (this->*calculateValueFunction)(counterIndex, item.indices);
		}
		if (rollupType == RollupFunction::eSUM) {
			return sum;
		}
		return sum / vectorInstanceKeys.size();
	}
	case RollupFunction::eMINIMUM:
	{
		double min = std::numeric_limits<double>::max();
		for (const auto& item : vectorInstanceKeys) {
			min = std::min(min, (this->*calculateValueFunction)(counterIndex, item.indices));
		}
		return min;
	}
	case RollupFunction::eMAXIMUM:
	{
		double max = -std::numeric_limits<double>::max();
		for (const auto& item : vectorInstanceKeys) {
			max = std::max(max, (this->*calculateValueFunction)(counterIndex, item.indices));
		}
		return max;
	}
	case RollupFunction::eFIRST:
		return vectorInstanceKeys.empty()
			? 0.0
			: (this->*calculateValueFunction)(counterIndex, vectorInstanceKeys[0].indices);
	default:
		log.error(L"unexpected rollupType {}", rollupType);
		return 0;
	}
}

template <double(ExpressionResolver::* calculateExpressionFunction)(const ExpressionTreeNode& expression)const>
double ExpressionResolver::calculateExpressionTotal(const RollupFunction rollupType,
	const ExpressionTreeNode& expression, bool rollup) const {
	auto& vectorInstanceKeys = rollup ? instanceManager.getRollupInstances() : instanceManager.getInstances();

	switch (rollupType) {
	case RollupFunction::eSUM:
	case RollupFunction::eAVERAGE:
	{
		double sum = 0.0;
		for (const auto& item : vectorInstanceKeys) {
			expressionCurrentItem = &item;
			sum += (this->*calculateExpressionFunction)(expression);
		}
		if (rollupType == RollupFunction::eAVERAGE) {
			if (vectorInstanceKeys.empty()) {
				return 0.0;
			}
			return sum / vectorInstanceKeys.size();
		}
		return sum;
	}
	case RollupFunction::eMINIMUM:
	{
		double min = std::numeric_limits<double>::max();
		for (const auto& item : vectorInstanceKeys) {
			expressionCurrentItem = &item;
			min = std::min(min, (this->*calculateExpressionFunction)(expression));
		}
		return min;
	}
	case RollupFunction::eMAXIMUM:
	{
		double max = -std::numeric_limits<double>::max();
		for (const auto& item : vectorInstanceKeys) {
			expressionCurrentItem = &item;
			max = std::max(max, (this->*calculateExpressionFunction)(expression));
		}
		return max;
	}
	case RollupFunction::eFIRST:
		if (vectorInstanceKeys.empty()) {
			return 0.0;
		}
		expressionCurrentItem = &vectorInstanceKeys[0];
		return (this->*calculateExpressionFunction)(expression);
	default:
		log.error(L"unexpected rollupType {}", rollupType);
		return 0;
	}
}

template <double(ExpressionResolver::* resolveReferenceFunction)(const Reference& ref) const>
double ExpressionResolver::calculateExpression(const ExpressionTreeNode& expression) const {
	switch (expression.type) {
	case ExpressionType::UNKNOWN:
		log.error(L"unknown expression being solved");
		return 0.0;
	case ExpressionType::NUMBER: return expression.number;
	case ExpressionType::SUM:
	{
		double value = 0;
		for (const ExpressionTreeNode& node : expression.nodes) {
			value += calculateExpression<resolveReferenceFunction>(node);
		}
		return value;
	}
	case ExpressionType::DIFF:
	{
		double value = calculateExpression<resolveReferenceFunction>(expression.nodes[0]);
		for (index i = 1; i < index(expression.nodes.size()); i++) {
			value -= calculateExpression<resolveReferenceFunction>(expression.nodes[i]);
		}
		return value;
	}
	case ExpressionType::INVERSE:
		return -calculateExpression<resolveReferenceFunction>(expression.nodes[0]);
	case ExpressionType::MULT:
	{
		double value = 1;
		for (const ExpressionTreeNode& node : expression.nodes) {
			value *= calculateExpression<resolveReferenceFunction>(node);
		}
		return value;
	}
	case ExpressionType::DIV:
	{
		double value = calculateExpression<resolveReferenceFunction>(expression.nodes[0]);
		for (index i = 1; i < index(expression.nodes.size()); i++) {
			const double denominator = calculateExpression<resolveReferenceFunction>(expression.nodes[i]);
			if (denominator == 0) {
				value = 0;
				break;
			}
			value /= denominator;
		}
		return value;
	}
	case ExpressionType::POWER:
		return std::pow(
			calculateExpression<resolveReferenceFunction>(expression.nodes[0]),
			calculateExpression<resolveReferenceFunction>(expression.nodes[1]));
	case ExpressionType::REF:
		return (this->*resolveReferenceFunction)(expression.ref);
	default:
		log.error(L"unknown expression type: {}", expression.type);
		return 0.0;
	}
}

bool ExpressionResolver::indexIsInBounds(index ind, index min, index max) {
	return ind >= min && ind <= max;
}
