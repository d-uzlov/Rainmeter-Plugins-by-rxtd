/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "TypeHolder.h"

using namespace utils;

std::map<Rainmeter::Skin, std::map<istring, ParentBase*, std::less<>>> ParentBase::globalMeasuresMap{};

TypeHolder::TypeHolder(Rainmeter&& _rain) : rain(std::move(_rain)) {
	logger = rain.createLogger();
	instanceKeeper = rain.getInstanceKeeper();
}

double TypeHolder::update() {
	if (!objectIsValid) {
		return 0.0;
	}

	try {
		resultDouble = vUpdate();
	} catch (std::runtime_error&) {}
	if (useResultString) {
		resultString = {};
		try {
			vUpdateString(resultString);
		} catch (std::runtime_error&) {}
	}
	return resultDouble;
}

void TypeHolder::reload() {
	objectIsValid = true;
	vReload();
}

void TypeHolder::command(const wchar_t* bangArgs) {
	if (!objectIsValid) {
		logger.warning(L"Skipping bang on a broken measure");
		return;
	}

	try {
		vCommand(bangArgs);
	} catch (std::runtime_error&) {}
}

const wchar_t* TypeHolder::resolve(int argc, const wchar_t* argv[]) {
	if (!objectIsValid) {
		logger.warning(L"Skipping resolve on a broken measure");
		return L"";
	}

	resolveVector.clear();
	for (index i = 0; i < argc; ++i) {
		const isview arg = argv[i];
		resolveVector.emplace_back(arg);
	}

	resolveString = {};

	try {
		vResolve(resolveVector, resolveString);
	} catch (std::runtime_error&) {}

	return resolveString.c_str();
}

const wchar_t* TypeHolder::resolve(array_view<isview> args) {
	if (!objectIsValid) {
		logger.warning(L"Skipping resolve on a broken measure");
		return L"";
	}

	resolveString = {};

	try {
		vResolve(args, resolveString);
	} catch (std::runtime_error&) {}

	return resolveString.c_str();
}

const wchar_t* TypeHolder::getString() const {
	if (!objectIsValid) {
		return L"broken";
	}

	return useResultString ? resultString.c_str() : nullptr;
}

ParentBase::ParentBase(Rainmeter&& _rain): TypeHolder(std::move(_rain)) {
	globalMeasuresMap[rain.getSkin()][rain.getMeasureName() % ciView() % own()] = this;
}

ParentBase::~ParentBase() {
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

ParentBase* ParentBase::findParent(Rainmeter::Skin skin, isview measureName) {
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
