// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018 Danil Uzlov

#include "rxtd/perfmon/PerfmonChild.h"
#include "rxtd/perfmon/PerfmonParent.h"
#include "rxtd/perfmon/PerfMonRXTD.h"

#define PLUGIN_EXPORT EXTERN_C __declspec(dllexport)

PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	rxtd::rainmeter::Rainmeter rain(rm);

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

PLUGIN_EXPORT const wchar_t* Resolve(void* data, const int argc, const wchar_t* argv[]) {
	return resolve(data, argc, argv);
}
