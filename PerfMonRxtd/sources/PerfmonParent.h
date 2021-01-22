/* 
 * Copyright (C) 2018-2021 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

/*
 * Parent measure in general does the following things:
   • Fetch new data from PerfMon
   • Create derived unique, sort and rollup names
   • Apply black and white lists to data
   • [optional] Perform rollup of instances sharing the same sort name
   • [optional] Calculate sort values (if need to sort by value) and perform sorting
   • Calculate values requested by Child measures

	 The longest operation is fetch, if used with complex PerfMon Objects such as Process.
   If only one simple skin with this plugin is present in Rainmeter then fetch (of Process Object)
   takes ~70-80% of all CPU time spent in Rainmeter application.
	 It is possible to make the plugin multithreaded by extracting fetch() function to another thread.
   It will also require doing something with name generation because creation of unique names and using
   them will probably be done in separated threads.
	 The advantage of such multithreaded case would not only be lowering the performance impact on Rainmeter
   but also lowering overall CPU performance impact because multiple parent measures fetching data from the same Performance Object
   could use one dataset instead of querying PerfMon several times. One another feature would be to perform several
   blacklistings and sortings on the same dataset.
	 Then why doesn't this plugin made this way? Because with complex skins, even when querying complex PerfMon Objects,
   CPU time spent in this plugin (whole plugin, including fetch, rollup and sort) is less than 10%,
   which is tolerable, and managing multithreading is not easy. I decided that multithreading doesn't worth the time it requires.


	 To perform sorting there are 3 possible ways:
   • Do not sort (if none)
   • Sort by names (which has been already assigned to instances)
   • Sort by values
   The latter needs some calculations. All values for are computed once for every instance and then used for sort only.

	 There are 3 caches that should make searching named instances and using totals fast.
   • foundNamesInstances and foundNamesRollup keep links to instances that match some search request, so that linear search
   of the whole instances array is only performed once for every request parameters. It should allow abused use of named instances.
   • totalsCache keeps all calculated total values for obvious purpose
   These caches are purged on every update so they don't contain outdated values.


	 To make black/white lists, name search and sorting case insensitive while keeping it as fast as possible
   plugin converts names to upper case. These converted names are used internally only
   and are not accessible by Child measure.




 */

#pragma once

#include "BlacklistManager.h"
#include "ExpressionResolver.h"
#include "expressions.h"
#include "InstanceManager.h"
#include "TypeHolder.h"
#include "pdh/PdhWrapper.h"

namespace rxtd::perfmon {

	class PerfmonParent : public utils::ParentBase {
		enum class State {
			eFETCH_ERROR,
			eNO_DATA,
			eOK,
		};

		State state = State::eOK;

		string objectName;

		bool stopped = false;
		bool needUpdate = true;

		pdh::PdhWrapper pdhWrapper;

		pdh::PdhSnapshot snapshotCurrent;
		pdh::PdhSnapshot snapshotPrevious;

		BlacklistManager blacklistManager;

		InstanceManager instanceManager{ logger, pdhWrapper, snapshotCurrent, snapshotPrevious, blacklistManager };

		ExpressionResolver expressionResolver{ logger, instanceManager };

	public:
		explicit PerfmonParent(utils::Rainmeter&& _rain);

	protected:
		void vReload() override;
		double vUpdate() override;
		void vUpdateString(string& resultStringBuffer) override;
		void vCommand(isview bangArgs) override;
		void vResolve(array_view<isview> args, string& resolveBufferString) override;

	public:
		std::pair<double, sview> getValues(const Reference& ref, index sortedIndex, ResultString stringType, utils::Rainmeter::Logger& logger) const {
			if (!instanceManager.canGetRaw() || ref.type == ReferenceType::COUNTER_FORMATTED && !instanceManager.canGetFormatted()) {
				return { 0, ref.total ? L"Total" : L"" };
			}

			if (ref.total) {
				return { expressionResolver.getValue(ref, nullptr, logger), L"Total" };
			} else {
				auto instance = instanceManager.findInstance(ref, sortedIndex);
				return { expressionResolver.getValue(ref, instance, logger), getInstanceName(*instance, stringType) };
			}
		}

		sview getInstanceName(const InstanceInfo& instance, ResultString stringType) const;
	};
}
