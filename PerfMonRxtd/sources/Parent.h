/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "PerfMonRXTD.h"
#include "ContinuousBuffersHolder.h"
#include "expressions.h"

namespace pmr {
	class ParentData {
	public:
		TypeHolder* typeHolder;

	private:
		// read only options
		std::wstring objectName;

		// changeable options
		pmre::sortByType sortBy = pmre::SORTBY_NONE;
		int sortIndex = 0;
		pmre::sortOrderType sortOrder = pmre::SORTORDER_DESCENDING;
		int instanceIndexOffset = 0;
		bool rollup = false;
		pmre::rollupFunctionType sortRollupFunction = pmre::ROLLUP_SUM;
		bool syncRawFormatted = true;
		bool limitIndexOffset = false;

		// data

		void (ParentData::* generateNamesFunction)() = nullptr;
		pmre::getIDsType getIDs = pmre::GETIDS_NONE;
		PDH_HQUERY hQuery = nullptr;

		std::vector<PDH_HCOUNTER> hCounter;
		pmrcbh::ContinuousBuffersHolder<PDH_RAW_COUNTER_ITEM_W> rawBuffersCurrent;
		unsigned long itemCountCurrent = 0;
		pmrcbh::ContinuousBuffersHolder<PDH_RAW_COUNTER_ITEM_W> rawBuffersPrevious;
		unsigned long itemCountPrevious = 0;
		std::vector<pmrexp::ExpressionTreeNode> expressions;
		std::vector<pmrexp::ExpressionTreeNode> rollupExpressions;
		const instanceKeyItem* expCurrentItem = nullptr;

		std::vector<std::pair<std::wstring, bool>> blacklist;
		std::vector<std::pair<std::wstring, bool>> blacklistOrig;
		std::vector<std::pair<std::wstring, bool>> whitelist;
		std::vector<std::pair<std::wstring, bool>> whitelistOrig;

		// unique_reference_name -> value
		std::map<std::wstring_view, double> referencesCache;

		PDH_HCOUNTER hCounterID = nullptr;
		pmrda::DynamicArray<PDH_RAW_COUNTER_ITEM_W> idRawBuffer;
		unsigned long idItemCount = 0;

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

		/** Returns Raw value of a given counter. */
		double getRawValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType) const;
		/** Returns Formatted value of a given counter. */
		double getFormattedValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType);
		/** Returns value of Expression with given number. */
		double getExpressionValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType);
		/** Returns value of RollupExpression with given number. */
		double getRollupExpressionValue(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType);
		/** Returns count of the elements after black/white listing and rollup. */
		unsigned int getCount() const;

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
		const instanceKeyItem* findInstance(const std::wstring& name, unsigned long sortedIndex, bool instanceNameMatchPartial, bool useOrigName, pmre::nameSearchPlace searchPlace) const;
		/** Retrieves name from internal buffer */
		const wchar_t* getName(const instanceKeyItem& instance, pmre::rsltStringType stringType) const;
	private:
		void freeBuffers();
		const instanceKeyItem* findInstanceByNameInList(const std::wstring& name, bool instanceNameMatchPartial, bool useOrigName, const std::vector<instanceKeyItem>& instances) const;
		const instanceKeyItem* findInstanceByName(const std::wstring& name, bool instanceNameMatchPartial, bool useOrigName, bool useRollup, pmre::nameSearchPlace searchPlace) const;

		/** Parsing any "match" list */
		static void parseList(std::vector<std::pair<std::wstring, bool>>& list, const std::wstring& listString, bool upperCase);
		/** Checks if name matches list (with *fuzzy* match) */
		static bool nameIsInList(const std::vector<std::pair<std::wstring, bool>>& list, const wchar_t* name);
		/** Checks white/black lists */
		inline bool isInstanceAllowed(const wchar_t* searchName, const wchar_t* originalName) const;
		/** previous buffer <- current buffer <- new PerfMon snapshot */
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
		void sortInstanceKeys(std::vector<instanceKeyItem>& instances);

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

		inline long long calculateRaw(unsigned counterIndex, const instanceKeyItem& instance) const;
		double calculateFormatted(unsigned counterIndex, const instanceKeyItem& instance);
		double calculateRawRollup(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType) const;
		double calculateFormattedRollup(unsigned counterIndex, const instanceKeyItem& instance, pmre::rollupFunctionType rollupType);


		double resolveExpression(const pmrexp::ExpressionTreeNode& expression);
		double resolveReference(const pmrexp::reference& ref);

		double resolveExpressionRollup(const pmrexp::ExpressionTreeNode& expression, pmre::rollupFunctionType rollupFunction);
		double resolveRollupExpression(const pmrexp::ExpressionTreeNode& expression);
		double resolveRollupReference(const pmrexp::reference& ref);

		inline double extractFormattedValue(
			PDH_HCOUNTER hCounter,
			PDH_RAW_COUNTER_ITEM_W* lpRawBufferCurrent,
			PDH_RAW_COUNTER_ITEM_W* lpRawBufferPrevious,
			const indexesItem& item,
			PDH_FMT_COUNTERVALUE& formattedValue) const;
	};
}