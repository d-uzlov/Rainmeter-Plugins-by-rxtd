/* This plugin was originally written by buckb, then was forked by rxtd from version 2.0.1.0.
 * To prevent compatibility issues names was changed from PerfMonPDH to PerfMonRXTD.
 * Version was reset to 1.0.0 due to name change.
 *
 * Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */


 //
 // PerfMonPDH Plugin for Rainmeter
 // Author: buckb
 //
 // v1.0.0.0: initial release
 //
 // v1.0.0.1: suppress log entries for PDH "negative error" status codes
 //
 // v2.0.0.0: added rollup, removed "unsorted" child measures, renamed various options
 //
 // v2.0.0.1: moved two-samples check from buildInstanceKeys to updateParent
 //
 // v2.0.1.0: mod by rxtd: added rollup for LogicalDisk category
 //
 // fork by rxtd
 //
 // v1.0.0: Rewrote whole plugin from C to C++ to simplify maintenance.
 //         Added Expressions and RollupExpression and related options.
 //         Black and white lists can now work together.
 //         Added "SyncRawFormatted" option.
 //         Added "LimitIndexOffset" option.
 //         Type of Child measure can be changed in runtime.
 //
 // v1.1.0: Fixed: child measures with RESULTSTRING_NUMBER have string value ""
 //         Fixed: sort by name works incorrectly with non ASCII letters
 //         Fixed: expressions rollup for min/max values solved incorrectly
 //         Fixed: expressions does not recognize floating pointer numbers
 //         Fixed: some valid math expressions can't be parsed (e.g. "1 - 2 + 3")
 //         Fixed: some expressions can crash plugin (e.g. "-1")
 //         Fixed: very long process names could potentially crash plugin when using "GPU Engine" and "GPU Process Memory" objects
 //         Added: child option "SearchOriginalName" (bool)
 //         Added: child option "NameSource" (enum)
 //         Added: expressions now support "SearchOriginalName" and "NameSource"
 //         Added: WhitelistOrig, BlacklistOrig options: case sensitive, match original names instead of display names
 //         Added: "LogicalDisk" can be rolled up by mount folder (ported from PerfMonPDH 2.0.1.3, thanks to buckb for the idea and code)
 //         Change: display name and rollup name merged into display name
 //         Change: black/white lists are case insensitive
 //         Change: "LogicalDisk" have three Display names: Original, DriveLetter, MountFolder. If Original is set then rollup can not be performed.
 //         Change: "GPU Engine" and "GPU Process Memory": removed "Gpu" from display names. Old options are still supported
 //         Change: ResultString names: removed "Instance" part. Old names are still supported
 //         Change: removed counters max number limit
 //         Change: if sortBy set to expressions, but no expressions found, then sort is set to none instead of breaking the measure
 //         Change: LimitIndexOffset is set to false by default
 //         Code: parent and child initializers merged with constructors
 //         Code: adjusted comments
 //         Code: removed unnecessary memory reallocations
 //         Code: rollup keys building optimization
 //         Code: add bounds checks in public get Raw/Formatted methods
 //         
 //         
 //

 //
 // PdhCollectQueryData collects data for one or more counters previously added to a query.  The counters can all be from the
 // same PerfMon Object, or from different PerfMon Objects.  Query data can then be retrieved using PdhGetRawCounterArray or
 // PdhGetFormattedCounterArray.  The returned arrays contain one or more instances of a given counter.  Separate calls to
 // PdhGetRawCounterArray or PdhGetFormattedCounterArray must be made for each individual counter.
 //
 // PDH maintains counter data for both the current and previous PdhCollectQueryData calls, but there is a twist.  Between two
 // calls to PdhCollectQueryData, it is possible for the instances of a given counter to change.  For example, this happens when
 // removable disks are inserted/removed, and when processes start/end.  A call to PdhGetRawCounterArray always succeeds because
 // only the most recent counter values are returned.  However, a call to PdhGetFormattedCounterArray, which needs to use both the
 // current and previous counter values, fails with PDH_CSTATUS_INVALID_DATA if the instances have changed between the two calls.
 //
 // This makes PdhGetFormattedCounterArray useless unless we are certain that instances will not change, or unless the "hiccup"
 // caused by the failed PdhGetFormattedCounterArray call is tolerable.  In order to maintain a seamless stream of performance
 // counter results, this plugin does its own matching of current and previous instances, and calls PdhCalculateCounterFromRawValue
 // "manually" to get formatted results.
 //
 // This plugin assumes that the arrays of counters returned by PdhGetRawCounterArray for a given Perfmon Object will have the same
 // number of instances, and that the instances are ordered identically, provided they come from the same PdhCollectQueryData call.
 // That is, counterA[n] and counterB[n] refer to the same unique instance for a given n.  Using LogicalDisk as an example, it is
 // assumed that if "Disk Read Bytes/sec" returns an array of 3 instances ordered {index 0 is C:, index 1 is D:, index 2 is E:}, then
 // the array for "Disk Write Bytes/sec" will also have 3 instances that are ordered {index 0 is C:, index 1 is D:, index 2 is E:}.
 //
 // PdhCalculateCounterFromRawValue sometimes fails with the status PDH_CALC_NEGATIVE_VALUE, PDH_CALC_NEGATIVE_DENOMINATOR, or
 // PDH_CALC_NEGATIVE_TIMEBASE.  This has mainly been observed for "_Total" instances, and occurs when an instance has dropped out
 // between two query calls.  When a process ends, or when a removable drive is ejected, its counters disappear.  "_Total" thus
 // decreases between the two query calls.
 //
 // Access to ObjectName="Processor Performance" requires that Rainmeter run with Administrator privileges.  If not, PdhAddCounter
 // fails with an error code that says the object doesn't exist.  Are there other objects and/or counters that require Administrator
 // privileges?
 //
 // The current and previous raw buffers are foundational data sources.  Other data structures maintain pointers and indexes into
 // these buffers.  Be careful to invalidate or delete these pointers when the underlying raw buffers are changed or deleted.
 //
 //
 // Object       Original                     Unique                       Display                      DisplayName
 // -----------  -------------------------    -------------------------    -------------------------    -------------------
 // Process      Rainmeter                    Rainmeter#1234               Rainmeter
 // Thread       Rainmeter/12                 Rainmeter#5678               Rainmeter
 // GPU          pid_1234..engtype_3D         pid_1234..engtype_3D         pid_1234..engtype_3D         (Original)
 // GPU          pid_1234..engtype_3D         pid_1234..engtype_3D         Rainmeter                    (GpuProcesName)
 // GPU          pid_1234..engtype_3D         pid_1234..engtype_3D         engtype_3D                   (GpuEngType)
 // LogicalDisk  D:\mount\disk1               D:\mount\disk1               D:\mount\disk1               (Original)
 // LogicalDisk  D:\mount\disk1               D:\mount\disk1               D:                           (DriveLetter)
 // LogicalDisk  D:\mount\disk1               D:\mount\disk1               D:\mount\                    (MountFolder)
 // 
 // PhysicalDisk 0 C:                         0 C:                         0 C:                         0 C:
 // Net          Realtek PCIe GBE             Realtek PCIe GBE             Realtek PCIe GBE             Realtek PCIe GBE
 //
 //
 //
 // whitelist/blacklist filtering matches against DisplayNames
 // vectorInstanceKeys[n].instanceNames are DisplayNames
 // vectorRollupKeys[n].instanceNames are RollupNames
 //

