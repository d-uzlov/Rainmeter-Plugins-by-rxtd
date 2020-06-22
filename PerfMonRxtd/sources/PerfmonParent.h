/* 
 * Copyright (C) 2018-2019 rxtd
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

#include "expressions.h"
#include "TypeHolder.h"
#include "pdh/PdhWrapper.h"
#include "BlacklistManager.h"
#include "InstanceManager.h"
#include "ExpressionResolver.h"

namespace rxtd::perfmon {

	class PerfmonParent : public utils::TypeHolder {
	private:
		static utils::ParentManager<PerfmonParent> parentManager;

		string objectName;

		bool stopped = false;
		bool needUpdate = true;

		pdh::PdhWrapper pdhWrapper;

		pdh::Snapshot idSnapshot;
		pdh::Snapshot snapshotCurrent;
		pdh::Snapshot snapshotPrevious;

		BlacklistManager blacklistManager;

		InstanceManager instanceManager { logger, pdhWrapper, idSnapshot, snapshotCurrent, snapshotPrevious, blacklistManager };

		ExpressionResolver expressionResolver { logger, instanceManager };

		string bufferString;

	public:
		explicit PerfmonParent(utils::Rainmeter&& _rain);
		~PerfmonParent();
		/** This class is non copyable */
		PerfmonParent(const PerfmonParent& other) = delete;
		PerfmonParent(PerfmonParent&& other) = delete;
		PerfmonParent& operator=(const PerfmonParent& other) = delete;
		PerfmonParent& operator=(PerfmonParent&& other) = delete;

		static PerfmonParent* findInstance(utils::Rainmeter::Skin skin, isview measureName);

	protected:
		void _reload() override;
		std::tuple<double, const wchar_t*> _update() override;
		void _command(isview bangArgs) override;
		const wchar_t* _resolve(int argc, const wchar_t* argv[]) override;

	public:
		/** Parent measure can be stopped.
		 *  In this state it does not retrieve new information from PerfMon
		 *  but can be reloaded to change options. */
		void setStopped(bool value);

		void changeStopState();
		void setIndexOffset(item_t value);
		item_t getIndexOffset() const;

		double getValue(const Reference& ref, const InstanceInfo* instance, utils::Rainmeter::Logger& logger) const;

		counter_t getCountersCount() const;

		/** We only need one snapshot for raw values, but if sync is enabled then we'll wait for two snapshots */
		bool canGetRaw() const;
		/** We need two complete snapshots for formatted values values */
		bool canGetFormatted() const;
		
		const InstanceInfo* findInstance(const Reference& ref, item_t sortedIndex) const;
		
		sview getInstanceName(const InstanceInfo& instance, ResultString stringType) const;
	};

}
