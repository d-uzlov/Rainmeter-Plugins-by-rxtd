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

#pragma warning(disable : 4458)

#pragma comment(lib, "Rainmeter.lib")

rxu::Rainmeter::Logger::Logger(void* rm) : rm(rm) { }

rxu::Rainmeter::Logger::Logger(Logger&& other) noexcept :
	rm(other.rm),
	printer(std::move(other.printer)) {
	other.rm = { };
}

rxu::Rainmeter::Logger::Logger(const Logger& other) :
	rm(other.rm),
	printer(other.printer) { }

rxu::Rainmeter::Logger& rxu::Rainmeter::Logger::operator=(const Logger& other) {
	if (this == &other)
		return *this;

	rm = other.rm;
	printer = other.printer;

	return *this;
}

rxu::Rainmeter::Logger& rxu::Rainmeter::Logger::operator=(Logger&& other) noexcept {
	if (this == &other)
		return *this;

	rm = other.rm;
	other.rm = { };
	printer = std::move(other.printer);

	return *this;
}

void rxu::Rainmeter::Logger::log(LEVEL logLevel, const wchar_t* string) {
	RmLog(rm, static_cast<int>(logLevel), string);
}

rxu::Rainmeter::Skin::Skin(void* skin) : skin(skin) { }

void* rxu::Rainmeter::Skin::getRawPointer() const {
	return skin;
}

rxu::Rainmeter::Rainmeter() = default;

rxu::Rainmeter::Rainmeter(void* rm) :
	rm(rm),
	skin(RmGetSkin(rm)),
	measureName(RmGetMeasureName(rm)),
	logger(rm) {

}

rxu::Rainmeter::Rainmeter(Rainmeter&& other) noexcept :
	rm(other.rm),
	skin(other.skin),
	measureName(std::move(other.measureName)),
	logger(std::move(other.logger)) {
	other.rm = { };
}

rxu::Rainmeter::Rainmeter(const Rainmeter& other) :
	rm(other.rm),
	skin(other.skin),
	measureName(other.measureName),
	logger(other.logger),
	optionNameBuffer(other.optionNameBuffer) { }

rxu::Rainmeter& rxu::Rainmeter::operator=(const Rainmeter& other) {
	if (this == &other)
		return *this;
	rm = other.rm;
	skin = other.skin;
	measureName = other.measureName;
	logger = other.logger;
	optionNameBuffer = other.optionNameBuffer;
	return *this;
}

rxu::Rainmeter& rxu::Rainmeter::operator=(Rainmeter&& other) noexcept {
	if (this == &other)
		return *this;

	rm = other.rm;
	other.rm = { };
	skin = std::move(other.skin);
	measureName = std::move(other.measureName);
	logger = std::move(other.logger);
	optionNameBuffer = std::move(other.optionNameBuffer);

	return *this;
}

const wchar_t* rxu::Rainmeter::readString(std::wstring_view optionName, const wchar_t* defaultValue) const {
	return RmReadString(rm, makeNullTerminated(optionName), defaultValue);
}

const wchar_t* rxu::Rainmeter::readPath(std::wstring_view optionName, const wchar_t* defaultValue) const {
	return RmReadPath(rm, makeNullTerminated(optionName), defaultValue);
}

double rxu::Rainmeter::readDouble(std::wstring_view optionName, double defaultValue) const {
	return RmReadFormula(rm, makeNullTerminated(optionName), defaultValue);
}

const wchar_t* rxu::Rainmeter::replaceVariables(std::wstring_view string) const {
	return RmReplaceVariables(rm, makeNullTerminated(string));
}

const wchar_t* rxu::Rainmeter::transformPathToAbsolute(std::wstring_view path) const {
	return RmPathToAbsolute(rm, makeNullTerminated(path));
}

void rxu::Rainmeter::executeCommand(std::wstring_view command, Skin skin) {
	RmExecute(skin.getRawPointer(), makeNullTerminated(command));
}

rxu::Rainmeter::Logger& rxu::Rainmeter::getLogger() {
	return logger;
}

void* rxu::Rainmeter::getRawPointer() {
	return rm;
}

rxu::Rainmeter::Skin rxu::Rainmeter::getSkin() const {
	return skin;
}

const std::wstring& rxu::Rainmeter::getMeasureName() const {
	return measureName;
}

void* rxu::Rainmeter::getWindowHandle() {
	return RmGetSkinWindow(rm);
}

const wchar_t* rxu::Rainmeter::makeNullTerminated(std::wstring_view view) const {
	if (view.data()[view.length()] == L'\0') {
		return view.data();
	}
	optionNameBuffer = view;
	return optionNameBuffer.c_str();
}