#include <windows.h>
#include <vector>
#include <string>
#include <cwctype>
#include "RainmeterAPI.h"

#include "PerfMonRXTD.h"
#include "Parent.h"
#include "Child.h"

#pragma comment(lib, "pdh.lib")

// global variables

std::vector<pmr::ParentData*> pmr::parentMeasuresVector;

bool pmr::stringsMatch(LPCWSTR str1, LPCWSTR str2, bool matchPartial) {
	if (matchPartial) {
		// check if substring str2 occurs in str1
		if (wcsstr(str1, str2) != nullptr)
			return true;
	} else {
		// check if str1 and str2 match exactly
		if (wcscmp(str1, str2) == 0)
			return true;
	}

	return false;
}
std::vector<std::wstring> pmr::Tokenize(const std::wstring& str, const std::wstring& delimiters) {
	std::vector<std::wstring> tokens;

	// Copied from Rainmeter library

	size_t pos = 0;
	do {
		size_t lastPos = str.find_first_not_of(delimiters, pos);
		if (lastPos == std::wstring::npos) break;

		pos = str.find_first_of(delimiters, lastPos + 1);
		std::wstring token = str.substr(lastPos, pos - lastPos);  // len = (pos != std::wstring::npos) ? pos - lastPos : pos

		size_t pos2 = token.find_first_not_of(L" \t\r\n");
		if (pos2 != std::wstring::npos) {
			size_t lastPos2 = token.find_last_not_of(L" \t\r\n");
			if (pos2 != 0 || lastPos2 != (token.size() - 1)) {
				// Trim white-space
				token.assign(token, pos2, lastPos2 - pos2 + 1);
			}
			tokens.push_back(token);
		}

		if (pos == std::wstring::npos) break;
		++pos;
	} while (true);

	return tokens;
}

