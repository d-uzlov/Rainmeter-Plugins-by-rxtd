/* 
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#include <my-windows.h>
#include "rainmeter/Rainmeter.h"

class LocalPluginLoader : NonMovableBase {
	common::rainmeter::Rainmeter rain;
	common::rainmeter::Logger logger;
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
