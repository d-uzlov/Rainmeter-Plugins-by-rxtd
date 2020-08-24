/*
 * Copyright (C) 2018-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */


#include "RainmeterAPI.h"
#include "PerfMonRXTD.h"
#include "PerfmonParent.h"
#include "PerfmonChild.h"
#include "TypeHolder.h"

#include "undef.h"

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	utils::Rainmeter rain(rm);

	const auto typeString = rain.read(L"Type").asIString();
	if (typeString == L"Parent") {
		*data = new perfmon::PerfmonParent(std::move(rain));
	} else {
		if (typeString != L"Child") {
			rain.createLogger().warning(L"Unknown type '{}', defaulting to Child is deprecated", typeString);
		}
		*data = new perfmon::PerfmonChild(std::move(rain));
	}

	utils::Rainmeter::printLogMessages();
}

PLUGIN_EXPORT void Reload(void* data, void*, double*) {
	if (data == nullptr) {
		return;
	}
	static_cast<utils::TypeHolder*>(data)->reload();

	utils::Rainmeter::printLogMessages();
}

PLUGIN_EXPORT double Update(void* data) {
	if (data == nullptr) {
		return 0.0;
	}
	const auto result = static_cast<utils::TypeHolder*>(data)->update();

	utils::Rainmeter::printLogMessages();

	return result;
}

PLUGIN_EXPORT const wchar_t* GetString(void* data) {
	if (data == nullptr) {
		return nullptr;
	}
	const auto result = static_cast<utils::TypeHolder*>(data)->getString();

	utils::Rainmeter::printLogMessages();

	return result;
}

PLUGIN_EXPORT void Finalize(void* data) {
	if (data == nullptr) {
		return;
	}
	delete static_cast<utils::TypeHolder*>(data);

	utils::Rainmeter::printLogMessages();
}

PLUGIN_EXPORT void ExecuteBang(void* data, const wchar_t* args) {
	if (data == nullptr) {
		return;
	}
	static_cast<utils::TypeHolder*>(data)->command(args);

	utils::Rainmeter::printLogMessages();
}

PLUGIN_EXPORT const wchar_t* resolve(void* data, const int argc, const wchar_t* argv[]) {
	const auto result = static_cast<utils::TypeHolder*>(data)->resolve(argc, argv);

	utils::Rainmeter::printLogMessages();

	return result;
}
