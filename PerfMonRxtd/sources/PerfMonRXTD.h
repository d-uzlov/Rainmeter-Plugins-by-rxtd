/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

 //
 // Version history:
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
 // Fork by rxtd
 //
 // v1.0.0: Rewrote whole plugin from C to C++ to simplify maintenance.
 //         Added Expressions and RollupExpression and related options.
 //         Black and white lists can now work together.
 //         Added "SyncRawFormatted" option.
 //         Added "LimitIndexOffset" option.
 //         Type of Child measure can be changed in runtime.
 //
 // v1.1.0: Fixed: child measures with NUMBER have string value ""
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
 // v1.2.0: Changed: child measure searches for parent case insensitive
 //         Changed: "GetInstanceCount", "GetInstanceName" now deprecated
 //         Removed: mixed discarded search modes
 //         Changed: discarded search syntax is now boolean
 //         Added: "KeepDiscarded" parent option
 //         Changed: "Count" rollup function now deprecated
 //         Added: "GetCount" child type
 //         Added: "Count" expression keyword
 //         Added: Total values
 //         
 //         Code: common files moved to upper directory
 //         Code: common functions moved to separated file
 //         Code: unified retrieving of values for parent expressions and child measures
 //         Code: migrated all enums to enum classes
 //         Code: moved parentMeasuresVector to Parent.cpp
 //         Code: moved parentMeasuresVector managing to Parent.cpp
 //         Code: sane references cache management
 //         Code: added more comments
 //         
 // v1.2.1: Code: generified some methods in parent
 //         
 //         
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
 // The arrays of counters returned by PdhGetRawCounterArray for a given Perfmon Object will have the same
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

#pragma once
#include <string>
#include <vector>

#undef UNIQUE_NAME

namespace pmr {
	struct indexesItem {
		unsigned long originalCurrentInx;
		unsigned long originalPreviousInx;
	};

	struct instanceKeyItem {
		const wchar_t* sortName = nullptr;
		double sortValue;
		indexesItem originalIndexes;
		std::vector<indexesItem> vectorIndexes;
	};

	enum class ResultString : unsigned char {
		NUMBER,
		ORIGINAL_NAME,
		UNIQUE_NAME,
		DISPLAY_NAME,
	};

	class ParentData;
	class ChildData;

	struct TypeHolder {
		bool isParent = false;
		
		ParentData* parentData = nullptr;
		ChildData* childData = nullptr;

		// common data

		void* const rm = nullptr;
		void* const skin = nullptr;
		const std::wstring measureName;

		bool broken = false;
		bool temporaryBroken = false;

		double resultDouble = 0.0;
		const wchar_t* resultString = nullptr;

		TypeHolder(void* rm)
			: rm(rm),
			  skin(RmGetSkin(rm)),
			  measureName(RmGetMeasureName(rm)) {
		}
		~TypeHolder();
		TypeHolder(const TypeHolder& other) = delete;
		TypeHolder(TypeHolder&& other) = delete;
		TypeHolder& operator=(const TypeHolder& other) = delete;
		TypeHolder& operator=(TypeHolder&& other) = delete;

		bool isBroken() const {
			return broken || temporaryBroken;
		}
	};
}
