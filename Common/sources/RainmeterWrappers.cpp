/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "RainmeterWrappers.h"
#include "RainmeterAPI.h"

#include "undef.h"

#pragma comment(lib, "Rainmeter.lib")

using namespace utils;

Rainmeter::Logger::Logger(void* rm) : rm(rm) { }

Rainmeter::Logger::Logger(const Logger& other) :
	rm(other.rm),
	printer(other.printer) { }

Rainmeter::Logger::Logger(Logger&& other) noexcept :
	rm(other.rm),
	printer(std::move(other.printer)) {
	other.rm = { };
}

Rainmeter::Logger& Rainmeter::Logger::operator=(const Logger& other) {
	if (this == &other)
		return *this;

	rm = other.rm;
	printer = other.printer;

	return *this;
}

Rainmeter::Logger& Rainmeter::Logger::operator=(Logger&& other) noexcept {
	if (this == &other)
		return *this;

	rm = other.rm;
	other.rm = { };
	printer = std::move(other.printer);

	return *this;
}

void Rainmeter::Logger::log(LogLevel logLevel, const wchar_t* string) {
	RmLog(rm, static_cast<int>(logLevel), string);
}

Rainmeter::Skin::Skin(void* skin) : skin(skin) { }

void* Rainmeter::Skin::getRawPointer() const {
	return skin;
}

Rainmeter::Rainmeter() = default;

Rainmeter::Rainmeter(void* rm) :
	rm(rm),
	skin(RmGetSkin(rm)),
	measureName(RmGetMeasureName(rm)),
	logger(rm) {

}

Rainmeter::Rainmeter(Rainmeter&& other) noexcept :
	rm(other.rm),
	skin(other.skin),
	measureName(std::move(other.measureName)),
	logger(std::move(other.logger)) {
	other.rm = { };
	other.skin = { };
}

Rainmeter::Rainmeter(const Rainmeter& other) :
	rm(other.rm),
	skin(other.skin),
	measureName(other.measureName),
	logger(other.logger),
	optionNameBuffer(other.optionNameBuffer) { }

Rainmeter& Rainmeter::operator=(const Rainmeter& other) {
	if (this == &other)
		return *this;
	rm = other.rm;
	skin = other.skin;
	measureName = other.measureName;
	logger = other.logger;
	optionNameBuffer = other.optionNameBuffer;
	return *this;
}

Rainmeter& Rainmeter::operator=(Rainmeter&& other) noexcept {
	if (this == &other)
		return *this;

	rm = other.rm;
	other.rm = { };
	skin = other.skin;
	other.skin = { };
	
	measureName = std::move(other.measureName);
	logger = std::move(other.logger);
	optionNameBuffer = std::move(other.optionNameBuffer);

	return *this;
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

Rainmeter::Logger& Rainmeter::getLogger() {
	return logger;
}

void* Rainmeter::getRawPointer() {
	return rm;
}

Rainmeter::Skin Rainmeter::getSkin() const {
	return skin;
}

const string& Rainmeter::getMeasureName() const {
	return measureName;
}

void* Rainmeter::getWindowHandle() {
	return RmGetSkinWindow(rm);
}

const wchar_t* Rainmeter::makeNullTerminated(sview view) const {
	if (view.data()[view.length()] == L'\0') {
		return view.data();
	}

	optionNameBuffer = view;
	return optionNameBuffer.c_str();
}
