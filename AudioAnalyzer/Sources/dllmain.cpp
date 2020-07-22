/*
 * Copyright (C) 2019 rxtd
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

#include "undef.h"

static_assert(std::is_same<WCHAR, wchar_t>::value);
static_assert(std::is_same<LPCWSTR, const wchar_t*>::value);


PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	utils::Rainmeter rain(rm);
	const auto typeString = rain.read(L"Type").asIString();
	if (typeString == L"Parent") {
		*data = new audio_analyzer::AudioParent(std::move(rain));
	} else {
		if (typeString != L"Child") {
			rain.getLogger().error(L"Type '{}' is not recognized, use Child instead", typeString);
			rain.getLogger().error(L"Defaulting to 'Child' is undocumented and unsupported, it will be removed in future");
		}
		*data = new audio_analyzer::AudioChild(std::move(rain));
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
	return static_cast<utils::TypeHolder*>(data)->update();
}

PLUGIN_EXPORT const wchar_t* GetString(void* data) {
	if (data == nullptr) {
		return nullptr;
	}
	return static_cast<utils::TypeHolder*>(data)->getString();
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
	return static_cast<utils::TypeHolder*>(data)->resolve(argc, argv);
}