pmr::TypeHolder::~TypeHolder() {
	if (isParent) {
		delete parentData;
	} else {
		delete childData;
	}
}
PLUGIN_EXPORT void Initialize(void** data, void* rm) {
	pmr::TypeHolder * typeHolder = new pmr::TypeHolder;
	*data = typeHolder;
	typeHolder->rm = rm;
	typeHolder->skin = RmGetSkin(rm);
	typeHolder->measureName = RmGetMeasureName(rm);

	//RmLogF(measure->typeHolder->rm, LOG_DEBUG, L"initialize '%s'", measure->measureName.c_str());

	LPCWSTR str = RmReadString(rm, L"Type", L"");
	if (_wcsicmp(str, L"Parent") == 0) {
		typeHolder->isParent = true;

		pmr::ParentData* parentData = new pmr::ParentData(typeHolder);
		typeHolder->parentData = parentData;

		pmr::parentMeasuresVector.push_back(parentData);

		return;
	}

	typeHolder->isParent = false;
	pmr::ChildData *childData = new pmr::ChildData(typeHolder);
	typeHolder->childData = childData;
}
PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue) {
	if (data == nullptr) {
		return;
	}
	// Reload is called:
	//   immediately after Initialize
	//   at every Update when DynamicVariables=1
	//   when !SetOption is used on the measure, even if DynamicVariables=0
	//
	// Rainmeter docs say:
	//   "maxValue: Pointer to a double that can be assigned to the default maximum value for this measure.
	//    A value of 0.0 will make it based on the highest value returned from the Update function.
	//    Do not set maxValue unless necessary."
	//
	// *maxValue = 0.0;

	auto typeHolder = static_cast<pmr::TypeHolder*>(data);
	// we skip reload only if the measure is unrecoverable
	if (typeHolder->broken) {
		return;
	}

	if (typeHolder->isParent) {
		if (typeHolder->parentData != nullptr) {
			typeHolder->parentData->reload();
		}
	} else {
		if (typeHolder->childData != nullptr) {
			typeHolder->childData->reload();
		}
	}
}

PLUGIN_EXPORT double Update(void* data) {
	if (data == nullptr) {
		return 0.0;
	}
	pmr::TypeHolder * typeHolder = static_cast<pmr::TypeHolder*>(data);
	if (typeHolder->isBroken()) {
		return 0.0;
	}

	// set default measure values: number=0, string=""
	typeHolder->resultDouble = 0;

	if (typeHolder->isParent) {
		if (typeHolder->parentData != nullptr) {
			typeHolder->parentData->update();
		}
	} else {
		if (typeHolder->childData != nullptr) {
			typeHolder->childData->update();
		}
	}

	return typeHolder->resultDouble;
}
PLUGIN_EXPORT LPCWSTR GetString(void* data) {
	if (data == nullptr) {
		return nullptr;
	}
	pmr::TypeHolder * typeHolder = static_cast<pmr::TypeHolder*>(data);
	if (typeHolder->isBroken()) {
		return L"broken";
	}
	return typeHolder->resultString;
}
PLUGIN_EXPORT void Finalize(void* data) {
	if (data == nullptr) {
		return;
	}
	pmr::TypeHolder * typeHolder = static_cast<pmr::TypeHolder*>(data);

	delete typeHolder;
}
PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args) {
	if (data == nullptr) {
		return;
	}
	pmr::TypeHolder * typeHolder = static_cast<pmr::TypeHolder*>(data);
	if (typeHolder->isBroken()) {
		RmLogF(typeHolder->rm, LOG_WARNING, L"Skipping bang on the broken measure '%s'", typeHolder->measureName.c_str());
		return;
	}
	if (!typeHolder->isParent) {
		RmLogF(typeHolder->rm, LOG_WARNING, L"Skipping bang on child measure '%s'", typeHolder->measureName.c_str());
		return;
	}
	if (_wcsicmp(args, L"Stop") == 0) {
		typeHolder->parentData->setStopped(true);
		return;
	}
	if (_wcsicmp(args, L"Resume") == 0) {
		typeHolder->parentData->setStopped(false);
		return;
	}
	if (_wcsicmp(args, L"StopResume") == 0) {
		typeHolder->parentData->changeStopState();
		return;
	}
	std::wstring str = args;
	if (_wcsicmp(str.substr(0, 14).c_str(), L"SetIndexOffset") == 0) {
		std::wstring arg = str.substr(14);
		arg = pmr::trimSpaces(arg);
		if (arg[0] == L'-' || arg[0] == L'+') {
			const int offset = typeHolder->parentData->getIndexOffset() + _wtoi(arg.c_str());
			typeHolder->parentData->setIndexOffset(offset);
		} else {
			const int offset = _wtoi(arg.c_str());
			typeHolder->parentData->setIndexOffset(offset);
		}
	}
}