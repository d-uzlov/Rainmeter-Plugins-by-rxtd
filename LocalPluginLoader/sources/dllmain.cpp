// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#include "rxtd/LocalPluginLoader.h"

using rxtd::LocalPluginLoader;

#define PLUGIN_EXPORT EXTERN_C __declspec(dllexport)

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
	return static_cast<LocalPluginLoader*>(data)->solveSectionVariable(argc, argv, false);
}

PLUGIN_EXPORT const wchar_t* resolve(void* data, const int argc, const wchar_t* argv[]) {
	return static_cast<LocalPluginLoader*>(data)->solveSectionVariable(argc, argv, true);
}

PLUGIN_EXPORT const wchar_t* Resolve(void* data, const int argc, const wchar_t* argv[]) {
	return resolve(data, argc, argv);
}

PLUGIN_EXPORT void* LocalPluginLoaderRecursionPrevention_123_() {
	return nullptr;
}
