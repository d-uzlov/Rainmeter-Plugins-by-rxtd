/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "TypeHolder.h"

#pragma warning(disable : 4458)

rxu::TypeHolder::TypeHolder(Rainmeter&& rain) : rain(std::move(rain)), log(this->rain.getLogger()) {

}

rxu::TypeHolder::~TypeHolder() { }

void rxu::TypeHolder::_command(const wchar_t* bangArgs) {
	log.warning(L"Measure does not have commands");
}

const wchar_t* rxu::TypeHolder::_resolve(int argc, const wchar_t* argv[]) {
	return nullptr;
}

void rxu::TypeHolder::setMeasureState(MeasureState brokenState) {
	this->measureState = brokenState;
}

double rxu::TypeHolder::update() {
	if (measureState != MeasureState::WORKING) {
		return 0.0;
	}
	std::tuple<double, const wchar_t*> result = _update();
	resultDouble = std::get<0>(result);
	resultString = std::get<1>(result);
	return resultDouble;
}

void rxu::TypeHolder::reload() {
	if (measureState == MeasureState::BROKEN) { // skip reload only if the measure is unrecoverable
		return;
	}
	_reload();
}

void rxu::TypeHolder::command(const wchar_t *bangArgs) {
	if (measureState != MeasureState::WORKING) {
		log.warning(L"Skipping bang on the broken measure");
		return;
	}
	_command(bangArgs);
}

const wchar_t* rxu::TypeHolder::resolve(int argc, const wchar_t* argv[]) {
	if (measureState != MeasureState::WORKING) {
		log.printer.print(L"Measure {} is broken", rain.getMeasureName());
		return log.printer.getBufferPtr();
	}
	const auto result = _resolve(argc, argv);
	if (result == nullptr) {
		return L"";
	}
	return result;
}

const wchar_t* rxu::TypeHolder::getString() const {
	if (measureState != MeasureState::WORKING) {
		return L"broken";
	}
	return resultString;
}

rxu::MeasureState rxu::TypeHolder::getState() const {
	return measureState;
}
