/*
 * Copyright (C) 2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "PdhFormat.h"

#include <iomanip>

// there are some strange errors without #include <Pdh.h>
#include <Pdh.h>
#include <PdhMsg.h>

static std::optional<rxtd::sview> formatPdhCode(rxtd::index code) {
	switch (uint32_t(code)) {
	case uint32_t(PDH_CSTATUS_VALID_DATA): return L"PDH_CSTATUS_VALID_DATA";
	case uint32_t(PDH_CSTATUS_NEW_DATA): return L"PDH_CSTATUS_NEW_DATA";
	case uint32_t(PDH_CSTATUS_NO_MACHINE): return L"PDH_CSTATUS_NO_MACHINE";
	case uint32_t(PDH_CSTATUS_NO_INSTANCE): return L"PDH_CSTATUS_NO_INSTANCE";
	case uint32_t(PDH_MORE_DATA): return L"PDH_MORE_DATA";
	case uint32_t(PDH_CSTATUS_ITEM_NOT_VALIDATED): return L"PDH_CSTATUS_ITEM_NOT_VALIDATED";
	case uint32_t(PDH_RETRY): return L"PDH_RETRY";
	case uint32_t(PDH_NO_DATA): return L"PDH_NO_DATA";
	case uint32_t(PDH_CALC_NEGATIVE_DENOMINATOR): return L"PDH_CALC_NEGATIVE_DENOMINATOR";
	case uint32_t(PDH_CALC_NEGATIVE_TIMEBASE): return L"PDH_CALC_NEGATIVE_TIMEBASE";
	case uint32_t(PDH_CALC_NEGATIVE_VALUE): return L"PDH_CALC_NEGATIVE_VALUE";
	case uint32_t(PDH_DIALOG_CANCELLED): return L"PDH_DIALOG_CANCELLED";
	case uint32_t(PDH_END_OF_LOG_FILE): return L"PDH_END_OF_LOG_FILE";
	case uint32_t(PDH_ASYNC_QUERY_TIMEOUT): return L"PDH_ASYNC_QUERY_TIMEOUT";
	case uint32_t(PDH_CANNOT_SET_DEFAULT_REALTIME_DATASOURCE): return L"PDH_CANNOT_SET_DEFAULT_REALTIME_DATASOURCE";
	case uint32_t(PDH_UNABLE_MAP_NAME_FILES): return L"PDH_UNABLE_MAP_NAME_FILES";
	case uint32_t(PDH_PLA_VALIDATION_WARNING): return L"PDH_PLA_VALIDATION_WARNING";
	case uint32_t(PDH_CSTATUS_NO_OBJECT): return L"PDH_CSTATUS_NO_OBJECT";
	case uint32_t(PDH_CSTATUS_NO_COUNTER): return L"PDH_CSTATUS_NO_COUNTER";
	case uint32_t(PDH_CSTATUS_INVALID_DATA): return L"PDH_CSTATUS_INVALID_DATA";
	case uint32_t(PDH_MEMORY_ALLOCATION_FAILURE): return L"PDH_MEMORY_ALLOCATION_FAILURE";
	case uint32_t(PDH_INVALID_HANDLE): return L"PDH_INVALID_HANDLE";
	case uint32_t(PDH_INVALID_ARGUMENT): return L"PDH_INVALID_ARGUMENT";
	case uint32_t(PDH_FUNCTION_NOT_FOUND): return L"PDH_FUNCTION_NOT_FOUND";
	case uint32_t(PDH_CSTATUS_NO_COUNTERNAME): return L"PDH_CSTATUS_NO_COUNTERNAME";
	case uint32_t(PDH_CSTATUS_BAD_COUNTERNAME): return L"PDH_CSTATUS_BAD_COUNTERNAME";
	case uint32_t(PDH_INVALID_BUFFER): return L"PDH_INVALID_BUFFER";
	case uint32_t(PDH_INSUFFICIENT_BUFFER): return L"PDH_INSUFFICIENT_BUFFER";
	case uint32_t(PDH_CANNOT_CONNECT_MACHINE): return L"PDH_CANNOT_CONNECT_MACHINE";
	case uint32_t(PDH_INVALID_PATH): return L"PDH_INVALID_PATH";
	case uint32_t(PDH_INVALID_INSTANCE): return L"PDH_INVALID_INSTANCE";
	case uint32_t(PDH_INVALID_DATA): return L"PDH_INVALID_DATA";
	case uint32_t(PDH_NO_DIALOG_DATA): return L"PDH_NO_DIALOG_DATA";
	case uint32_t(PDH_CANNOT_READ_NAME_STRINGS): return L"PDH_CANNOT_READ_NAME_STRINGS";
	case uint32_t(PDH_LOG_FILE_CREATE_ERROR): return L"PDH_LOG_FILE_CREATE_ERROR";
	case uint32_t(PDH_LOG_FILE_OPEN_ERROR): return L"PDH_LOG_FILE_OPEN_ERROR";
	case uint32_t(PDH_LOG_TYPE_NOT_FOUND): return L"PDH_LOG_TYPE_NOT_FOUND";
	case uint32_t(PDH_NO_MORE_DATA): return L"PDH_NO_MORE_DATA";
	case uint32_t(PDH_ENTRY_NOT_IN_LOG_FILE): return L"PDH_ENTRY_NOT_IN_LOG_FILE";
	case uint32_t(PDH_DATA_SOURCE_IS_LOG_FILE): return L"PDH_DATA_SOURCE_IS_LOG_FILE";
	case uint32_t(PDH_DATA_SOURCE_IS_REAL_TIME): return L"PDH_DATA_SOURCE_IS_REAL_TIME";
	case uint32_t(PDH_UNABLE_READ_LOG_HEADER): return L"PDH_UNABLE_READ_LOG_HEADER";
	case uint32_t(PDH_FILE_NOT_FOUND): return L"PDH_FILE_NOT_FOUND";
	case uint32_t(PDH_FILE_ALREADY_EXISTS): return L"PDH_FILE_ALREADY_EXISTS";
	case uint32_t(PDH_NOT_IMPLEMENTED): return L"PDH_NOT_IMPLEMENTED";
	case uint32_t(PDH_STRING_NOT_FOUND): return L"PDH_STRING_NOT_FOUND";
	case uint32_t(PDH_UNKNOWN_LOG_FORMAT): return L"PDH_UNKNOWN_LOG_FORMAT";
	case uint32_t(PDH_UNKNOWN_LOGSVC_COMMAND): return L"PDH_UNKNOWN_LOGSVC_COMMAND";
	case uint32_t(PDH_LOGSVC_QUERY_NOT_FOUND): return L"PDH_LOGSVC_QUERY_NOT_FOUND";
	case uint32_t(PDH_LOGSVC_NOT_OPENED): return L"PDH_LOGSVC_NOT_OPENED";
	case uint32_t(PDH_WBEM_ERROR): return L"PDH_WBEM_ERROR";
	case uint32_t(PDH_ACCESS_DENIED): return L"PDH_ACCESS_DENIED";
	case uint32_t(PDH_LOG_FILE_TOO_SMALL): return L"PDH_LOG_FILE_TOO_SMALL";
	case uint32_t(PDH_INVALID_DATASOURCE): return L"PDH_INVALID_DATASOURCE";
	case uint32_t(PDH_INVALID_SQLDB): return L"PDH_INVALID_SQLDB";
	case uint32_t(PDH_NO_COUNTERS): return L"PDH_NO_COUNTERS";
	case uint32_t(PDH_SQL_ALLOC_FAILED): return L"PDH_SQL_ALLOC_FAILED";
	case uint32_t(PDH_SQL_ALLOCCON_FAILED): return L"PDH_SQL_ALLOCCON_FAILED";
	case uint32_t(PDH_SQL_EXEC_DIRECT_FAILED): return L"PDH_SQL_EXEC_DIRECT_FAILED";
	case uint32_t(PDH_SQL_FETCH_FAILED): return L"PDH_SQL_FETCH_FAILED";
	case uint32_t(PDH_SQL_ROWCOUNT_FAILED): return L"PDH_SQL_ROWCOUNT_FAILED";
	case uint32_t(PDH_SQL_MORE_RESULTS_FAILED): return L"PDH_SQL_MORE_RESULTS_FAILED";
	case uint32_t(PDH_SQL_CONNECT_FAILED): return L"PDH_SQL_CONNECT_FAILED";
	case uint32_t(PDH_SQL_BIND_FAILED): return L"PDH_SQL_BIND_FAILED";
	case uint32_t(PDH_CANNOT_CONNECT_WMI_SERVER): return L"PDH_CANNOT_CONNECT_WMI_SERVER";
	case uint32_t(PDH_PLA_COLLECTION_ALREADY_RUNNING): return L"PDH_PLA_COLLECTION_ALREADY_RUNNING";
	case uint32_t(PDH_PLA_ERROR_SCHEDULE_OVERLAP): return L"PDH_PLA_ERROR_SCHEDULE_OVERLAP";
	case uint32_t(PDH_PLA_COLLECTION_NOT_FOUND): return L"PDH_PLA_COLLECTION_NOT_FOUND";
	case uint32_t(PDH_PLA_ERROR_SCHEDULE_ELAPSED): return L"PDH_PLA_ERROR_SCHEDULE_ELAPSED";
	case uint32_t(PDH_PLA_ERROR_NOSTART): return L"PDH_PLA_ERROR_NOSTART";
	case uint32_t(PDH_PLA_ERROR_ALREADY_EXISTS): return L"PDH_PLA_ERROR_ALREADY_EXISTS";
	case uint32_t(PDH_PLA_ERROR_TYPE_MISMATCH): return L"PDH_PLA_ERROR_TYPE_MISMATCH";
	case uint32_t(PDH_PLA_ERROR_FILEPATH): return L"PDH_PLA_ERROR_FILEPATH";
	case uint32_t(PDH_PLA_SERVICE_ERROR): return L"PDH_PLA_SERVICE_ERROR";
	case uint32_t(PDH_PLA_VALIDATION_ERROR): return L"PDH_PLA_VALIDATION_ERROR";
	case uint32_t(PDH_PLA_ERROR_NAME_TOO_LONG): return L"PDH_PLA_ERROR_NAME_TOO_LONG";
	case uint32_t(PDH_INVALID_SQL_LOG_FORMAT): return L"PDH_INVALID_SQL_LOG_FORMAT";
	case uint32_t(PDH_COUNTER_ALREADY_IN_QUERY): return L"PDH_COUNTER_ALREADY_IN_QUERY";
	case uint32_t(PDH_BINARY_LOG_CORRUPT): return L"PDH_BINARY_LOG_CORRUPT";
	case uint32_t(PDH_LOG_SAMPLE_TOO_SMALL): return L"PDH_LOG_SAMPLE_TOO_SMALL";
	case uint32_t(PDH_OS_LATER_VERSION): return L"PDH_OS_LATER_VERSION";
	case uint32_t(PDH_OS_EARLIER_VERSION): return L"PDH_OS_EARLIER_VERSION";
	case uint32_t(PDH_INCORRECT_APPEND_TIME): return L"PDH_INCORRECT_APPEND_TIME";
	case uint32_t(PDH_UNMATCHED_APPEND_COUNTER): return L"PDH_UNMATCHED_APPEND_COUNTER";
	case uint32_t(PDH_SQL_ALTER_DETAIL_FAILED): return L"PDH_SQL_ALTER_DETAIL_FAILED";
	case uint32_t(PDH_QUERY_PERF_DATA_TIMEOUT): return L"PDH_QUERY_PERF_DATA_TIMEOUT";
	default: return {};
	}
}

void rxtd::perfmon::pdh::writeType(std::wostream& stream, const PdhReturnCode& code, sview options) {
	auto viewOpt = formatPdhCode(code.code);
	if (viewOpt.has_value()) {
		stream << viewOpt.value();
	} else {
		stream << L"0x";
		stream << std::setfill(L'0') << std::setw(sizeof(code.code) * 2) << std::hex;
		stream << code.code;
	}
}
