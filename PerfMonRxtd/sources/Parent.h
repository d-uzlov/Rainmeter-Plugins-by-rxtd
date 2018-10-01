/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
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
	  It is possible to make the plugin multithreaded by extracting Parent#fetchNewData() function to another thread.
	It will also require doing something with mane generation because creation of unique names and using
	them will probably be done in separated threads.
	  The advantage of such multithreaded case would not only be lowering the performance impact on Rainmeter
	but also lowering overall CPU performance impact because multiple parent measures fetching data from the same Performance Object
	could use one dataset instead of querying PerfMon several times. One another feature would be to perform several
	blacklistings and sortings on the same dataset.
	  Then why doesn't this plugin made this way? Because with complex skins, even when querying complex PerfMon Objects,
	CPU time spent in this plugin (whole plugin, including fetch, rollup and sort) is less than 10%,
	which is tolerable, and managing multithreading is not easy. I decided that multithreading doesn't worth the time it requires.


	  Names creation can change only on reload, so in reload function plugin sets function for name creation
	and on update it only uses pointer to this function. All functions for name creation are in ParentData class.
	  All names are created on every update, regardless of whether they used.

	  To perform sorting there are 3 possible ways:
	• Do not sort (if none)
	• Sort by names (which has been already assigned to instances)
	• Sort by values
	The latter needs some calculations. All values for are computed once for every instance and then used for sort only.

	  All expressions are kept as a tree, and are calculated recursively.
	Both resolveReference functions use expCurrentItem member variable to know what current resolved item is,
	so it is needed to be set to resolved item.

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
#include <Pdh.h>
#include <map>
#include <vector>

#include "PerfMonRXTD.h"
#include "ContinuousBuffersHolder.h"
#include "expressions.h"

namespace pmr {
	enum class SortBy : unsigned char {
		NONE,
		INSTANCE_NAME,
		RAW_COUNTER,
		FORMATTED_COUNTER,
		EXPRESSION,
		ROLLUP_EXPRESSION,
		COUNT,
	};
	enum class TotalSource : unsigned char {
		RAW_COUNTER,
		FORMATTED_COUNTER,
		EXPRESSION,
		ROLLUP_EXPRESSION,
	};
	enum class SortOrder : unsigned char {
		ASCENDING,
		DESCENDING,
	};

	struct modifiedNameItem {
		const wchar_t* originalName;
		const wchar_t* uniqueName;
		const wchar_t* displayName;
		const wchar_t* searchName;
	};

	class ParentData {
	public:
		TypeHolder* const typeHolder;

	private:
		// read only options
		std::wstring objectName;

		// changeable options
		SortBy sortBy = SortBy::NONE;
		int sortIndex = 0;
		SortOrder sortOrder = SortOrder::DESCENDING;
		int instanceIndexOffset = 0;
		bool rollup = false;
		pmre::RollupFunction sortRollupFunction = pmre::RollupFunction::SUM;
		bool syncRawFormatted = true;
		bool limitIndexOffset = false;
		bool keepDiscarded = false;

		// data

		void (ParentData::* generateNamesFunction)() = nullptr;
		bool needFetchExtraIDs = false;
		PDH_HQUERY hQuery = nullptr;

		std::vector<PDH_HCOUNTER> hCounter;
		pmrcbh::ContinuousBuffersHolder<PDH_RAW_COUNTER_ITEM_W> rawBuffersCurrent;
		unsigned long itemCountCurrent = 0;
		pmrcbh::ContinuousBuffersHolder<PDH_RAW_COUNTER_ITEM_W> rawBuffersPrevious;
		unsigned long itemCountPrevious = 0;

		PDH_HCOUNTER hCounterID = nullptr;
		pmrda::DynamicArray<PDH_RAW_COUNTER_ITEM_W> idRawBuffer;
		unsigned long idItemCount = 0;

		std::vector<pmrexp::ExpressionTreeNode> expressions;
		std::vector<pmrexp::ExpressionTreeNode> rollupExpressions;

		mutable const instanceKeyItem* expCurrentItem = nullptr;

		// (name template, fuzzy match)
		std::vector<std::pair<std::wstring, bool>> blacklist;
		std::vector<std::pair<std::wstring, bool>> blacklistOrig;
		std::vector<std::pair<std::wstring, bool>> whitelist;
		std::vector<std::pair<std::wstring, bool>> whitelistOrig;

		// (use orig name, is discarded, name) -> instanceInfo
		mutable std::map<std::tuple<bool, bool, std::wstring_view>, const instanceKeyItem*> foundNamesInstances;
		mutable decltype(foundNamesInstances) foundNamesRollup;
		// (source, counterIndex, rollupFunction) -> value
		mutable std::map<std::tuple<TotalSource, unsigned int, pmre::RollupFunction>, double> totalsCache;

		pmrda::DynamicArray<modifiedNameItem> namesCurrent;
		pmrda::DynamicArray<modifiedNameItem> namesPrevious;
		pmrda::DynamicArray<wchar_t> namesBufferCurrent;
		pmrda::DynamicArray<wchar_t> namesBufferPrevious;

		std::vector<instanceKeyItem> vectorInstanceKeys;
		std::vector<instanceKeyItem> vectorRollupKeys;
		std::vector<instanceKeyItem> vectorDiscardedKeys;

		bool stopped = false;
		bool needUpdate = true;
		wchar_t resultString[256] { };

