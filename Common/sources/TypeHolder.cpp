/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "TypeHolder.h"

#include "undef.h"

using namespace utils;

TypeHolder::TypeHolder(Rainmeter&& rain) : rain(std::move(rain)), logger(this->rain.getLogger()) {

}

void TypeHolder::_command(isview  bangArgs) {
	logger.warning(L"Measure does not have commands");
}

const wchar_t* TypeHolder::_resolve(int argc, const wchar_t* argv[]) {
	return nullptr;
}

void TypeHolder::setMeasureState(MeasureState brokenState) {
	this->measureState = brokenState;
}

void TypeHolder::setUseResultString(bool value) {
	useResultString = value;
}

double TypeHolder::update() {
	if (measureState != MeasureState::eWORKING) {
		return 0.0;
	}

	resultDouble = _update();
	if (useResultString) {
		resultString = { };
		_updateString(resultString);
	}
	return resultDouble;
}

void TypeHolder::reload() {
	if (measureState == MeasureState::eBROKEN) { // skip reload only if the measure is unrecoverable
		return;
	}

	measureState = MeasureState::eWORKING;
	_reload();
}

void TypeHolder::command(const wchar_t *bangArgs) {
	if (measureState != MeasureState::eWORKING) {
		logger.warning(L"Skipping bang on a broken measure");
		return;
	}

	_command(bangArgs);
}

const wchar_t* TypeHolder::resolve(int argc, const wchar_t* argv[]) {
	if (measureState != MeasureState::eWORKING) {
		logger.warning(L"Skipping resolve on a broken measure");
		return L"";
	}

	const auto result = _resolve(argc, argv);
	if (result == nullptr) {
		return L"";
	}
	return result;
}

const wchar_t* TypeHolder::getString() const {
	if (measureState != MeasureState::eWORKING) {
		return L"broken";
	}

	if (useResultString) {
		return resultString.c_str();
	} else {
		return nullptr;
	}
}

MeasureState TypeHolder::getState() const {
	return measureState;
}
