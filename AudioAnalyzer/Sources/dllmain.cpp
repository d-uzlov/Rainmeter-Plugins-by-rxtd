/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */


#include <my-windows.h>
#include "RainmeterAPI.h"

#include "TypeHolder.h"
#include "AudioParent.h"
#include "AudioChild.h"
#include "RainmeterWrappers.h"

static_assert(std::is_same<WCHAR, wchar_t>::value);
static_assert(std::is_same<LPCWSTR, const wchar_t*>::value);

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	utils::Rainmeter rain(rm);

	const auto typeString = rain.read(L"Type").asIString();
	try {
		if (typeString == L"Parent") {
			*data = new audio_analyzer::AudioParent(std::move(rain));
		} else {
			*data = new audio_analyzer::AudioChild(std::move(rain));
		}
	} catch (std::runtime_error&) {
		*data = nullptr;
	}
}

PLUGIN_EXPORT void Reload(void* data, void*, double*) {
	if (data == nullptr) {
		return;
	}
	static_cast<utils::TypeHolder*>(data)->reload();
}

PLUGIN_EXPORT double Update(void* data) {
	if (data == nullptr) {
		return 0.0;
	}
	const auto result = static_cast<utils::TypeHolder*>(data)->update();
	return result;
}

PLUGIN_EXPORT const wchar_t* GetString(void* data) {
	if (data == nullptr) {
		return L"";
	}
	const auto result = static_cast<utils::TypeHolder*>(data)->getString();
	return result;
}

PLUGIN_EXPORT void Finalize(void* data) {
	if (data == nullptr) {
		return;
	}
	delete static_cast<utils::TypeHolder*>(data);
}

PLUGIN_EXPORT void ExecuteBang(void* data, const wchar_t* args) {
	if (data == nullptr) {
		return;
	}
	static_cast<utils::TypeHolder*>(data)->command(args);
}

PLUGIN_EXPORT const wchar_t* resolve(void* data, const int argc, const wchar_t* argv[]) {
	if (data == nullptr) {
		return nullptr;
	}
	const auto result = static_cast<utils::TypeHolder*>(data)->resolve(argc, argv);
	return result;
}
