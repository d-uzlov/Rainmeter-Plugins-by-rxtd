/*
 * Copyright (C) 2019-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "Rainmeter.h"

#include <filesystem>


#include "RainmeterAPI.h"

using rxtd::rainmeter::Rainmeter;

static_assert(std::is_same<WCHAR, wchar_t>::value);
static_assert(std::is_same<LPCWSTR, const wchar_t*>::value);

Rainmeter::Rainmeter(void* rm) :
	dataHandle(rm), instanceKeeper(dataHandle) {
	skin = SkinHandle{ RmGetSkin(rm) };
	measureName = RmGetMeasureName(rm);
}

rxtd::option_parsing::Option Rainmeter::read(sview optionName, bool replaceVariables) const {
	auto result = option_parsing::Option{
		RmReadString(dataHandle.getRawHandle(), makeNullTerminated(optionName), L"", replaceVariables)
	};
	result.own();
	return result;
}

rxtd::sview Rainmeter::replaceVariables(sview string) const {
	return RmReplaceVariables(dataHandle.getRawHandle(), makeNullTerminated(string));
}

rxtd::sview Rainmeter::transformPathToAbsolute(sview path) const {
	return RmPathToAbsolute(dataHandle.getRawHandle(), makeNullTerminated(path));
}

void Rainmeter::executeCommandAsync(sview command, SkinHandle skin) {
	if (command.empty()) {
		return;
	}
	instanceKeeper.sendCommand(dataHandle, skin, command % own());
}

rxtd::string Rainmeter::getPathFromCurrent(string folder) const {
	std::filesystem::path path{ folder };
	if (!path.is_absolute()) {
		string f = std::move(folder);
		folder = replaceVariables(L"[#CURRENTPATH]");
		folder += f;
	}

	folder = std::filesystem::absolute(folder).wstring();
	folder = LR"(\\?\)" + folder;

	if (folder.back() != L'\\') {
		folder += L'\\';
	}

	return folder;
}

int Rainmeter::parseLogLevel(Logger::LogLevel level) {
	switch (level) {
	case Logger::LogLevel::eERROR: return LOG_ERROR;
	case Logger::LogLevel::eWARNING: return LOG_WARNING;
	case Logger::LogLevel::eNOTICE: return LOG_NOTICE;
	case Logger::LogLevel::eDEBUG: return LOG_DEBUG;
	}
	return LOG_DEBUG;
}

const wchar_t* Rainmeter::makeNullTerminated(sview view) const {
	// can't use view[view.length()] because it's out of view's range
	if (view.data()[view.length()] == L'\0') {
		return view.data();
	}

	optionNameBuffer = view;
	return optionNameBuffer.c_str();
}
