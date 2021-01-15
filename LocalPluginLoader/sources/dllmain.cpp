/*
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "LocalPluginLoader.h"
#include "RainmeterAPI.h"

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	*data = new LocalPluginLoader(rm);
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
	static_cast<LocalPluginLoader*>(data)->reload(maxValue, rm);
}

PLUGIN_EXPORT double Update(void* data) {
	const auto result = static_cast<LocalPluginLoader*>(data)->update();
	return result;
}

PLUGIN_EXPORT LPCWSTR GetString(void* data) {
	const auto result = static_cast<LocalPluginLoader*>(data)->getStringValue();
	return result;
}

PLUGIN_EXPORT void Finalize(void* data) {
	delete static_cast<LocalPluginLoader*>(data);
}

PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args) {
	static_cast<LocalPluginLoader*>(data)->executeBang(args);
}

PLUGIN_EXPORT LPCWSTR sv(void* data, const int argc, const WCHAR* argv[]) {
	const auto result = static_cast<LocalPluginLoader*>(data)->solveSectionVariable(argc, argv);
	return result;
}

PLUGIN_EXPORT void* LocalPluginLoaderRecursionPrevention_123_() {
	return nullptr;
}
