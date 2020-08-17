/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include <set>
#include <functional>

#include "HandlerCacheHelper.h"
#include "RainmeterWrappers.h"
#include "sound-processing/Channel.h"
#include "sound-processing/sound-handlers/SoundHandler.h"
#include "audio-utils/filter-utils/FilterCascadeParser.h"

namespace rxtd::audio_analyzer {
	class ParamParser {
		using Logger = utils::Rainmeter::Logger;
		using Rainmeter = utils::Rainmeter;

	public:
		struct HandlerPatchersInfo {
			std::vector<istring> order;
			std::map<istring, PatchInfo*, std::less<>> patchers;
		};

		struct ProcessingData {
			string rawFccDescription;
			audio_utils::FilterCascadeCreator fcc;

			index targetRate{ };

			std::set<Channel> channels;
			HandlerPatchersInfo handlersInfo;
		};

		using ProcessingsInfoMap = std::map<istring, ProcessingData, std::less<>>;

	private:
		Rainmeter rain;

		bool unusedOptionsWarning = true;
		index defaultTargetRate = 44100;
		ProcessingsInfoMap parseResult;
		index legacyNumber = 0;
		HandlerCacheHelper hch;

		mutable bool anythingChanged = false;

	public:
		ParamParser() = default;

		~ParamParser() = default;
		/** This class is non copyable */
		ParamParser(const ParamParser& other) = delete;
		ParamParser(ParamParser&& other) = delete;
		ParamParser& operator=(const ParamParser& other) = delete;
		ParamParser& operator=(ParamParser&& other) = delete;

		void setRainmeter(Rainmeter value) {
			rain = value;
			hch.setRain(value);
		}

		// return true is there were any changes since last update, false if there were none
		bool parse();

		[[nodiscard]]
		const ProcessingsInfoMap& getParseResult() const {
			return parseResult;
		}

		[[nodiscard]]
		index getLegacyNumber() const {
			return legacyNumber;
		}

	private:
		void parseFilters(const utils::OptionMap& optionMap, ProcessingData& data, Logger& cl) const;
		void parseTargetRate(const utils::OptionMap& optionMap, ProcessingData& data, Logger& cl) const;

		[[nodiscard]]
		static bool checkListUnique(const utils::OptionList& list);

		void parseProcessing(sview name, Logger cl, ProcessingData& oldData);

		[[nodiscard]]
		std::set<Channel> parseChannels(const utils::OptionList& channelsStringList, Logger& logger) const;

		[[nodiscard]]
		HandlerPatchersInfo parseHandlers(const utils::OptionList& indices);
	};
}