	public:
		/** All plugin initialization is done in the ctor */
		explicit ParentData(TypeHolder* typeHolder);
		~ParentData();
		/** This class is non copyable */
		ParentData(const ParentData& other) = delete;
		ParentData(ParentData&& other) = delete;
		ParentData& operator=(const ParentData& other) = delete;
		ParentData& operator=(ParentData&& other) = delete;

		/** Re-read non-constant measure options */
		void reload();
		void update();
		/** Parent measure can be stopped. In this state it does not retrieve new information from PerfMon but can be reloaded. */
		void setStopped(bool value);
		/** Switches stopped state */
		void changeStopState();
		void setIndexOffset(int value);
		int getIndexOffset() const;

		double getValue(const pmrexp::reference& ref, const instanceKeyItem* instance, void* rm) const;

		size_t getCountersCount() const {
			return hCounter.size();
		}
		bool isRollupEnabled() const {
			return rollup;
		}

		/** We only need one snapshot for raw values, but if sync is enabled then we'll wait for two snapshots */
		bool canGetRaw() const;
		/** We need two complete snapshots for formatted values values */
		bool canGetFormatted() const;
		/** Searches for instance with given name or index */
		const instanceKeyItem* findInstance(const pmrexp::reference& ref, unsigned long sortedIndex) const;
		/** Retrieves name from internal buffer */
		const wchar_t* getName(const instanceKeyItem& instance, pmre::ResultString stringType) const;
	private:
		void freeBuffers();
		const instanceKeyItem* findInstanceByNameInList(const pmrexp::reference& ref, const std::vector<instanceKeyItem>& instances) const;
		const instanceKeyItem* findInstanceByName(const pmrexp::reference& ref, bool useRollup) const;

		/** Parsing any "match" list */
		static void parseList(std::vector<std::pair<std::wstring, bool>>& list, const std::wstring& listString, bool upperCase);
		/** Checks if name matches list (with *fuzzy* match) */
		static bool nameIsInList(const std::vector<std::pair<std::wstring, bool>>& list, const wchar_t* name);
		/** Checks white/black lists */
		inline bool isInstanceAllowed(const wchar_t* searchName, const wchar_t* originalName) const;
		/** previous buffer <- current buffer <- new PerfMon snapshot
		 * @returns false if error occurred, true otherwise
		 */
		bool fetchNewData();

		/** Searches instance in previous buffer with unique name matching name of currentIndex qwe */
		long findPreviousName(unsigned long currentIndex) const;
		/** Fill ParentData::vectorInstanceKeys with checking for white/black lists and search for item in previous PrefMon snapshot */
		void buildInstanceKeys();
		/** Same as ParentData::buildInstanceKeys() but don't search items in previous buffer as this function is called when no previous data exists */
		void buildInstanceKeysZero();
		/** Search thru ParentData::vectorInstanceKeys and combine instances whose display names match */
		void buildRollupKeys();
		/** Sorts given vector according to current settings */
		void sortInstanceKeys(bool rollup);

		static long long copyUpperCase(const wchar_t* source, wchar_t* dest);
		/** Transforms names (fills names buffer) */
		void createModifiedNames();
		void modifyNameNone();
		void modifyNameProcess();
		void modifyNameThread();
		void modifyNameLogicalDiskDriveLetter();
		void modifyNameLogicalDiskMountPath();
		void modifyNameGPUProcessName();
		void modifyNameGPUEngtype();


		double calculateTotal(TotalSource source, unsigned counterIndex, pmre::RollupFunction rollupFunction) const;

		const instanceKeyItem* findAndCacheName(const pmrexp::reference& ref, bool useRollup) const;
		double calculateAndCacheTotal(TotalSource source, unsigned int counterIndex, pmre::RollupFunction rollupFunction) const;

		inline long long calculateRaw(unsigned counterIndex, const instanceKeyItem& instance) const;
		double calculateRawRollup(unsigned counterIndex, const instanceKeyItem& instance, pmre::RollupFunction rollupType) const;
		double calculateRawTotal(unsigned counterIndex, pmre::RollupFunction rollupType) const;

		double calculateFormatted(unsigned counterIndex, const instanceKeyItem& instance) const;
		double calculateFormattedRollup(unsigned counterIndex, const instanceKeyItem& instance, pmre::RollupFunction rollupType) const;
		double calculateFormattedTotal(unsigned counterIndex, pmre::RollupFunction rollupType) const;

		double resolveExpression(const pmrexp::ExpressionTreeNode& expression) const;
		double resolveReference(const pmrexp::reference& ref) const;
		double calculateExpressionRollup(const pmrexp::ExpressionTreeNode& expression, pmre::RollupFunction rollupFunction) const;
		double calculateExpressionTotal(const pmrexp::ExpressionTreeNode& expression, pmre::RollupFunction rollupFunction) const;
		double calculateCountTotal(pmre::RollupFunction rollupFunction) const;

		double resolveRollupExpression(const pmrexp::ExpressionTreeNode& expression) const;
		double resolveRollupReference(const pmrexp::reference& ref) const;
		double calculateRollupExpressionTotal(const pmrexp::ExpressionTreeNode& expression, pmre::RollupFunction rollupFunction) const;
		double calculateRollupCountTotal(pmre::RollupFunction rollupFunction) const;

		inline double extractFormattedValue(
			PDH_HCOUNTER hCounter,
			const PDH_RAW_COUNTER_ITEM_W* lpRawBufferCurrent,
			const PDH_RAW_COUNTER_ITEM_W* lpRawBufferPrevious,
			const indexesItem& item,
			PDH_FMT_COUNTERVALUE& formattedValue) const;
	};
}