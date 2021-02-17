/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Rainmeter.h"

#include "RainmeterAPI.h"

#pragma comment(lib, "Rainmeter.lib")

using namespace ::rxtd::common::rainmeter;

Rainmeter::Rainmeter(void* rm) :
	dataHandle(rm), instanceKeeper(dataHandle) {
	skin = SkinHandle{ RmGetSkin(rm) };
	measureName = RmGetMeasureName(rm);
}

common::options::Option Rainmeter::read(sview optionName, bool replaceVariables) const {
	auto result = options::Option{
		RmReadString(dataHandle.getRawHandle(), makeNullTerminated(optionName), L"", replaceVariables)
	};
	result.own();
	return result;
}

sview Rainmeter::replaceVariables(sview string) const {
	return RmReplaceVariables(dataHandle.getRawHandle(), makeNullTerminated(string));
}

sview Rainmeter::transformPathToAbsolute(sview path) const {
	return RmPathToAbsolute(dataHandle.getRawHandle(), makeNullTerminated(path));
}

void Rainmeter::executeCommandAsync(sview command, SkinHandle skin) {
	if (command.empty()) {
		return;
	}
	instanceKeeper.sendCommand(dataHandle, skin, command % own());
}

const wchar_t* Rainmeter::makeNullTerminated(sview view) const {
	// can't use view[view.length()] because it's out of view's range
	if (view.data()[view.length()] == L'\0') {
		return view.data();
	}

	optionNameBuffer = view;
	return optionNameBuffer.c_str();
}
