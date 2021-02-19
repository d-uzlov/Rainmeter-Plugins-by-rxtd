/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PerfmonChild.h"
#include "PerfmonParent.h"
#include "PerfMonRXTD.h"
#include "RainmeterAPI.h"
#include "MeasureBase.h"

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	rxtd::common::rainmeter::Rainmeter rain(rm);

	const auto typeString = rain.read(L"Type").asIString();
	try {
		if (typeString == L"Parent") {
			*data = new rxtd::perfmon::PerfmonParent(std::move(rain));
		} else {
			*data = new rxtd::perfmon::PerfmonChild(std::move(rain));
		}
	} catch (std::runtime_error&) {
		*data = nullptr;
	}
}

PLUGIN_EXPORT void Reload(void* data, void*, double*) {
	if (data == nullptr) {
		return;
	}
	static_cast<rxtd::utils::MeasureBase*>(data)->reload();
}

PLUGIN_EXPORT double Update(void* data) {
	if (data == nullptr) {
		return 0.0;
	}
	const auto result = static_cast<rxtd::utils::MeasureBase*>(data)->update();

	return result;
}

PLUGIN_EXPORT const wchar_t* GetString(void* data) {
	if (data == nullptr) {
		return L"";
	}
	const auto result = static_cast<rxtd::utils::MeasureBase*>(data)->getString();

	return result;
}

PLUGIN_EXPORT void Finalize(void* data) {
	if (data == nullptr) {
		return;
	}
	delete static_cast<rxtd::utils::MeasureBase*>(data);
}

PLUGIN_EXPORT void ExecuteBang(void* data, const wchar_t* args) {
	if (data == nullptr) {
		return;
	}
	static_cast<rxtd::utils::MeasureBase*>(data)->command(args);
}

PLUGIN_EXPORT const wchar_t* resolve(void* data, const int argc, const wchar_t* argv[]) {
	const auto result = static_cast<rxtd::utils::MeasureBase*>(data)->resolve(argc, argv);

	return result;
}
