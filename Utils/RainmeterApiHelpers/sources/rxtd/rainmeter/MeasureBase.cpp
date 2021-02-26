/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "MeasureBase.h"

using rxtd::utils::MeasureBase;
using rxtd::utils::ParentMeasureBase;

std::map<rxtd::rainmeter::SkinHandle, std::map<rxtd::istring, ParentMeasureBase*, std::less<>>> ParentMeasureBase::globalMeasuresMap{};
// NOLINT(clang-diagnostic-exit-time-destructors)

MeasureBase::MeasureBase(Rainmeter&& _rain) : rain(std::move(_rain)), logger(rain.createLogger()) {}

double MeasureBase::update() {
	if (!objectIsValid) {
		return 0.0;
	}

	try {
		resultDouble = vUpdate();
	} catch (std::runtime_error&) {
		logger.error(L"Measure '{}' unexpectedly stopped update", rain.getMeasureName());
		setInvalid();
	}
	if (useResultString) {
		resultString = {};
		try {
			vUpdateString(resultString);
		} catch (std::runtime_error&) {
			logger.error(L"Measure '{}' unexpectedly stopped string update", rain.getMeasureName());
			setInvalid();
		}
	}
	return resultDouble;
}

void MeasureBase::reload() {
	objectIsValid = true;
	try {
		vReload();
	} catch (std::runtime_error&) {
		logger.error(L"Measure '{}' unexpectedly stopped reload", rain.getMeasureName());
		setInvalid();
	}
}

void MeasureBase::command(const wchar_t* bangArgs) {
	if (!objectIsValid) {
		logger.warning(L"Skipping bang on a broken measure");
		return;
	}

	try {
		vCommand(bangArgs);
	} catch (std::runtime_error&) {
		logger.error(L"Measure '{}' unexpectedly stopped command", rain.getMeasureName());
		setInvalid();
	}
}

const wchar_t* MeasureBase::resolve(int argc, const wchar_t* argv[]) {
	if (!objectIsValid) {
		logger.warning(L"Skipping resolve on a broken measure");
		return L"";
	}

	resolveVector.clear();
	for (index i = 0; i < argc; ++i) {
		const isview arg = argv[i];
		resolveVector.emplace_back(arg);
	}

	return resolve(resolveVector);
}

const wchar_t* MeasureBase::resolve(array_view<isview> args) {
	if (!objectIsValid) {
		logger.warning(L"Skipping resolve on a broken measure");
		return L"";
	}

	resolveString = {};

	try {
		vResolve(args, resolveString);
	} catch (std::runtime_error&) {
		logger.error(L"Measure '{}' unexpectedly stopped section variable resolve", rain.getMeasureName());
		setInvalid();
	}

	return resolveString.c_str();
}

const wchar_t* MeasureBase::getString() const {
	if (!objectIsValid) {
		return L"broken";
	}

	return useResultString ? resultString.c_str() : nullptr;
}

ParentMeasureBase::ParentMeasureBase(Rainmeter&& _rain): MeasureBase(std::move(_rain)) {
	globalMeasuresMap[rain.getSkin()][rain.getMeasureName() % ciView() % own()] = this;
}

ParentMeasureBase::~ParentMeasureBase() {
	const auto skinIter = globalMeasuresMap.find(rain.getSkin());
	if (skinIter == globalMeasuresMap.end()) {
		std::terminate();
	}
	auto& measuresMap = skinIter->second;

	const auto measureIter = measuresMap.find(rain.getMeasureName() % ciView());
	if (measureIter == measuresMap.end()) {
		std::terminate();
	}
	measuresMap.erase(measureIter);

	if (measuresMap.empty()) {
		globalMeasuresMap.erase(skinIter);
	}
}

ParentMeasureBase* ParentMeasureBase::findParent(SkinHandle skin, isview measureName) {
	const auto skinIter = globalMeasuresMap.find(skin);
	if (skinIter == globalMeasuresMap.end()) {
		return nullptr;
	}
	const auto& measuresMap = skinIter->second;

	const auto measureIter = measuresMap.find(measureName);
	if (measureIter == measuresMap.end()) {
		return nullptr;
	}

	return measureIter->second;
}
