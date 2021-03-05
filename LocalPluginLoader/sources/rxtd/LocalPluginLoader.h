// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#pragma once

#include "rxtd/my-windows.h"
#include "rxtd/rainmeter/Rainmeter.h"

namespace rxtd {
	class LocalPluginLoader : NonMovableBase {
		rainmeter::Rainmeter rain;
		Logger logger;
		HMODULE hLib = {};
		void* pluginData = nullptr;

		void (*reloadFunc)(void* data, void* rm, double* maxValue) = nullptr;
		double (*updateFunc)(void* data) = nullptr;
		const wchar_t* (*getStringFunc)(void* data) = nullptr;
		void (*executeBangFunc)(void* data, const wchar_t* args) = nullptr;

	public:
		LocalPluginLoader(void* rm);
		~LocalPluginLoader();

		void reload(double* maxValue, void* rm) const;
		double update() const;
		const wchar_t* getStringValue() const;
		void executeBang(const wchar_t* args) const;
		const wchar_t* solveSectionVariable(int count, const wchar_t* args[]);
	};
}
