/* 
 * Copyright (C) 2018-2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "RainmeterAPI.h"

#include "LocalPluginLoader.h"
#include "StringUtils.h"

#include "undef.h"

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	*data = new LocalPluginLoader(rm);
}
PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
	static_cast<LocalPluginLoader*>(data)->reload(maxValue);
}
PLUGIN_EXPORT double Update(void* data) {
	return static_cast<LocalPluginLoader*>(data)->update();
}
PLUGIN_EXPORT LPCWSTR GetString(void* data) {
	return static_cast<LocalPluginLoader*>(data)->getStringValue();
}
PLUGIN_EXPORT void Finalize(void* data) {
	delete static_cast<LocalPluginLoader*>(data);
}
PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args) {
	static_cast<LocalPluginLoader*>(data)->executeBang(args);
}
PLUGIN_EXPORT LPCWSTR sv(void* data, const int argc, const WCHAR* argv[]) {
	return static_cast<LocalPluginLoader*>(data)->solveSectionVariable(argc, argv);
}
PLUGIN_EXPORT void* LocalPluginLoaderRecursionPrevention_123_() {
	return nullptr;
}

LocalPluginLoader::LocalPluginLoader(void* rm) {
	this->rm = rm;
	string pluginPath = RmReadPath(rm, L"PluginPath", L"");
	if (pluginPath.empty()) {
		RmLogF(rm, LOG_ERROR, L"Plugin path must be specified");
		return;
	}
	const wchar_t lastSymbol = pluginPath[pluginPath.length() - 1];
	if (lastSymbol != L'/' && lastSymbol != L'\\') {
		pluginPath += L'/';
	}
#ifdef _WIN64
	pluginPath += L"_64";
#else // win32
	pluginPath += L"_32";
#endif // end ifdef _WIN64

	pluginPath += L".dll";
	
	hLib = LoadLibraryW(pluginPath.c_str());
	if (hLib == nullptr) {
		RmLogF(rm, LOG_ERROR, L"Can't load library in path '%s' (error %ld)", pluginPath.c_str(), GetLastError());
		return;
	}
	if (GetProcAddress(hLib, "LocalPluginLoaderRecursionPrevention_123_") != nullptr) {
		RmLogF(rm, LOG_ERROR, L"Loaded plugin must not be LocalPluginLoader");
		return;
	}

	reloadFunc = reinterpret_cast<decltype(reloadFunc)>(GetProcAddress(hLib, "Reload"));
	updateFunc = reinterpret_cast<decltype(updateFunc)>(GetProcAddress(hLib, "Update"));
	getStringFunc = reinterpret_cast<decltype(getStringFunc)>(GetProcAddress(hLib, "GetString"));
	executeBangFunc = reinterpret_cast<decltype(executeBangFunc)>(GetProcAddress(hLib, "ExecuteBang"));

	const auto initializeFunc = reinterpret_cast<void(*)(void**, void*)>(GetProcAddress(hLib, "Initialize"));
	if (initializeFunc != nullptr) {
		initializeFunc(&pluginData, rm);
	}
}

LocalPluginLoader::~LocalPluginLoader() {
	if (hLib != nullptr) {
		const auto finalizeFunc = GetProcAddress(hLib, "Finalize");
		if (finalizeFunc != nullptr) {
			reinterpret_cast<void(*)(void *)>(finalizeFunc)(pluginData);
		}

		pluginData = nullptr;

		updateFunc = nullptr;
		reloadFunc = nullptr;
		getStringFunc = nullptr;
		executeBangFunc = nullptr;

		FreeLibrary(hLib);
		hLib = nullptr;
	}
}

double LocalPluginLoader::update() const {
	if (updateFunc == nullptr) {
		return 0.0;
	}
	return updateFunc(pluginData);
}

void LocalPluginLoader::reload(double* maxValue) const {
	if (reloadFunc == nullptr) {
		return;
	}
	reloadFunc(pluginData, rm, maxValue);
}

const wchar_t* LocalPluginLoader::getStringValue() const {
	if (getStringFunc == nullptr) {
		return nullptr;
	}
	return getStringFunc(pluginData);
}

void LocalPluginLoader::executeBang(const wchar_t* args) const {
	if (executeBangFunc == nullptr) {
		return;
	}
	executeBangFunc(pluginData, args);
}

const wchar_t* LocalPluginLoader::solveSectionVariable(const int count, const wchar_t* args[]) const {
	if (hLib == nullptr) {
		return nullptr;
	}
	if (count < 1) {
		RmLogF(rm, LOG_ERROR, L"Function name must be specified");
		return nullptr;
	}

	const auto funcName = utils::StringUtils::trim(args[0]) % own();
	// Prevent calling known API functions
	if (funcName == L"Initialize" ||
		funcName == L"Reload" ||
		funcName == L"Update" ||
		funcName == L"GetString" ||
		funcName == L"ExecuteBang" ||
		funcName == L"Finalize" ||
		funcName == L"Update2" ||				// Old API
		funcName == L"GetPluginAuthor" ||		// Old API
		funcName == L"GetPluginVersion") {		// Old API
		return nullptr;
	}

	std::string byteFuncName;
	byteFuncName.resize(funcName.length());
	for (index i = 0; i < index(funcName.length()); ++i) {
		// let's only support ascii
		const wchar_t wc = funcName[i];
		const char c = static_cast<char>(wc);
		if (c != wc) {
			RmLogF(rm, LOG_ERROR, L"Can not find function '%s'", funcName.c_str());
			return nullptr;
		}
		byteFuncName[i] = c;
	}

	const auto funcPtr = reinterpret_cast<const wchar_t* (*)(void* data, int argc, const wchar_t* argv[])>(GetProcAddress(hLib, byteFuncName.c_str()));
	if (funcPtr == nullptr) {
		RmLogF(rm, LOG_ERROR, L"Can not find function '%s'", funcName.c_str());
		return nullptr;
	}
	return funcPtr(pluginData, count - 1, args + 1);
}
