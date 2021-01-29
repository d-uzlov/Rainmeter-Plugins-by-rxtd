/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "WinApiReturnCode.h"

#include <iomanip>

#include "Audioclient.h"

using namespace ::rxtd::common::winapi_wrappers;

static std::optional<sview> formatWinApiCode(index value) {
	switch (uint32_t(value)) {
	case uint32_t(E_POINTER): return L"E_POINTER";
	case uint32_t(E_OUTOFMEMORY): return L"E_OUTOFMEMORY";
	case uint32_t(E_NOINTERFACE): return L"E_NOINTERFACE";
	case uint32_t(E_INVALIDARG): return L"E_INVALIDARG";

	case uint32_t(AUDCLNT_E_NOT_INITIALIZED): return L"AUDCLNT_E_NOT_INITIALIZED";
	case uint32_t(AUDCLNT_E_ALREADY_INITIALIZED): return L"AUDCLNT_E_ALREADY_INITIALIZED";
	case uint32_t(AUDCLNT_E_WRONG_ENDPOINT_TYPE): return L"AUDCLNT_E_WRONG_ENDPOINT_TYPE";
	case uint32_t(AUDCLNT_E_DEVICE_INVALIDATED): return L"AUDCLNT_E_DEVICE_INVALIDATED";
	case uint32_t(AUDCLNT_E_NOT_STOPPED): return L"AUDCLNT_E_NOT_STOPPED";
	case uint32_t(AUDCLNT_E_BUFFER_TOO_LARGE): return L"AUDCLNT_E_BUFFER_TOO_LARGE";
	case uint32_t(AUDCLNT_E_OUT_OF_ORDER): return L"AUDCLNT_E_OUT_OF_ORDER";
	case uint32_t(AUDCLNT_E_UNSUPPORTED_FORMAT): return L"AUDCLNT_E_UNSUPPORTED_FORMAT";
	case uint32_t(AUDCLNT_E_INVALID_SIZE): return L"AUDCLNT_E_INVALID_SIZE";
	case uint32_t(AUDCLNT_E_DEVICE_IN_USE): return L"AUDCLNT_E_DEVICE_IN_USE";
	case uint32_t(AUDCLNT_E_BUFFER_OPERATION_PENDING): return L"AUDCLNT_E_BUFFER_OPERATION_PENDING";
	case uint32_t(AUDCLNT_E_THREAD_NOT_REGISTERED): return L"AUDCLNT_E_THREAD_NOT_REGISTERED";
	case uint32_t(AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED): return L"AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
	case uint32_t(AUDCLNT_E_ENDPOINT_CREATE_FAILED): return L"AUDCLNT_E_ENDPOINT_CREATE_FAILED";
	case uint32_t(AUDCLNT_E_SERVICE_NOT_RUNNING): return L"AUDCLNT_E_SERVICE_NOT_RUNNING";
	case uint32_t(AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED): return L"AUDCLNT_E_EVENTHANDLE_NOT_EXPECTED";
	case uint32_t(AUDCLNT_E_EXCLUSIVE_MODE_ONLY): return L"AUDCLNT_E_EXCLUSIVE_MODE_ONLY";
	case uint32_t(AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL): return L"AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL";
	case uint32_t(AUDCLNT_E_EVENTHANDLE_NOT_SET): return L"AUDCLNT_E_EVENTHANDLE_NOT_SET";
	case uint32_t(AUDCLNT_E_INCORRECT_BUFFER_SIZE): return L"AUDCLNT_E_INCORRECT_BUFFER_SIZE";
	case uint32_t(AUDCLNT_E_BUFFER_SIZE_ERROR): return L"AUDCLNT_E_BUFFER_SIZE_ERROR";
	case uint32_t(AUDCLNT_E_CPUUSAGE_EXCEEDED): return L"AUDCLNT_E_CPUUSAGE_EXCEEDED";
	case uint32_t(AUDCLNT_E_BUFFER_ERROR): return L"AUDCLNT_E_BUFFER_ERROR";
	case uint32_t(AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED): return L"AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";
	case uint32_t(AUDCLNT_E_INVALID_DEVICE_PERIOD): return L"AUDCLNT_E_INVALID_DEVICE_PERIOD";
	case uint32_t(AUDCLNT_E_INVALID_STREAM_FLAG): return L"AUDCLNT_E_INVALID_STREAM_FLAG";
	case uint32_t(AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE): return L"AUDCLNT_E_ENDPOINT_OFFLOAD_NOT_CAPABLE";
	case uint32_t(AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES): return L"AUDCLNT_E_OUT_OF_OFFLOAD_RESOURCES";
	case uint32_t(AUDCLNT_E_OFFLOAD_MODE_ONLY): return L"AUDCLNT_E_OFFLOAD_MODE_ONLY";
	case uint32_t(AUDCLNT_E_NONOFFLOAD_MODE_ONLY): return L"AUDCLNT_E_NONOFFLOAD_MODE_ONLY";
	case uint32_t(AUDCLNT_E_RESOURCES_INVALIDATED): return L"AUDCLNT_E_RESOURCES_INVALIDATED";
	case uint32_t(AUDCLNT_E_RAW_MODE_UNSUPPORTED): return L"AUDCLNT_E_RAW_MODE_UNSUPPORTED";
	case uint32_t(AUDCLNT_E_ENGINE_PERIODICITY_LOCKED): return L"AUDCLNT_E_ENGINE_PERIODICITY_LOCKED";
	case uint32_t(AUDCLNT_E_ENGINE_FORMAT_LOCKED): return L"AUDCLNT_E_ENGINE_FORMAT_LOCKED";
	case uint32_t(AUDCLNT_E_HEADTRACKING_ENABLED): return L"AUDCLNT_E_HEADTRACKING_ENABLED";
	case uint32_t(AUDCLNT_E_HEADTRACKING_UNSUPPORTED): return L"AUDCLNT_E_HEADTRACKING_UNSUPPORTED";

	default: return {};
	}
}

void rxtd::common::winapi_wrappers::writeType(std::wostream& stream, const WinApiReturnCode& code, sview options) {
	auto viewOpt = formatWinApiCode(code.code);
	if (viewOpt.has_value()) {
		stream << viewOpt.value();
	} else {
		stream << L"0x";
		stream << std::setfill(L'0') << std::setw(sizeof(code.code) * 2) << std::hex;
		stream << code.code;
	}
}
