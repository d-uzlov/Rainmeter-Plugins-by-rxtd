// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#include "LocalPluginLoader.h"
#include "rxtd/std_fixes/StringUtils.h"

using rxtd::LocalPluginLoader;
using rxtd::std_fixes::StringUtils;

namespace {
	template<typename FunctionSignature>
	FunctionSignature getDllFunction(HMODULE libHandle, const char* functionName) {
		const auto ptr = GetProcAddress(libHandle, functionName);
		const auto rawPtr = reinterpret_cast<void*>(ptr);
		return reinterpret_cast<FunctionSignature>(rawPtr);
	}
}

LocalPluginLoader::LocalPluginLoader(void* rm) {
	rain = rainmeter::Rainmeter{ rm };
	logger = rain.createLogger();

	string pluginPath = rain.transformPathToAbsolute(rain.read(L"PluginPath").asString()) % own();
	if (pluginPath.empty()) {
		logger.error(L"Plugin path must be specified");
		return;
	}
	const wchar_t lastSymbol = pluginPath[pluginPath.length() - 1];
	if (lastSymbol != L'/' && lastSymbol != L'\\') {
		pluginPath += L'\\';
	}
#ifdef _WIN64
	pluginPath += L"64-bit";
#else // win32
	pluginPath += L"32-bit";
#endif // end ifdef _WIN64

	pluginPath += L".dll";

	hLib = LoadLibraryW(pluginPath.c_str());
	if (hLib == nullptr) {
		const auto errorCode = GetLastError();
		logger.error(L"Can't load library in path '{}' (error {})", pluginPath, errorCode);

		wchar_t* receiveBuffer = nullptr;
		const auto messageLength = FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
			nullptr,
			errorCode,
			0,
			reinterpret_cast<LPWSTR>(&receiveBuffer),
			0,
			nullptr
		);
		if (messageLength != 0) {
			logger.error(L"More info: {}", receiveBuffer);
		} else {
			logger.debug(
				L"Can't get readable explanation of error because FormatMessageW failed (error {})",
				GetLastError()
			);
		}

		LocalFree(receiveBuffer);
		return;
	}
	if (GetProcAddress(hLib, "LocalPluginLoaderRecursionPrevention_123_") != nullptr) {
		logger.error(L"Loaded plugin must not be LocalPluginLoader");
		return;
	}

	reloadFunc = getDllFunction<decltype(reloadFunc)>(hLib, "Reload");
	updateFunc = getDllFunction<decltype(updateFunc)>(hLib, "Update");
	getStringFunc = getDllFunction<decltype(getStringFunc)>(hLib, "GetString");
	executeBangFunc = getDllFunction<decltype(executeBangFunc)>(hLib, "ExecuteBang");

	const auto initializeFunc = getDllFunction<void(*)(void**, void*)>(hLib, "Initialize");
	if (initializeFunc != nullptr) {
		initializeFunc(&pluginData, rm);
	}
}

LocalPluginLoader::~LocalPluginLoader() {
	if (hLib != nullptr) {
		const auto finalizeFunc = getDllFunction<void(*)(void*)>(hLib, "Finalize");
		if (finalizeFunc != nullptr) {
			finalizeFunc(pluginData);
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

void LocalPluginLoader::reload(double* maxValue, void* rm) const {
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

const wchar_t* LocalPluginLoader::solveSectionVariable(const int count, const wchar_t* args[]) {
	if (hLib == nullptr) {
		return nullptr;
	}
	if (count < 1) {
		logger.error(L"Function name must be specified");
		return nullptr;
	}

	const auto funcName = StringUtils::trim(sview{ args[0] });
	// Prevent calling known API functions
	if (funcName == L"Initialize" ||
		funcName == L"Reload" ||
		funcName == L"Update" ||
		funcName == L"GetString" ||
		funcName == L"ExecuteBang" ||
		funcName == L"Finalize" ||
		funcName == L"Update2" || // Old API
		funcName == L"GetPluginAuthor" || // Old API
		funcName == L"GetPluginVersion") {
		// Old API
		return nullptr;
	}

	std::string byteFuncName;
	byteFuncName.resize(static_cast<std::string::size_type>(funcName.length()));
	for (index i = 0; i < funcName.length(); ++i) {
		// let's only support ascii
		const wchar_t wc = funcName[i];
		const char c = static_cast<char>(wc);
		if (c != wc) {
			logger.error(L"Can not find function '{}'", funcName);
			return nullptr;
		}
		byteFuncName[static_cast<size_t>(i)] = c;
	}

	using ResolveFunction = const wchar_t* (*)(void* data, int argc, const wchar_t* argv[]);

	const ResolveFunction funcPtr = getDllFunction<ResolveFunction>(hLib, byteFuncName.c_str());
	if (funcPtr == nullptr) {
		logger.error(L"Can not find function '{}'", funcName);
		return nullptr;
	}
	return funcPtr(pluginData, count - 1, args + 1);
}
