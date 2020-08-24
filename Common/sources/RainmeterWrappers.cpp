/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "RainmeterWrappers.h"

#include <mutex>

#include "RainmeterAPI.h"

#pragma comment(lib, "Rainmeter.lib")

using namespace utils;

std::mutex logMutex;
std::vector<wchar_t> logBuffer;

struct LogMessageInfo {
	void* rm;
	index offset;
	Rainmeter::Logger::LogLevel logLevel;
};

std::vector<LogMessageInfo> logMessagesInfo;

static void saveLogMessage(void* rm, Rainmeter::Logger::LogLevel logLevel, sview message) {
	std::unique_lock<std::mutex> lock{ logMutex };

	const index logBufferOffset = logBuffer.size();
	const index messageSize = message.size();
	logBuffer.resize(logBufferOffset + messageSize);
	std::copy(message.begin(), message.end(), logBuffer.begin() + logBufferOffset);
	logBuffer.push_back(L'\0');

	logMessagesInfo.push_back({ rm, logBufferOffset, logLevel });
}

void Rainmeter::Logger::logRainmeter(LogLevel logLevel, sview message) const {
	saveLogMessage(rm, logLevel, message);
}

void Rainmeter::printLogMessages() {
	std::unique_lock<std::mutex> lock{ logMutex };

	for (auto info : logMessagesInfo) {
		RmLog(info.rm, static_cast<int>(info.logLevel), logBuffer.data() + info.offset);
	}

	logMessagesInfo.clear();
	logBuffer.clear();
}

Rainmeter::Rainmeter(void* rm) :
	rm(rm) {
	skin = Skin{ RmGetSkin(rm) };
	measureName = RmGetMeasureName(rm);
}

sview Rainmeter::readString(sview optionName, const wchar_t* defaultValue) const {
	return RmReadString(rm, makeNullTerminated(optionName), defaultValue);
}

sview Rainmeter::readPath(sview optionName, const wchar_t* defaultValue) const {
	return RmReadPath(rm, makeNullTerminated(optionName), defaultValue);
}

double Rainmeter::readDouble(sview optionName, double defaultValue) const {
	return RmReadFormula(rm, makeNullTerminated(optionName), defaultValue);
}

sview Rainmeter::replaceVariables(sview string) const {
	return RmReplaceVariables(rm, makeNullTerminated(string));
}

sview Rainmeter::transformPathToAbsolute(sview path) const {
	return RmPathToAbsolute(rm, makeNullTerminated(path));
}

void Rainmeter::executeCommand(sview command, Skin skin) {
	RmExecute(skin.getRawPointer(), makeNullTerminated(command));
}

void* Rainmeter::getWindowHandle() {
	return RmGetSkinWindow(rm);
}

void Rainmeter::sourcelessLog(const wchar_t* message) {
	saveLogMessage(nullptr, Logger::LogLevel::eDEBUG, message);
}

const wchar_t* Rainmeter::makeNullTerminated(sview view) const {
	// can't use view[view.length()] because it's out of view's range
	if (view.data()[view.length()] == L'\0') {
		return view.data();
	}

	optionNameBuffer = view;
	return optionNameBuffer.c_str();
}
